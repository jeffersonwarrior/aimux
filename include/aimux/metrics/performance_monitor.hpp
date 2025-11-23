#pragma once

#include "metrics_collector.hpp"
#include "time_series_db.hpp"
#include <chrono>
#include <string>
#include <unordered_map>
#include <memory>
#include <atomic>
#include <mutex>
#include <functional>

namespace aimux {
namespace metrics {

/**
 * @brief High-resolution performance timer with automatic metrics recording
 *
 * RAII timer that automatically records timing metrics when destroyed.
 * Supports nested timing and custom tags.
 */
class PerformanceTimer {
public:
    /**
     * @brief Construct a performance timer
     *
     * @param collector Metrics collector to record to
     * @param name Timer name for metrics
     * @param tags Optional tags to associate with the timer
     * @param auto_record Whether to automatically record on destruction
     */
    PerformanceTimer(std::shared_ptr<MetricsCollector> collector,
                    const std::string& name,
                    const std::unordered_map<std::string, std::string>& tags = {},
                    bool auto_record = true);

    ~PerformanceTimer();

    // Timer control
    void start();
    void stop();
    void pause();
    void resume();

    // Get elapsed time
    std::chrono::nanoseconds elapsed() const;
    double elapsed_ms() const;

    // Manual recording
    void record(const std::string& metric_name = "") const;

    // Add additional tags
    void add_tag(const std::string& key, const std::string& value);
    void add_tags(const std::unordered_map<std::string, std::string>& tags);

private:
    std::shared_ptr<MetricsCollector> collector_;
    std::string name_;
    std::unordered_map<std::string, std::string> tags_;
    bool auto_record_;

    std::chrono::high_resolution_clock::time_point start_time_;
    std::chrono::high_resolution_clock::time_point end_time_;
    std::chrono::nanoseconds paused_duration_{0};
    std::chrono::high_resolution_clock::time_point pause_start_;
    bool running_{false};
    bool paused_{false};
};

/**
 * @brief Plugin performance tracking and analytics
 *
 * Tracks performance metrics for prettifier plugins including:
 * - Processing times and throughput
 * - Success rates and error analysis
 * - Resource usage patterns
 * - Comparative performance analysis
 */
class PluginPerformanceTracker {
public:
    /**
     * @brief Performance snapshot for a plugin
     */
    struct PerformanceSnapshot {
        std::string plugin_name;
        std::chrono::system_clock::time_point timestamp;

        // Timing metrics
        double avg_processing_time_ms = 0.0;
        double p95_processing_time_ms = 0.0;
        double p99_processing_time_ms = 0.0;
        double min_processing_time_ms = 0.0;
        double max_processing_time_ms = 0.0;

        // Throughput metrics
        double requests_per_second = 0.0;
        double bytes_processed_per_second = 0.0;
        size_t total_requests = 0;

        // Quality metrics
        double success_rate = 0.0;
        double error_rate = 0.0;
        std::unordered_map<std::string, double> error_type_rates;

        // Resource metrics
        double avg_input_size_bytes = 0.0;
        double avg_output_size_bytes = 0.0;
        double compression_ratio = 0.0;

        nlohmann::json to_json() const;
    };

    /**
     * @brief Performance comparison between plugins
     */
    struct PerformanceComparison {
        std::string reference_plugin;
        std::variant<std::string, std::vector<std::string>> comparison_plugins;
        std::chrono::system_clock::time_point comparison_start;
        std::chrono::system_clock::time_point comparison_end;

        // Speed comparison
        double speed_improvement_percent = 0.0;
        bool faster = false;
        double statistical_significance = 0.0;

        // Quality comparison
        double success_rate_improvement_percent = 0.0;
        bool more_reliable = false;

        // Efficiency comparison
        double resource_efficiency_percent = 0.0;
        bool more_efficient = false;

        nlohmann::json to_json() const;
    };

    explicit PluginPerformanceTracker(std::shared_ptr<MetricsCollector> collector);

    // Performance tracking
    void record_plugin_execution(const std::string& plugin_name,
                               const PrettificationEvent& event);

    void record_plugin_start(const std::string& plugin_name,
                           const std::string& provider,
                           const std::string& input_format);

    void record_plugin_completion(const std::string& plugin_name,
                                bool success,
                                double processing_time_ms,
                                size_t input_size,
                                size_t output_size,
                                const std::string& error_type = "");

    // Analytics and reporting
    PerformanceSnapshot get_performance_snapshot(
        const std::string& plugin_name,
        const std::chrono::system_clock::time_point& start,
        const std::chrono::system_clock::time_point& end) const;

    std::vector<PerformanceSnapshot> get_all_plugin_snapshots(
        const std::chrono::system_clock::time_point& start,
        const std::chrono::system_clock::time_point& end) const;

    PerformanceComparison compare_plugins(
        const std::string& reference_plugin,
        const std::vector<std::string>& comparison_plugins,
        const std::chrono::system_clock::time_point& start,
        const std::chrono::system_clock::time_point& end) const;

    // Performance optimization suggestions
    struct OptimizationSuggestion {
        enum Type { PERFORMANCE, RELIABILITY, EFFICIENCY };
        Type type;
        std::string plugin_name;
        std::string description;
        double potential_improvement_percent;
        std::string recommendation;
        int priority; // 1-10, higher is more urgent
    };

    std::vector<OptimizationSuggestion> analyze_for_optimizations(
        const std::string& plugin_name) const;

    // Real-time monitoring
    struct RealTimeAlert {
        enum Severity { INFO, WARNING, ERROR, CRITICAL };
        Severity severity;
        std::string plugin_name;
        std::string metric_name;
        std::string message;
        double current_value;
        double threshold_value;
        std::chrono::system_clock::time_point timestamp;

        nlohmann::json to_json() const;
    };

    struct AlertConfig {
        double max_processing_time_ms = 1000.0;
        double min_success_rate = 0.95;
        double max_error_rate = 0.05;
        double min_throughput_rps = 10.0;
        size_t alert_window_size = 100;
        std::chrono::seconds alert_cooldown{60};
    };

    void set_alert_config(const AlertConfig& config);
    AlertConfig get_alert_config() const { return alert_config_; }

    std::vector<RealTimeAlert> check_for_alerts() const;
    void clear_alerts();

    // Configuration
    void update_tracking_config(const nlohmann::json& config);
    nlohmann::json get_tracking_config() const;

private:
    std::shared_ptr<MetricsCollector> collector_;
    AlertConfig alert_config_;

    // Active tracking sessions
    mutable std::mutex sessions_mutex_;
    std::unordered_map<std::string, std::chrono::system_clock::time_point> active_sessions_;

    // Alert state management
    mutable std::mutex alerts_mutex_;
    std::vector<RealTimeAlert> recent_alerts_;
    std::unordered_map<std::string, std::chrono::system_clock::time_point> last_alert_times_;

    // Utility methods
    double calculate_statistical_significance(
        const std::vector<double>& sample1,
        const std::vector<double>& sample2) const;

    std::vector<RealTimeAlert> check_performance_alerts(
        const std::string& plugin_name,
        const PerformanceSnapshot& snapshot) const;
};

/**
 * @brief System-wide performance monitoring coordinator
 *
 * Coordinates performance monitoring across all components:
 * - Metrics collection and storage
 * - Real-time alerting
 * - Performance optimization
 * - Capacity planning
 */
class SystemPerformanceMonitor {
public:
    /**
     * @brief System performance overview
     */
    struct SystemOverview {
        std::chrono::system_clock::time_point timestamp;

        // Request metrics
        double total_requests_per_second = 0.0;
        double successful_requests_per_second = 0.0;
        double failed_requests_per_second = 0.0;

        // Timing metrics
        double avg_response_time_ms = 0.0;
        double p95_response_time_ms = 0.0;
        double p99_response_time_ms = 0.0;

        // Plugin metrics
        size_t active_plugin_count = 0;
        std::vector<PluginPerformanceTracker::PerformanceSnapshot> plugin_snapshots;

        // Resource metrics
        double cpu_usage_percent = 0.0;
        double memory_usage_mb = 0.0;
        double disk_io_rate_mb_per_sec = 0.0;
        double network_io_rate_mb_per_sec = 0.0;

        // Quality metrics
        double overall_success_rate = 0.0;
        std::vector<PluginPerformanceTracker::RealTimeAlert> active_alerts;

        nlohmann::json to_json() const;
    };

    /**
     * @brief Capacity planning metrics
     */
    struct CapacityMetrics {
        std::chrono::system_clock::time_point timestamp;

        // Current load
        double current_load_percent = 0.0;
        double peak_load_percent = 0.0;
        double avg_load_percent = 0.0;

        // Trend analysis
        double load_growth_rate_percent = 0.0;
        double predicted_peak_load_percent = 0.0;
        std::chrono::system_clock::time_point predicted_capacity_exhaustion;

        // Resource utilization
        std::unordered_map<std::string, double> resource_utilization;

        // Scaling recommendations
        bool scaling_recommended = false;
        std::string scaling_recommendation;
        std::chrono::days scaling_timeline{0};

        nlohmann::json to_json() const;
    };

    explicit SystemPerformanceMonitor(std::shared_ptr<MetricsCollector> collector,
                                     std::shared_ptr<TimeSeriesDB> tsdb);

    // System monitoring
    SystemOverview get_system_overview() const;
    CapacityMetrics get_capacity_metrics() const;

    // Performance alerts
    void register_alert_callback(std::function<void(const PluginPerformanceTracker::RealTimeAlert&)> callback);
    void unregister_alert_callback();

    // Historical analysis
    std::vector<SystemOverview> get_historical_overview(
        const std::chrono::system_clock::time_point& start,
        const std::chrono::system_clock::time_point& end,
        const std::chrono::minutes& interval = std::chrono::minutes{5}) const;

    // Performance optimization
    struct OptimizationReport {
        std::chrono::system_clock::time_point generated_at;
        std::vector<PluginPerformanceTracker::OptimizationSuggestion> suggestions;
        CapacityMetrics capacity_insights;
        double overall_performance_score = 0.0; // 0-100
        std::priority_queue<std::pair<int, std::string>> prioritized_actions;

        nlohmann::json to_json() const;
    };

    OptimizationReport generate_optimization_report() const;

    // Configuration
    struct MonitorConfig {
        bool enable_system_monitoring = true;
        bool enable_capacity_planning = true;
        bool enable_optimization_analysis = true;
        std::chrono::seconds metrics_collection_interval{30};
        std::chrono::minutes capacity_analysis_interval{60};
        std::chrono::hours optimization_report_interval{24};
        size_t max_historical_data_points = 10000;
        nlohmann::json custom_thresholds;
    };

    void update_config(const MonitorConfig& config);
    MonitorConfig get_config() const { return config_; }

    // Lifecycle management
    void start_monitoring();
    void stop_monitoring();

private:
    std::shared_ptr<MetricsCollector> collector_;
    std::shared_ptr<TimeSeriesDB> tsdb_;
    MonitorConfig config_;

    std::unique_ptr<PluginPerformanceTracker> plugin_tracker_;

    // Alert callbacks
    std::function<void(const PluginPerformanceTracker::RealTimeAlert&)> alert_callback_;

    // Monitoring thread
    std::thread monitoring_thread_;
    std::atomic<bool> monitoring_active_{false};

    // Historical data cache
    mutable std::mutex cache_mutex_;
    std::vector<SystemOverview> historical_overviews_;

    // Monitoring methods
    void run_monitoring_loop();
    SystemOverview collect_current_metrics() const;
    CapacityMetrics analyze_capacity() const;

    // Performance analysis
    double calculate_performance_score(const SystemOverview& overview) const;
    std::string generate_scaling_recommendation(const CapacityMetrics& metrics) const;

    // State management
    mutable std::mutex state_mutex_;
    std::chrono::system_clock::time_point last_capacity_analysis_;
    std::chrono::system_clock::time_point last_optimization_report_;
};

// Utility macros for easy timing
#define AIMUX_PERFORMANCE_TIMER(collector, name, ...) \
    auto _timer_ ## __LINE__ = ::aimux::metrics::PerformanceTimer( \
        collector, name, ##__VA_ARGS__)

#define AIMUX_TIMER_SCOPE(collector, name) \
    ::aimux::metrics::PerformanceTimer _timer(collector, name)

#define AIMUX_TIMER_SCOPE_TAGS(collector, name, tags) \
    ::aimux::metrics::PerformanceTimer _timer(collector, name, tags)

} // namespace metrics
} // namespace aimux