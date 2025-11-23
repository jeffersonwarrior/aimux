#include "aimux/metrics/metrics_collector.hpp"
#include <algorithm>
#include <cmath>
#include <random>
#include <sstream>
#include <iomanip>
#include <numeric>

namespace aimux {
namespace metrics {

// MetricPoint implementation
nlohmann::json MetricPoint::to_json() const {
    nlohmann::json j;
    j["name"] = name;
    j["type"] = static_cast<int>(type);
    j["value"] = value;
    j["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        timestamp.time_since_epoch()).count();
    j["tags"] = tags;
    j["fields"] = fields;
    return j;
}

MetricPoint MetricPoint::from_json(const nlohmann::json& j) {
    MetricPoint point;
    point.name = j.at("name").get<std::string>();
    point.type = static_cast<MetricType>(j.at("type").get<int>());
    point.value = j.at("value").get<double>();
    point.timestamp = std::chrono::system_clock::from_time_t(
        j.at("timestamp").get<int64_t>() / 1000);

    if (j.contains("tags")) {
        point.tags = j.at("tags").get<std::unordered_map<std::string, std::string>>();
    }
    if (j.contains("fields")) {
        point.fields = j.at("fields").get<std::unordered_map<std::string, double>>();
    }
    return point;
}

// MetricStatistics implementation
nlohmann::json MetricStatistics::to_json() const {
    nlohmann::json j;
    j["name"] = name;
    j["type"] = static_cast<int>(type);
    j["count"] = count;
    j["sum"] = sum;
    j["min"] = min;
    j["max"] = max;
    j["mean"] = mean;
    j["median"] = median;
    j["p95"] = p95;
    j["p99"] = p99;
    j["std_dev"] = std_dev;
    return j;
}

// PrettificationEvent implementation
nlohmann::json PrettificationEvent::to_json() const {
    nlohmann::json j;
    j["plugin_name"] = plugin_name;
    j["provider"] = provider;
    j["model"] = model;
    j["input_format"] = input_format;
    j["output_format"] = output_format;
    j["processing_time_ms"] = processing_time_ms;
    j["input_size_bytes"] = input_size_bytes;
    j["output_size_bytes"] = output_size_bytes;
    j["success"] = success;
    j["error_type"] = error_type;
    j["tokens_processed"] = tokens_processed;
    j["capabilities_used"] = capabilities_used;
    j["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        timestamp.time_since_epoch()).count();
    j["metadata"] = metadata;
    return j;
}

PrettificationEvent PrettificationEvent::from_json(const nlohmann::json& j) {
    PrettificationEvent event;
    event.plugin_name = j.at("plugin_name").get<std::string>();
    event.provider = j.at("provider").get<std::string>();
    event.model = j.at("model").get<std::string>();
    event.input_format = j.at("input_format").get<std::string>();
    event.output_format = j.at("output_format").get<std::string>();
    event.processing_time_ms = j.at("processing_time_ms").get<double>();
    event.input_size_bytes = j.at("input_size_bytes").get<size_t>();
    event.output_size_bytes = j.at("output_size_bytes").get<size_t>();
    event.success = j.at("success").get<bool>();
    event.error_type = j.value("error_type", "");
    event.tokens_processed = j.at("tokens_processed").get<size_t>();
    event.capabilities_used = j.value("capabilities_used", std::vector<std::string>{});
    event.timestamp = std::chrono::system_clock::from_time_t(
        j.at("timestamp").get<int64_t>() / 1000);
    event.metadata = j.value("metadata", std::unordered_map<std::string, std::string>{});
    return event;
}

// MetricsCollector implementation
MetricsCollector::MetricsCollector(const Config& config)
    : config_(config) {
    if (config_.enable_real_time) {
        start_collection();
    }
}

MetricsCollector::~MetricsCollector() {
    stop_collection();
    flush();
}

void MetricsCollector::record_counter(const std::string& name, double value,
                                     const std::unordered_map<std::string, std::string>& tags) {
    if (config_.sampling_rate < 1.0) {
        static thread_local std::random_device rd;
        static thread_local std::mt19937 gen(rd());
        static thread_local std::uniform_real_distribution<> dis(0.0, 1.0);
        if (dis(gen) > config_.sampling_rate) {
            return;
        }
    }

    auto point = create_metric_point(name, MetricType::COUNTER, value, tags);
    record_event(point);
}

void MetricsCollector::record_gauge(const std::string& name, double value,
                                   const std::unordered_map<std::string, std::string>& tags) {
    auto point = create_metric_point(name, MetricType::GAUGE, value, tags);
    record_event(point);
}

void MetricsCollector::record_histogram(const std::string& name, double value,
                                       const std::unordered_map<std::string, std::string>& tags) {
    auto point = create_metric_point(name, MetricType::HISTOGRAM, value, tags);
    record_event(point);
}

void MetricsCollector::record_timer(const std::string& name, std::chrono::nanoseconds duration,
                                   const std::unordered_map<std::string, std::string>& tags) {
    double value_ms = std::chrono::duration<double, std::milli>(duration).count();
    auto point = create_metric_point(name, MetricType::TIMER, value_ms, tags);
    record_event(point);
}

void MetricsCollector::record_event(const MetricPoint& event) {
    if (!collecting_.load()) return;

    {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        metrics_buffer_.push(event);

        // Prevent buffer overflow
        while (metrics_buffer_.size() > config_.buffer_size) {
            metrics_buffer_.pop();
        }
    }

    update_real_time_aggregation(event);

    if (metric_callback_) {
        metric_callback_(event);
    }
}

void MetricsCollector::record_prettification_event(const PrettificationEvent& event) {
    if (!collecting_.load()) return;

    {
        std::lock_guard<std::mutex> lock(events_mutex_);
        events_buffer_.push(event);

        // Prevent buffer overflow
        while (events_buffer_.size() > config_.buffer_size) {
            events_buffer_.pop();
        }
    }

    if (event_callback_) {
        event_callback_(event);
    }
}

void MetricsCollector::record_batch(const std::vector<MetricPoint>& metrics) {
    for (const auto& metric : metrics) {
        record_event(metric);
    }
}

void MetricsCollector::record_prettification_batch(const std::vector<PrettificationEvent>& events) {
    for (const auto& event : events) {
        record_prettification_event(event);
    }
}

std::vector<MetricPoint> MetricsCollector::query_metrics(
    const std::string& /*name*/,
    const std::chrono::system_clock::time_point& /*start*/,
    const std::chrono::system_clock::time_point& /*end*/,
    const std::unordered_map<std::string, std::string>& /*tags*/) const {
    // This would be implemented by the specific storage backend
    // For now, return empty results
    return {};
}

MetricStatistics MetricsCollector::get_statistics(
    const std::string& name,
    const std::chrono::system_clock::time_point& /*start*/,
    const std::chrono::system_clock::time_point& /*end*/) const {
    // This would be implemented by the specific storage backend
    MetricStatistics stats;
    stats.name = name;
    return stats;
}

std::vector<MetricStatistics> MetricsCollector::get_real_time_stats(const std::vector<std::string>& metric_names) const {
    std::vector<MetricStatistics> stats;

    std::lock_guard<std::mutex> lock(aggregation_mutex_);

    for (const auto& name : metric_names) {
        auto it = recent_values_.find(name);
        if (it != recent_values_.end() && !it->second.empty()) {
            const auto& values = it->second;

            MetricStatistics stat;
            stat.name = name;
            stat.type = MetricType::GAUGE;
            stat.count = values.size();
            stat.sum = std::accumulate(values.begin(), values.end(), 0.0);
            stat.mean = stat.sum / stat.count;
            stat.min = *std::min_element(values.begin(), values.end());
            stat.max = *std::max_element(values.begin(), values.end());

            // Calculate percentiles
            auto sorted_values = values;
            std::sort(sorted_values.begin(), sorted_values.end());

            if (!sorted_values.empty()) {
                auto percentile = [&](double p) {
                    size_t index = static_cast<size_t>(p * (sorted_values.size() - 1));
                    return sorted_values[index];
                };

                stat.median = percentile(0.5);
                stat.p95 = percentile(0.95);
                stat.p99 = percentile(0.99);

                // Calculate standard deviation
                double variance = 0.0;
                for (double value : values) {
                    variance += (value - stat.mean) * (value - stat.mean);
                }
                variance /= stat.count;
                stat.std_dev = std::sqrt(variance);
            }

            stats.push_back(stat);
        }
    }

    return stats;
}

MetricsCollector::PluginAnalytics MetricsCollector::get_plugin_analytics(
    const std::string& plugin_name,
    const std::chrono::system_clock::time_point& start,
    const std::chrono::system_clock::time_point& end) const {
    PluginAnalytics analytics;
    analytics.plugin_name = plugin_name;
    analytics.window_start = start;
    analytics.window_end = end;

    // This would query the stored events from the backend
    // For now, return empty analytics
    return analytics;
}

void MetricsCollector::start_collection() {
    if (collecting_.load()) return;

    collecting_ = true;
    should_stop_.store(false);
    processor_thread_ = std::thread(&MetricsCollector::process_batch, this);
}

void MetricsCollector::stop_collection() {
    if (!collecting_.load()) return;

    collecting_ = false;
    should_stop_.store(true);
    processor_cv_.notify_one();

    if (processor_thread_.joinable()) {
        processor_thread_.join();
    }
}

void MetricsCollector::flush() {
    process_batch();
}

void MetricsCollector::clear_old_data() {
    // Implementation would depend on storage backend
}

void MetricsCollector::update_config(const Config& config) {
    config_ = config;
}

nlohmann::json MetricsCollector::get_status() const {
    nlohmann::json status;
    status["collecting"] = collecting_.load();
    status["buffer_size"] = config_.buffer_size;
    status["flush_interval_ms"] = config_.flush_interval.count();
    status["sampling_rate"] = config_.sampling_rate;

    {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        status["metrics_buffer_size"] = metrics_buffer_.size();
    }

    {
        std::lock_guard<std::mutex> lock(events_mutex_);
        status["events_buffer_size"] = events_buffer_.size();
    }

    {
        std::lock_guard<std::mutex> lock(aggregation_mutex_);
        status["real_time_metrics_count"] = recent_values_.size();
    }

    return status;
}

void MetricsCollector::process_batch() {
    while (!should_stop_.load()) {
        std::vector<MetricPoint> metrics_batch;
        std::vector<PrettificationEvent> events_batch;

        // Collect metrics batch
        {
            std::lock_guard<std::mutex> lock(metrics_mutex_);
            while (!metrics_buffer_.empty() && metrics_batch.size() < config_.buffer_size) {
                metrics_batch.push_back(metrics_buffer_.front());
                metrics_buffer_.pop();
            }
        }

        // Collect events batch
        {
            std::lock_guard<std::mutex> lock(events_mutex_);
            while (!events_buffer_.empty() && events_batch.size() < config_.buffer_size) {
                events_batch.push_back(events_buffer_.front());
                events_buffer_.pop();
            }
        }

        // Store batches
        if (!metrics_batch.empty()) {
            store_metrics(metrics_batch);
        }

        if (!events_batch.empty()) {
            store_events(events_batch);
        }

        // Wait for next flush interval or stop signal
        std::unique_lock<std::mutex> lock(metrics_mutex_);
        processor_cv_.wait_for(lock, config_.flush_interval, [this] {
            return should_stop_.load() ||
                   !metrics_buffer_.empty() ||
                   !events_buffer_.empty();
        });
    }
}

void MetricsCollector::store_metrics(const std::vector<MetricPoint>& /*metrics*/) {
    // Base implementation does nothing - derived classes override
}

void MetricsCollector::store_events(const std::vector<PrettificationEvent>& /*events*/) {
    // Base implementation does nothing - derived classes override
}

MetricPoint MetricsCollector::create_metric_point(
    const std::string& name,
    MetricType type,
    double value,
    const std::unordered_map<std::string, std::string>& tags) const {
    MetricPoint point;
    point.name = name;
    point.type = type;
    point.value = value;
    point.timestamp = std::chrono::system_clock::now();
    point.tags = tags;
    return point;
}

void MetricsCollector::update_real_time_aggregation(const MetricPoint& point) {
    std::lock_guard<std::mutex> lock(aggregation_mutex_);

    auto& values = recent_values_[point.name];
    values.push_back(point.value);

    // Keep only recent values (last 1000 for efficiency)
    if (values.size() > 1000) {
        values.erase(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(values.size() - 1000));
    }

    last_update_[point.name] = point.timestamp;
}

std::vector<double> MetricsCollector::calculate_percentiles(const std::vector<double>& values) const {
    if (values.empty()) return {};

    auto sorted_values = values;
    std::sort(sorted_values.begin(), sorted_values.end());

    auto percentile = [&](double p) {
        size_t index = static_cast<size_t>(p * (sorted_values.size() - 1));
        return sorted_values[index];
    };

    return {percentile(0.5), percentile(0.95), percentile(0.99)};
}

// InMemoryMetricsCollector implementation
InMemoryMetricsCollector::InMemoryMetricsCollector(const Config& config)
    : MetricsCollector(config) {}

void InMemoryMetricsCollector::clear_stored_data() {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    stored_metrics_.clear();
    stored_events_.clear();
}

void InMemoryMetricsCollector::store_metrics(const std::vector<MetricPoint>& metrics) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    stored_metrics_.insert(stored_metrics_.end(), metrics.begin(), metrics.end());
}

void InMemoryMetricsCollector::store_events(const std::vector<PrettificationEvent>& events) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    stored_events_.insert(stored_events_.end(), events.begin(), events.end());
}

} // namespace metrics
} // namespace aimux