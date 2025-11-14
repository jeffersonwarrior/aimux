#pragma once

#include "aimux/monitoring/metrics.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <functional>
#include <deque>
#include <nlohmann/json.hpp>

namespace aimux {
namespace monitoring {

/**
 * @brief Enhanced Performance Monitoring System with Granular Metrics
 *
 * This system provides detailed performance monitoring for all aspects of the Aimux system,
 * including per-endpoint metrics, provider-specific performance, memory usage,
 * and detailed request/response tracking.
 *
 * Features:
 * - Per-endpoint response time tracking
 * - Provider-specific performance metrics
 * - Memory usage monitoring with detailed breakdowns
 * - Request/response performance percentiles
 * - Real-time performance aggregation
 * - Performance trend analysis
 * - Alerting on performance degradation
 * - Detailed performance dashboards
 */

// Performance event types
enum class PerformanceEventType {
    REQUEST_START,
    REQUEST_END,
    PROVIDER_START,
    PROVIDER_END,
    CACHE_HIT,
    CACHE_MISS,
    ERROR,
    TIMEOUT,
    RATE_LIMIT_HIT,
    FAILOVER_INITIATED
};

// Detailed performance event
struct PerformanceEvent {
    std::string id;
    PerformanceEventType type;
    std::chrono::high_resolution_clock::time_point timestamp;
    std::chrono::duration<double, std::milli> duration{0};
    std::string component; // endpoint, provider, cache, etc.
    std::string operation; // specific operation being performed
    std::unordered_map<std::string, std::string> metadata;

    /**
     * Constructor for performance event
     *
     * @param event_id Unique identifier for the event
     * @param event_type Type of performance event
     * @param comp Component where the event occurred
     * @param ops Specific operation being performed
     */
    PerformanceEvent(const std::string& event_id, PerformanceEventType event_type,
                    const std::string& comp, const std::string& ops = "")
        : id(event_id), type(event_type), timestamp(std::chrono::high_resolution_clock::now()),
          component(comp), operation(ops) {}

    /**
     * Convert event to JSON format
     *
     * @return JSON representation of the performance event
     */
    nlohmann::json toJson() const {
        nlohmann::json j;
        j["id"] = id;
        j["type"] = static_cast<int>(type);
        j["timestamp"] = std::chrono::duration_cast<std::chrono::microseconds>(
            timestamp.time_since_epoch()).count();
        j["duration_ms"] = duration.count();
        j["component"] = component;
        j["operation"] = operation;
        j["metadata"] = metadata;
        return j;
    }
};

// Performance statistics for analysis
struct PerformanceStats {
    std::string component;
    std::string operation;
    uint64_t total_operations = 0;
    uint64_t successful_operations = 0;
    uint64_t failed_operations = 0;
    double total_duration_ms = 0.0;
    double min_duration_ms = std::numeric_limits<double>::infinity();
    double max_duration_ms = 0.0;
    double p50_duration_ms = 0.0;
    double p95_duration_ms = 0.0;
    double p99_duration_ms = 0.0;
    double errors_per_second = 0.0;
    double operations_per_second = 0.0;
    std::chrono::system_clock::time_point last_update;

    /**
     * Update statistics with new duration
     *
     * @param duration_ms Duration of a new operation
     * @param success Whether the operation was successful
     */
    void update(double duration_ms, bool success = true);

    /**
     * Calculate percentiles from recent durations
     *
     * @param durations Recent duration measurements
     */
    void calculatePercentiles(const std::deque<double>& durations);

    /**
     * Convert statistics to JSON
     *
     * @return JSON representation of performance statistics
     */
    nlohmann::json toJson() const;
};

// Memory usage metrics
struct MemoryMetrics {
    size_t heap_used_mb = 0;
    size_t heap_free_mb = 0;
    size_t stack_used_mb = 0;
    size_t anonymous_mb = 0;
    size_t file_cache_mb = 0;
    size_t shared_mb = 0;
    size_t total_process_mb = 0;
    double memory_pressure_percent = 0.0;
    size_t page_faults = 0;
    size_t major_page_faults = 0;
    std::chrono::system_clock::time_point timestamp;

    /**
     * Collect current memory metrics
     *
     * @return Current memory usage information
     */
    static MemoryMetrics collectCurrent();

    /**
     * Convert memory metrics to JSON
     *
     * @return JSON representation of memory metrics
     */
    nlohmann::json toJson() const;
};

// Provider-specific performance metrics
struct ProviderPerformance {
    std::string provider_name;
    std::unordered_map<std::string, PerformanceStats> model_stats; // per model stats
    uint64_t total_requests = 0;
    uint64_t successful_requests = 0;
    uint64_t failed_requests = 0;
    uint64_t timeout_requests = 0;
    uint64_t rate_limited_requests = 0;
    double average_response_time_ms = 0.0;
    double success_rate_percent = 0.0;
    double cost_per_request = 0.0; // Estimated cost
    std::chrono::system_clock::time_point last_success;
    std::chrono::system_clock::time_point last_failure;
    bool healthy = true;

    /**
     * Update provider performance with new request
     *
     * @param model Model used for the request
     * @param duration_ms Time taken for request
     * @param success Whether request was successful
     * @param cost Estimated cost of request
     */
    void updateRequest(const std::string& model, double duration_ms,
                     bool success, double cost = 0.0);

    /**
     * Get performance for specific model
     *
     * @param model Model name
     * @return Performance statistics for model
     */
    PerformanceStats getModelStats(const std::string& model);

    /**
     * Convert provider performance to JSON
     *
     * @return JSON representation of provider performance
     */
    nlohmann::json toJson() const;
};

// Endpoint-specific performance tracking
struct EndpointPerformance {
    std::string endpoint_path;
    std::string method;
    PerformanceStats stats;
    std::unordered_map<int, uint64_t> status_code_counts;
    std::deque<double> recent_response_times; // Last 1000 response times
    std::unordered_map<std::string, double> parameter_averages; // Average request size, etc.
    std::chrono::system_clock::time_point last_access;

    /**
     * Record an endpoint request
     *
     * @param response_time_ms Response time for the request
     * @param status_code HTTP status code
     * @param request_size Size of request in bytes
     * @param response_size Size of response in bytes
     */
    void recordRequest(double response_time_ms, int status_code,
                      size_t request_size = 0, size_t response_size = 0);

    /**
     * Convert endpoint performance to JSON
     *
     * @return JSON representation of endpoint performance
     */
    nlohmann::json toJson() const;

private:
    static constexpr size_t MAX_RECENT_TIMES = 1000;
};

/**
 * @brief Main Performance Monitor class
 *
 * Coordinates all performance monitoring activities, collects granular metrics,
 * and provides performance analysis capabilities.
 */
class PerformanceMonitor {
public:
    /**
     * @brief Get singleton instance
     *
     * @return Reference to the PerformanceMonitor singleton
     */
    static PerformanceMonitor& getInstance();

    /**
     * @brief Start performance monitoring
     *
     * Initializes monitoring threads and begins collecting metrics.
     *
     * @param collection_interval Interval for metric collection
     * @param retention_hours Hours to retain detailed performance data
     */
    void startMonitoring(std::chrono::seconds collection_interval = std::chrono::seconds(5),
                        std::chrono::hours retention_hours = std::chrono::hours(24));

    /**
     * @brief Stop performance monitoring
     *
     * Gracefully stops all monitoring threads and cleans up resources.
     */
    void stopMonitoring();

    /**
     * @brief Record a performance event
     *
     * @param event Performance event to record
     */
    void recordEvent(const PerformanceEvent& event);

    /**
     * @brief Record the start of a request
     *
     * @param request_id Unique request identifier
     * @param endpoint Endpoint path
     * @param method HTTP method
     * @param provider_name Provider being used (if known)
     * @param model Model being requested (if known)
     * @return Event ID for matching with end event
     */
    std::string recordRequestStart(const std::string& request_id, const std::string& endpoint,
                                  const std::string& method, const std::string& provider_name = "",
                                  const std::string& model = "");

    /**
     * @brief Record the end of a request
     *
     * @param event_id Event ID from request start
     * @param success Whether request was successful
     * @param status_code HTTP status code
     * @param response_size Size of response in bytes
     * @param error_message Error message if request failed
     * @param cost Estimated cost of request
     */
    void recordRequestEnd(const std::string& event_id, bool success, int status_code,
                         size_t response_size = 0, const std::string& error_message = "",
                         double cost = 0.0);

    /**
     * @brief Record provider-specific request
     *
     * @param provider_name Name of the provider
     * @param model Model being requested
     * @param duration_ms Time taken for provider request
     * @param success Whether request was successful
     * @param error_type Type of error if failed
     * @param cost Estimated cost of request
     */
    void recordProviderRequest(const std::string& provider_name, const std::string& model,
                              double duration_ms, bool success, const std::string& error_type = "",
                              double cost = 0.0);

    /**
     * @brief Get current memory metrics
     *
     * @return Current memory usage information
     */
    MemoryMetrics getCurrentMemoryMetrics() const;

    /**
     * @brief Get provider performance metrics
     *
     * @param provider_name Name of provider (empty for all)
     * @return Provider performance metrics
     */
    std::unordered_map<std::string, ProviderPerformance> getProviderPerformance(
        const std::string& provider_name = "") const;

    /**
     * @brief Get endpoint performance metrics
     *
     * @param endpoint_path Endpoint path (empty for all)
     * @return Endpoint performance metrics
     */
    std::unordered_map<std::string, EndpointPerformance> getEndpointPerformance(
        const std::string& endpoint_path = "") const;

    /**
     * @brief Get comprehensive performance report
     *
     * Includes system metrics, provider performance, endpoint performance,
     * memory usage, and performance trends.
     *
     * @return Detailed performance report as JSON
     */
    nlohmann::json getPerformanceReport() const;

    /**
     * @brief Get performance summary for dashboard
     *
     * Tailored for dashboard display with key performance indicators.
     *
     * @return Performance summary as JSON
     */
    nlohmann::json getPerformanceSummary() const;

    /**
     * @brief Export performance data for external systems
     *
     * @param format Export format ("json", "prometheus", "csv")
     * @param since_time Export data since this time
     * @return Exported performance data
     */
    std::string exportPerformanceData(const std::string& format = "json",
                                   std::chrono::system_clock::time_point since_time = {}) const;

    /**
     * @brief Set performance alert thresholds
     *
     * @param metric_name Name of metric to monitor
     * @param threshold_value Alert threshold value
     * @param comparison Comparison type ("<", ">", "=")
     * @param severity Alert severity
     */
    void setAlertThreshold(const std::string& metric_name, double threshold_value,
                          const std::string& comparison = ">",
                          const std::string& severity = "warning");

    /**
     * @brief Check for performance alerts
     *
     * @return List of current performance alerts
     */
    std::vector<std::string> checkPerformanceAlerts() const;

private:
    PerformanceMonitor() = default;
    ~PerformanceMonitor() = default;
    PerformanceMonitor(const PerformanceMonitor&) = delete;
    PerformanceMonitor& operator=(const PerformanceMonitor&) = delete;

    void monitoringLoop();
    void cleanupOldData();
    void aggregateMetrics();
    void calculatePerformanceTrends();

    // Monitoring configuration
    std::atomic<bool> running_{false};
    std::chrono::seconds collection_interval_{5};
    std::chrono::hours retention_hours_{24};
    std::thread monitoring_thread_;

    // Data storage
    std::deque<PerformanceEvent> recent_events_;
    std::unordered_map<std::string, ProviderPerformance> provider_performance_;
    std::unordered_map<std::string, EndpointPerformance> endpoint_performance_;
    std::deque<MemoryMetrics> memory_history_;
    std::unordered_map<std::string, PerformanceStats> component_stats_;

    // Thread safety
    mutable std::mutex events_mutex_;
    mutable std::mutex provider_mutex_;
    mutable std::mutex endpoint_mutex_;
    mutable std::mutex memory_mutex_;
    mutable std::mutex stats_mutex_;

    // Alert thresholds
    struct AlertThreshold {
        std::string metric;
        double threshold;
        std::string comparison;
        std::string severity;
        std::chrono::system_clock::time_point last_triggered;
    };
    std::vector<AlertThreshold> alert_thresholds_;
    mutable std::mutex alerts_mutex_;
};

/**
 * @brief RAII Performance Tracking Helper
 *
 * Automatically tracks the duration of a block and records it.
 */
class ScopedPerformanceTracker {
public:
    /**
     * @brief Constructor for scoped tracking
     *
     * @param component Component being tracked
     * @param operation Specific operation being performed
     * @param metadata Additional metadata to track
     */
    ScopedPerformanceTracker(const std::string& component, const std::string& operation = "",
                            const std::unordered_map<std::string, std::string>& metadata = {})
        : component_(component), operation_(operation), metadata_(metadata) {
        start_time_ = std::chrono::high_resolution_clock::now();
        event_id_ = std::to_string(std::chrono::duration_cast<std::chrono::microseconds>(
            start_time_.time_since_epoch()).count());

        PerformanceEvent start_event(event_id_, PerformanceEventType::REQUEST_START,
                                  component_, operation_);
        start_event.metadata = metadata_;
        PerformanceMonitor::getInstance().recordEvent(start_event);
    }

    /**
     * @brief Destructor - records completion with duration
     */
    ~ScopedPerformanceTracker() {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration<double, std::milli>(end_time - start_time_);

        PerformanceEvent end_event(event_id_, PerformanceEventType::REQUEST_END,
                                 component_, operation_);
        end_event.duration = duration;
        end_event.metadata = metadata_;
        PerformanceMonitor::getInstance().recordEvent(end_event);
    }

    /**
     * @brief Add metadata to the tracking session
     *
     * @param key Metadata key
     * @param value Metadata value
     */
    void addMetadata(const std::string& key, const std::string& value) {
        metadata_[key] = value;
    }

private:
    std::string component_;
    std::string operation_;
    std::string event_id_;
    std::unordered_map<std::string, std::string> metadata_;
    std::chrono::high_resolution_clock::time_point start_time_;
};

// Convenience macros for performance tracking
#define AIMUX_TRACK_COMPONENT(component) \
    ScopedPerformanceTracker _tracker(component)

#define AIMUX_TRACK_OPERATION(component, operation) \
    ScopedPerformanceTracker _tracker(component, operation)

#define AIMUX_TRACK_OPERATION_WITH_METADATA(component, operation, metadata) \
    ScopedPerformanceTracker _tracker(component, operation, metadata)

} // namespace monitoring
} // namespace aimux