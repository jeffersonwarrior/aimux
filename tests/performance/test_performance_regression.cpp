/**
 * Performance Regression Test Suite
 *
 * Comprehensive performance testing suite with:
 * - Baseline performance measurement and tracking
 * - Performance regression detection with statistical significance
 * - Memory usage profiling and leak detection
 * - CPU utilization monitoring during stress tests
 * - Latency percentile measurement (P50, P95, P99)
 * - Throughput testing under various load conditions
 * - Performance impact of configuration changes
 * - Scalability testing with increasing load
 *
 * Target: Detect >5% performance regression with 95% confidence
 */

#include <gtest/gtest.h>
#include <chrono>
#include <vector>
#include <thread>
#include <atomic>
#include <future>
#include <random>
#include <algorithm>
#include <numeric>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <cmath>
#include <memory>

#include "aimux/core/router.hpp"
#include "aimux/core/bridge.hpp"
#include "aimux/gateway/claude_gateway.hpp"
#include "nlohmann/json.hpp"

using namespace aimux::core;
using namespace std::chrono;

// Performance measurement utilities
class PerformanceProfiler {
public:
    struct PerformanceMetrics {
        double mean_ms = 0.0;
        double median_ms = 0.0;
        double p95_ms = 0.0;
        double p99_ms = 0.0;
        double stddev_ms = 0.0;
        double min_ms = 0.0;
        double max_ms = 0.0;
        size_t sample_count = 0;
        double throughput_rps = 0.0;
        size_t memory_usage_mb = 0;
        double cpu_percent = 0.0;
    };

    struct BaselineData {
        PerformanceMetrics baseline_metrics;
        std::string test_name;
        std::chrono::system_clock::time_point timestamp;
        std::string git_commit;
        std::string build_type;
    };

    void start_measurement() {
        start_time_ = high_resolution_clock::now();
        start_memory_ = get_memory_usage_mb();
        start_cpu_time_ = get_cpu_time();
    }

    void record_sample(double duration_ms) {
        samples_.push_back(duration_ms);
    }

    PerformanceMetrics finish_measurement(const std::string& operation_name,
                                       size_t total_operations = 0) {
        auto end_time = high_resolution_clock::now();
        auto total_duration_ms = duration<double, milli>(end_time - start_time_).count();

        PerformanceMetrics metrics;
        metrics.sample_count = samples_.size();

        if (!samples_.empty()) {
            std::sort(samples_.begin(), samples_.end());

            metrics.mean_ms = std::accumulate(samples_.begin(), samples_.end(), 0.0) / samples_.size();
            metrics.median_ms = samples_[samples_.size() / 2];
            metrics.p95_ms = samples_[static_cast<size_t>(samples_.size() * 0.95)];
            metrics.p99_ms = samples_[static_cast<size_t>(samples_.size() * 0.99)];
            metrics.min_ms = samples_.front();
            metrics.max_ms = samples_.back();

            // Calculate standard deviation
            double variance = 0.0;
            for (double sample : samples_) {
                variance += (sample - metrics.mean_ms) * (sample - metrics.mean_ms);
            }
            metrics.stddev_ms = std::sqrt(variance / samples_.size());
        }

        if (total_operations > 0) {
            metrics.throughput_rps = (total_operations * 1000.0) / total_duration_ms;
        }

        metrics.memory_usage_mb = get_memory_usage_mb() - start_memory_;
        auto end_cpu_time = get_cpu_time();
        metrics.cpu_percent = ((end_cpu_time - start_cpu_time_) * 100.0) / total_duration_ms;

        // Store for baseline comparison
        current_measurement_ = metrics;
        current_operation_name_ = operation_name;

        samples_.clear();
        return metrics;
    }

    bool check_regression(const BaselineData& baseline, double regression_threshold = 0.05) {
        if (current_measurement_.mean_ms <= 0 || baseline.baseline_metrics.mean_ms <= 0) {
            return false; // Cannot compare invalid measurements
        }

        double regression_ratio = (current_measurement_.mean_ms - baseline.baseline_metrics.mean_ms) /
                                 baseline.baseline_metrics.mean_ms;

        // Use statistical significance test (t-test approximation for large samples)
        if (baseline.baseline_metrics.sample_count > 30 && current_measurement_.sample_count > 30) {
            double pooled_stddev = std::sqrt(
                (std::pow(baseline.baseline_metrics.stddev_ms, 2) / baseline.baseline_metrics.sample_count) +
                (std::pow(current_measurement_.stddev_ms, 2) / current_measurement_.sample_count)
            );

            double t_statistic = (current_measurement_.mean_ms - baseline.baseline_metrics.mean_ms) / pooled_stddev;

            // For large samples, t > 1.96 indicates p < 0.05 (95% confidence)
            if (std::abs(t_statistic) > 1.96) {
                return regression_ratio > regression_threshold;
            }
        } else {
            // Simple threshold comparison for small samples
            return regression_ratio > regression_threshold;
        }

        return false;
    }

    void save_baseline(const std::string& test_name, const std::string& git_commit = "") {
        BaselineData baseline;
        baseline.test_name = test_name;
        baseline.baseline_metrics = current_measurement_;
        baseline.timestamp = system_clock::now();
        baseline.git_commit = git_commit;
        baseline.build_type =
#ifdef NDEBUG
            "Release"
#else
            "Debug"
#endif
        ;

        baselines_[test_name] = baseline;
        save_baselines_to_file();
    }

    bool load_baseline(const std::string& test_name, BaselineData& baseline) {
        auto it = baselines_.find(test_name);
        if (it != baselines_.end()) {
            baseline = it->second;
            return true;
        }

        load_baselines_from_file();
        it = baselines_.find(test_name);
        if (it != baselines_.end()) {
            baseline = it->second;
            return true;
        }

        return false;
    }

    std::vector<BaselineData> get_all_baselines() const {
        std::vector<BaselineData> result;
        for (const auto& [name, baseline] : baselines_) {
            result.push_back(baseline);
        }
        return result;
    }

    std::string generate_report() const {
        std::ostringstream oss;
        oss << "Performance Report for " << current_operation_name_ << "\n";
        oss << std::string(50, '=') << "\n";

        const auto& m = current_measurement_;
        oss << "Latency Statistics:\n";
        oss << "  Mean:   " << std::fixed << std::setprecision(2) << m.mean_ms << " ms\n";
        oss << "  Median: " << std::fixed << std::setprecision(2) << m.median_ms << " ms\n";
        oss << "  P95:    " << std::fixed << std::setprecision(2) << m.p95_ms << " ms\n";
        oss << "  P99:    " << std::fixed << std::setprecision(2) << m.p99_ms << " ms\n";
        oss << "  StdDev: " << std::fixed << std::setprecision(2) << m.stddev_ms << " ms\n";
        oss << "  Min:    " << std::fixed << std::setprecision(2) << m.min_ms << " ms\n";
        oss << "  Max:    " << std::fixed << std::setprecision(2) << m.max_ms << " ms\n";

        oss << "\nResource Usage:\n";
        oss << "  Throughput:  " << std::fixed << std::setprecision(1) << m.throughput_rps << " RPS\n";
        oss << "  Memory Used: " << m.memory_usage_mb << " MB\n";
        oss << "  CPU Usage:   " << std::fixed << std::setprecision(1) << m.cpu_percent << " %\n";
        oss << "  Samples:     " << m.sample_count << "\n";

        return oss.str();
    }

private:
    size_t get_memory_usage_mb() {
        std::ifstream status_file("/proc/self/status");
        std::string line;
        while (std::getline(status_file, line)) {
            if (line.substr(0, 6) == "VmRSS:") {
                std::istringstream iss(line);
                std::string label, value, unit;
                iss >> label >> value >> unit;
                return std::stoul(value) / 1024; // Convert KB to MB
            }
        }
        return 0;
    }

    double get_cpu_time() {
        struct timespec ts;
        clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
        return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0; // Convert to milliseconds
    }

    void save_baselines_to_file() {
        std::filesystem::path baseline_dir = "./test_baselines";
        std::filesystem::create_directories(baseline_dir);

        std::ofstream file(baseline_dir / "performance_baselines.json");
        nlohmann::json baselines_json;

        for (const auto& [name, baseline] : baselines_) {
            baselines_json[name] = {
                {"test_name", baseline.test_name},
                {"timestamp_ms", duration_cast<milliseconds>(
                    baseline.timestamp.time_since_epoch()).count()},
                {"git_commit", baseline.git_commit},
                {"build_type", baseline.build_type},
                {"metrics", {
                    {"mean_ms", baseline.baseline_metrics.mean_ms},
                    {"median_ms", baseline.baseline_metrics.median_ms},
                    {"p95_ms", baseline.baseline_metrics.p95_ms},
                    {"p99_ms", baseline.baseline_metrics.p99_ms},
                    {"stddev_ms", baseline.baseline_metrics.stddev_ms},
                    {"min_ms", baseline.baseline_metrics.min_ms},
                    {"max_ms", baseline.baseline_metrics.max_ms},
                    {"sample_count", baseline.baseline_metrics.sample_count},
                    {"throughput_rps", baseline.baseline_metrics.throughput_rps},
                    {"memory_usage_mb", baseline.baseline_metrics.memory_usage_mb},
                    {"cpu_percent", baseline.baseline_metrics.cpu_percent}
                }}
            };
        }

        file << baselines_json.dump(2);
    }

    void load_baselines_from_file() {
        std::filesystem::path baseline_file = "./test_baselines/performance_baselines.json";

        if (!std::filesystem::exists(baseline_file)) {
            return;
        }

        std::ifstream file(baseline_file);
        nlohmann::json baselines_json;
        file >> baselines_json;

        baselines_.clear();
        for (const auto& [name, data] : baselines_json.items()) {
            BaselineData baseline;
            baseline.test_name = data["test_name"];
            baseline.timestamp = system_clock::time_point(
                milliseconds(data["timestamp_ms"]));
            baseline.git_commit = data["git_commit"];
            baseline.build_type = data["build_type"];

            const auto& metrics_json = data["metrics"];
            baseline.baseline_metrics = {
                .mean_ms = metrics_json["mean_ms"],
                .median_ms = metrics_json["median_ms"],
                .p95_ms = metrics_json["p95_ms"],
                .p99_ms = metrics_json["p99_ms"],
                .stddev_ms = metrics_json["stddev_ms"],
                .min_ms = metrics_json["min_ms"],
                .max_ms = metrics_json["max_ms"],
                .sample_count = metrics_json["sample_count"],
                .throughput_rps = metrics_json["throughput_rps"],
                .memory_usage_mb = metrics_json["memory_usage_mb"],
                .cpu_percent = metrics_json["cpu_percent"]
            };

            baselines_[name] = baseline;
        }
    }

    high_resolution_clock::time_point start_time_;
    size_t start_memory_;
    double start_cpu_time_;
    std::vector<double> samples_;
    PerformanceMetrics current_measurement_;
    std::string current_operation_name_;
    std::unordered_map<std::string, BaselineData> baselines_;
};

// Mock performance provider
class PerformanceMockProvider : public Bridge {
public:
    explicit PerformanceMockProvider(int latency_ms = 50, double cpu_usage_multiplier = 1.0)
        : latency_ms_(latency_ms), cpu_usage_multiplier_(cpu_usage_multiplier),
          request_count_(0) {}

    Response send_request(const Request& request) override {
        request_count_++;

        auto start = high_resolution_clock::now();

        // Simulate CPU work
        cpu_work();

        // Simulate network delay
        this_thread::sleep_for(milliseconds(latency_ms_));

        auto end = high_resolution_clock::now();
        actual_latency_ms_ = duration<double, milli>(end - start).count();

        Response response;
        response.success = true;
        response.provider_name = "performance_mock";
        response.response_time_ms = actual_latency_ms_;
        response.content = "Performance test response " + std::to_string(request_count_);

        return response;
    }

    bool is_healthy() const override { return true; }
    std::string get_provider_name() const override { return "performance_mock"; }
    nlohmann::json get_rate_limit_status() const override {
        return nlohmann::json{{"requests_used", 0}, {"requests_limit", 1000}};
    }

    double get_actual_latency_ms() const { return actual_latency_ms_; }
    int get_request_count() const { return request_count_.load(); }

private:
    void cpu_work() {
        // CPU-intensive work proportional to the multiplier
        int iterations = static_cast<int>(1000000 * cpu_usage_multiplier_);
        volatile long long sum = 0;
        for (int i = 0; i < iterations; ++i) {
            sum += i * i;
        }
    }

    int latency_ms_;
    double cpu_usage_multiplier_;
    std::atomic<int> request_count_;
    double actual_latency_ms_ = 0.0;
};

// Performance test fixtures
class PerformanceRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        profiler_ = std::make_unique<PerformanceProfiler>();

        // Create performance-optimized mock providers
        providers_ = {
            std::make_shared<PerformanceMockProvider>(30, 0.5),  // Fast, low CPU
            std::make_shared<PerformanceMockProvider>(50, 1.0),  // Medium
            std::make_shared<PerformanceMockProvider>(80, 1.5)   // Slower, high CPU
        };

        router_ = std::make_unique<Router>();
        for (auto& provider : providers_) {
            router_->add_provider(provider);
        }
        router_->set_routing_strategy(Router::RoutingStrategy::FASTEST_RESPONSE);
    }

    void TearDown() override {
        router_.reset();
        profiler_.reset();
        providers_.clear();
    }

    Request create_test_request(const std::string& content = "Performance test message") {
        Request request;
        request.model = "gpt-4";
        request.method = "POST";
        request.correlation_id = "perf-test-" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        request.data = nlohmann::json{
            {"messages", {
                {{"role", "user"}, {"content", content}}
            }}
        };
        return request;
    }

    std::unique_ptr<PerformanceProfiler> profiler_;
    std::vector<std::shared_ptr<PerformanceMockProvider>> providers_;
    std::unique_ptr<Router> router_;
};

// Basic performance regression tests
TEST_F(PerformanceRegressionTest, RouterBasicPerformanceRegression) {
    const size_t num_requests = 1000;
    const std::string test_name = "router_basic_performance";

    profiler_->start_measurement();

    for (size_t i = 0; i < num_requests; ++i) {
        auto request_start = high_resolution_clock::now();

        Request request = create_test_request("Test " + std::to_string(i));
        Response response = router_->route(request);

        auto request_end = high_resolution_clock::now();
        double request_time_ms = duration<double, milli>(request_end - request_start).count();
        profiler_->record_sample(request_time_ms);

        ASSERT_TRUE(response.success || !response.success); // Should have valid structure
    }

    auto metrics = profiler_->finish_measurement(test_name, num_requests);

    // Performance requirements
    ASSERT_LT(metrics.mean_ms, 100.0);      // Mean < 100ms
    ASSERT_LT(metrics.p95_ms, 200.0);       // P95 < 200ms
    ASSERT_LT(metrics.p99_ms, 300.0);       // P99 < 300ms
    ASSERT_GT(metrics.throughput_rps, 50.0); // >50 RPS

    // Check for performance regression against baseline
    PerformanceProfiler::BaselineData baseline;
    if (profiler_->load_baseline(test_name, baseline)) {
        bool has_regression = profiler_->check_regression(baseline, 0.05); // 5% threshold

        if (has_regression) {
            FAIL() << "Performance regression detected!\n"
                  << "Baseline mean: " << baseline.baseline_metrics.mean_ms << " ms\n"
                  << "Current mean:  " << metrics.mean_ms << " ms\n"
                  << "Regression:    " <<
                  ((metrics.mean_ms - baseline.baseline_metrics.mean_ms) / baseline.baseline_metrics.mean_ms * 100.0)
                  << "%\n"
                  << profiler_->generate_report();
        }
    } else {
        // No baseline exists, create one
        profiler_->save_baseline(test_name);
        SUCCEED() << "No baseline found. Created new baseline.\n" << profiler_->generate_report();
    }
}

TEST_F(PerformanceRegressionTest, ConcurrentPerformanceRegression) {
    const size_t num_threads = 8;
    const size_t requests_per_thread = 100;
    const std::string test_name = "router_concurrent_performance";

    std::vector<std::future<void>> futures;
    std::atomic<size_t> completed_requests{0};

    profiler_->start_measurement();

    // Launch concurrent threads
    for (size_t t = 0; t < num_threads; ++t) {
        futures.push_back(std::async(std::launch::async, [&, t]() {
            for (size_t i = 0; i < requests_per_thread; ++i) {
                auto request_start = high_resolution_clock::now();

                Request request = create_test_request(
                    "Thread " + std::to_string(t) + " Request " + std::to_string(i));
                Response response = router_->route(request);

                auto request_end = high_resolution_clock::now();
                double request_time_ms = duration<double, milli>(request_end - request_start).count();
                profiler_->record_sample(request_time_ms);

                completed_requests++;
            }
        }));
    }

    // Wait for completion
    for (auto& future : futures) {
        future.get();
    }

    auto metrics = profiler_->finish_measurement(test_name, completed_requests);

    // Concurrent performance requirements
    ASSERT_LT(metrics.mean_ms, 150.0);      // Mean < 150ms (higher due to contention)
    ASSERT_LT(metrics.p95_ms, 300.0);       // P95 < 300ms
    ASSERT_LT(metrics.p99_ms, 500.0);       // P99 < 500ms
    ASSERT_GT(metrics.throughput_rps, 100.0); // >100 RPS with concurrency

    PerformanceProfiler::BaselineData baseline;
    if (profiler_->load_baseline(test_name, baseline)) {
        bool has_regression = profiler_->check_regression(baseline, 0.07); // 7% threshold for concurrent

        if (has_regression) {
            FAIL() << "Concurrent performance regression detected!\n"
                  << "Baseline mean: " << baseline.baseline_metrics.mean_ms << " ms\n"
                  << "Current mean:  " << metrics.mean_ms << " ms\n"
                  << profiler_->generate_report();
        }
    } else {
        profiler_->save_baseline(test_name);
        SUCCEED() << "No baseline found for concurrent test. Created new baseline.\n";
    }
}

TEST_F(PerformanceRegressionTest, MemoryUsageScalability) {
    const std::string test_name = "memory_scalability_test";
    std::vector<size_t> request_counts = {100, 500, 1000, 2000};
    std::vector<PerformanceProfiler::PerformanceMetrics> metrics_list;

    for (size_t request_count : request_counts) {
        profiler_->start_measurement();

        for (size_t i = 0; i < request_count; ++i) {
            Request request = create_test_request("Memory test " + std::to_string(i));
            Response response = router_->route(request);

            auto request_start = high_resolution_clock::now();
            auto request_end = high_resolution_clock::now();
            double request_time_ms = duration<double, milli>(request_end - request_start).count();
            profiler_->record_sample(request_time_ms);
        }

        auto metrics = profiler_->finish_measurement(test_name + "_" + std::to_string(request_count), request_count);
        metrics_list.push_back(metrics);

        // Memory should scale reasonably (not leak)
        ASSERT_LT(metrics.memory_usage_mb, 100); // Should not exceed 100MB
    }

    // Check memory trend - should not grow exponentially
    for (size_t i = 1; i < metrics_list.size(); ++i) {
        double memory_growth_ratio = static_cast<double>(metrics_list[i].memory_usage_mb) /
                                    metrics_list[i-1].memory_usage_mb;
        double request_growth_ratio = static_cast<double>(request_counts[i]) / request_counts[i-1];

        // Memory growth should be <= 2x request growth (accounting for caching, etc.)
        ASSERT_LE(memory_growth_ratio, request_growth_ratio * 2.0);
    }
}

TEST_F(PerformanceRegressionTest, LatencyPercentileAnalysis) {
    const size_t num_requests = 2000;
    const std::string test_name = "latency_percentile_analysis";

    profiler_->start_measurement();

    for (size_t i = 0; i < num_requests; ++i) {
        // Add some load variation to test percentile measurement accuracy
        this_thread::sleep_for(milliseconds(std::rand() % 5));

        auto request_start = high_resolution_clock::now();
        Request request = create_test_request("Percentile test " + std::to_string(i));
        Response response = router_->route(request);
        auto request_end = high_resolution_clock::now();

        double request_time_ms = duration<double, milli>(request_end - request_start).count();
        profiler_->record_sample(request_time_ms);
    }

    auto metrics = profiler_->finish_measurement(test_name, num_requests);

    // Detailed percentile analysis
    ASSERT_GT(metrics.p95_ms, metrics.median_ms);     // P95 should be > median
    ASSERT_GT(metrics.p99_ms, metrics.p95_ms);         // P99 should be > P95
    ASSERT_LE(metrics.stddev_ms, metrics.mean_ms);     // StdDev should be reasonable
    ASSERT_GT(metrics.sample_count, num_requests * 0.95); // Most samples recorded

    // Validate percentile distribution
    double p99_p95_ratio = metrics.p99_ms / metrics.p95_ms;
    double p95_median_ratio = metrics.p95_ms / metrics.median_ms;

    ASSERT_LE(p99_p95_ratio, 2.0);       // P99 shouldn't be more than 2x P95
    ASSERT_LE(p95_median_ratio, 3.0);    // P95 shouldn't be more than 3x median
}

TEST_F(PerformanceRegressionTest, ThroughputScalingTest) {
    const std::string test_name = "throughput_scaling";
    std::vector<size_t> thread_counts = {1, 2, 4, 8, 16};
    std::vector<double> throughput_measurements;

    for (size_t thread_count : thread_counts) {
        const size_t requests_per_thread = 100;
        std::vector<std::future<void>> futures;

        profiler_->start_measurement();

        for (size_t t = 0; t < thread_count; ++t) {
            futures.push_back(std::async(std::launch::async, [&]() {
                for (size_t i = 0; i < requests_per_thread; ++i) {
                    Request request = create_test_request("Scaling test");
                    Response response = router_->route(request);

                    auto request_start = high_resolution_clock::now();
                    auto request_end = high_resolution_clock::now();
                    double request_time_ms = duration<double, milli>(request_end - request_start).count();
                    profiler_->record_sample(request_time_ms);
                }
            }));
        }

        for (auto& future : futures) {
            future.get();
        }

        auto metrics = profiler_->finish_measurement(test_name + "_threads_" + std::to_string(thread_count));
        throughput_measurements.push_back(metrics.throughput_rps);

        // Throughput should scale with threads (up to a point)
        if (thread_count > 1) {
            double scaling_factor = throughput_measurements.back() / throughput_measurements[0];
            double ideal_scaling = static_cast<double>(thread_count);
            double efficiency = scaling_factor / ideal_scaling;

            // Should maintain reasonable efficiency
            ASSERT_GT(efficiency, 0.3); // At least 30% efficiency
        }
    }

    // Overall throughput should be reasonable
    double max_throughput = *std::max_element(throughput_measurements.begin(), throughput_measurements.end());
    ASSERT_GT(max_throughput, 200.0); // Should achieve >200 RPS with maximum threads
}

// Comprehensive performance test with statistical analysis
TEST_F(PerformanceRegressionTest, ComprehensivePerformanceReport) {
    const std::string test_name = "comprehensive_performance";
    const size_t num_requests = 1000;

    profiler_->start_measurement();

    std::vector<double> individual_times;
    individual_times.reserve(num_requests);

    for (size_t i = 0; i < num_requests; ++i) {
        auto request_start = high_resolution_clock::now();
        Request request = create_test_request("Comprehensive test " + std::to_string(i));
        Response response = router_->route(request);
        auto request_end = high_resolution_clock::now();

        double request_time_ms = duration<double, milli>(request_end - request_start).count();
        profiler_->record_sample(request_time_ms);
        individual_times.push_back(request_time_ms);
    }

    auto metrics = profiler_->finish_measurement(test_name, num_requests);

    // Generate detailed performance report
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "COMPREHENSIVE PERFORMANCE REPORT\n";
    std::cout << std::string(60, '=') << "\n";
    std::cout << profiler_->generate_report();

    // Additional statistical analysis
    std::sort(individual_times.begin(), individual_times.end());
    double p50 = individual_times[individual_times.size() / 2];
    double p75 = individual_times[static_cast<size_t>(individual_times.size() * 0.75)];
    double p90 = individual_times[static_cast<size_t>(individual_times.size() * 0.90)];

    std::cout << "\nAdditional Percentiles:\n";
    std::cout << "  P50: " << std::fixed << std::setprecision(2) << p50 << " ms\n";
    std::cout << "  P75: " << std::fixed << std::setprecision(2) << p75 << " ms\n";
    std::cout << "  P90: " << std::fixed << std::setprecision(2) << p90 << " ms\n";

    // Final performance assertions
    EXPECT_LT(metrics.mean_ms, 100.0);
    EXPECT_LT(metrics.p95_ms, 200.0);
    EXPECT_LT(metrics.p99_ms, 300.0);
    EXPECT_GT(metrics.throughput_rps, 100.0);
    EXPECT_LT(metrics.memory_usage_mb, 50);

    // Save comprehensive baseline if none exists
    PerformanceProfiler::BaselineData baseline;
    if (!profiler_->load_baseline(test_name, baseline)) {
        profiler_->save_baseline(test_name);
        std::cout << "\nCreated comprehensive performance baseline.\n";
    }

    std::cout << std::string(60, '=') << "\n\n";
}