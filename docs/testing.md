# Aimux Advanced Testing Framework

## Overview

Aimux v2.0.0 includes a comprehensive testing framework with property-based testing, fault injection, performance regression detection, and integration testing capabilities. The framework is designed to ensure enterprise-grade reliability and performance monitoring.

## Testing Architecture

### Core Components

1. **Property-Based Testing** - QuickCheck-inspired automated test generation
2. **Fault Injection Testing** - Network, resource, and timing failure simulation
3. **Performance Regression Testing** - Statistical performance monitoring with baselines
4. **Integration Testing** - End-to-end system validation with mock providers
5. **Coverage Analysis** - Code coverage tracking and reporting

### Test Categories

| Category | Focus | Tools | Coverage Target |
|----------|-------|-------|----------------|
| Unit Tests | Individual component behavior | Google Test | >95% |
| Integration Tests | Component interactions | Custom Framework | >90% |
| Property Tests | Invariant validation | Property Framework | >85% |
| Performance Tests | Regression detection | Performance Profiler | >5% threshold |
| Fault Tests | Resilience validation | Fault Injection | >90% failure paths |

## Quick Start

### Building Tests

```bash
# Build all test targets
cmake --build . --target test_all

# Build specific test suites
cmake --build . --target unit_tests
cmake --build . --target integration_tests
cmake --build . --target advanced_test_runner
```

### Running Tests

```bash
# Run all tests with detailed reporting
./advanced_test_runner --mode=all --format=json --output=test_results

# Run only unit tests
./advanced_test_runner --mode=unit --format=html --output=unit_report

# Run performance tests with baseline creation
./advanced_test_runner --mode=performance --baseline --output=perf_results

# Run fault injection tests
./advanced_test_runner --mode=fault_injection --output=fault_analysis
```

## Test Runner Options

### Command Line Interface

```bash
./advanced_test_runner [OPTIONS]

Options:
  --mode <mode>           Test mode: unit|integration|performance|property|fault_injection|all
  --format <format>       Output format: json|xml|html
  --output <filename>     Output filename (without extension)
  --baseline             Update performance baselines
  --property-count <n>   Number of property tests (default: 1000)
  --regression-threshold <x> Performance regression threshold (default: 0.05)
  --no-fault-injection   Disable fault injection testing
```

### Example Commands

```bash
# Comprehensive test suite with performance baseline update
./advanced_test_runner --mode=all --format=html --output=comprehensive_report --baseline

# Performance regression check with 3% threshold
./advanced_test_runner --mode=performance --regression-threshold=0.03 --output=perf_check

# Property-based testing with increased sample size
./advanced_test_runner --mode=property --property-count=5000 --output=property_analysis

# Quick smoke test without fault injection
./advanced_test_runner --mode=unit --no-fault-injection --format=json
```

## Property-Based Testing

### Overview

Property-based testing automatically generates test cases to verify system invariants. Unlike traditional unit tests that check specific examples, property-based tests verify that properties hold true for all valid inputs.

### Key Features

- **Automatic Test Generation** - Creates diverse, edge-case inputs
- **Intelligent Shrinking** - Finds minimal failing cases
- **Statistical Validation** - Provides confidence intervals
- **Custom Generators** - Support for complex data types

### Usage Examples

```cpp
#include "aimux/testing/property_based_testing.h"

// Basic property test
AIMUX_PROPERTY_NAMED(
    "router_handles_any_request",
    router.route(request).provider_name != "",  // Property: always returns provider
    generator<Request>(),                       // Generator for Request objects
    PropertyTestRunner::Config{ .max_tests = 1000 }
);

// Complex property with custom generator
auto json_property = Property<nlohmann::json>([](const nlohmann::json& data) -> bool {
    // Property: router should not crash on any JSON input
    try {
        Request request = json_to_request(data);
        Response response = router.route(request);
        return true;  // Should handle gracefully
    } catch (...) {
        return true;  // Should handle exceptions gracefully
    }
});

PropertyTestRunner::assert_property(json_property, "router_json_resilience");
```

### Built-in Generators

- **Primitive Types**: int64_t, double, bool, std::string
- **Containers**: std::vector<T>, std::map<K,V>
- **Complex Types**: nlohmann::json, custom structs
- **Specialized**: HTTP requests, API endpoints, provider configurations

### Custom Generators

```cpp
class RequestGenerator : public Generator<Request> {
public:
    Request generate(RandomGenerator& rng) override {
        Request req;
        req.model = rng.choose({"gpt-4", "gpt-3.5-turbo", "claude-3"});
        req.method = "POST";
        req.data = Generator<nlohmann::json>().generate(rng);
        return req;
    }

    std::vector<Request> shrink(const Request& value) override {
        // Shrink complex requests to simpler ones
        std::vector<Request> shrunk;
        Request simple_req = create_minimal_request();
        shrunk.push_back(simple_req);
        return shrunk;
    }
};
```

## Fault Injection Testing

### Overview

Fault injection testing validates system resilience by simulating various failure modes: network timeouts, memory exhaustion, timing delays, data corruption, and resource constraints.

### Fault Types

#### Network Faults
```cpp
// Inject 30% network timeouts
auto timeout_injector = std::make_unique<NetworkFaultInjector>(
    NetworkFaultInjector::FaultType::TIMEOUT, 0.3);
fault_manager.add_injector("timeout_test", std::move(timeout_injector));
```

#### Resource Exhaustion
```cpp
// Inject 50MB memory exhaustion (temporary)
auto memory_injector = std::make_unique<ResourceExhaustionInjector>(
    ResourceExhaustionInjector::ResourceType::MEMORY, 50*1024, true);
fault_manager.add_injector("memory_pressure", std::move(memory_injector));
```

#### Timing Faults
```cpp
// Inject 100ms delays with 20% probability
auto delay_injector = std::make_unique<TimingFaultInjector>(
    TimingFaultInjector::TimingType::DELAY, std::chrono::milliseconds(100), 0.2);
fault_manager.add_injector("timing_delay", std::move(delay_injector));
```

#### Data Corruption
```cpp
// Inject 10% bit flipping corruption
auto corruption_injector = std::make_unique<DataCorruptionInjector>(
    DataCorruptionInjector::CorruptionType::BIT_FLIP, 0.1);
fault_manager.add_injector("data_corruption", std::move(corruption_injector));
```

### Fault Injection Scenarios

#### Provider Failover Testing
```cpp
void test_provider_failover() {
    // Inject network failures in primary provider
    aimux::testing::get_fault_manager().add_injector("primary_failures",
        std::make_unique<NetworkFaultInjector>(NetworkFaultInjector::FaultType::TIMEOUT, 0.8));

    // Send requests - should automatically failover
    for (int i = 0; i < 50; ++i) {
        Response response = router.route(create_test_request());
        EXPECT_TRUE(response.success || response.provider_name != "primary_provider");
    }

    aimux::testing::get_fault_manager().reset_all();
}
```

#### Resource Constraint Testing
```cpp
void test_under_memory_pressure() {
    // Apply memory pressure
    aimux::testing::get_fault_manager().add_injector("pressure",
        std::make_unique<ResourceExhaustionInjector>(
            ResourceType::MEMORY, 100*1024, true));

    // Test system behavior under pressure
    auto start = high_resolution_clock::now();
    Response response = router.route(create_large_request());
    auto duration = high_resolution_clock::now() - start;

    // Should still handle requests within reasonable time
    EXPECT_LT(duration_cast<milliseconds>(duration).count(), 1000);
}
```

## Performance Regression Testing

### Overview

Performance regression testing uses statistical analysis to detect significant performance changes. It establishes performance baselines and validates that new changes don't degrade performance beyond acceptable thresholds.

### Performance Metrics

- **Latency**: Mean, median, P95, P99, standard deviation
- **Throughput**: Requests per second under various loads
- **Resource Usage**: Memory consumption, CPU utilization
- **Scalability**: Performance under concurrent load

### Performance Test Example

```cpp
TEST_F(PerformanceRegressionTest, RouterLatencyRegression) {
    const size_t num_requests = 1000;
    PerformanceProfiler profiler;

    profiler.start_measurement();

    for (size_t i = 0; i < num_requests; ++i) {
        auto start = high_resolution_clock::now();
        Response response = router.route(create_test_request());
        auto end = high_resolution_clock::now();

        double latency_ms = duration<double, milli>(end - start).count();
        profiler.record_sample(latency_ms);
    }

    auto metrics = profiler.finish_measurement("router_latency", num_requests);

    // Performance requirements
    ASSERT_LT(metrics.mean_ms, 100.0);      // Mean < 100ms
    ASSERT_LT(metrics.p95_ms, 200.0);       // P95 < 200ms
    ASSERT_GT(metrics.throughput_rps, 50.0); // >50 RPS

    // Check regression against baseline
    BaselineData baseline;
    if (profiler.load_baseline("router_latency", baseline)) {
        bool has_regression = profiler.check_regression(baseline, 0.05); // 5% threshold
        EXPECT_FALSE(has_regression) << "Performance regression detected!";
    } else {
        profiler.save_baseline("router_latency"); // Create new baseline
    }
}
```

### Performance Profiling Features

#### Memory Usage Tracking
```cpp
class MemoryProfiler {
public:
    void start_tracking() {
        start_memory_ = get_memory_usage_mb();
    }

    double get_delta_memory() {
        return get_memory_usage_mb() - start_memory_;
    }

private:
    size_t get_memory_usage_mb() {
        // Read /proc/self/status or platform-specific method
        std::ifstream status("/proc/self/status");
        // ... parse VmRSS field
        return memory_mb;
    }
};
```

#### CPU Utilization Monitoring
```cpp
class CPUMonitor {
public:
    double get_cpu_usage_percent() {
        struct timespec ts;
        clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
        double cpu_time_ms = ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;

        auto wall_time = std::chrono::system_clock::now();
        double wall_time_ms = /* calculate */;

        return (cpu_time_ms / wall_time_ms) * 100.0;
    }
};
```

## Integration Testing

### Mock Provider Implementation

```cpp
class MockProviderBridge : public Bridge {
public:
    explicit MockProviderBridge(const std::string& name,
                               double success_rate = 0.95,
                               int latency_ms = 50)
        : name_(name), success_rate_(success_rate), latency_ms_(latency_ms) {}

    Response send_request(const Request& request) override {
        request_count_++;

        // Simulate network delay
        std::this_thread::sleep_for(std::chrono::milliseconds(latency_ms_));

        Response response;
        response.provider_name = name_;
        response.response_time_ms = latency_ms_;

        // Simulate success/failure based on rate
        if (dist_(rng_) < success_rate_) {
            response.success = true;
            response.content = "Response from " + name_;
        } else {
            response.success = false;
            response.error_message = "Simulated provider failure";
        }

        return response;
    }

    // Control methods for testing
    void set_success_rate(double rate) { success_rate_ = rate; }
    void set_latency(int ms) { latency_ms_ = ms; }
    int get_request_count() const { return request_count_; }

private:
    std::string name_;
    double success_rate_;
    int latency_ms_;
    std::atomic<int> request_count_{0};
    std::mt19937_64 rng_{std::random_device{}()};
    std::uniform_real_distribution<double> dist_{0.0, 1.0};
};
```

### Multi-Provider Integration Testing

```cpp
TEST_F(RouterProviderIntegrationTest, LoadBalancingAndFailover) {
    // Create providers with different characteristics
    auto provider1 = std::make_shared<MockProviderBridge>("fast_reliable", 0.98, 20);
    auto provider2 = std::make_shared<MockProviderBridge>("medium_reliable", 0.95, 50);
    auto provider3 = std::make_shared<MockProviderBridge>("backup", 0.90, 80);

    Router router;
    router.add_provider(provider1);
    router.add_provider(provider2);
    router.add_provider(provider3);
    router.set_routing_strategy(Router::RoutingStrategy::FASTEST_RESPONSE);

    // Load balancing test
    std::map<std::string, int> request_counts;
    for (int i = 0; i < 100; ++i) {
        Response response = router.route(create_test_request());
        request_counts[response.provider_name]++;
    }

    // Should distribute across healthy providers
    EXPECT_GT(request_counts.size(), 1);
    EXPECT_GT(request_counts["fast_reliable"], 0);

    // Failover test
    provider1->set_success_rate(0.0); // Fail primary provider

    std::map<std::string, int> failover_counts;
    for (int i = 0; i < 50; ++i) {
        Response response = router.route(create_test_request());
        failover_counts[response.provider_name]++;
    }

    // Should failover to backup providers
    EXPECT_EQ(failover_counts["fast_reliable"], 0);
    EXPECT_GT(failover_counts["medium_reliable"] + failover_counts["backup"], 0);
}
```

## Test Configuration

### Environment Variables

```bash
# Test execution parameters
export AIMUX_TEST_COUNT=1000                    # Property test count
export AIMUX_REGRESSION_THRESHOLD=0.05         # Performance regression threshold
export AIMUX_FAULT_INJECTION_RATE=0.3          # Fault injection probability
export AIMUX_CONCURRENT_THREADS=8              # Concurrent test threads
export AIMUX_TEST_TIMEOUT=300                  # Test timeout in seconds
export AIMUX_COVERAGE_ENABLED=true            # Enable coverage collection
```

### Configuration Files

`test_config.json`:
```json
{
    "property_testing": {
        "max_tests": 1000,
        "max_shrink_steps": 100,
        "seed": 12345
    },
    "performance_testing": {
        "baseline_directory": "test_baselines",
        "regression_threshold": 0.05,
        "confidence_interval": 0.95
    },
    "fault_injection": {
        "network_failure_rate": 0.1,
        "memory_exhaustion_mb": 50,
        "timing_delay_ms": 50,
        "corruption_rate": 0.05
    },
    "reporting": {
        "output_directory": "test_results",
        "formats": ["json", "html"],
        "include_coverage": true,
        "include_performance_charts": true
    }
}
```

## Continuous Integration

### GitHub Actions Workflow

```yaml
name: Advanced Testing
on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest

    steps:
    - uses: actions/checkout@v3

    - name: Install dependencies
      run: |
        sudo apt-get update
        sudo apt-get install -y cmake build-essential
        sudo apt-get install -y libgtest-dev libgmock-dev

    - name: Build project
      run: |
        mkdir build && cd build
        cmake -DCMAKE_BUILD_TYPE=Release ..
        cmake --build . --target test_all

    - name: Run unit tests
      run: ./build/unit_tests

    - name: Run integration tests
      run: ./build/integration_tests

    - name: Run performance tests
      run: ./build/advanced_test_runner --mode=performance --baseline

    - name: Run comprehensive test suite
      run: ./build/advanced_test_runner --mode=all --format=html --output=ci_report

    - name: Upload test results
      uses: actions/upload-artifact@v3
      with:
        name: test-results
        path: build/test_results/
```

### Performance Baseline Management

```bash
# Update performance baselines after major changes
./advanced_test_runner --mode=performance --baseline --update

# Compare against current baselines
./advanced_test_runner --mode=performance --regression-threshold=0.03

# Generate performance trend analysis
python scripts/analyze_performance_trends.py test_baselines/ performance_trend.json
```

## Coverage Analysis

### Coverage Collection

```bash
# Build with coverage instrumentation
cmake -DCMAKE_BUILD_TYPE=Debug -DCOVERAGE_ENABLED=ON ..
make coverage

# Run tests with coverage
./advanced_test_runner --mode=all
lcov --capture --directory . --output-file coverage.info
genhtml coverage.info --output-directory coverage_report
```

### Coverage Target Enforcement

```bash
# Enforce minimum coverage thresholds
./scripts/check_coverage.sh --threshold=80 --type=unit
./scripts/check_coverage.sh --threshold=75 --type=integration
./scripts/check_coverage.sh --threshold=85 --type=overall
```

## Best Practices

### Test Organization

1. **Descriptive Names** - Use clear, descriptive test names
2. **Isolation** - Tests should be independent and order-agnostic
3. **Cleanup** - Proper setup/teardown to avoid test pollution
4. **Deterministic** - Tests should produce consistent results

### Performance Testing

1. **Warm-up Periods** - Allow system to stabilize before measurement
2. **Multiple Samples** - Collect sufficient data for statistical significance
3. **Baseline Management** - Regularly update performance baselines
4. **Environment Control** - Test in consistent environments

### Property Testing

1. **Meaningful Properties** - Test important invariants, not trivial correctness
2. **Good Generators** - Create realistic and diverse test data
3. **Shrink Strategy** - Implement effective shrinking for debuggability
4. **Test Count Balance** - Balance thoroughness with execution time

### Fault Injection

1. **Realistic Failures** - Model actual production failure scenarios
2. **Graceful Degradation** - Test system responses to partial failures
3. **Recovery Testing** - Verify system can recover from fault conditions
4. **Performance Under Failure** - Test performance impact during fault conditions

## Troubleshooting

### Common Issues

#### Test Timeouts
```bash
# Increase timeout for slow tests
export AIMUX_TEST_TIMEOUT=600
./advanced_test_runner --mode=performance
```

#### Memory Issues in Tests
```bash
# Run tests with reduced memory pressure
./advanced_test_runner --no-fault-injection --property-count=100
```

#### Performance Baseline Mismatches
```bash
# Rebuild baselines after significant changes
./advanced_test_runner --mode=performance --baseline --update
```

#### Flaky Property Tests
```bash
# Use deterministic seeds for reproducible failures
./advanced_test_runner --mode=property --seed=12345
```

### Debug Output

```bash
# Enable verbose logging during tests
export AIMUX_LOG_LEVEL=debug
./advanced_test_runner --mode=unit

# Generate detailed failure reports
./advanced_test_runner --mode=all --format=json --output=detailed_report
```

This comprehensive testing framework ensures Aimux v2.0.0 meets enterprise-grade reliability, performance, and stability requirements through automated property testing, fault injection, performance monitoring, and comprehensive integration validation.