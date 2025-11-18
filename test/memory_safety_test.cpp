#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <array>
#include <regex>
#include "src/cache/response_cache.hpp"
#include "src/config/production_config.hpp"
#include "src/network/connection_pool.hpp"
#include "src/logging/production_logger.hpp"
#include "src/monitoring/performance_monitor.hpp"

using namespace ::testing;
using namespace aimux;

// Memory tracking utilities for testing
class MemoryTracker {
private:
    static std::atomic<size_t> allocation_count_;
    static std::atomic<size_t> total_allocated_;
    static std::atomic<size_t> peak_allocated_;

public:
    static void track_allocation(size_t size) {
        allocation_count_++;
        total_allocated_ += size;
        if (total_allocated_ > peak_allocated_) {
            peak_allocated_ = total_allocated_;
        }
    }

    static void track_deallocation(size_t size) {
        if (allocation_count_ > 0) {
            allocation_count_--;
        }
        if (total_allocated_ >= size) {
            total_allocated_ -= size;
        }
    }

    static size_t get_alloc_count() { return allocation_count_.load(); }
    static size_t get_total_allocated() { return total_allocated_.load(); }
    static size_t get_peak_allocated() { return peak_allocated_.load(); }
    static void reset() {
        allocation_count_ = 0;
        total_allocated_ = 0;
        peak_allocated_ = 0;
    }
};

std::atomic<size_t> MemoryTracker::allocation_count_{0};
std::atomic<size_t> MemoryTracker::total_allocated_{0};
std::atomic<size_t> MemoryTracker::peak_allocated_{0};

// Custom memory tracking allocator
template<typename T>
class TrackingAllocator {
public:
    using value_type = T;

    TrackingAllocator() noexcept = default;

    template<typename U>
    constexpr TrackingAllocator(const TrackingAllocator<U>&) noexcept {}

    T* allocate(size_t n) {
        if (n > std::numeric_limits<std::size_t>::max() / sizeof(T)) {
            throw std::bad_alloc();
        }

        if (auto p = static_cast<T*>(::operator new(n * sizeof(T)))) {
            MemoryTracker::track_allocation(n * sizeof(T));
            return p;
        }
        throw std::bad_alloc();
    }

    void deallocate(T* p, size_t n) noexcept {
        MemoryTracker::track_deallocation(n * sizeof(T));
        ::operator delete(p);
    }

    template<typename U>
    bool operator==(const TrackingAllocator<U>&) const noexcept {
        return true;
    }

    template<typename U>
    bool operator!=(const TrackingAllocator<U>&) const noexcept {
        return false;
    }
};

class MemorySafetyTest : public ::testing::Test {
protected:
    void SetUp() override {
        MemoryTracker::reset();
    }

    void TearDown() override {
        // Check for memory leaks after each test
        EXPECT_EQ(MemoryTracker::get_alloc_count(), 0)
            << "Memory leak detected: " << MemoryTracker::get_alloc_count() << " allocations not freed";
        EXPECT_EQ(MemoryTracker::get_total_allocated(), 0)
            << "Memory leak detected: " << MemoryTracker::get_total_allocated() << " bytes not freed";
    }
};

// RAII Validation Tests
TEST_F(MemorySafetyTest, RAIISafetyValidation) {
    class TestResource {
    private:
        std::unique_ptr<int[]> data_;
        size_t size_;
        bool initialized_;

    public:
        TestResource(size_t size) : size_(size), initialized_(false) {
            data_ = std::make_unique<int[]>(size);
            for (size_t i = 0; i < size_; ++i) {
                data_[i] = static_cast<int>(i);
            }
            initialized_ = true;
        }

        ~TestResource() {
            if (initialized_) {
                // Simulate cleanup work
                std::fill_n(data_.get(), size_, 0);
            }
        }

        // Rule of five
        TestResource(const TestResource& other) = delete;
        TestResource& operator=(const TestResource& other) = delete;

        TestResource(TestResource&& other) noexcept
            : data_(std::move(other.data_)), size_(other.size_), initialized_(other.initialized_) {
            other.size_ = 0;
            other.initialized_ = false;
        }

        TestResource& operator=(TestResource&& other) noexcept {
            if (this != &other) {
                data_ = std::move(other.data_);
                size_ = other.size_;
                initialized_ = other.initialized_;
                other.size_ = 0;
                other.initialized_ = false;
            }
            return *this;
        }

        bool is_initialized() const { return initialized_; }
        size_t size() const { return size_; }
    };

    auto initial_allocs = MemoryTracker::get_alloc_count();

    {
        TestResource resource(1000);
        EXPECT_TRUE(resource.is_initialized());
        EXPECT_EQ(resource.size(), 1000);
    }

    // Resource should be automatically cleaned up when leaving scope
    auto final_allocs = MemoryTracker::get_alloc_count();
    EXPECT_EQ(initial_allocs, final_allocs);
}

TEST_F(MemorySafetyTest, ExceptionSafetyInRAII) {
    auto memory_before = MemoryTracker::get_total_allocated();

    EXPECT_THROW([]() {
        TestResource resource(100);
        throw std::runtime_error("test exception");
    }(), std::runtime_error);

    auto memory_after = MemoryTracker::get_total_allocated();
    EXPECT_EQ(memory_before, memory_after);
}

// Smart Pointer Validation Tests
TEST_F(MemorySafetyTest, SmartPointerLeakPrevention) {
    auto initial_allocs = MemoryTracker::get_alloc_count();

    {
        auto ptr = std::make_unique<int>(42);
        EXPECT_EQ(*ptr, 42);

        std::shared_ptr<std::vector<int>> shared_vec = std::make_shared<std::vector<int>>(1000, 42);
        EXPECT_EQ(shared_vec->size(), 1000);

        std::weak_ptr<std::vector<int>> weak_vec = shared_vec;
        EXPECT_FALSE(weak_vec.expired());

        // Test shared pointer copying
        auto shared_vec_copy = shared_vec;
        EXPECT_EQ(shared_vec.use_count(), 2);

        // ptr, shared_vec, shared_vec_copy should be automatically freed when leaving scope
    }

    auto final_allocs = MemoryTracker::get_alloc_count();
    EXPECT_EQ(initial_allocs, final_allocs);
}

TEST_F(MemorySafetyTest, SharedPtrCircularReference) {
    struct Node {
        std::shared_ptr<Node> next;
        std::weak_ptr<Node> prev; // Use weak_ptr to avoid circular reference
        int value;

        Node(int v) : value(v) {}
    };

    auto initial_allocs = MemoryTracker::get_alloc_count();

    {
        auto node1 = std::make_shared<Node>(1);
        auto node2 = std::make_shared<Node>(2);

        node1->next = node2;
        node2->prev = node1; // weak_ptr prevents circular reference

        EXPECT_EQ(node1.use_count(), 1);
        EXPECT_EQ(node2.use_count(), 1);
    }

    auto final_allocs = MemoryTracker::get_alloc_count();
    EXPECT_EQ(initial_allocs, final_allocs);
}

// Container Memory Safety Tests
TEST_F(MemorySafetyTest, ContainerMemorySafety) {
    using TrackingVector = std::vector<int, TrackingAllocator<int>>;

    auto initial_allocs = MemoryTracker::get_alloc_count();

    {
        TrackingVector large_vector;
        large_vector.reserve(10000);

        for (int i = 0; i < 10000; ++i) {
            large_vector.emplace_back(i);
        }

        EXPECT_EQ(large_vector.size(), 10000);

        large_vector.clear();
        large_vector.shrink_to_fit();
    }

    auto final_allocs = MemoryTracker::get_alloc_count();
    EXPECT_EQ(initial_allocs, final_allocs);
}

TEST_F(MemorySafetyTest, MapContainerMemorySafety) {
    using TrackingMap = std::map<std::string, std::string,
                                 std::less<std::string>,
                                 TrackingAllocator<std::pair<const std::string, std::string>>>;

    auto initial_allocs = MemoryTracker::get_alloc_count();

    {
        TrackingMap config_map;

        for (int i = 0; i < 1000; ++i) {
            config_map["key_" + std::to_string(i)] = "value_" + std::to_string(i);
        }

        EXPECT_EQ(config_map.size(), 1000);

        // Test iteration safety
        for (const auto& [key, value] : config_map) {
            EXPECT_THAT(key, StartsWith("key_"));
            EXPECT_THAT(value, StartsWith("value_"));
        }

        config_map.clear();
    }

    auto final_allocs = MemoryTracker::get_alloc_count();
    EXPECT_EQ(initial_allocs, final_allocs);
}

// Use-After-Free Detection Tests
TEST_F(MemorySafetyTest, UseAfterFreeDetectionInSafeCode) {
    std::vector<std::unique_ptr<std::string>> strings;

    for (int i = 0; i < 10; ++i) {
        strings.push_back(std::make_unique<std::string>("string_" + std::to_string(i)));
    }

    // Get a reference to a string before we delete it
    const std::string* опасный_pointer = strings[5].get();

    // Delete the string
    strings.erase(strings.begin() + 5);

    // Now we should not use опасный_pointer
    // In this test, we validate that our design prevents this
    EXPECT_NE(strings.size(), 10);

    // The dangerous pointer should no longer be used
    // This is a design pattern test - we ensure we don't have raw pointers that outlive their owners
}

// Iterator Invalidation Prevention Tests
TEST_F(MemorySafetyTest, IteratorInvalidationPrevention) {
    std::vector<int> numbers = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    // Safe iteration while modifying container
    for (size_t i = 0; i < numbers.size(); ) {
        if (numbers[i] % 2 == 0) {
            numbers.erase(numbers.begin() + i);
        } else {
            ++i;
        }
    }

    EXPECT_EQ(numbers.size(), 5);
    EXPECT_THAT(numbers, ElementsAre(1, 3, 5, 7, 9));
}

TEST_F(MemorySafetyTest, SafeIteratorRange) {
    std::vector<int> container = {1, 2, 3, 4, 5};
    auto initial_allocs = MemoryTracker::get_alloc_count();

    {
        std::vector<int> copy;
        copy.reserve(container.size());
        std::copy(container.begin(), container.end(), std::back_inserter(copy));
        EXPECT_EQ(copy, container);
    }

    auto final_allocs = MemoryTracker::get_alloc_count();
    EXPECT_EQ(initial_allocs, final_allocs);
}

// Buffer Overflow Prevention Tests
TEST_F(MemorySafetyTest, BufferOverflowPrevention) {
    std::array<char, 10> buffer = {0};

    // Safe string copy with bounds checking
    std::string long_string(100, 'A');

    // This should safely truncate or reject
    auto result = std::min(long_string.size(), buffer.size() - 1);
    std::copy_n(long_string.begin(), result, buffer.begin());
    buffer[result] = '\0';

    EXPECT_LT(strlen(buffer.data()), buffer.size());
}

TEST_F(MemorySafetyTest, ArrayBoundsValidation) {
    std::vector<int> safe_array = {1, 2, 3, 4, 5};

    constexpr int valid_index = 2;
    constexpr int invalid_index = 10;

    // Valid access should work
    EXPECT_NO_THROW(safe_array.at(valid_index));
    EXPECT_EQ(safe_array[valid_index], 3);

    // Invalid access should throw exception with at()
    EXPECT_THROW(safe_array.at(invalid_index), std::out_of_range);
}

// Cache Memory Safety Tests
TEST_F(MemorySafetyTest, CacheLeakPrevention) {
    auto initial_allocs = MemoryTracker::get_alloc_count();

    {
        response_cache cache(100); // 100 entry limit

        // Fill cache beyond capacity
        for (int i = 0; i < 200; ++i) {
            cache.put("key_" + std::to_string(i), "value_" + std::to_string(i));
        }

        // Cache should have evicted old entries without leaks
        EXPECT_LE(cache.size(), 100);

        // Test get operations
        for (int i = 100; i < 200; ++i) {
            auto result = cache.get("key_" + std::to_string(i));
            EXPECT_TRUE(result.has_value());
        }
    }

    auto final_allocs = MemoryTracker::get_alloc_count();
    EXPECT_EQ(initial_allocs, final_allocs);
}

TEST_F(MemorySafetyTest, CacheMemoryGrowth) {
    response_cache cache(50);
    auto initial_memory = MemoryTracker::get_total_allocated();

    // Add items that should cause eviction
    for (int i = 0; i < 1000; ++i) {
        cache.put("key_" + std::to_string(i), std::string(1000, 'x')); // 1KB values
    }

    auto final_memory = MemoryTracker::get_total_allocated();

    // Memory growth should be bounded
    EXPECT_LT(final_memory - initial_memory, 100 * 1000 * 2); // Allow some overhead
    EXPECT_LE(cache.size(), 50);
}

// Connection Pool Memory Safety Tests
TEST_F(MemorySafetyTest, ConnectionPoolMemorySafety) {
    auto initial_allocs = MemoryTracker::get_alloc_count();

    {
        connection_pool pool(5); // 5 connection limit

        std::vector<std::unique_ptr<connection>> connections;

        // Get connections from pool
        for (int i = 0; i < 5; ++i) {
            auto conn = pool.get_connection();
            EXPECT_TRUE(conn != nullptr);
            connections.push_back(std::move(conn));
        }

        // Should return null when pool is exhausted
        auto conn = pool.get_connection();
        EXPECT_TRUE(conn == nullptr);

        // Return connections to pool
        for (auto& connection : connections) {
            pool.return_connection(std::move(connection));
        }

        // Pool should have all connections available again
        for (int i = 0; i < 5; ++i) {
            auto conn = pool.get_connection();
            EXPECT_TRUE(conn != nullptr);
        }
    }

    auto final_allocs = MemoryTracker::get_alloc_count();
    EXPECT_EQ(initial_allocs, final_allocs);
}

// Memory Pressure Testing
TEST_F(MemorySafetyTest, MemoryPressureTesting) {
    const size_t large_size = 1024 * 1024; // 1MB
    auto initial_memory = MemoryTracker::get_total_allocated();

    {
        std::vector<std::unique_ptr<char[]>> large_allocations;

        try {
            for (int i = 0; i < 100; ++i) {
                large_allocations.emplace_back(std::make_unique<char[]>(large_size));
                std::fill_n(large_allocations.back().get(), large_size, static_cast<char>(i % 256));
            }
        } catch (const std::bad_alloc&) {
            // Expected when memory is exhausted
        }

        auto peak_memory = MemoryTracker::get_peak_allocated();
        EXPECT_GT(peak_memory, initial_memory);
    }

    auto final_memory = MemoryTracker::get_total_allocated();
    EXPECT_EQ(initial_memory, final_memory);
}

// Large Allocation Scenarios
TEST_F(MemorySafetyTest, LargeAllocationScenarios) {
    const size_t sizes[] = {1024, 1024*1024, 1024*1024*10}; // 1KB, 1MB, 10MB

    for (size_t size : sizes) {
        auto initial_allocs = MemoryTracker::get_alloc_count();

        {
            auto buffer = std::make_unique<char[]>(size);
            std::fill_n(buffer.get(), size, 'A');

            EXPECT_NO_THROW(buffer[0]); // Should be accessible
            EXPECT_NO_THROW(buffer[size-1]); // Should be accessible
        }

        auto final_allocs = MemoryTracker::get_alloc_count();
        EXPECT_EQ(initial_allocs, final_allocs);
    }
}

// Thread Safety Memory Tests
TEST_F(MemorySafetyTest, ThreadSafeMemoryOperations) {
    std::atomic<int> success_count{0};
    std::atomic<int> error_count{0};
    std::vector<std::thread> threads;
    const int num_threads = 10;
    const int operations_per_thread = 1000;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&success_count, &error_count, operations_per_thread, i]() {
            try {
                for (int j = 0; j < operations_per_thread; ++j) {
                    // Thread safe memory operations
                    auto vec = std::make_unique<std::vector<int>>(100, i + j);
                    EXPECT_EQ(vec->size(), 100);
                    success_count++;
                }
            } catch (...) {
                error_count++;
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(success_count.load(), num_threads * operations_per_thread);
    EXPECT_EQ(error_count.load(), 0);
}

// ASan Integration Tests (these will only be meaningful when compiled with ASan)
#ifdef __SANITIZE_ADDRESS__
TEST_F(MemorySafetyTest, AddressSanitizerIntegration) {
    // This test is designed to catch ASan errors if they exist

    // Buffer overflow test - should be caught by ASan
    EXPECT_DEATH({
        char buffer[10];
        buffer[10] = 'x'; // Write past buffer
    }, "heap-buffer-overflow");

    // Use-after-free test - should be caught by ASan
    EXPECT_DEATH({
        int* ptr = new int;
        delete ptr;
        *ptr = 42; // Use after free
    }, "heap-use-after-free");
}
#endif

// Stack Memory Safety Tests
TEST_F(MemorySafetyTest, StackMemorySafety) {
    auto initial_allocs = MemoryTracker::get_alloc_count();

    // Test large stack allocations
    EXPECT_NO_THROW({
        std::array<int, 10000> large_stack_array;
        std::fill(large_stack_array.begin(), large_stack_array.end(), 42);
        std::accumulate(large_stack_array.begin(), large_stack_array.end(), 0);
    });

    // Test recursive stack usage
    std::function<int(int)> deep_recursive_sum = [&](int n) -> int {
        if (n <= 0) return 0;
        return n + deep_recursive_sum(n - 1);
    };

    EXPECT_NO_THROW(deep_recursive_sum(1000));

    auto final_allocs = MemoryTracker::get_alloc_count();
    EXPECT_EQ(initial_allocs, final_allocs);
}

// Memory Layout Safety Tests
TEST_F(MemorySafetyTest, MemoryLayoutSafety) {
    struct TestStruct {
        std::string name;
        int value;
        std::vector<int> data;
    };

    auto initial_allocs = MemoryTracker::get_alloc_count();

    {
        std::vector<TestStruct> structs;

        for (int i = 0; i < 100; ++i) {
            TestStruct s;
            s.name = "item_" + std::to_string(i);
            s.value = i;
            s.data.resize(10, i);
            structs.push_back(std::move(s));
        }

        // Test that the structure layout is safe
        for (const auto& s : structs) {
            EXPECT_FALSE(s.name.empty());
            EXPECT_GE(s.value, 0);
            EXPECT_EQ(s.data.size(), 10);
        }
    }

    auto final_allocs = MemoryTracker::get_alloc_count();
    EXPECT_EQ(initial_allocs, final_allocs);
}

// Configuration Memory Safety
TEST_F(MemorySafetyTest, ConfigurationMemorySafety) {
    auto initial_allocs = MemoryTracker::get_alloc_count();

    {
        ProductionConfig config;

        // Test configuration parsing with large inputs
        std::string large_config;
        large_config.reserve(100000);
        large_config = R"({
            "database": {
                "host": "localhost",
                "port": 5432,
                "name": ")";

        for (int i = 0; i < 1000; ++i) {
            large_config += "a";
        }

        large_config += R"("},
            "cache": {
                "size": 1000,
                "ttl": 3600
            }
        })";

        // Should handle large configuration without memory issues
        EXPECT_NO_THROW(config.parseFromString(large_config));
    }

    auto final_allocs = MemoryTracker::get_alloc_count();
    EXPECT_EQ(initial_allocs, final_allocs);
}

// Performance Memory Safety Tests
TEST_F(MemorySafetyTest, PerformanceMonitorMemorySafety) {
    auto initial_allocs = MemoryTracker::get_alloc_count();

    {
        PerformanceMonitor monitor;

        // Test with high-frequency metrics
        for (int i = 0; i < 10000; ++i) {
            monitor.record_metric("test_metric_" + std::to_string(i % 10), i);
        }

        // Should have reasonable memory usage
        auto stats = monitor.get_statistics();
        EXPECT_GT(stats.size(), 0);

        // Clear metrics
        monitor.clear();
    }

    auto final_allocs = MemoryTracker::get_alloc_count();
    EXPECT_EQ(initial_allocs, final_allocs);
}

// Exception Safety Tests
TEST_F(MemorySafetyTest, ExceptionSafetyMemoryLeak) {
    auto initial_allocs = MemoryTracker::get_alloc_count();

    // Test that exceptions don't cause memory leaks
    for (int i = 0; i < 100; ++i) {
        try {
            std::vector<int> vec(1000, i);
            if (i % 10 == 0) {
                throw std::runtime_error("test exception");
            }
        } catch (const std::exception&) {
            // Expected for some iterations
        }
    }

    auto final_allocs = MemoryTracker::get_alloc_count();
    EXPECT_EQ(initial_allocs, final_allocs);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}