#pragma once

#include <chrono>
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <thread>
#include <optional>
#include <unordered_map>
#include <limits>
#include <functional>
#include <nlohmann/json.hpp>

namespace aimux {
namespace metrics {

/**
 * @brief Metric types supported by the collection system
 */
enum class MetricType {
    COUNTER,        // Cumulative counter
    GAUGE,          // Current value
    HISTOGRAM,      // Distribution of values
    TIMER,          // Duration measurements
    RAW_EVENT       // Custom event data
};

/**
 * @brief Time-series metric data point
 */
struct MetricPoint {
    std::string name;
    MetricType type;
    double value;
    std::chrono::system_clock::time_point timestamp;
    std::unordered_map<std::string, std::string> tags;
    std::unordered_map<std::string, double> fields;

    nlohmann::json to_json() const;
    static MetricPoint from_json(const nlohmann::json& j);
};

/**
 * @brief Aggregated metric statistics
 */
struct MetricStatistics {
    std::string name;
    MetricType type;
    double count = 0.0;
    double sum = 0.0;
    double min = std::numeric_limits<double>::infinity();
    double max = -std::numeric_limits<double>::infinity();
    double mean = 0.0;
    double median = 0.0;
    double p95 = 0.0;    // 95th percentile
    double p99 = 0.0;    // 99th percentile
    double std_dev = 0.0; // Standard deviation

    nlohmann::json to_json() const;
};

/**
 * @brief Prettification-specific event metrics
 */
struct PrettificationEvent {
    std::string plugin_name;
    std::string provider;
    std::string model;
    std::string input_format;
    std::string output_format;
    double processing_time_ms;
    size_t input_size_bytes;
    size_t output_size_bytes;
    bool success;
    std::string error_type;
    size_t tokens_processed;
    std::vector<std::string> capabilities_used;
    std::chrono::system_clock::time_point timestamp;
    std::unordered_map<std::string, std::string> metadata;

    nlohmann::json to_json() const;
    static PrettificationEvent from_json(const nlohmann::json& j);
};

/**
 * @brief Real-time metrics collector interface
 *
 * Provides high-performance metrics collection with minimal overhead.
 * Supports both real-time streaming and batch collection modes.
 * Thread-safe implementation for concurrent access patterns.
 */
class MetricsCollector {
public:
    /**
     * @brief Configuration for metrics collection
     */
    struct Config {
        size_t buffer_size;                    // Buffer size for batching
        std::chrono::milliseconds flush_interval; // Flush interval
        std::chrono::hours retention_period;   // Data retention (7 days)
        bool enable_real_time;                  // Enable real-time collection
        bool enable_compression;                // Compress stored metrics
        double sampling_rate;                    // Sampling rate (0.0-1.0)
        std::string storage_backend;      // Storage backend type
        nlohmann::json backend_config;                 // Backend-specific config

        Config() : buffer_size(10000),
                  flush_interval(std::chrono::milliseconds(100)),
                  retention_period(std::chrono::hours(24 * 7)),
                  enable_real_time(true),
                  enable_compression(true),
                  sampling_rate(1.0),
                  storage_backend("influxdb") {}
    };

    explicit MetricsCollector(const Config& config = Config());
    virtual ~MetricsCollector();

    // Core collection methods
    void record_counter(const std::string& name, double value = 1.0,
                       const std::unordered_map<std::string, std::string>& tags = {});
    void record_gauge(const std::string& name, double value,
                     const std::unordered_map<std::string, std::string>& tags = {});
    void record_histogram(const std::string& name, double value,
                         const std::unordered_map<std::string, std::string>& tags = {});
    void record_timer(const std::string& name, std::chrono::nanoseconds duration,
                     const std::unordered_map<std::string, std::string>& tags = {});
    void record_event(const MetricPoint& event);
    void record_prettification_event(const PrettificationEvent& event);

    // Batch operations
    void record_batch(const std::vector<MetricPoint>& metrics);
    void record_prettification_batch(const std::vector<PrettificationEvent>& events);

    // Query operations
    std::vector<MetricPoint> query_metrics(
        const std::string& name,
        const std::chrono::system_clock::time_point& start,
        const std::chrono::system_clock::time_point& end,
        const std::unordered_map<std::string, std::string>& tags = {}) const;

    MetricStatistics get_statistics(
        const std::string& name,
        const std::chrono::system_clock::time_point& start,
        const std::chrono::system_clock::time_point& end) const;

    // Real-time aggregation
    std::vector<MetricStatistics> get_real_time_stats(const std::vector<std::string>& metric_names) const;

    // Plugin-specific analytics
    struct PluginAnalytics {
        std::string plugin_name;
        std::chrono::system_clock::time_point window_start;
        std::chrono::system_clock::time_point window_end;
        double total_requests = 0;
        double success_rate = 0;
        double avg_processing_time_ms = 0;
        double p95_processing_time_ms = 0;
        double throughput_rps = 0;
        std::unordered_map<std::string, double> error_rates;
        std::unordered_map<std::string, double> provider_performance;
    };

    PluginAnalytics get_plugin_analytics(
        const std::string& plugin_name,
        const std::chrono::system_clock::time_point& start,
        const std::chrono::system_clock::time_point& end) const;

    // Management operations
    void start_collection();
    void stop_collection();
    void flush();
    void clear_old_data();

    // Configuration and status
    void update_config(const Config& config);
    Config get_config() const { return config_; }
    nlohmann::json get_status() const;

    // Callbacks for real-time processing
    using MetricCallback = std::function<void(const MetricPoint&)>;
    using EventCallback = std::function<void(const PrettificationEvent&)>;

    void set_metric_callback(MetricCallback callback) { metric_callback_ = callback; }
    void set_event_callback(EventCallback callback) { event_callback_ = callback; }

protected:
    // Internal processing
    void process_batch();
    virtual void store_metrics(const std::vector<MetricPoint>& metrics) = 0;
    virtual void store_events(const std::vector<PrettificationEvent>& events) = 0;

private:
    Config config_;
    std::atomic<bool> collecting_{false};

    // Thread-safe buffers
    mutable std::mutex metrics_mutex_;
    mutable std::mutex events_mutex_;
    std::queue<MetricPoint> metrics_buffer_;
    std::queue<PrettificationEvent> events_buffer_;

    // Processing thread
    std::thread processor_thread_;
    std::condition_variable processor_cv_;
    std::atomic<bool> should_stop_{false};

    // Callbacks
    MetricCallback metric_callback_;
    EventCallback event_callback_;

    // In-memory aggregation for real-time stats
    mutable std::mutex aggregation_mutex_;
    std::unordered_map<std::string, std::vector<double>> recent_values_;
    std::unordered_map<std::string, std::chrono::system_clock::time_point> last_update_;

    // Utility methods
    MetricPoint create_metric_point(
        const std::string& name,
        MetricType type,
        double value,
        const std::unordered_map<std::string, std::string>& tags = {}) const;

    void update_real_time_aggregation(const MetricPoint& point);
    std::vector<double> calculate_percentiles(const std::vector<double>& values) const;
};

/**
 * @brief In-memory metrics collector for testing and development
 */
class InMemoryMetricsCollector : public MetricsCollector {
public:
    explicit InMemoryMetricsCollector(const Config& config = Config{});

    // Access stored data for testing
    const std::vector<MetricPoint>& get_stored_metrics() const { return stored_metrics_; }
    const std::vector<PrettificationEvent>& get_stored_events() const { return stored_events_; }
    void clear_stored_data();

protected:
    void store_metrics(const std::vector<MetricPoint>& metrics) override;
    void store_events(const std::vector<PrettificationEvent>& events) override;

private:
    std::vector<MetricPoint> stored_metrics_;
    std::vector<PrettificationEvent> stored_events_;
    mutable std::mutex storage_mutex_;
};

} // namespace metrics
} // namespace aimux