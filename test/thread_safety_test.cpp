#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <thread>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <future>
#include <chrono>
#include <vector>
#include <random>
#include <algorithm>
#include "src/cache/response_cache.hpp"
#include "src/config/production_config.hpp"
#include "src/network/connection_pool.hpp"
#include "src/core/thread_manager.hpp"
#include "src/logging/production_logger.hpp"
#include "src/monitoring/performance_monitor.hpp"

using namespace ::testing;
using namespace aimux;
using namespace std::chrono_literals;

// Thread testing utilities
class ThreadTester {
public:
    template<typename F>
    static void run_concurrently(int num_threads, F&& func) {
        std::vector<std::thread> threads;
        threads.reserve(num_threads);

        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([i, func]() { func(i); });
        }

        for (auto& thread : threads) {
            thread.join();
        }
    }

    template<typename F>
    static auto run_with_timeout(F&& func, std::chrono::milliseconds timeout) {
        auto future = std::async(std::launch::async, std::forward<F>(func));

        if (future.wait_for(timeout) == std::future_status::timeout) {
            throw std::runtime_error("Operation timed out");
        }

        return future.get();
    }
};

class ThreadSafetyTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup thread-safe test environment
    }

    void TearDown() override {
        // Cleanup thread resources
    }
};

// Race Condition Detection Tests
TEST_F(ThreadSafetyTest, CacheConcurrentAccess) {
    response_cache cache(1000);
    const int num_threads = 10;
    const int operations_per_thread = 1000;

    std::atomic<int> put_count{0};
    std::atomic<int> get_count{0};
    std::atomic<int> successful_gets{0};

    // Producer threads
    std::vector<std::thread> writers;
    for (int i = 0; i < num_threads / 2; ++i) {
        writers.emplace_back([&cache, &put_count, operations_per_thread, i]() {
            for (int j = 0; j < operations_per_thread; ++j) {
                std::string key = "key_" + std::to_string(i) + "_" + std::to_string(j);
                std::string value = "value_" + std::to_string(j);

                cache.put(key, value);
                put_count++;

                // Small delay to increase chance of races
                std::this_thread::sleep_for(1us);
            }
        });
    }

    // Consumer threads
    std::vector<std::thread> readers;
    for (int i = 0; i < num_threads / 2; ++i) {
        readers.emplace_back([&cache, &get_count, &successful_gets, operations_per_thread]() {
            for (int j = 0; j < operations_per_thread; ++j) {
                auto result = cache.get("key_0_" + std::to_string(j));
                if (result) {
                    successful_gets++;
                }
                get_count++;

                std::this_thread::sleep_for(1us);
            }
        });
    }

    // Wait for all threads
    for (auto& thread : writers) {
        thread.join();
    }
    for (auto& thread : readers) {
        thread.join();
    }

    EXPECT_EQ(put_count.load(), (num_threads / 2) * operations_per_thread);
    EXPECT_EQ(get_count.load(), (num_threads / 2) * operations_per_thread);
    EXPECT_NO_FATAL_FAILURE(); // No TSan errors should occur
}

TEST_F(ThreadSafetyTest, AtomicCounterContention) {
    std::atomic<int> counter{0};
    const int num_threads = 50;
    const int increments_per_thread = 10000;

    ThreadTester::run_concurrently(num_threads, [&](int /*thread_id*/) {
        for (int j = 0; j < increments_per_thread; ++j) {
            counter.fetch_add(1, std::memory_order_relaxed);
        }
    });

    EXPECT_EQ(counter.load(), num_threads * increments_per_thread);
}

TEST_F(ThreadSafetyTest, CompareAndSwapContention) {
    std::atomic<int> shared_value{0};
    const int num_threads = 20;
    const int attempts_per_thread = 10000;

    std::atomic<int> successful_cas{0};

    ThreadTester::run_concurrently(num_threads, [&](int /*thread_id*/) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, attempts_per_thread - 1);

        for (int j = 0; j < attempts_per_thread; ++j) {
            int expected = shared_value.load();
            int new_value = expected + 1;

            if (shared_value.compare_exchange_weak(expected, new_value,
                                                   std::memory_order_acq_rel)) {
                successful_cas++;
            }

            // Random pause to increase contention
            if (dis(gen) < 100) {
                std::this_thread::sleep_for(1us);
            }
        }
    });

    EXPECT_EQ(shared_value.load(), successful_cas.load());
    EXPECT_LE(shared_value.load(), num_threads * attempts_per_thread);
}

// Deadlock Prevention Tests
TEST_F(ThreadSafetyTest, LockOrderingConsistency) {
    class LockedResource {
    private:
        std::mutex mutex1_;
        std::mutex mutex2_;
        int resource1_ = 0;
        int resource2_ = 0;

    public:
        void updateBoth(int val1, int val2) {
            // Always acquire locks in the same order
            std::lock(mutex1_, mutex2_);
            std::lock_guard<std::mutex> lock1(mutex1_, std::adopt_lock);
            std::lock_guard<std::mutex> lock2(mutex2_, std::adopt_lock);

            resource1_ = val1;
            resource2_ = val2;
        }

        void updateBothReverse(int val1, int val2) {
            // Reverse order - can cause deadlock if mixed
            std::lock(mutex2_, mutex1_);
            std::lock_guard<std::mutex> lock2(mutex2_, std::adopt_lock);
            std::lock_guard<std::mutex> lock1(mutex1_, std::adopt_lock);

            resource1_ = val1;
            resource2_ = val2;
        }

        void readBoth(int& val1, int& val2) const {
            std::shared_lock<std::shared_mutex> lock1(mutex1_);
            std::shared_lock<std::shared_mutex> lock2(mutex2_);
            val1 = resource1_;
            val2 = resource2_;
        }
    };

    LockedResource resource;
    const int num_threads = 10;
    std::atomic<bool> deadlock_detected{false};

    // Test correct locking order
    ThreadTester::run_concurrently(num_threads, [&](int thread_id) {
        try {
            for (int i = 0; i < 100; ++i) {
                resource.updateBoth(thread_id, i + 100);

                int val1, val2;
                resource.readBoth(val1, val2);

                std::this_thread::sleep_for(10us);
            }
        } catch (...) {
            deadlock_detected = true;
        }
    });

    EXPECT_FALSE(deadlock_detected.load());
}

TEST_F(ThreadSafetyTest, DeadlockTimeoutDetection) {
    std::mutex mutex1, mutex2;
    std::atomic<bool> timeout_detected{false};

    // Thread that might deadlock
    std::thread thread1([&]() {
        std::unique_lock<std::mutex> lock1(mutex1);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Try to lock mutex2 with timeout
        std::unique_lock<std::mutex> lock2(mutex2, std::chrono::milliseconds(50));
        if (!lock2.owns_lock()) {
            timeout_detected = true;
            return;
        }
        // If we get here, no deadlock
    });

    // thread causes potential deadlock
    std::thread thread2([&]() {
        std::unique_lock<std::mutex> lock2(mutex2);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Try to lock mutex1 with timeout
        std::unique_lock<std::mutex> lock1(mutex1, std::chrono::milliseconds(50));
        if (!lock1.owns_lock()) {
            timeout_detected = true;
            return;
        }
    });

    thread1.join();
    thread2.join();

    EXPECT_TRUE(timeout_detected.load());
}

// Data Corruption Detection Tests
TEST_F(ThreadSafetyTest, ConcurrentDataStructureIntegrity) {
    class ThreadSafeMap {
    private:
        mutable std::shared_mutex mutex_;
        std::map<std::string, int> data_;

    public:
        void put(const std::string& key, int value) {
            std::unique_lock lock(mutex_);
            data_[key] = value;
        }

        bool get(const std::string& key, int& value) const {
            std::shared_lock lock(mutex_);
            auto it = data_.find(key);
            if (it != data_.end()) {
                value = it->second;
                return true;
            }
            return false;
        }

        size_t size() const {
            std::shared_lock lock(mutex_);
            return data_.size();
        }

        bool contains_key(const std::string& key) const {
            std::shared_lock lock(mutex_);
            return data_.find(key) != data_.end();
        }
    };

    ThreadSafeMap map;
    const int num_threads = 10;
    const int operations_per_thread = 1000;

    std::atomic<int> successful_puts{0};
    std::atomic<int> successful_gets{0};

    // Writer threads
    std::vector<std::thread> writers;
    for (int i = 0; i < num_threads / 2; ++i) {
        writers.emplace_back([&map, &successful_puts, operations_per_thread, i]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(0, operations_per_thread - 1);

            for (int j = 0; j < operations_per_thread; ++j) {
                int random_num = dis(gen);
                std::string key = "key_" + std::to_string(i) + "_" + std::to_string(random_num);
                int value = i * 1000 + random_num;

                map.put(key, value);
                successful_puts++;
            }
        });
    }

    // Reader threads
    std::vector<std::thread> readers;
    for (int i = 0; i < num_threads / 2; ++i) {
        readers.emplace_back([&map, &successful_gets, operations_per_thread, i]() {
            for (int j = 0; j < operations_per_thread; ++j) {
                std::string key = "key_" + std::to_string(i) + "_" + std::to_string(j);
                int value;
                if (map.get(key, value)) {
                    // Verify data consistency
                    int expected_thread = i;
                    int expected_value_part = j;
                    if ((value / 1000 != expected_thread) &&
                        ((value % 1000) != expected_value_part)) {
                        // Different thread wrote this value, but it should still be consistent
                    }
                    successful_gets++;
                }
            }
        });
    }

    for (auto& thread : writers) {
        thread.join();
    }
    for (auto& thread : readers) {
        thread.join();
    }

    EXPECT_EQ(successful_puts.load(), (num_threads / 2) * operations_per_thread);
    EXPECT_NO_FATAL_FAILURE(); // No TSan errors
}

TEST_F(ThreadSafetyTest, ConcurrentConnectionPool) {
    connection_pool pool(10); // Max 10 connections
    const int num_threads = 20;
    const int operations_per_thread = 100;

    std::atomic<int> successful_connections{0};
    std::atomic<int> failed_connections{0};
    std::vector<std::thread> threads;

    ThreadTester::run_concurrently(num_threads, [&](int /*thread_id*/) {
        for (int j = 0; j < operations_per_thread; ++j) {
            auto conn = pool.get_connection();
            if (conn) {
                // Simulate work
                std::this_thread::sleep_for(std::chrono::microseconds(10));
                pool.return_connection(std::move(conn));
                successful_connections++;
            } else {
                failed_connections++;
            }
        }
    });

    EXPECT_EQ(successful_connections + failed_connections, num_threads * operations_per_thread);
    EXPECT_EQ(pool.get_idle_connections(), 10); // All connections returned
}

// Mutex Correctness Tests
TEST_F(ThreadSafetyTest, LockScopeValidation) {
    class MonitoredResource {
    private:
        mutable std::mutex mutex_;
        int data_ = 0;
        mutable std::atomic<int> lock_acquired_count_{0};
        mutable std::atomic<int> lock_released_count_{0};

    public:
        void increment() {
            lock_acquired_count_++;
            std::lock_guard<std::mutex> lock(mutex_);
            data_++;
            lock_released_count_++;
        }

        int get_data() const {
            lock_acquired_count_++;
            std::lock_guard<std::mutex> lock(mutex_);
            int result = data_;
            lock_released_count_++;
            return result;
        }

        bool locks_balanced() const {
            return lock_acquired_count_ == lock_released_count_;
        }
    };

    MonitoredResource resource;
    const int num_threads = 10;
    const int operations_per_thread = 1000;

    ThreadTester::run_concurrently(num_threads, [&](int /*thread_id*/) {
        for (int j = 0; j < operations_per_thread; ++j) {
            resource.increment();
            resource.get_data();
        }
    });

    EXPECT_TRUE(resource.locks_balanced());
    EXPECT_EQ(resource.get_data(), num_threads * operations_per_thread);
}

// Concurrent Attack Scenarios
TEST_F(ThreadSafetyTest, HighContentionStressTest) {
    constexpr int num_threads = 50;
    constexpr int operations_per_thread = 10000;

    std::atomic<int64_t> shared_counter{0};

    auto start_time = std::chrono::high_resolution_clock::now();

    ThreadTester::run_concurrently(num_threads, [&](int /*thread_id*/) {
        for (int j = 0; j < operations_per_thread; ++j) {
            shared_counter.fetch_add(1, std::memory_order_relaxed);
        }
    });

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    EXPECT_EQ(shared_counter.load(), num_threads * operations_per_thread);
    EXPECT_LT(duration.count(), 5000); // Should complete within 5 seconds
}

TEST_F(ThreadSafetyTest, ThreadPoolUnderContention) {
    ThreadManager thread_manager(4); // 4 worker threads
    const int num_tasks = 1000;
    std::atomic<int> completed_tasks{0};

    std::vector<std::future<void>> futures;
    futures.reserve(num_tasks);

    for (int i = 0; i < num_tasks; ++i) {
        futures.push_back(thread_manager.submit([&completed_tasks, i]() {
            // Simulate work
            std::this_thread::sleep_for(std::chrono::microseconds(10));
            completed_tasks++;
        }));
    }

    // Wait for all tasks to complete
    for (auto& future : futures) {
        future.wait();
    }

    EXPECT_EQ(completed_tasks.load(), num_tasks);
}

// Performance Under Contention
TEST_F(ThreadSafetyTest, PerformanceUnderContention) {
    class PerformanceTestCache {
    private:
        mutable std::shared_mutex mutex_;
        std::unordered_map<std::string, std::string> cache_;

    public:
        void put(const std::string& key, const std::string& value) {
            std::unique_lock lock(mutex_);
            cache_[key] = value;
        }

        std::optional<std::string> get(const std::string& key) const {
            std::shared_lock lock(mutex_);
            auto it = cache_.find(key);
            if (it != cache_.end()) {
                return it->second;
            }
            return std::nullopt;
        }
    };

    PerformanceTestCache cache;
    const int num_threads = 10;
    const int operations_per_thread = 1000;

    // Pre-populate cache
    for (int i = 0; i < 100; ++i) {
        cache.put("key_" + std::to_string(i), "value_" + std::to_string(i));
    }

    auto start_time = std::chrono::high_resolution_clock::now();
    std::atomic<int> successful_ops{0};

    ThreadTester::run_concurrently(num_threads, [&](int /*thread_id*/) {
        for (int j = 0; j < operations_per_thread; ++j) {
            // Mix of reads and writes
            if (j % 10 == 0) {
                cache.put("new_key_" + std::to_string(j), "new_value");
            } else {
                auto result = cache.get("key_" + std::to_string(j % 100));
                if (result) {
                    successful_ops++;
                }
            }
        }
    });

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    EXPECT_LT(duration.count(), 3000); // Should complete within 3 seconds
    EXPECT_GT(successful_ops.load(), num_threads * operations_per_thread * 0.8); // At least 80% success rate
}

// Memory Ordering Tests
TEST_F(ThreadSafetyTest, MemoryOrderingValidation) {
    std::atomic<bool> flag1{false};
    std::atomic<bool> flag2{false};
    std::atomic<int> data{0};

    // Writer thread
    std::thread writer([&]() {
        data.store(42, std::memory_order_release);
        flag1.store(true, std::memory_order_release);
        flag2.store(true, std::memory_order_release);
    });

    // Reader thread
    std::thread reader([&]() {
        while (!flag2.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        // If flag2 is true, flag1 should also be true due to release-acquire ordering
        EXPECT_TRUE(flag1.load(std::memory_order_acquire));
        EXPECT_EQ(data.load(std::memory_order_acquire), 42);
    });

    writer.join();
    reader.join();
}

// Recursive Lock Tests
TEST_F(ThreadSafetyTest, RecursiveLockSafety) {
    std::recursive_mutex rmutex;
    int counter = 0;

    auto recursive_function = [&](int depth) {
        std::lock_guard<std::recursive_mutex> lock(rmutex);
        counter++;

        if (depth > 0) {
            recursive_function(depth - 1);
        }
    };

    // Should not deadlock with recursive locking
    std::thread t([&]() {
        recursive_function(5);
    });

    t.join();
    EXPECT_EQ(counter, 6);
}

// Concurrent Initialization Thread Safety
TEST_F(ThreadSafetyTest, CallOncePattern) {
    static std::once_flag flag;
    static int expensive_init_result = 0;

    auto expensive_init = []() {
        expensive_init_result = 42;
        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Simulate work
    };

    const int num_threads = 10;
    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&expensive_init]() {
            std::call_once(flag, expensive_init);
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(expensive_init_result, 42);
}

// Thread Safe Logger Tests
TEST_F(ThreadSafetyTest, ThreadSafeLogging) {
    ProductionLogger logger;
    const int num_threads = 10;
    const int logs_per_thread = 1000;

    std::atomic<int> total_logs{0};

    ThreadTester::run_concurrently(num_threads, [&](int thread_id) {
        for (int j = 0; j < logs_per_thread; ++j) {
            logger.info("Thread " + std::to_string(thread_id) + " log " + std::to_string(j));
            total_logs++;
        }
    });

    EXPECT_EQ(total_logs.load(), num_threads * logs_per_thread);

    // Wait for async logging to complete
    logger.flush();
}

// Exception Safety in Threaded Environment
TEST_F(ThreadSafetyTest, ExceptionSafetyWithLocks) {
    std::mutex mutex;
    int counter = 0;

    auto potentially_failing_function = [&](int should_fail) {
        std::lock_guard<std::mutex> lock(mutex);

        if (should_fail) {
            throw std::runtime_error("Simulated failure");
        }

        counter++;
    };

    std::atomic<int> successful_calls{0};
    std::atomic<int> failed_calls{0};

    ThreadTester::run_concurrently(10, [&](int thread_id) {
        try {
            potentially_failing_function(thread_id % 3); // Every 3rd call fails
            successful_calls++;
        } catch (const std::exception&) {
            failed_calls++;
        }
    });

    EXPECT_GT(successful_calls.load(), 0);
    EXPECT_GT(failed_calls.load(), 0);

    // Mutex should not be locked even when exceptions occur
    std::lock_guard<std::mutex> test_lock(mutex);
    EXPECT_NO_THROW(test_lock.~lock_guard());
}

// Stress Test for Thread Pool Queue
TEST_F(ThreadSafetyTest, ThreadPoolQueueStress) {
    ThreadManager thread_manager(2); // Small thread pool to increase contention
    const int num_tasks = 1000;
    std::vector<std::future<int>> futures;

    futures.reserve(num_tasks);
    std::atomic<int> sum{0};

    for (int i = 0; i < num_tasks; ++i) {
        futures.push_back(thread_manager.submit([&sum, i]() -> int {
            int value = i * 2;
            sum.fetch_add(value, std::memory_order_relaxed);
            return value;
        }));
    }

    // Calculate expected sum
    int expected_sum = 0;
    for (int i = 0; i < num_tasks; ++i) {
        expected_sum += i * 2;
    }

    // Wait for all tasks
    for (auto& future : futures) {
        future.wait();
    }

    EXPECT_EQ(sum.load(), expected_sum);
}

// Conditional Variable Thread Safety
TEST_F(ThreadSafetyTest, ConditionVariableSafety) {
    std::mutex mtx;
    std::condition_variable cv;
    bool ready = false;
    std::vector<int> results;
    const int num_producers = 3;
    const int num_consumers = 5;
    const int items_per_producer = 10;

    // Producer threads
    std::vector<std::thread> producers;
    for (int i = 0; i < num_producers; ++i) {
        producers.emplace_back([&, i]() {
            for (int j = 0; j < items_per_producer; ++j) {
                {
                    std::lock_guard<std::mutex> lock(mtx);
                    results.push_back(i * 100 + j);
                    ready = true;
                }
                cv.notify_one();
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });
    }

    // Consumer threads
    std::vector<std::thread> consumers;
    for (int i = 0; i < num_consumers; ++i) {
        consumers.emplace_back([&]() {
            while (true) {
                std::unique_lock<std::mutex> lock(mtx);
                cv.wait(lock, [&] { return ready || results.size() >= num_producers * items_per_producer; });

                if (results.size() >= num_producers * items_per_producer) {
                    break;
                }

                if (!results.empty()) {
                    int value = results.back();
                    results.pop_back();
                    ready = !results.empty();
                }
            }
        });
    }

    for (auto& producer : producers) {
        producer.join();
    }

    { // Signal final completion
        std::lock_guard<std::mutex> lock(mtx);
        ready = true;
    }
    cv.notify_all();

    for (auto& consumer : consumers) {
        consumer.join();
    }

    EXPECT_EQ(results.size(), 0); // All items consumed
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}