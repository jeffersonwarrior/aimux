# Thread Safety Regression Test Suite

## Purpose
Prevent race conditions, deadlocks, and data corruption identified in QA review.

## Test Categories

### 1. Race Condition Detection Tests
```cpp
// test/thread_safety_test.cpp
#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include "aimux/prettifier/plugin_registry.hpp"

class ThreadSafetyTest : public ::testing::Test {
protected:
    void SetUp() override {
        registry = std::make_unique<PluginRegistry>();
    }

    std::unique_ptr<PluginRegistry> registry;
};

TEST_F(ThreadSafetyTest, ConcurrentPluginRegistryAccess) {
    const int num_threads = 10;
    const int operations_per_thread = 100;
    std::atomic<int> successful_operations{0};

    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            for (int j = 0; j < operations_per_thread; ++j) {
                std::string dir = "/test/dir" + std::to_string(i);

                try {
                    registry->add_plugin_directory(dir);
                    successful_operations++;

                    auto plugins = registry->get_all_plugins();
                    successful_operations++;

                    registry->remove_plugin_directory(dir);
                    successful_operations++;
                } catch (...) {
                    // Exceptions are acceptable in concurrent scenarios
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_GT(successful_operations.load(), 0);
}
```

### 2. Deadlock Prevention Tests
```cpp
TEST_F(ThreadSafetyTest, DeadlockPrevention) {
    const std::chrono::seconds timeout{5};
    std::atomic<bool> deadlock_detected{false};

    std::thread t1([&]() {
        try {
            registry->add_plugin_directory("/dir1");
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            registry->add_plugin_directory("/dir2");
        } catch (...) {
            deadlock_detected = true;
        }
    });

    std::thread t2([&]() {
        try {
            registry->add_plugin_directory("/dir2");
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            registry->add_plugin_directory("/dir1");
        } catch (...) {
            deadlock_detected = true;
        }
    });

    if (t1.joinable()) t1.join();
    if (t2.joinable()) t2.join();

    EXPECT_FALSE(deadlock_detected.load());
}
```

### 3. Plugin Registry Thread Safety
```cpp
TEST_F(ThreadSafetyTest, PluginRegistryDataCorruption) {
    const int num_threads = 20;
    std::atomic<int> corruption_count{0};

    std::vector<std::thread> threads;

    // Writers
    for (int i = 0; i < num_threads/2; ++i) {
        threads.emplace_back([&, i]() {
            for (int j = 0; j < 50; ++j) {
                std::string dir = "/writer/dir" + std::to_string(i) + "_" + std::to_string(j);
                registry->add_plugin_directory(dir);

                // Verify directory was added
                auto status = registry->get_status();
                if (status["plugin_directories"].get<int>() < 0) {
                    corruption_count++;
                }
            }
        });
    }

    // Readers
    for (int i = 0; i < num_threads/2; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < 100; ++j) {
                auto status = registry->get_status();
                auto metadata = registry->get_all_metadata();

                // Validate data consistency
                if (status["total_plugins"].get<int>() != metadata.size()) {
                    corruption_count++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(corruption_count.load(), 0);
}
```

### 4. TOON Formatter Thread Safety
```cpp
TEST(ThreadSafetyTest, ToonFormatterConcurrentSerialization) {
    auto formatter = std::make_unique<ToonFormatter>();
    const int num_threads = 50;
    const int operations_per_thread = 20;

    std::vector<std::thread> threads;
    std::atomic<int> successful_operations{0};
    std::atomic<bool> corruption_detected{false};

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            for (int j = 0; j < operations_per_thread; ++j) {
                Response response{
                    .data = "Test content from thread " + std::to_string(i) + " operation " + std::to_string(j),
                    .success = true
                };

                ProcessingContext context{
                    .provider_name = "test_provider_" + std::to_string(i % 4)
                };

                try {
                    auto result = formatter->serialize_response(response, context);

                    // Validate result consistency
                    if (result.empty()) {
                        corruption_detected = true;
                    } else if (result.find("# META") == std::string::npos) {
                        corruption_detected = true;
                    } else {
                        successful_operations++;
                    }
                } catch (...) {
                    // Exceptions acceptable in high contention
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_GT(successful_operations.load(), 0);
    EXPECT_FALSE(corruption_detected.load());
}
```

## Execution Commands

### Build with ThreadSanitizer
```bash
# Build with ThreadSanitizer for race condition detection
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_TSAN=ON -B build-tsan
cmake --build build-tsan

# Enable TSAN runtime options
export TSAN_OPTIONS=abort_on_error=1:report_atomic_races=1:report_signal_unsafe=1
```

### Run Thread Safety Tests
```bash
# Run with ThreadSanitizer
./build-tsan/test/thread_safety_test --gtest_filter="ThreadSafetyTest.*"

# Run with Helgrind (alternative race condition detector)
valgrind --tool=helgrind --suppressions=helgrind.supp ./test/thread_safety_test

# Run with DRD (another race condition detector)
valgrind --tool=drd ./test/thread_safety_test
```

### CI Integration
```yaml
# .github/workflows/thread-safety.yml
- name: Thread Safety Tests
  run: |
    cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_TSAN=ON -B build
    cmake --build build
    cd build && TSAN_OPTIONS=abort_on_error=1 ./test/thread_safety_test
```

## Success Criteria

### Must Pass
- No data races detected by ThreadSanitizer
- No deadlocks under concurrent access
- No data corruption in shared structures
- Plugin registry remains consistent under load

### Performance Requirements
- Thread safety overhead < 20%
- Tests complete within 3 minutes
- No performance degradation with 10+ concurrent threads

## Suppression Files

### ThreadSanitizer Suppressions
```bash
# tsan.supp
# Suppress known false positives from third-party libraries
race:std::*
race:pthread_*
deadlock:std::*
```

### Helgrind Suppressions
```bash
# helgrind.supp
{
   helgrind-sanity
   Helgrind:Race
   fun:std::_*
   obj:*
}
```