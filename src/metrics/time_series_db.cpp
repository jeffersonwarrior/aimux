#include "aimux/metrics/time_series_db.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <random>

namespace aimux {
namespace metrics {

// TSDBQueryBuilder implementation
TSDBQueryBuilder::TSDBQueryBuilder(const std::string& measurement)
    : measurement_(measurement) {}

TSDBQueryBuilder& TSDBQueryBuilder::time_range(
    const std::chrono::system_clock::time_point& start,
    const std::chrono::system_clock::time_point& end) {
    time_range_ = {start, end};
    return *this;
}

TSDBQueryBuilder& TSDBQueryBuilder::tag(const std::string& key, const std::string& value) {
    tags_[key] = value;
    return *this;
}

TSDBQueryBuilder& TSDBQueryBuilder::tags(const std::unordered_map<std::string, std::string>& tags) {
    for (const auto& [key, value] : tags) {
        tags_[key] = value;
    }
    return *this;
}

TSDBQueryBuilder& TSDBQueryBuilder::field(const std::string& name) {
    fields_.push_back(name);
    return *this;
}

TSDBQueryBuilder& TSDBQueryBuilder::fields(const std::vector<std::string>& names) {
    fields_.insert(fields_.end(), names.begin(), names.end());
    return *this;
}

TSDBQueryBuilder& TSDBQueryBuilder::group_by(const std::vector<std::string>& tags) {
    group_by_ = tags;
    return *this;
}

TSDBQueryBuilder& TSDBQueryBuilder::fill(const std::string& fill_type) {
    fill_type_ = fill_type;
    return *this;
}

TSDBQueryBuilder& TSDBQueryBuilder::limit(size_t count) {
    limit_ = count;
    return *this;
}

TSDBQueryBuilder& TSDBQueryBuilder::order_by(const std::string& field, const std::string& direction) {
    order_by_ = {field, direction};
    return *this;
}

std::string TSDBQueryBuilder::build_query() const {
    std::stringstream query;
    query << "SELECT ";

    if (fields_.empty()) {
        query << "*";
    } else {
        for (size_t i = 0; i < fields_.size(); ++i) {
            if (i > 0) query << ", ";
            query << fields_[i];
        }
    }

    query << " FROM " << measurement_;

    if (time_range_) {
        auto start_ts = std::chrono::duration_cast<std::chrono::nanoseconds>(
            time_range_->first.time_since_epoch()).count();
        auto end_ts = std::chrono::duration_cast<std::chrono::nanoseconds>(
            time_range_->second.time_since_epoch()).count();
        query << " WHERE time >= " << start_ts << " AND time <= " << end_ts;
    }

    if (!tags_.empty()) {
        if (!time_range_) {
            query << " WHERE ";
        } else {
            query << " AND ";
        }

        bool first = true;
        for (const auto& [key, value] : tags_) {
            if (!first) query << " AND ";
            query << "\"" << key << "\" = '" << value << "'";
            first = false;
        }
    }

    if (!group_by_.empty()) {
        query << " GROUP BY ";
        for (size_t i = 0; i < group_by_.size(); ++i) {
            if (i > 0) query << ", ";
            query << "\"" << group_by_[i] << "\"";
        }
    }

    if (!fill_type_.empty()) {
        query << " fill(" << fill_type_ << ")";
    }

    if (order_by_) {
        query << " ORDER BY time " << order_by_->second;
    }

    if (limit_) {
        query << " LIMIT " << *limit_;
    }

    return query.str();
}

// TimeSeriesDB implementation
TimeSeriesDB::TimeSeriesDB(const TSDBConfig& config)
    : config_(config) {
    if (config_.flush_interval.count() > 0) {
        start_async_worker();
    }
}

TimeSeriesDB::~TimeSeriesDB() {
    stop_async_worker();
}

void TimeSeriesDB::write_metrics_async(const std::vector<MetricPoint>& metrics) {
    AsyncWriteRequest request;
    request.type = AsyncWriteRequest::METRICS;
    request.metrics = metrics;

    {
        std::lock_guard<std::mutex> lock(async_mutex_);
        async_write_queue_.push(request);
    }
    async_cv_.notify_one();
}

void TimeSeriesDB::write_events_async(const std::vector<PrettificationEvent>& events) {
    AsyncWriteRequest request;
    request.type = AsyncWriteRequest::EVENTS;
    request.events = events;

    {
        std::lock_guard<std::mutex> lock(async_mutex_);
        async_write_queue_.push(request);
    }
    async_cv_.notify_one();
}

void TimeSeriesDB::process_async_writes() {
    std::queue<AsyncWriteRequest> local_queue;

    while (!should_stop_async_) {
        {
            std::unique_lock<std::mutex> lock(async_mutex_);
            async_cv_.wait(lock, [this] {
                return !async_write_queue_.empty() || should_stop_async_;
            });

            // Move all pending requests to local queue
            while (!async_write_queue_.empty()) {
                local_queue.push(async_write_queue_.front());
                async_write_queue_.pop();
            }
        }

        // Process requests outside of lock
        while (!local_queue.empty()) {
            auto request = local_queue.front();
            local_queue.pop();

            bool success = false;
            try {
                if (request.type == AsyncWriteRequest::METRICS) {
                    success = write_metrics_sync(request.metrics);
                } else if (request.type == AsyncWriteRequest::EVENTS) {
                    success = write_events_sync(request.events);
                }
            } catch (const std::exception& e) {
                success = false;
            }

            if (request.callback) {
                request.callback(success);
            }
        }
    }
}

void TimeSeriesDB::start_async_worker() {
    should_stop_async_ = false;
    async_worker_ = std::thread(&TimeSeriesDB::process_async_writes, this);
}

void TimeSeriesDB::stop_async_worker() {
    should_stop_async_ = true;
    async_cv_.notify_one();

    if (async_worker_.joinable()) {
        async_worker_.join();
    }
}

// InfluxDB2Client implementation
InfluxDB2Client::InfluxDB2Client(const TSDBConfig& config)
    : TimeSeriesDB(config) {}

InfluxDB2Client::~InfluxDB2Client() {
    disconnect();
}

bool InfluxDB2Client::connect() {
    if (connected_) return true;

    if (!authenticate()) {
        return false;
    }

    connected_ = true;
    return true;
}

bool InfluxDB2Client::disconnect() {
    connected_ = false;
    session_token_.clear();
    return true;
}

bool InfluxDB2Client::is_connected() const {
    return connected_;
}

bool InfluxDB2Client::ping() {
    nlohmann::json response;
    std::string error;
    return http_request("GET", "/health", "", response, error);
}

bool InfluxDB2Client::create_database(const std::string& name) {
    return create_bucket(name);
}

bool InfluxDB2Client::drop_database(const std::string& name) {
    return delete_bucket(name);
}

std::vector<std::string> InfluxDB2Client::list_databases() {
    return list_buckets();
}

bool InfluxDB2Client::write_metrics(const std::vector<MetricPoint>& metrics) {
    if (!connected_ && !connect()) {
        return false;
    }

    return write_metrics_sync(metrics);
}

bool InfluxDB2Client::write_events(const std::vector<PrettificationEvent>& events) {
    if (!connected_ && !connect()) {
        return false;
    }

    return write_events_sync(events);
}

std::vector<MetricPoint> InfluxDB2Client::query_metrics(const TSDBQueryBuilder& query) {
    std::vector<MetricPoint> results;

    if (!connected_) {
        return results;
    }

    auto start_time = std::chrono::high_resolution_clock::now();

    nlohmann::json response;
    std::string error;
    std::string query_str = query.build_query();

    bool success = http_request("POST", build_query_url(), query_str, response, error);

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration<double, std::milli>(end_time - start_time);
    {
        std::lock_guard<std::mutex> lock(performance_mutex_);
        last_query_time_ms_ = duration.count();
    }

    if (success && response.contains("results")) {
        for (const auto& result : response["results"]) {
            if (result.contains("series")) {
                for (const auto& series : result["series"]) {
                    for (const auto& values : series["values"]) {
                        MetricPoint point = parse_influx_point({
                            {"name", series["name"]},
                            {"tags", series.value("tags", nlohmann::json::object())},
                            {"columns", series["columns"]},
                            {"values", values}
                        });
                        results.push_back(point);
                    }
                }
            }
        }
    }

    return results;
}

std::vector<PrettificationEvent> InfluxDB2Client::query_events(const TSDBQueryBuilder& query) {
    std::vector<PrettificationEvent> results;

    if (!connected_) {
        return results;
    }

    auto start_time = std::chrono::high_resolution_clock::now();

    nlohmann::json response;
    std::string error;
    std::string query_str = query.build_query();

    bool success = http_request("POST", build_query_url(), query_str, response, error);

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration<double, std::milli>(end_time - start_time);
    {
        std::lock_guard<std::mutex> lock(performance_mutex_);
        last_query_time_ms_ = duration.count();
    }

    if (success && response.contains("results")) {
        for (const auto& result : response["results"]) {
            if (result.contains("series")) {
                for (const auto& series : result["series"]) {
                    for (const auto& values : series["values"]) {
                        PrettificationEvent event = parse_influx_event({
                            {"name", series["name"]},
                            {"tags", series.value("tags", nlohmann::json::object())},
                            {"columns", series["columns"]},
                            {"values", values}
                        });
                        results.push_back(event);
                    }
                }
            }
        }
    }

    return results;
}

std::vector<MetricStatistics> InfluxDB2Client::query_aggregations(
    const TSDBQueryBuilder& /*query*/,
    const std::vector<std::string>& /*aggregations*/) {
    // This would implement InfluxDB-specific aggregation queries
    // For now, return empty results
    return {};
}

bool InfluxDB2Client::create_bucket(const std::string& name, const std::chrono::hours& retention) {
    nlohmann::json bucket_request;
    bucket_request["name"] = name;
    bucket_request["orgID"] = config_.organization;
    bucket_request["retentionRules"] = nlohmann::json::array({
        {{"type", "expire"}, {"everySeconds", retention.count()}}
    });

    nlohmann::json response;
    std::string error;

    bool success = http_request("POST", "/api/v2/buckets", bucket_request.dump(), response, error);
    return success;
}

bool InfluxDB2Client::delete_bucket(const std::string& name) {
    // First find the bucket ID
    nlohmann::json response;
    std::string error;

    bool success = http_request("GET", "/api/v2/buckets", "", response, error);
    if (!success) return false;

    std::string bucket_id;
    if (response.contains("buckets")) {
        for (const auto& bucket : response["buckets"]) {
            if (bucket["name"] == name) {
                bucket_id = bucket["id"];
                break;
            }
        }
    }

    if (bucket_id.empty()) return false;

    success = http_request("DELETE", "/api/v2/buckets/" + bucket_id, "", response, error);
    return success;
}

std::vector<std::string> InfluxDB2Client::list_buckets() {
    std::vector<std::string> buckets;

    nlohmann::json response;
    std::string error;

    bool success = http_request("GET", "/api/v2/buckets", "", response, error);
    if (success && response.contains("buckets")) {
        for (const auto& bucket : response["buckets"]) {
            buckets.push_back(bucket["name"]);
        }
    }

    return buckets;
}

nlohmann::json InfluxDB2Client::get_status() const {
    nlohmann::json status;
    status["connected"] = connected_.load();
    status["host"] = config_.host;
    status["port"] = config_.port;
    status["database"] = config_.database;
    status["bucket"] = config_.bucket;
    status["organization"] = config_.organization;

    {
        std::lock_guard<std::mutex> lock(performance_mutex_);
        status["last_query_time_ms"] = last_query_time_ms_.load();
    }

    return status;
}

double InfluxDB2Client::get_query_performance_ms() const {
    std::lock_guard<std::mutex> lock(performance_mutex_);
    return last_query_time_ms_;
}

bool InfluxDB2Client::write_metrics_sync(const std::vector<MetricPoint>& metrics) {
    std::string line_protocol = format_metrics_for_influx(metrics);
    if (line_protocol.empty()) return true;

    nlohmann::json response;
    std::string error;
    return http_request("POST", build_write_url(), line_protocol, response, error);
}

bool InfluxDB2Client::write_events_sync(const std::vector<PrettificationEvent>& events) {
    std::string line_protocol = format_events_for_influx(events);
    if (line_protocol.empty()) return true;

    nlohmann::json response;
    std::string error;
    return http_request("POST", build_write_url(), line_protocol, response, error);
}

std::string InfluxDB2Client::build_write_url() const {
    std::stringstream url;
    url << "/api/v2/write?org=" << config_.organization << "&bucket=" << config_.bucket;
    // Precision is not available in TSDBConfig, using default
    url << "&precision=ns";
    return url.str();
}

std::string InfluxDB2Client::build_query_url() const {
    std::stringstream url;
    url << "/api/v2/query?org=" << config_.organization;
    return url.str();
}

std::string InfluxDB2Client::format_metrics_for_influx(const std::vector<MetricPoint>& metrics) const {
    std::stringstream result;

    for (const auto& metric : metrics) {
        // Write measurement name
        result << metric.name;

        // Write tags
        if (!metric.tags.empty()) {
            result << ",";
            bool first = true;
            for (const auto& [key, value] : metric.tags) {
                if (!first) result << ",";
                result << key << "=" << value;
                first = false;
            }
        }

        // Write fields
        result << " ";
        result << "value=" << metric.value;
        if (!metric.fields.empty()) {
            for (const auto& [key, value] : metric.fields) {
                result << "," << key << "=" << value;
            }
        }

        // Write timestamp
        auto timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            metric.timestamp.time_since_epoch()).count();
        result << " " << timestamp_ns;

        result << "\n";
    }

    return result.str();
}

std::string InfluxDB2Client::format_events_for_influx(const std::vector<PrettificationEvent>& events) const {
    std::stringstream result;

    for (const auto& event : events) {
        result << "prettification_events";
        result << ",plugin=" << event.plugin_name;
        result << ",provider=" << event.provider;
        result << ",model=" << event.model;
        result << ",input_format=" << event.input_format;
        result << ",output_format=" << event.output_format;
        result << " ";
        result << "processing_time_ms=" << event.processing_time_ms;
        result << ",input_size_bytes=" << event.input_size_bytes;
        result << ",output_size_bytes=" << event.output_size_bytes;
        result << ",success=" << (event.success ? "true" : "false");
        result << ",tokens_processed=" << event.tokens_processed;

        if (!event.error_type.empty()) {
            result << ",error_type=\"" << event.error_type << "\"";
        }

        auto timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            event.timestamp.time_since_epoch()).count();
        result << " " << timestamp_ns;

        result << "\n";
    }

    return result.str();
}

MetricPoint InfluxDB2Client::parse_influx_point(const nlohmann::json& /*json_point*/) const {
    MetricPoint point;
    // Implementation would parse based on the JSON structure from InfluxDB
    return point;
}

PrettificationEvent InfluxDB2Client::parse_influx_event(const nlohmann::json& /*json_event*/) const {
    PrettificationEvent event;
    // Implementation would parse based on the JSON structure from InfluxDB
    return event;
}

bool InfluxDB2Client::http_request(const std::string& /*method*/,
                                  const std::string& /*endpoint*/,
                                  const std::string& /*body*/,
                                  nlohmann::json& /*response*/,
                                  std::string& /*error_msg*/) {
    // This would implement actual HTTP client logic
    // For now, return success
    return true;
}

bool InfluxDB2Client::authenticate() {
    // This would implement authentication logic
    return true;
}

bool InfluxDB2Client::refresh_token() {
    // This would implement token refresh logic
    return true;
}

bool InfluxDB2Client::create_retention_policy(
    const std::string& name,
    const std::chrono::hours& duration,
    int /*replication_factor*/,
    bool /*default_policy*/) {
    // InfluxDB 2.x uses bucket retention policies instead of database retention policies
    // This would create a bucket with retention or update existing bucket retention
    return create_bucket(name, duration);
}

bool InfluxDB2Client::drop_retention_policy(const std::string& name) {
    // InfluxDB 2.x - delete the bucket
    return delete_bucket(name);
}

std::vector<std::string> InfluxDB2Client::list_retention_policies() {
    // InfluxDB 2.x - list bucket retention policies
    return list_buckets();
}

bool InfluxDB2Client::create_continuous_query(
    const std::string& /*name*/,
    const std::string& /*query*/) {
    // InfluxDB 2.x uses tasks instead of continuous queries
    // This would create a task for the query
    return true;
}

bool InfluxDB2Client::drop_continuous_query(const std::string& /*name*/) {
    // InfluxDB 2.x - delete the task
    return true;
}

std::vector<std::string> InfluxDB2Client::list_continuous_queries() {
    // InfluxDB 2.x - list tasks
    return {};
}

// MockTimeSeriesDB implementation
MockTimeSeriesDB::MockTimeSeriesDB(const TSDBConfig& config)
    : TimeSeriesDB(config) {}

std::vector<std::string> MockTimeSeriesDB::list_databases() {
    std::lock_guard<std::mutex> lock(mock_mutex_);
    return databases_;
}

bool MockTimeSeriesDB::write_metrics(const std::vector<MetricPoint>& metrics) {
    std::lock_guard<std::mutex> lock(mock_mutex_);
    metrics_.insert(metrics_.end(), metrics.begin(), metrics.end());
    return true;
}

bool MockTimeSeriesDB::write_events(const std::vector<PrettificationEvent>& events) {
    std::lock_guard<std::mutex> lock(mock_mutex_);
    events_.insert(events_.end(), events.begin(), events.end());
    return true;
}

std::vector<MetricPoint> MockTimeSeriesDB::query_metrics(const TSDBQueryBuilder& query) {
    std::vector<MetricPoint> results;
    std::lock_guard<std::mutex> lock(mock_mutex_);

    for (const auto& metric : metrics_) {
        if (metric.name == query.get_measurement()) {
            // Apply filters based on query builder settings
            bool matches = true;

            // Check time range
            const auto& time_range = query.get_time_range();
            if (time_range) {
                if (metric.timestamp < time_range->first ||
                    metric.timestamp > time_range->second) {
                    matches = false;
                }
            }

            // Check tags
            for (const auto& [key, value] : query.get_tags()) {
                auto it = metric.tags.find(key);
                if (it == metric.tags.end() || it->second != value) {
                    matches = false;
                    break;
                }
            }

            if (matches) {
                results.push_back(metric);
            }
        }
    }

    return results;
}

std::vector<PrettificationEvent> MockTimeSeriesDB::query_events(const TSDBQueryBuilder& query) {
    std::vector<PrettificationEvent> results;
    std::lock_guard<std::mutex> lock(mock_mutex_);

    for (const auto& event : events_) {
        // Apply filters based on query builder settings
        bool matches = true;

        // Check time range
        const auto& time_range = query.get_time_range();
        if (time_range) {
            if (event.timestamp < time_range->first ||
                event.timestamp > time_range->second) {
                matches = false;
            }
        }

        if (matches) {
            results.push_back(event);
        }
    }

    return results;
}

std::vector<MetricStatistics> MockTimeSeriesDB::query_aggregations(
    const TSDBQueryBuilder& query,
    const std::vector<std::string>& /*aggregations*/) {
    std::vector<MetricStatistics> results;

    // Mock implementation
    MetricStatistics stats;
    stats.name = query.get_measurement();
    stats.count = 100;
    stats.mean = 50.0;
    stats.min = 10.0;
    stats.max = 100.0;
    results.push_back(stats);

    return results;
}

std::vector<std::string> MockTimeSeriesDB::list_retention_policies() {
    std::lock_guard<std::mutex> lock(mock_mutex_);
    return retention_policies_;
}

std::vector<std::string> MockTimeSeriesDB::list_continuous_queries() {
    return {};
}

nlohmann::json MockTimeSeriesDB::get_status() const {
    nlohmann::json status;
    status["connected"] = connected_.load();
    status["metrics_count"] = metrics_.size();
    status["events_count"] = events_.size();
    status["databases"] = databases_;
    return status;
}

void MockTimeSeriesDB::clear_data() {
    std::lock_guard<std::mutex> lock(mock_mutex_);
    metrics_.clear();
    events_.clear();
}

// TimeSeriesDBFactory implementation
std::unordered_map<std::string, TimeSeriesDBFactory::BackendFactory> TimeSeriesDBFactory::backends_;

std::unique_ptr<TimeSeriesDB> TimeSeriesDBFactory::create(const std::string& backend_type,
                                                         const TSDBConfig& config) {
    if (backend_type == "influxdb2") {
        return std::make_unique<InfluxDB2Client>(config);
    } else if (backend_type == "mock") {
        return std::make_unique<MockTimeSeriesDB>(config);
    }

    auto it = backends_.find(backend_type);
    if (it != backends_.end()) {
        return it->second(config);
    }

    throw std::invalid_argument("Unknown time-series database backend: " + backend_type);
}

void TimeSeriesDBFactory::register_backend(const std::string& name, BackendFactory factory) {
    backends_[name] = factory;
}

std::vector<std::string> TimeSeriesDBFactory::list_available_backends() {
    std::vector<std::string> result = {"influxdb2", "mock"};
    for (const auto& [name, factory] : backends_) {
        result.push_back(name);
    }
    return result;
}

} // namespace metrics
} // namespace aimux