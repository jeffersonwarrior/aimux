# Performance Regression Tests

## Overview
This document defines comprehensive performance regression tests to detect performance degradation, memory leaks, resource consumption issues, and scalability problems in the Aimux system.

## Test Categories

### 1. Benchmark Validation Framework

#### 1.1 Microbenchmark Collection
**Objective**: Establish baseline performance for critical operations.

**Benchmark Categories**:
- HTTP request/response latency
- JSON parsing and serialization
- Cache operation performance
- Provider failover timing
- Connection pool efficiency
- Thread pool throughput

**Implementation Framework**:
```cpp
// test/performance_benchmark.hpp
class PerformanceBenchmark {
private:
    std::chrono::high_resolution_clock::time_point start_time_;
    std::string benchmark_name_;
    static std::map<std::string, std::vector<double>> results_;

public:
    PerformanceBenchmark(const std::string& name) : benchmark_name_(name) {
        start_time_ = std::chrono::high_resolution_clock::now();
    }

    ~PerformanceBenchmark() {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            end_time - start_time_);
        results_[benchmark_name_].push_back(duration.count() / 1000.0); // ms
    }

    static void report_results() {
        for (const auto& [name, times] : results_) {
            double sum = std::accumulate(times.begin(), times.end(), 0.0);
            double mean = sum / times.size();
            auto minmax = std::minmax_element(times.begin(), times.end());

            std::cout << name << ":\n"
                      << "  Mean: " << mean << "ms\n"
                      << "  Min: " << *minmax.first << "ms\n"
                      << "  Max: " << *minmax.second << "ms\n"
                      << "  Count: " << times.size() << "\n";
        }
    }
};
```

#### 1.2 Performance Thresholds
**Objective**: Define acceptable performance thresholds for regression detection.

**Threshold Definitions**:
```cpp
// test/performance_thresholds.hpp
struct PerformanceThresholds {
    struct HttpLatency {
        static constexpr double MEAN_MS = 50.0;      // 50ms average
        static constexpr double P95_MS = 100.0;      // 95th percentile
        static constexpr double P99_MS = 200.0;      // 99th percentile
    };

    struct CacheOperations {
        static constexpr double PUT_MS = 1.0;        // 1ms cache put
        static constexpr double GET_MS = 0.5;        // 0.5ms cache get
        static constexpr double EVICTION_MS = 5.0;   // 5ms eviction
    };

    struct JsonParsing {
        static constexpr double SMALL_DOC_MS = 0.1;  // 0.1ms small document
        static constexpr double LARGE_DOC_MS = 10.0; // 10ms large document
    };

    struct FailoverTime {
        static constexpr double DETECTION_MS = 100.0; // 100ms detect failure
        static constexpr double SWITCHOVER_MS = 50.0; // 50ms switch providers
    };
};
```

#### 1.3 Baseline Establishment
**Objective**: Create and maintain performance baselines.

**Baseline Collection Process**:
```bash
# Run baseline collection
./performance_regression_tests --baseline

# Store results
mkdir -p qa/baselines/$(date +%Y%m%d)
cp benchmark_results.json qa/baselines/$(date +%Y%m%d)/

# Generate baseline report
python3 qa/scripts/generate_baseline_report.py qa/baselines/$(date +%Y%m%d)/
```

### 2. Performance Degradation Detection

#### 2.1 Regression Detection Algorithm
**Objective**: Detect statistically significant performance regressions.

**Detection Logic**:
```cpp
// test/performance_regression_detector.hpp
class PerformanceRegressionDetector {
private:
    struct BaselineMetric {
        double mean;
        double std_dev;
        double threshold_percent;
    };

    std::map<std::string, BaselineMetric> baselines_;

public:
    bool hasRegression(const std::string& metric_name,
                      double current_value) {
        auto it = baselines_.find(metric_name);
        if (it == baselines_.end()) {
            return false; // No baseline available
        }

        const auto& baseline = it->second;
        double z_score = (current_value - baseline.mean) / baseline.std_dev;
        double percent_change = ((current_value - baseline.mean) / baseline.mean) * 100.0;

        return (z_score > 2.0) || (percent_change > baseline.threshold_percent);
    }

    void loadBaseline(const std::string& baseline_file) {
        // Load baseline metrics from JSON file
        // Include mean, std_dev, and thresholds
    }
};
```

#### 2.2 Critical Path Monitoring
**Objective**: Monitor performance of critical application paths.

**Critical Paths**:
- End-to-end API request processing
- Provider failover sequence
- Cache miss handling
- Configuration reload
- Health check processing

**Implementation Test**:
```cpp
// test/critical_path_performance_test.cpp
TEST(PerformanceRegressionTest, APIRequestLatency) {
    PerformanceRegressionDetector detector;
    detector.loadBaseline("qa/baselines/latest/api_latency.json");

    const int num_requests = 1000;
    std::vector<double> latencies;

    for (int i = 0; i < num_requests; ++i) {
        auto start = std::chrono::high_resolution_clock::now();

        // Simulate API request processing
        auto response = api_gateway.process_test_request(
            json{{"model", "claude-3"}, {"prompt", "test"}});

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            end - start);
        latencies.push_back(duration.count() / 1000.0); // Convert to ms
    }

    double mean_latency = std::accumulate(latencies.begin(), latencies.end(), 0.0) / latencies.size();

    EXPECT_FALSE(detector.hasRegression("api_request_mean", mean_latency))
        << "API request latency regressed: " << mean_latency << "ms";

    EXPECT_LT(mean_latency, PerformanceThresholds::HttpLatency::MEAN_MS);
}
```

#### 2.3 Performance Trend Analysis
**Objective**: Analyze performance trends over time.

**Trend Analysis Features**:
- Moving average calculations
- Performance trend detection
- Anomaly identification
- Predictive performance modeling

### 3. Memory Usage Validation

#### 3.1 Memory Profiling Framework
**Objective**: Track memory usage patterns and detect leaks.

**Implementation Framework**:
```cpp
// test/memory_profiler.hpp
class MemoryProfiler {
private:
    struct MemorySnapshot {
        size_t heap_used;
        size_t stack_used;
        size_t cache_used;
        std::chrono::system_clock::time_point timestamp;
    };

    std::vector<MemorySnapshot> snapshots_;

public:
    void takeSnapshot() {
        MemorySnapshot snapshot;
        snapshot.heap_used = getCurrentHeapUsage();
        snapshot.stack_used = getCurrentStackUsage();
        snapshot.cache_used = getCurrentCacheUsage();
        snapshot.timestamp = std::chrono::system_clock::now();

        snapshots_.push_back(snapshot);
    }

    bool hasMemoryLeak(size_t threshold_bytes = 1024 * 1024) {
        if (snapshots_.size() < 2) return false;

        auto& first = snapshots_.front();
        auto& last = snapshots_.back();

        return (last.heap_used - first.heap_used) > threshold_bytes;
    }

    void generateReport() {
        // Generate memory usage report with graphs and analysis
    }
};
```

#### 3.2 Memory Growth Detection
**Objective**: Detect unacceptable memory growth patterns.

**Test Cases**:
- Long-running server memory stability
- Cache memory growth under load
- Connection pool memory usage
- JSON parsing memory efficiency

**Implementation Test**:
```cpp
TEST(PerformanceRegressionTest, MemoryGrowthDetection) {
    MemoryProfiler profiler;
    profiler.takeSnapshot(); // Initial baseline

    // Run operations that might cause memory growth
    for (int i = 0; i < 10000; ++i) {
        // Process API requests
        auto response = api_gateway.process_request(test_payload);

        // Take periodic snapshots
        if (i % 1000 == 0) {
            profiler.takeSnapshot();
        }
    }

    profiler.takeSnapshot(); // Final snapshot

    EXPECT_FALSE(profiler.hasMemoryLeak(10 * 1024 * 1024)) // 10MB threshold
        << "Memory leak detected in API processing";

    profiler.generateReport();
}
```

#### 3.3 Cache Memory Efficiency
**Objective**: Ensure cache memory usage is efficient and bounded.

**Test Cases**:
- Cache respects memory limits
- Cache eviction effectiveness
- Memory fragmentation prevention
- Cache hit/miss ratio optimization

### 4. Load Testing Under Stress

#### 4.1 Load Testing Framework
**Objective**: Simulate realistic production loads.

**Load Scenarios**:
- Concurrent user simulation
- Request pattern variation
- Provider failure simulation
- Network latency simulation
- Resource exhaustion scenarios

**Implementation Framework**:
```cpp
// test/load_test_framework.hpp
class LoadTestRunner {
private:
    struct LoadTestConfig {
        int concurrent_users;
        int requests_per_second;
        int test_duration_seconds;
        bool simulate_errors;
        double error_rate_percent;
    };

public:
    LoadTestResult runLoadTest(const LoadTestConfig& config) {
        std::atomic<int> successful_requests{0};
        std::atomic<int> failed_requests{0};
        std::vector<double> response_times;
        std::mutex response_times_mutex;

        auto start_time = std::chrono::high_resolution_clock::now();

        std::vector<std::thread> threads;
        for (int i = 0; i < config.concurrent_users; ++i) {
            threads.emplace_back([&, i]() {
                auto user_start = std::chrono::high_resolution_clock::now();
                int requests_sent = 0;

                while (std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::high_resolution_clock::now() - user_start).count()
                    < config.test_duration_seconds) {

                    auto request_start = std::chrono::high_resolution_clock::now();

                    try {
                        auto response = api_gateway.process_request(test_payload);
                        successful_requests++;
                    } catch (...) {
                        failed_requests++;
                    }

                    auto request_end = std::chrono::high_resolution_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                        request_end - request_start);

                    std::lock_guard<std::mutex> lock(response_times_mutex);
                    response_times.push_back(duration.count() / 1000.0);

                    requests_sent++;

                    // Rate limiting
                    if (config.requests_per_second > 0) {
                        std::this_thread::sleep_for(
                            std::chrono::milliseconds(1000 / config.requests_per_second));
                    }
                }
            });
        }

        for (auto& thread : threads) {
            thread.join();
        }

        return LoadTestResult{
            .successful_requests = successful_requests.load(),
            .failed_requests = failed_requests.load(),
            .response_times = response_times
        };
    }
};
```

#### 4.2 Performance Under Load
**Objective**: Validate performance characteristics under load.

**Test Cases**:
- Response time under increasing load
- Throughput limitations
- Error rate under stress
- Resource utilization efficiency

**Implementation Test**:
```cpp
TEST(PerformanceRegressionTest, PerformanceUnderLoad) {
    LoadTestRunner runner;

    // Test with different load levels
    std::vector<int> load_levels = {10, 50, 100, 200, 500};

    for (int load : load_levels) {
        LoadTestConfig config{
            .concurrent_users = load,
            .requests_per_second = load * 2,
            .test_duration_seconds = 30,
            .simulate_errors = false,
            .error_rate_percent = 0.0
        };

        auto result = runner.runLoadTest(config);

        // Calculate performance metrics
        double success_rate = (double)result.successful_requests /
                            (result.successful_requests + result.failed_requests) * 100.0;

        auto response_time_stats = calculateStatistics(result.response_times);

        // Validate performance doesn't degrade significantly
        EXPECT_GT(success_rate, 99.0) << "Success rate too low at load " << load;
        EXPECT_LT(response_time_stats.p95, 500.0) << "95th percentile too high at load " << load;

        // Log results for trend analysis
        logPerformanceMetrics("load_test_" + std::to_string(load), {
            {"success_rate", success_rate},
            {"mean_response_time", response_time_stats.mean},
            {"p95_response_time", response_time_stats.p95},
            {"throughput", (double)result.successful_requests / 30.0}
        });
    }
}
```

#### 4.3 Resource Exhaustion Testing
**Objective**: Test behavior under resource exhaustion scenarios.

**Scenarios**:
- Out-of-memory conditions
- File descriptor exhaustion
- Thread pool exhaustion
- Network connection limits

### 5. Scalability Regression Detection

#### 5.1 Scalability Metrics
**Objective**: Define and monitor scalability metrics.

**Key Metrics**:
- Performance per resource unit
- Horizontal scaling efficiency
- Vertical scaling effectiveness
- Cost-performance ratio

#### 5.2 Scaling Efficiency Tests
**Objective**: Validate scaling efficiency across different dimensions.

**Test Cases**:
- Thread count scaling
- Memory scaling
- Network bandwidth scaling
- Cache size scaling

**Implementation Test**:
```cpp
TEST(PerformanceRegressionTest, ScalingEfficiency) {
    std::vector<int> thread_counts = {1, 2, 4, 8, 16, 32};
    std::map<int, double> throughputs;

    for (int threads : thread_counts) {
        // Set thread pool size
        thread_pool.setSize(threads);

        // Run performance test
        auto throughput = measureThroughput(threads);
        throughputs[threads] = throughput;

        std::cout << threads << " threads: " << throughput << " ops/sec\n";
    }

    // Check scaling efficiency (should be close to linear up to CPU cores)
    for (size_t i = 1; i < thread_counts.size(); ++i) {
        int current_threads = thread_counts[i];
        int previous_threads = thread_counts[i-1];

        double current_throughput = throughputs[current_threads];
        double previous_throughput = throughputs[previous_threads];

        double scaling_factor = current_throughput / previous_throughput;
        double ideal_scaling = (double)current_threads / previous_threads;
        double efficiency = scaling_factor / ideal_scaling;

        // Should have at least 70% efficiency for reasonable scaling
        EXPECT_GT(efficiency, 0.7) << "Poor scaling efficiency from "
                                   << previous_threads << " to " << current_threads
                                   << " threads. Efficiency: " << efficiency;
    }
}
```

#### 5.3 Performance Isolation Tests
**Objective**: Ensure performance isolation between components.

**Test Cases**:
- Heavy cache usage doesn't affect API processing
- Background tasks don't impact response times
- Error handling doesn't degrade normal operations

### 6. Resource Leak Monitoring

#### 6.1 Resource Tracking Framework
**Objective**: Track various resource types for leaks.

**Resource Types**:
- Memory allocations
- File descriptors
- Network sockets
- Thread handles
- Database connections

**Implementation Framework**:
```cpp
// test/resource_tracker.hpp
class ResourceTracker {
private:
    struct ResourceSnapshot {
        size_t memory_allocations;
        int file_descriptors;
        int network_sockets;
        int active_threads;
        int db_connections;
    };

    ResourceSnapshot baseline_;

public:
    ResourceSnapshot takeSnapshot() {
        return ResourceSnapshot{
            .memory_allocations = getMemoryAllocationCount(),
            .file_descriptors = getFileDescriptorCount(),
            .network_sockets = getSocketCount(),
            .active_threads = getThreadCount(),
            .db_connections = getDbConnectionCount()
        };
    }

    bool hasResourceLeaks(const ResourceSnapshot& current,
                         const ResourceSnapshot& thresholds) {
        return (current.memory_allocations - baseline_.memory_allocations) > thresholds.memory_allocations ||
               (current.file_descriptors - baseline_.file_descriptors) > thresholds.file_descriptors ||
               (current.network_sockets - baseline_.network_sockets) > thresholds.network_sockets ||
               (current.active_threads - baseline_.active_threads) > thresholds.active_threads ||
               (current.db_connections - baseline_.db_connections) > thresholds.db_connections;
    }
};
```

#### 6.2 Long-Running Resource Monitoring
**Objective**: Monitor resource usage over extended periods.

**Test Scenarios**:
- 24-hour stability test
- Memory leak detection over time
- Connection pool leak detection
- Thread leak monitoring

#### 6.3 Cleanup Validation
**Objective**: Ensure proper resource cleanup on shutdown.

**Test Cases**:
- Graceful shutdown releases all resources
- Exception handling doesn't prevent cleanup
- Timeout scenarios don't leave resources

**Implementation Test**:
```cpp
TEST(PerformanceRegressionTest, ResourceCleanupValidation) {
    ResourceTracker tracker;
    auto initial = tracker.takeSnapshot();

    {
        // Create resources that should be cleaned up
        std::vector<std::unique_ptr<TestResource>> resources;
        for (int i = 0; i < 100; ++i) {
            resources.push_back(std::make_unique<TestResource>());
        }

        auto peak = tracker.takeSnapshot();
        EXPECT_GT(peak.memory_allocations - initial.memory_allocations, 0);

        // Resources should be automatically cleaned up when leaving scope
    }

    // Force garbage collection if needed
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto final = tracker.takeSnapshot();
    ResourceSnapshot thresholds{100, 5, 5, 2, 5}; // Allow some variance

    EXPECT_FALSE(tracker.hasResourceLeaks(final, thresholds))
        << "Resource leak detected after scope cleanup";
}
```

## Test Execution Framework

### Build Configuration
```cmake
# Performance Regression Test Target
add_executable(performance_regression_tests
    test/performance_regression_tests.cpp
    test/microbenchmarks.cpp
    test/load_test_framework.cpp
    test/memory_profiler.cpp
    test/resource_tracker.cpp
)

# Optimized build for accurate performance measurements
target_compile_options(performance_regression_tests PRIVATE
    -O3 -DNDEBUG -march=native -mtune=native)

# Link with performance monitoring libraries
target_link_libraries(performance_regression_tests PRIVATE
    ${CMAKE_THREAD_LIBS_INIT}
    perf
)

# Add benchmark library if available
find_package(benchmark QUIET)
if(benchmark_FOUND)
    target_link_libraries(performance_regression_tests PRIVATE benchmark::benchmark)
endif()
```

### Test Execution
```bash
# Build performance tests
cmake -DCMAKE_BUILD_TYPE=Release ..
make performance_regression_tests

# Run full performance test suite
./performance_regression_tests --all

# Run microbenchmarks only
./performance_regression_tests --microbenchmarks

# Run load tests only
./performance_regression_tests --load-tests

# Compare against baseline
./performance_regression_tests --compare baseline.json

# Generate performance report
./performance_regression_tests --report --output performance_report.html
```

### Continuous Integration
```yaml
# .github/workflows/performance_tests.yml
- name: Run Performance Regression Tests
  run: |
    cmake -DCMAKE_BUILD_TYPE=Release ..
    make performance_regression_tests
    ./performance_regression_tests --compare baselines/latest.json
    ./performance_regression_tests --report --output artifacts/performance_report.html

    # Check for regressions
    if ./performance_regression_tests --check-regressions; then
      echo "No performance regressions detected"
    else
      echo "PERFORMANCE REGRESSION DETECTED"
      exit 1
    fi
```

## Success Criteria

### Must Pass
- No performance regressions detected vs baseline
- Memory usage stays within expected bounds
- Load tests meet throughput requirements
- No resource leaks detected

### Should Pass
- Scaling efficiency meets minimum thresholds
- Performance under stress remains acceptable
- Resource cleanup completes properly

### Performance Thresholds
- API response time < 100ms (95th percentile)
- Cache operations < 1ms (average)
- Failover time < 200ms
- Memory growth < 10MB over 24 hours
- Thread scaling efficiency > 70%

## Monitoring and Reporting

### Performance Metrics Dashboard
- Real-time performance graphs
- Regression detection alerts
- Trend analysis
- Capacity planning metrics

### Automated Reporting
- Daily performance summaries
- Weekly trend analysis
- Monthly capacity reports
- Regression incident reports

### Alert Conditions
- Performance regression > 10%
- Memory leak detected
- Resource exhaustion warning
- Scaling efficiency degradation

## Documentation Requirements

### Performance Baselines
- Historical performance data
- Environment specifications
- Load profiles and patterns
- Performance tuning recommendations

### Regression Analysis
- Root cause analysis for regressions
- Performance optimization techniques
- Capacity planning guidelines
- Performance improvement tracking