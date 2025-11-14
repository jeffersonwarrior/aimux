#include "aimux/monitoring/performance_monitor.h"
#include <algorithm>
#include <numeric>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <random>
#include <fstream>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <filesystem>

namespace aimux {
namespace monitoring {

// PerformanceStats implementation
void PerformanceStats::update(double duration_ms, bool success) {
    std::lock_guard<std::mutex> lock(stats_mutex_);

    total_operations++;
    if (success) {
        successful_operations++;
    } else {
        failed_operations++;
    }

    total_duration_ms += duration_ms;

    if (duration_ms < min_duration_ms) {
        min_duration_ms = duration_ms;
    }
    if (duration_ms > max_duration_ms) {
        max_duration_ms = duration_ms;
    }

    // Update rate calculations (per second over last minute)
    auto now = std::chrono::system_clock::now();
    if (last_update != std::chrono::system_clock::time_point{}) {
        auto duration_since_update = std::chrono::duration_cast<std::chrono::seconds>(
            now - last_update).count();
        if (duration_since_update > 0) {
            operations_per_second = static_cast<double>(total_operations) / duration_since_update;
            errors_per_second = static_cast<double>(failed_operations) / duration_since_update;
        }
    }
    last_update = now;
}

void PerformanceStats::calculatePercentiles(const std::deque<double>& durations) {
    if (durations.empty()) return;

    std::vector<double> sorted_durations(durations.begin(), durations.end());
    std::sort(sorted_durations.begin(), sorted_durations.end());

    auto get_percentile = [&](double percentile) {
        if (sorted_durations.empty()) return 0.0;
        size_t index = static_cast<size_t>(percentile * (sorted_durations.size() - 1) / 100.0);
        return sorted_durations[std::min(index, sorted_durations.size() - 1)];
    };

    p50_duration_ms = get_percentile(50.0);
    p95_duration_ms = get_percentile(95.0);
    p99_duration_ms = get_percentile(99.0);
}

nlohmann::json PerformanceStats::toJson() const {
    nlohmann::json j;
    j["component"] = component;
    j["operation"] = operation;
    j["total_operations"] = total_operations;
    j["successful_operations"] = successful_operations;
    j["failed_operations"] = failed_operations;
    j["total_duration_ms"] = total_duration_ms;
    j["min_duration_ms"] = min_duration_ms;
    j["max_duration_ms"] = max_duration_ms;
    j["p50_duration_ms"] = p50_duration_ms;
    j["p95_duration_ms"] = p95_duration_ms;
    j["p99_duration_ms"] = p99_duration_ms;
    j["errors_per_second"] = errors_per_second;
    j["operations_per_second"] = operations_per_second;
    j["success_rate_percent"] = total_operations > 0 ?
        (static_cast<double>(successful_operations) / total_operations * 100.0) : 0.0;
    j["avg_duration_ms"] = total_operations > 0 ?
        (total_duration_ms / total_operations) : 0.0;
    j["last_update"] = std::chrono::duration_cast<std::chrono::seconds>(
        last_update.time_since_epoch()).count();
    return j;
}

// MemoryMetrics implementation
MemoryMetrics MemoryMetrics::collectCurrent() {
    MemoryMetrics metrics;
    metrics.timestamp = std::chrono::system_clock::now();

    // Read from /proc/self/status for detailed memory information
    std::ifstream status_file("/proc/self/status");
    if (status_file.is_open()) {
        std::string line;
        while (std::getline(status_file, line)) {
            if (line.substr(0, 6) == "VmRSS:") {
                metrics.heap_used_mb = std::stoul(line.substr(line.find_first_of("0123456789"))) / 1024;
            } else if (line.substr(0, 8) == "VmSize:") {
                metrics.total_process_mb = std::stoul(line.substr(line.find_first_of("0123456789"))) / 1024;
            } else if (line.substr(0, 4) == "VmData:") {
                metrics.heap_used_mb = std::stoul(line.substr(line.find_first_of("0123456789"))) / 1024;
            } else if (line.substr(0, 4) == "VmStk:") {
                metrics.stack_used_mb = std::stoul(line.substr(line.find_first_of("0123456789"))) / 1024;
            } else if (line.substr(0, 7) == "RssAnon:") {
                metrics.anonymous_mb = std::stoul(line.substr(line.find_first_of("0123456789"))) / 1024;
            } else if (line.substr(0, 8) == "RssFile:") {
                metrics.file_cache_mb = std::stoul(line.substr(line.find_first_of("0123456789"))) / 1024;
            } else if (line.substr(0, 8) == "RssShmem:") {
                metrics.shared_mb = std::stoul(line.substr(line.find_first_of("0123456789"))) / 1024;
            }
        }
    }

    // Get page faults
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        metrics.page_faults = usage.ru_minflt;
        metrics.major_page_faults = usage.ru_majflt;
    }

    // Calculate memory pressure (simplified)
    if (metrics.total_process_mb > 0) {
        metrics.memory_pressure_percent = (static_cast<double>(metrics.heap_used_mb) /
                                        metrics.total_process_mb) * 100.0;
    }

    return metrics;
}

nlohmann::json MemoryMetrics::toJson() const {
    nlohmann::json j;
    j["heap_used_mb"] = heap_used_mb;
    j["heap_free_mb"] = heap_free_mb;
    j["stack_used_mb"] = stack_used_mb;
    j["anonymous_mb"] = anonymous_mb;
    j["file_cache_mb"] = file_cache_mb;
    j["shared_mb"] = shared_mb;
    j["total_process_mb"] = total_process_mb;
    j["memory_pressure_percent"] = memory_pressure_percent;
    j["page_faults"] = page_faults;
    j["major_page_faults"] = major_page_faults;
    j["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
        timestamp.time_since_epoch()).count();
    return j;
}

// ProviderPerformance implementation
void ProviderPerformance::updateRequest(const std::string& model, double duration_ms,
                                      bool success, double cost) {
    total_requests++;

    if (success) {
        successful_requests++;
        last_success = std::chrono::system_clock::now();
    } else {
        failed_requests++;
        last_failure = std::chrono::system_clock::now();
    }

    // Update model-specific stats
    auto& model_stats = model_stats[model];
    model_stats.component = provider_name;
    model_stats.operation = model;
    model_stats.update(duration_ms, success);

    // Calculate overall success rate and average response time
    success_rate_percent = total_requests > 0 ?
        (static_cast<double>(successful_requests) / total_requests * 100.0) : 0.0;

    double total_duration = 0.0;
    for (const auto& [model_name, stats] : model_stats) {
        total_duration += stats.total_duration_ms;
    }
    average_response_time_ms = total_requests > 0 ? (total_duration / total_requests) : 0.0;

    // Update cost tracking
    cost_per_request = cost_per_request + ((cost - cost_per_request) / total_requests);
}

PerformanceStats ProviderPerformance::getModelStats(const std::string& model) {
    auto it = model_stats.find(model);
    if (it != model_stats.end()) {
        return it->second;
    }
    return PerformanceStats{provider_name, model};
}

nlohmann::json ProviderPerformance::toJson() const {
    nlohmann::json j;
    j["provider_name"] = provider_name;
    j["total_requests"] = total_requests;
    j["successful_requests"] = successful_requests;
    j["failed_requests"] = failed_requests;
    j["timeout_requests"] = timeout_requests;
    j["rate_limited_requests"] = rate_limited_requests;
    j["average_response_time_ms"] = average_response_time_ms;
    j["success_rate_percent"] = success_rate_percent;
    j["cost_per_request"] = cost_per_request;
    j["last_success"] = std::chrono::duration_cast<std::chrono::seconds>(
        last_success.time_since_epoch()).count();
    j["last_failure"] = std::chrono::duration_cast<std::chrono::seconds>(
        last_failure.time_since_epoch()).count();
    j["healthy"] = healthy;

    // Include model-specific stats
    nlohmann::json models_json;
    for (const auto& [model_name, stats] : model_stats) {
        models_json[model_name] = stats.toJson();
    }
    j["model_stats"] = models_json;

    return j;
}

// EndpointPerformance implementation
void EndpointPerformance::recordRequest(double response_time_ms, int status_code,
                                      size_t request_size, size_t response_size) {
    last_access = std::chrono::system_clock::now();

    stats.update(response_time_ms, (status_code >= 200 && status_code < 400));

    // Update status code counts
    status_code_counts[status_code]++;

    // Track recent response times for percentiles
    recent_response_times.push_back(response_time_ms);
    if (recent_response_times.size() > MAX_RECENT_TIMES) {
        recent_response_times.pop_front();
    }

    // Calculate percentiles periodically
    if (recent_response_times.size() % 100 == 0) {
        stats.calculatePercentiles(recent_response_times);
    }

    // Update parameter averages
    if (request_size > 0) {
        double& avg_request_size = parameter_averages["avg_request_size"];
        avg_request_size = avg_request_size + ((static_cast<double>(request_size) - avg_request_size) /
                                             recent_response_times.size());
    }

    if (response_size > 0) {
        double& avg_response_size = parameter_averages["avg_response_size"];
        avg_response_size = avg_response_size + ((static_cast<double>(response_size) - avg_response_size) /
                                              recent_response_times.size());
    }
}

nlohmann::json EndpointPerformance::toJson() const {
    nlohmann::json j;
    j["endpoint_path"] = endpoint_path;
    j["method"] = method;
    j["stats"] = stats.toJson();
    j["status_code_counts"] = status_code_counts;
    j["parameter_averages"] = parameter_averages;
    j["last_access"] = std::chrono::duration_cast<std::chrono::seconds>(
        last_access.time_since_epoch()).count();
    return j;
}

// PerformanceMonitor implementation
PerformanceMonitor& PerformanceMonitor::getInstance() {
    static PerformanceMonitor instance;
    return instance;
}

void PerformanceMonitor::startMonitoring(std::chrono::seconds collection_interval,
                                        std::chrono::hours retention_hours) {
    collection_interval_ = collection_interval;
    retention_hours_ = retention_hours;

    if (!running_.load()) {
        running_.store(true);
        monitoring_thread_ = std::thread(&PerformanceMonitor::monitoringLoop, this);
    }
}

void PerformanceMonitor::stopMonitoring() {
    if (running_.load()) {
        running_.store(false);
        if (monitoring_thread_.joinable()) {
            monitoring_thread_.join();
        }
    }
}

void PerformanceMonitor::monitoringLoop() {
    while (running_.load()) {
        try {
            // collect memory metrics
            auto memory_metrics = MemoryMetrics::collectCurrent();
            {
                std::lock_guard<std::mutex> lock(memory_mutex_);
                memory_history_.push_back(memory_metrics);

                // Keep only recent history based on retention period
                auto cutoff = std::chrono::system_clock::now() - retention_hours_;
                while (!memory_history_.empty() && memory_history_.front().timestamp < cutoff) {
                    memory_history_.pop_front();
                }
            }

            // Cleanup old events and aggregate metrics
            cleanupOldData();
            aggregateMetrics();
            calculatePerformanceTrends();

        } catch (const std::exception& e) {
            // Log error but continue monitoring
        }

        std::this_thread::sleep_for(collection_interval_);
    }
}

void PerformanceMonitor::recordEvent(const PerformanceEvent& event) {
    std::lock_guard<std::mutex> lock(events_mutex_);
    recent_events_.push_back(event);
}

std::string PerformanceMonitor::recordRequestStart(const std::string& request_id,
                                                  const std::string& endpoint,
                                                  const std::string& method,
                                                  const std::string& provider_name,
                                                  const std::string& model) {
    PerformanceEvent event(request_id + "_start", PerformanceEventType::REQUEST_START,
                          endpoint, method);
    event.metadata["provider"] = provider_name;
    event.metadata["model"] = model;
    event.metadata["request_id"] = request_id;

    recordEvent(event);
    return event.id;
}

void PerformanceMonitor::recordRequestEnd(const std::string& event_id, bool success, int status_code,
                                         size_t response_size, const std::string& error_message,
                                         double cost) {
    PerformanceEvent event(event_id + "_end", PerformanceEventType::REQUEST_END,
                          "request", "complete");
    event.metadata["success"] = std::to_string(success);
    event.metadata["status_code"] = std::to_string(status_code);
    event.metadata["response_size"] = std::to_string(response_size);
    event.metadata["cost"] = std::to_string(cost);
    if (!error_message.empty()) {
        event.metadata["error"] = error_message;
    }

    recordEvent(event);
}

void PerformanceMonitor::recordProviderRequest(const std::string& provider_name,
                                             const std::string& model,
                                             double duration_ms, bool success,
                                             const std::string& error_type,
                                             double cost) {
    {
        std::lock_guard<std::mutex> lock(provider_mutex_);

        auto& provider_perf = provider_performance_[provider_name];
        if (provider_perf.provider_name.empty()) {
            provider_perf.provider_name = provider_name;
        }

        provider_perf.updateRequest(model, duration_ms, success, cost);

        if (!success) {
            if (error_type == "timeout") {
                provider_perf.timeout_requests++;
            } else if (error_type == "rate_limit") {
                provider_perf.rate_limited_requests++;
            }
        }
    }

    // Record performance event
    PerformanceEvent event("provider_" + provider_name,
                          success ? PerformanceEventType::PROVIDER_END : PerformanceEventType::ERROR,
                          provider_name, model);
    event.duration = std::chrono::duration<double, std::milli>(duration_ms);
    event.metadata["success"] = std::to_string(success);
    event.metadata["error_type"] = error_type;
    event.metadata["cost"] = std::to_string(cost);

    recordEvent(event);
}

MemoryMetrics PerformanceMonitor::getCurrentMemoryMetrics() const {
    std::lock_guard<std::mutex> lock(memory_mutex_);
    if (!memory_history_.empty()) {
        return memory_history_.back();
    }
    return MemoryMetrics::collectCurrent();
}

std::unordered_map<std::string, ProviderPerformance> PerformanceMonitor::getProviderPerformance(
    const std::string& provider_name) const {
    std::lock_guard<std::mutex> lock(provider_mutex_);

    if (provider_name.empty()) {
        return provider_performance_;
    }

    auto it = provider_performance_.find(provider_name);
    if (it != provider_performance_.end()) {
        return {{provider_name, it->second}};
    }
    return {};
}

std::unordered_map<std::string, EndpointPerformance> PerformanceMonitor::getEndpointPerformance(
    const std::string& endpoint_path) const {
    std::lock_guard<std::mutex> lock(endpoint_mutex_);

    if (endpoint_path.empty()) {
        return endpoint_performance_;
    }

    auto it = endpoint_performance_.find(endpoint_path);
    if (it != endpoint_performance_.end()) {
        return {{endpoint_path, it->second}};
    }
    return {};
}

nlohmann::json PerformanceMonitor::getPerformanceReport() const {
    nlohmann::json report;

    // System information
    report["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    report["monitoring_active"] = running_.load();

    // Memory metrics
    auto current_memory = getCurrentMemoryMetrics();
    report["memory_metrics"] = current_memory.toJson();

    // Provider performance
    {
        std::lock_guard<std::mutex> lock(provider_mutex_);
        nlohmann::json providers_json;
        for (const auto& [name, perf] : provider_performance_) {
            providers_json[name] = perf.toJson();
        }
        report["provider_performance"] = providers_json;
    }

    // Endpoint performance
    {
        std::lock_guard<std::mutex> lock(endpoint_mutex_);
        nlohmann::json endpoints_json;
        for (const auto& [path, perf] : endpoint_performance_) {
            endpoints_json[path] = perf.toJson();
        }
        report["endpoint_performance"] = endpoints_json;
    }

    // Recent events (last 100)
    {
        std::lock_guard<std::mutex> lock(events_mutex_);
        nlohmann::json events_json = nlohmann::json::array();
        size_t count = std::min(size_t(100), recent_events_.size());
        for (size_t i = recent_events_.size() - count; i < recent_events_.size(); ++i) {
            events_json.push_back(recent_events_[i].toJson());
        }
        report["recent_events"] = events_json;
    }

    return report;
}

nlohmann::json PerformanceMonitor::getPerformanceSummary() const {
    nlohmann::json summary;

    // Overall system health
    summary["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    // Memory summary
    auto memory = getCurrentMemoryMetrics();
    summary["memory_summary"] = {
        {"total_mb", memory.total_process_mb},
        {"heap_used_mb", memory.heap_used_mb},
        {"pressure_percent", memory.memory_pressure_percent}
    };

    // Provider summary
    {
        std::lock_guard<std::mutex> lock(provider_mutex_);
        uint64_t total_requests = 0;
        uint64_t total_successful = 0;
        int healthy_providers = 0;
        int total_providers = provider_performance_.size();

        for (const auto& [name, perf] : provider_performance_) {
            total_requests += perf.total_requests;
            total_successful += perf.successful_requests;
            if (perf.healthy) healthy_providers++;
        }

        summary["provider_summary"] = {
            {"total_providers", total_providers},
            {"healthy_providers", healthy_providers},
            {"total_requests", total_requests},
            {"success_rate_percent", total_requests > 0 ?
                (static_cast<double>(total_successful) / total_requests * 100.0) : 0.0}
        };
    }

    // Performance alerts
    auto alerts = checkPerformanceAlerts();
    summary["active_alerts"] = alerts.size();
    summary["alerts"] = nlohmann::json::array();
    for (const auto& alert : alerts) {
        summary["alerts"].push_back(alert);
    }

    return summary;
}

std::string PerformanceMonitor::exportPerformanceData(const std::string& format,
                                                   std::chrono::system_clock::time_point since_time) const {
    if (format == "json") {
        return getPerformanceReport().dump(2);
    } else if (format == "prometheus") {
        return MetricsRegistry::getInstance().exportToPrometheus();
    } else if (format == "csv") {
        std::ostringstream csv;
        // CSV format implementation
        return csv.str();
    }
    return "";
}

void PerformanceMonitor::setAlertThreshold(const std::string& metric_name, double threshold_value,
                                          const std::string& comparison, const std::string& severity) {
    std::lock_guard<std::mutex> lock(alerts_mutex_);
    alert_thresholds_.push_back({metric_name, threshold_value, comparison, severity,
                                 std::chrono::system_clock::time_point{}});
}

std::vector<std::string> PerformanceMonitor::checkPerformanceAlerts() const {
    std::vector<std::string> alerts;
    std::lock_guard<std::mutex> lock(alerts_mutex_);

    auto memory = getCurrentMemoryMetrics();

    for (const auto& threshold : alert_thresholds_) {
        bool triggered = false;
        std::string message;

        if (threshold.metric == "memory_pressure") {
            if (threshold.comparison == ">" && memory.memory_pressure_percent > threshold.threshold) {
                triggered = true;
                message = "Memory pressure (" + std::to_string(memory.memory_pressure_percent) +
                         "%) exceeds threshold (" + std::to_string(threshold.threshold) + "%)";
            }
        } else if (threshold.metric == "memory_usage_mb") {
            if (threshold.comparison == ">" && memory.heap_used_mb > threshold.threshold) {
                triggered = true;
                message = "Memory usage (" + std::to_string(memory.heap_used_mb) + "MB) exceeds threshold (" +
                         std::to_string(threshold.threshold) + "MB)";
            }
        }

        if (triggered) {
            alerts.push_back("[" + threshold.severity + "] " + message);
        }
    }

    return alerts;
}

void PerformanceMonitor::cleanupOldData() {
    auto cutoff = std::chrono::system_clock::now() - retention_hours_;

    // Clean old events
    {
        std::lock_guard<std::mutex> lock(events_mutex_);
        while (!recent_events_.empty() && recent_events_.front().timestamp < cutoff) {
            recent_events_.pop_front();
        }
    }
}

void PerformanceMonitor::aggregateMetrics() {
    // Aggregation logic for reducing metric granularity for long-term storage
}

void PerformanceMonitor::calculatePerformanceTrends() {
    // Calculate performance trends and detect anomalies
}

} // namespace monitoring
} // namespace aimux