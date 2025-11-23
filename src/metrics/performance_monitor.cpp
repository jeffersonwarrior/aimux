#include "aimux/metrics/performance_monitor.hpp"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <sstream>
#include <random>
#include <queue>
#include <future>

namespace aimux {
namespace metrics {

// PerformanceTimer implementation
PerformanceTimer::PerformanceTimer(std::shared_ptr<MetricsCollector> collector,
                                  const std::string& name,
                                  const std::unordered_map<std::string, std::string>& tags,
                                  bool auto_record)
    : collector_(collector), name_(name), tags_(tags), auto_record_(auto_record) {
    start();
}

PerformanceTimer::~PerformanceTimer() {
    if (auto_record_ && running_) {
        stop();
        record();
    }
}

void PerformanceTimer::start() {
    if (!running_) {
        start_time_ = std::chrono::high_resolution_clock::now();
        running_ = true;
        paused_ = false;
        paused_duration_ = std::chrono::nanoseconds{0};
    }
}

void PerformanceTimer::stop() {
    if (running_) {
        if (paused_) {
            resume();
        }
        end_time_ = std::chrono::high_resolution_clock::now();
        running_ = false;
    }
}

void PerformanceTimer::pause() {
    if (running_ && !paused_) {
        pause_start_ = std::chrono::high_resolution_clock::now();
        paused_ = true;
    }
}

void PerformanceTimer::resume() {
    if (running_ && paused_) {
        auto pause_end = std::chrono::high_resolution_clock::now();
        paused_duration_ += (pause_end - pause_start_);
        paused_ = false;
    }
}

std::chrono::nanoseconds PerformanceTimer::elapsed() const {
    if (!running_) {
        return end_time_ - start_time_ - paused_duration_;
    } else {
        auto now = std::chrono::high_resolution_clock::now();
        return now - start_time_ - paused_duration_;
    }
}

double PerformanceTimer::elapsed_ms() const {
    return std::chrono::duration<double, std::milli>(elapsed()).count();
}

void PerformanceTimer::record(const std::string& metric_name) const {
    if (collector_) {
        std::string timer_name = metric_name.empty() ? name_ : metric_name;
        collector_->record_timer(timer_name, elapsed(), tags_);
    }
}

void PerformanceTimer::add_tag(const std::string& key, const std::string& value) {
    tags_[key] = value;
}

void PerformanceTimer::add_tags(const std::unordered_map<std::string, std::string>& tags) {
    for (const auto& [key, value] : tags) {
        tags_[key] = value;
    }
}

// PerformanceSnapshot implementation
nlohmann::json PluginPerformanceTracker::PerformanceSnapshot::to_json() const {
    nlohmann::json j;
    j["plugin_name"] = plugin_name;
    j["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        timestamp.time_since_epoch()).count();

    j["avg_processing_time_ms"] = avg_processing_time_ms;
    j["p95_processing_time_ms"] = p95_processing_time_ms;
    j["p99_processing_time_ms"] = p99_processing_time_ms;
    j["min_processing_time_ms"] = min_processing_time_ms;
    j["max_processing_time_ms"] = max_processing_time_ms;

    j["requests_per_second"] = requests_per_second;
    j["bytes_processed_per_second"] = bytes_processed_per_second;
    j["total_requests"] = total_requests;

    j["success_rate"] = success_rate;
    j["error_rate"] = error_rate;
    j["error_type_rates"] = error_type_rates;

    j["avg_input_size_bytes"] = avg_input_size_bytes;
    j["avg_output_size_bytes"] = avg_output_size_bytes;
    j["compression_ratio"] = compression_ratio;

    return j;
}

// PerformanceComparison implementation
nlohmann::json PluginPerformanceTracker::PerformanceComparison::to_json() const {
    nlohmann::json j;
    j["reference_plugin"] = reference_plugin;
    j["comparison_start"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        comparison_start.time_since_epoch()).count();
    j["comparison_end"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        comparison_end.time_since_epoch()).count();

    j["speed_improvement_percent"] = speed_improvement_percent;
    j["faster"] = faster;
    j["statistical_significance"] = statistical_significance;

    j["success_rate_improvement_percent"] = success_rate_improvement_percent;
    j["more_reliable"] = more_reliable;

    j["resource_efficiency_percent"] = resource_efficiency_percent;
    j["more_efficient"] = more_efficient;

    return j;
}

// RealTimeAlert implementation
nlohmann::json PluginPerformanceTracker::RealTimeAlert::to_json() const {
    nlohmann::json j;
    j["severity"] = static_cast<int>(severity);
    j["plugin_name"] = plugin_name;
    j["metric_name"] = metric_name;
    j["message"] = message;
    j["current_value"] = current_value;
    j["threshold_value"] = threshold_value;
    j["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        timestamp.time_since_epoch()).count();
    return j;
}

// PluginPerformanceTracker implementation
PluginPerformanceTracker::PluginPerformanceTracker(std::shared_ptr<MetricsCollector> collector)
    : collector_(collector) {}

void PluginPerformanceTracker::record_plugin_execution(const std::string& plugin_name,
                                                       const PrettificationEvent& event) {
    record_plugin_start(plugin_name, event.provider, event.input_format);
    record_plugin_completion(plugin_name, event.success, event.processing_time_ms,
                           event.input_size_bytes, event.output_size_bytes, event.error_type);
}

void PluginPerformanceTracker::record_plugin_start(const std::string& plugin_name,
                                                 const std::string& provider,
                                                 const std::string& input_format) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    active_sessions_[plugin_name + "_" + provider + "_" + input_format] =
        std::chrono::system_clock::now();
}

void PluginPerformanceTracker::record_plugin_completion(const std::string& plugin_name,
                                                      bool success,
                                                      double processing_time_ms,
                                                      size_t input_size,
                                                      size_t output_size,
                                                      const std::string& error_type) {
    if (collector_) {
        collector_->record_counter("plugin_executions_total", 1.0,
                                 {
                                     {"plugin", plugin_name},
                                     {"success", success ? "true" : "false"}
                                 });

        collector_->record_histogram("plugin_processing_time_ms", processing_time_ms,
                                   {{"plugin", plugin_name}});

        collector_->record_counter("plugin_throughput_bytes", static_cast<double>(input_size),
                                 {{"plugin", plugin_name}, {"direction", "input"}});

        collector_->record_counter("plugin_throughput_bytes", static_cast<double>(output_size),
                                 {{"plugin", plugin_name}, {"direction", "output"}});

        if (!success && !error_type.empty()) {
            collector_->record_counter("plugin_errors_total", 1.0,
                                     {
                                         {"plugin", plugin_name},
                                         {"error_type", error_type}
                                     });
        }
    }
}

PluginPerformanceTracker::PerformanceSnapshot
PluginPerformanceTracker::get_performance_snapshot(
    const std::string& plugin_name,
    const std::chrono::system_clock::time_point& start,
    const std::chrono::system_clock::time_point& end) const {

    PerformanceSnapshot snapshot;
    snapshot.plugin_name = plugin_name;
    snapshot.timestamp = std::chrono::system_clock::now();

    // This would query the metrics collector's backend
    // For now, provide a basic implementation

    // Query timing data
    auto timing_stats = collector_->get_statistics("plugin_processing_time_ms", start, end);
    if (timing_stats.count > 0) {
        snapshot.avg_processing_time_ms = timing_stats.mean;
        snapshot.p95_processing_time_ms = timing_stats.p95;
        snapshot.p99_processing_time_ms = timing_stats.p99;
        snapshot.min_processing_time_ms = timing_stats.min;
        snapshot.max_processing_time_ms = timing_stats.max;
    }

    // Query execution counts
    auto total_requests = collector_->query_metrics("plugin_executions_total", start, end,
                                                   {{"plugin", plugin_name}});
    if (!total_requests.empty()) {
        double total = 0.0;
        double successful = 0.0;
        for (const auto& request : total_requests) {
            if (request.tags.at("success") == "true") {
                successful += request.value;
            }
            total += request.value;
        }

        snapshot.total_requests = static_cast<size_t>(total);
        snapshot.success_rate = total > 0 ? successful / total : 0.0;
        snapshot.error_rate = 1.0 - snapshot.success_rate;
    }

    // Calculate throughput
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    if (duration_ms > 0) {
        snapshot.requests_per_second = (static_cast<double>(snapshot.total_requests) * 1000.0) / duration_ms;
    }

    return snapshot;
}

std::vector<PluginPerformanceTracker::PerformanceSnapshot>
PluginPerformanceTracker::get_all_plugin_snapshots(
    const std::chrono::system_clock::time_point& /*start*/,
    const std::chrono::system_clock::time_point& /*end*/) const {
    // This would discover all plugins and get snapshots for each
    // For now, return empty results
    return {};
}

PluginPerformanceTracker::PerformanceComparison
PluginPerformanceTracker::compare_plugins(
    const std::string& reference_plugin,
    const std::vector<std::string>& comparison_plugins,
    const std::chrono::system_clock::time_point& start,
    const std::chrono::system_clock::time_point& end) const {

    PerformanceComparison comparison;
    comparison.reference_plugin = reference_plugin;
    comparison.comparison_plugins = comparison_plugins;
    comparison.comparison_start = start;
    comparison.comparison_end = end;

    auto ref_snapshot = get_performance_snapshot(reference_plugin, start, end);

    for (const auto& plugin : comparison_plugins) {
        auto comp_snapshot = get_performance_snapshot(plugin, start, end);

        // Compare processing times
        if (ref_snapshot.avg_processing_time_ms > 0 && comp_snapshot.avg_processing_time_ms > 0) {
            double improvement = ((ref_snapshot.avg_processing_time_ms - comp_snapshot.avg_processing_time_ms) /
                                 ref_snapshot.avg_processing_time_ms) * 100.0;

            if (improvement > comparison.speed_improvement_percent) {
                comparison.speed_improvement_percent = improvement;
                comparison.faster = improvement > 0;
            }
        }

        // Compare success rates
        double reliability_improvement = comp_snapshot.success_rate - ref_snapshot.success_rate;
        if (reliability_improvement > comparison.success_rate_improvement_percent) {
            comparison.success_rate_improvement_percent = reliability_improvement * 100.0;
            comparison.more_reliable = reliability_improvement > 0;
        }

        // Compare efficiency (processing speed relative to input size)
        if (ref_snapshot.avg_input_size_bytes > 0 && comp_snapshot.avg_input_size_bytes > 0) {
            double ref_efficiency = ref_snapshot.requests_per_second / (ref_snapshot.avg_input_size_bytes / 1024.0);
            double comp_efficiency = comp_snapshot.requests_per_second / (comp_snapshot.avg_input_size_bytes / 1024.0);

            double efficiency_improvement = ((comp_efficiency - ref_efficiency) / ref_efficiency) * 100.0;
            if (efficiency_improvement > comparison.resource_efficiency_percent) {
                comparison.resource_efficiency_percent = efficiency_improvement;
                comparison.more_efficient = efficiency_improvement > 0;
            }
        }
    }

    // Calculate statistical significance (mock implementation)
    comparison.statistical_significance = 0.95; // 95% confidence

    return comparison;
}

std::vector<PluginPerformanceTracker::OptimizationSuggestion>
PluginPerformanceTracker::analyze_for_optimizations(const std::string& plugin_name) const {
    std::vector<OptimizationSuggestion> suggestions;

    auto now = std::chrono::system_clock::now();
    auto start = now - std::chrono::hours{24};
    auto snapshot = get_performance_snapshot(plugin_name, start, now);

    // Check for performance issues
    if (snapshot.avg_processing_time_ms > alert_config_.max_processing_time_ms) {
        OptimizationSuggestion suggestion;
        suggestion.type = OptimizationSuggestion::PERFORMANCE;
        suggestion.plugin_name = plugin_name;
        suggestion.description = "Average processing time exceeds threshold";
        suggestion.potential_improvement_percent =
            ((snapshot.avg_processing_time_ms - alert_config_.max_processing_time_ms) /
             snapshot.avg_processing_time_ms) * 100.0;
        suggestion.recommendation = "Consider optimizing algorithm or reducing complexity";
        suggestion.priority = static_cast<int>(std::min(10.0,
            (snapshot.avg_processing_time_ms / alert_config_.max_processing_time_ms) * 5.0));
        suggestions.push_back(suggestion);
    }

    // Check for reliability issues
    if (snapshot.success_rate < alert_config_.min_success_rate) {
        OptimizationSuggestion suggestion;
        suggestion.type = OptimizationSuggestion::RELIABILITY;
        suggestion.plugin_name = plugin_name;
        suggestion.description = "Success rate below threshold";
        suggestion.potential_improvement_percent =
            (alert_config_.min_success_rate - snapshot.success_rate) * 100.0;
        suggestion.recommendation = "Improve error handling and input validation";
        suggestion.priority = static_cast<int>((1.0 - snapshot.success_rate) * 10.0);
        suggestions.push_back(suggestion);
    }

    // Check for efficiency issues
    if (snapshot.requests_per_second < alert_config_.min_throughput_rps) {
        OptimizationSuggestion suggestion;
        suggestion.type = OptimizationSuggestion::EFFICIENCY;
        suggestion.plugin_name = plugin_name;
        suggestion.description = "Throughput below threshold";
        suggestion.potential_improvement_percent =
            ((alert_config_.min_throughput_rps - snapshot.requests_per_second) /
             alert_config_.min_throughput_rps) * 100.0;
        suggestion.recommendation = "Optimize caching and reduce I/O operations";
        suggestion.priority = static_cast<int>((1.0 - (snapshot.requests_per_second / alert_config_.min_throughput_rps)) * 8.0);
        suggestions.push_back(suggestion);
    }

    return suggestions;
}

void PluginPerformanceTracker::set_alert_config(const AlertConfig& config) {
    alert_config_ = config;
}

std::vector<PluginPerformanceTracker::RealTimeAlert>
PluginPerformanceTracker::check_for_alerts() const {
    std::vector<RealTimeAlert> alerts;

    // This needs to be implemented with actual plugin tracking
    // For now, return an empty alerts vector
    return alerts;
}

void PluginPerformanceTracker::clear_alerts() {
    std::lock_guard<std::mutex> lock(alerts_mutex_);
    recent_alerts_.clear();
}

double PluginPerformanceTracker::calculate_statistical_significance(
    const std::vector<double>& sample1,
    const std::vector<double>& sample2) const {

    if (sample1.size() < 2 || sample2.size() < 2) {
        return 0.0;
    }

    // Calculate means
    double mean1 = std::accumulate(sample1.begin(), sample1.end(), 0.0) / sample1.size();
    double mean2 = std::accumulate(sample2.begin(), sample2.end(), 0.0) / sample2.size();

    // Calculate standard deviations
    double var1 = 0.0, var2 = 0.0;
    for (double val : sample1) {
        var1 += (val - mean1) * (val - mean1);
    }
    for (double val : sample2) {
        var2 += (val - mean2) * (val - mean2);
    }
    var1 /= (sample1.size() - 1);
    var2 /= (sample2.size() - 1);

    // Calculate t-statistic
    double pooled_variance = ((sample1.size() - 1) * var1 + (sample2.size() - 1) * var2) /
                           (sample1.size() + sample2.size() - 2);
    double standard_error = std::sqrt(pooled_variance * (1.0 / sample1.size() + 1.0 / sample2.size()));
    double t_statistic = (mean1 - mean2) / standard_error;

    // For simplicity, return absolute T-value as significance indicator
    return std::abs(t_statistic);
}

std::vector<PluginPerformanceTracker::RealTimeAlert>
PluginPerformanceTracker::check_performance_alerts(
    const std::string& plugin_name,
    const PerformanceSnapshot& snapshot) const {

    std::vector<RealTimeAlert> alerts;
    auto now = std::chrono::system_clock::now();

    // Check processing time alert
    if (snapshot.avg_processing_time_ms > alert_config_.max_processing_time_ms) {
        RealTimeAlert alert;
        alert.severity = RealTimeAlert::WARNING;
        alert.plugin_name = plugin_name;
        alert.metric_name = "processing_time";
        alert.message = "Processing time exceeds threshold";
        alert.current_value = snapshot.avg_processing_time_ms;
        alert.threshold_value = alert_config_.max_processing_time_ms;
        alert.timestamp = now;
        alerts.push_back(alert);
    }

    // Check success rate alert
    if (snapshot.success_rate < alert_config_.min_success_rate) {
        RealTimeAlert alert;
        alert.severity = RealTimeAlert::ERROR;
        alert.plugin_name = plugin_name;
        alert.metric_name = "success_rate";
        alert.message = "Success rate below threshold";
        alert.current_value = snapshot.success_rate;
        alert.threshold_value = alert_config_.min_success_rate;
        alert.timestamp = now;
        alerts.push_back(alert);
    }

    // Check throughput alert
    if (snapshot.requests_per_second < alert_config_.min_throughput_rps) {
        RealTimeAlert alert;
        alert.severity = RealTimeAlert::WARNING;
        alert.plugin_name = plugin_name;
        alert.metric_name = "throughput";
        alert.message = "Throughput below threshold";
        alert.current_value = snapshot.requests_per_second;
        alert.threshold_value = alert_config_.min_throughput_rps;
        alert.timestamp = now;
        alerts.push_back(alert);
    }

    return alerts;
}

// SystemOverview implementation
nlohmann::json SystemPerformanceMonitor::SystemOverview::to_json() const {
    nlohmann::json j;
    j["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        timestamp.time_since_epoch()).count();

    j["total_requests_per_second"] = total_requests_per_second;
    j["successful_requests_per_second"] = successful_requests_per_second;
    j["failed_requests_per_second"] = failed_requests_per_second;

    j["avg_response_time_ms"] = avg_response_time_ms;
    j["p95_response_time_ms"] = p95_response_time_ms;
    j["p99_response_time_ms"] = p99_response_time_ms;

    j["active_plugin_count"] = active_plugin_count;

    j["cpu_usage_percent"] = cpu_usage_percent;
    j["memory_usage_mb"] = memory_usage_mb;
    j["disk_io_rate_mb_per_sec"] = disk_io_rate_mb_per_sec;
    j["network_io_rate_mb_per_sec"] = network_io_rate_mb_per_sec;

    j["overall_success_rate"] = overall_success_rate;

    nlohmann::json plugin_snapshots_json;
    for (const auto& snapshot : plugin_snapshots) {
        plugin_snapshots_json.push_back(snapshot.to_json());
    }
    j["plugin_snapshots"] = plugin_snapshots_json;

    nlohmann::json active_alerts_json;
    for (const auto& alert : active_alerts) {
        active_alerts_json.push_back(alert.to_json());
    }
    j["active_alerts"] = active_alerts_json;

    return j;
}

// CapacityMetrics implementation
nlohmann::json SystemPerformanceMonitor::CapacityMetrics::to_json() const {
    nlohmann::json j;
    j["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        timestamp.time_since_epoch()).count();

    j["current_load_percent"] = current_load_percent;
    j["peak_load_percent"] = peak_load_percent;
    j["avg_load_percent"] = avg_load_percent;

    j["load_growth_rate_percent"] = load_growth_rate_percent;
    j["predicted_peak_load_percent"] = predicted_peak_load_percent;
    j["predicted_capacity_exhaustion"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        predicted_capacity_exhaustion.time_since_epoch()).count();

    j["resource_utilization"] = resource_utilization;

    j["scaling_recommended"] = scaling_recommended;
    j["scaling_recommendation"] = scaling_recommendation;
    j["scaling_timeline_days"] = scaling_timeline.count();

    return j;
}

// OptimizationReport implementation
nlohmann::json SystemPerformanceMonitor::OptimizationReport::to_json() const {
    nlohmann::json j;
    j["generated_at"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        generated_at.time_since_epoch()).count();

    nlohmann::json suggestions_json;
    for (const auto& suggestion : suggestions) {
        nlohmann::json s;
        s["type"] = static_cast<int>(suggestion.type);
        s["plugin_name"] = suggestion.plugin_name;
        s["description"] = suggestion.description;
        s["potential_improvement_percent"] = suggestion.potential_improvement_percent;
        s["recommendation"] = suggestion.recommendation;
        s["priority"] = suggestion.priority;
        suggestions_json.push_back(s);
    }
    j["suggestions"] = suggestions_json;

    j["capacity_insights"] = capacity_insights.to_json();
    j["overall_performance_score"] = overall_performance_score;

    // Convert prioritized_actions queue to JSON
    nlohmann::json actions_json;
    auto temp_queue = prioritized_actions;
    while (!temp_queue.empty()) {
        // Convert each priority queue item to a JSON object
        // Since priority_queue only gives access to top(), we need to pop all elements
        // This will empty the queue, so we should work with a copy if needed later
        temp_queue.pop();
    }
    j["prioritized_actions"] = actions_json;

    return j;
}

// SystemPerformanceMonitor implementation
SystemPerformanceMonitor::SystemPerformanceMonitor(std::shared_ptr<MetricsCollector> collector,
                                                   std::shared_ptr<TimeSeriesDB> tsdb)
    : collector_(collector), tsdb_(tsdb) {
    plugin_tracker_ = std::make_unique<PluginPerformanceTracker>(collector);
}

SystemPerformanceMonitor::SystemOverview
SystemPerformanceMonitor::get_system_overview() const {
    return collect_current_metrics();
}

SystemPerformanceMonitor::CapacityMetrics
SystemPerformanceMonitor::get_capacity_metrics() const {
    return analyze_capacity();
}

void SystemPerformanceMonitor::register_alert_callback(
    std::function<void(const PluginPerformanceTracker::RealTimeAlert&)> callback) {
    alert_callback_ = callback;
}

void SystemPerformanceMonitor::unregister_alert_callback() {
    alert_callback_ = nullptr;
}

std::vector<SystemPerformanceMonitor::SystemOverview>
SystemPerformanceMonitor::get_historical_overview(
    const std::chrono::system_clock::time_point& /*start*/,
    const std::chrono::system_clock::time_point& /*end*/,
    const std::chrono::minutes& /*interval*/) const {

    std::vector<SystemOverview> overviews;

    // This would query the time-series database for historical data
    // For now, return empty results
    return overviews;
}

SystemPerformanceMonitor::OptimizationReport
SystemPerformanceMonitor::generate_optimization_report() const {
    OptimizationReport report;
    report.generated_at = std::chrono::system_clock::now();
    report.capacity_insights = analyze_capacity();

    auto current_overview = collect_current_metrics();
    report.overall_performance_score = calculate_performance_score(current_overview);

    // Collect optimization suggestions from all plugins
    // This would iterate through all plugins
    // For now, use empty suggestions

    // Generate prioritized actions
    std::priority_queue<std::pair<int, std::string>> prioritized_queue;

    // Add capacity-related actions
    if (report.capacity_insights.scaling_recommended) {
        prioritized_queue.emplace(8, report.capacity_insights.scaling_recommendation);
    }

    // Add performance-related actions
    if (report.overall_performance_score < 70) {
        prioritized_queue.emplace(6, "Investigate performance bottlenecks in slow plugins");
    }

    // Add reliability-related actions
    if (current_overview.overall_success_rate < 0.95) {
        prioritized_queue.emplace(9, "Address reliability issues causing failures");
    }

    report.prioritized_actions = prioritized_queue;

    return report;
}

void SystemPerformanceMonitor::update_config(const MonitorConfig& config) {
    config_ = config;
}

void SystemPerformanceMonitor::start_monitoring() {
    if (monitoring_active_) return;

    monitoring_active_ = true;
    monitoring_thread_ = std::thread(&SystemPerformanceMonitor::run_monitoring_loop, this);
}

void SystemPerformanceMonitor::stop_monitoring() {
    if (!monitoring_active_) return;

    monitoring_active_ = false;
    if (monitoring_thread_.joinable()) {
        monitoring_thread_.join();
    }
}

void SystemPerformanceMonitor::run_monitoring_loop() {
    while (monitoring_active_) {
        try {
            // Collect current metrics
            auto overview = collect_current_metrics();

            // Store historical data
            {
                std::lock_guard<std::mutex> lock(cache_mutex_);
                historical_overviews_.push_back(overview);

                // Keep only recent data
                while (historical_overviews_.size() > config_.max_historical_data_points) {
                    historical_overviews_.erase(historical_overviews_.begin());
                }
            }

            // Check for alerts
            if (plugin_tracker_) {
                auto alerts = plugin_tracker_->check_for_alerts();
                for (const auto& alert : alerts) {
                    if (alert_callback_) {
                        alert_callback_(alert);
                    }
                }
            }

            // Periodic capacity analysis
            auto now = std::chrono::system_clock::now();
            if (now - last_capacity_analysis_ > config_.capacity_analysis_interval) {
                analyze_capacity();
                last_capacity_analysis_ = now;
            }

            // Periodic optimization reporting
            if (now - last_optimization_report_ > config_.optimization_report_interval) {
                generate_optimization_report();
                last_optimization_report_ = now;
            }

        } catch (const std::exception& e) {
            // Log error but continue monitoring
        }

        std::this_thread::sleep_for(config_.metrics_collection_interval);
    }
}

SystemPerformanceMonitor::SystemOverview
SystemPerformanceMonitor::collect_current_metrics() const {
    SystemOverview overview;
    overview.timestamp = std::chrono::system_clock::now();

    if (!collector_) {
        return overview;
    }

    // Get real-time statistics
    auto processing_stats = collector_->get_real_time_stats({"plugin_processing_time_ms",
                                                             "plugin_executions_total",
                                                             "plugin_throughput_bytes"});

    // Calculate request metrics
    for (const auto& stat : processing_stats) {
        if (stat.name == "plugin_processing_time_ms") {
            overview.avg_response_time_ms = stat.mean;
            overview.p95_response_time_ms = stat.p95;
            overview.p99_response_time_ms = stat.p99;
        } else if (stat.name == "plugin_executions_total") {
            overview.total_requests_per_second = stat.sum / 60.0; // Assuming per-second rate
        }
    }

    // This would collect system metrics like CPU, memory, etc.
    // For now, use placeholder values
    overview.cpu_usage_percent = 0.0;
    overview.memory_usage_mb = 0.0;
    overview.disk_io_rate_mb_per_sec = 0.0;
    overview.network_io_rate_mb_per_sec = 0.0;

    // Get plugin snapshots
    if (plugin_tracker_) {
        auto now = overview.timestamp;
        auto start = now - std::chrono::minutes{30};
        overview.plugin_snapshots = plugin_tracker_->get_all_plugin_snapshots(start, now);
        overview.active_plugin_count = overview.plugin_snapshots.size();
    }

    return overview;
}

SystemPerformanceMonitor::CapacityMetrics
SystemPerformanceMonitor::analyze_capacity() const {
    CapacityMetrics metrics;
    metrics.timestamp = std::chrono::system_clock::now();

    // This would perform actual capacity analysis
    // For now, return placeholder values
    metrics.current_load_percent = 0.0;
    metrics.peak_load_percent = 0.0;
    metrics.avg_load_percent = 0.0;
    metrics.load_growth_rate_percent = 0.0;
    metrics.predicted_peak_load_percent = 0.0;
    metrics.predicted_capacity_exhaustion = std::chrono::system_clock::now() + std::chrono::days{30};
    metrics.scaling_recommended = false;

    return metrics;
}

double SystemPerformanceMonitor::calculate_performance_score(const SystemOverview& overview) const {
    double score = 100.0;

    // Penalize for high response times
    if (overview.avg_response_time_ms > 100) {
        score -= (overview.avg_response_time_ms - 100) * 0.1;
    }

    // Penalize for low success rates
    if (overview.overall_success_rate < 0.95) {
        score -= (0.95 - overview.overall_success_rate) * 200;
    }

    // Penalize for high resource usage
    if (overview.cpu_usage_percent > 80) {
        score -= (overview.cpu_usage_percent - 80) * 0.5;
    }

    if (overview.memory_usage_mb > 1000) {
        score -= (overview.memory_usage_mb - 1000) * 0.01;
    }

    return std::max(0.0, std::min(100.0, score));
}

std::string SystemPerformanceMonitor::generate_scaling_recommendation(const CapacityMetrics& metrics) const {
    if (metrics.current_load_percent > 90) {
        return "Immediate scaling required - system at capacity limit";
    } else if (metrics.predicted_peak_load_percent > 85) {
        return "Plan scaling within " + std::to_string(metrics.scaling_timeline.count()) + " days";
    } else if (metrics.load_growth_rate_percent > 20) {
        return "Monitor growth rate - scaling may be needed soon";
    } else {
        return "Current capacity adequate";
    }
}

} // namespace metrics
} // namespace aimux