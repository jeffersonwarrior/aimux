#pragma once

#include "metrics_collector.hpp"
#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <queue>
#include <nlohmann/json.hpp>

namespace aimux {
namespace metrics {

/**
 * @brief Time-series database connection configuration
 */
struct TSDBConfig {
    std::string host = "localhost";
    int port = 8086;
    std::string database = "aimux_metrics";
    std::string username;
    std::string password;
    std::string token;                    // For token-based auth
    std::string organization = "aimux";
    std::string bucket = "prettification";
    bool enable_ssl = false;
    bool enable_compression = true;
    std::chrono::seconds connection_timeout{30};
    std::chrono::seconds query_timeout{60};
    size_t max_batch_size = 1000;
    std::chrono::milliseconds flush_interval{1000};
    size_t max_retries = 3;
    std::chrono::seconds retry_delay{5};
};

/**
 * @brief Query builder for time-series databases
 */
class TSDBQueryBuilder {
public:
    explicit TSDBQueryBuilder(const std::string& measurement);

    TSDBQueryBuilder& time_range(
        const std::chrono::system_clock::time_point& start,
        const std::chrono::system_clock::time_point& end);

    TSDBQueryBuilder& tag(const std::string& key, const std::string& value);
    TSDBQueryBuilder& tags(const std::unordered_map<std::string, std::string>& tags);
    TSDBQueryBuilder& field(const std::string& name);
    TSDBQueryBuilder& fields(const std::vector<std::string>& names);
    TSDBQueryBuilder& group_by(const std::vector<std::string>& tags);
    TSDBQueryBuilder& fill(const std::string& fill_type);
    TSDBQueryBuilder& limit(size_t count);
    TSDBQueryBuilder& order_by(const std::string& field, const std::string& direction = "desc");

    std::string build_query() const;

    // Getter methods for accessing private members
    const std::string& get_measurement() const { return measurement_; }
    const std::optional<std::pair<std::chrono::system_clock::time_point, std::chrono::system_clock::time_point>>& get_time_range() const { return time_range_; }
    const std::unordered_map<std::string, std::string>& get_tags() const { return tags_; }

private:
    std::string measurement_;
    std::optional<std::pair<std::chrono::system_clock::time_point, std::chrono::system_clock::time_point>> time_range_;
    std::unordered_map<std::string, std::string> tags_;
    std::vector<std::string> fields_;
    std::vector<std::string> group_by_;
    std::string fill_type_;
    std::optional<size_t> limit_;
    std::optional<std::pair<std::string, std::string>> order_by_;
};

/**
 * @brief Abstract time-series database interface
 *
 * Provides a unified interface for different time-series database backends.
 * Supports both InfluxDB and custom implementations.
 */
class TimeSeriesDB {
public:
    explicit TimeSeriesDB(const TSDBConfig& config);
    virtual ~TimeSeriesDB();

    // Connection management
    virtual bool connect() = 0;
    virtual bool disconnect() = 0;
    virtual bool is_connected() const = 0;
    virtual bool ping() = 0;

    // Database operations
    virtual bool create_database(const std::string& name) = 0;
    virtual bool drop_database(const std::string& name) = 0;
    virtual std::vector<std::string> list_databases() = 0;

    // Write operations
    virtual bool write_metrics(const std::vector<MetricPoint>& metrics) = 0;
    virtual bool write_events(const std::vector<PrettificationEvent>& events) = 0;

    // Async write operations
    void write_metrics_async(const std::vector<MetricPoint>& metrics);
    void write_events_async(const std::vector<PrettificationEvent>& events);

    // Query operations
    virtual std::vector<MetricPoint> query_metrics(const TSDBQueryBuilder& query) = 0;
    virtual std::vector<PrettificationEvent> query_events(const TSDBQueryBuilder& query) = 0;

    // Aggregation queries
    virtual std::vector<MetricStatistics> query_aggregations(
        const TSDBQueryBuilder& query,
        const std::vector<std::string>& aggregations = {"mean", "count"}) = 0;

    // Retention policy management
    virtual bool create_retention_policy(
        const std::string& name,
        const std::chrono::hours& duration,
        int replication_factor = 1,
        bool default_policy = false) = 0;

    virtual bool drop_retention_policy(const std::string& name) = 0;
    virtual std::vector<std::string> list_retention_policies() = 0;

    // Continuous queries (for real-time aggregation)
    virtual bool create_continuous_query(
        const std::string& name,
        const std::string& query) = 0;

    virtual bool drop_continuous_query(const std::string& name) = 0;
    virtual std::vector<std::string> list_continuous_queries() = 0;

    // Configuration and status
    const TSDBConfig& get_config() const { return config_; }
    virtual nlohmann::json get_status() const = 0;
    virtual double get_query_performance_ms() const = 0;

protected:
    TSDBConfig config_;
    std::atomic<bool> connected_{false};

    // Async processing
    struct AsyncWriteRequest {
        enum Type { METRICS, EVENTS };
        Type type;
        std::vector<MetricPoint> metrics;
        std::vector<PrettificationEvent> events;
        std::function<void(bool)> callback;
    };

    std::queue<AsyncWriteRequest> async_write_queue_;
    std::mutex async_mutex_;
    std::condition_variable async_cv_;
    std::thread async_worker_;
    std::atomic<bool> should_stop_async_{false};

    void process_async_writes();
    virtual bool write_metrics_sync(const std::vector<MetricPoint>& metrics) = 0;
    virtual bool write_events_sync(const std::vector<PrettificationEvent>& events) = 0;

private:
    void start_async_worker();
    void stop_async_worker();
};

/**
 * @brief InfluxDB 2.x implementation of TimeSeriesDB
 */
class InfluxDB2Client : public TimeSeriesDB {
public:
    explicit InfluxDB2Client(const TSDBConfig& config);
    ~InfluxDB2Client() override;

    bool connect() override;
    bool disconnect() override;
    bool is_connected() const override;
    bool ping() override;

    bool create_database(const std::string& name) override;
    bool drop_database(const std::string& name) override;
    std::vector<std::string> list_databases() override;

    bool write_metrics(const std::vector<MetricPoint>& metrics) override;
    bool write_events(const std::vector<PrettificationEvent>& events) override;

    std::vector<MetricPoint> query_metrics(const TSDBQueryBuilder& query) override;
    std::vector<PrettificationEvent> query_events(const TSDBQueryBuilder& query) override;
    std::vector<MetricStatistics> query_aggregations(
        const TSDBQueryBuilder& query,
        const std::vector<std::string>& aggregations) override;

    // InfluxDB 2.x specific bucket operations
    bool create_bucket(const std::string& name, const std::chrono::hours& retention = std::chrono::hours{24 * 30});
    bool delete_bucket(const std::string& name);
    std::vector<std::string> list_buckets();

    nlohmann::json get_status() const override;
    double get_query_performance_ms() const override;

    // Retention policy management (InfluxDB 2.x uses bucket retention)
    bool create_retention_policy(
        const std::string& name,
        const std::chrono::hours& duration,
        int replication_factor = 1,
        bool default_policy = false) override;
    bool drop_retention_policy(const std::string& name) override;
    std::vector<std::string> list_retention_policies() override;

    // Continuous queries (InfluxDB 2.x uses tasks)
    bool create_continuous_query(const std::string& name, const std::string& query) override;
    bool drop_continuous_query(const std::string& name) override;
    std::vector<std::string> list_continuous_queries() override;

protected:
    bool write_metrics_sync(const std::vector<MetricPoint>& metrics) override;
    bool write_events_sync(const std::vector<PrettificationEvent>& events) override;

private:
    // Internal HTTP client methods
    bool http_request(const std::string& method,
                     const std::string& endpoint,
                     const std::string& body,
                     nlohmann::json& response,
                     std::string& error_msg);

    std::string build_write_url() const;
    std::string build_query_url() const;
    std::string format_metrics_for_influx(const std::vector<MetricPoint>& metrics) const;
    std::string format_events_for_influx(const std::vector<PrettificationEvent>& events) const;

    std::string parse_line_protocol(const std::string& line, MetricPoint& point) const;
    MetricPoint parse_influx_point(const nlohmann::json& json_point) const;
    PrettificationEvent parse_influx_event(const nlohmann::json& json_event) const;

    // Authentication and session
    std::string auth_token_;
    std::string session_token_;
    std::chrono::system_clock::time_point token_expiry_;

    bool authenticate();
    bool refresh_token();

    // Performance tracking
    mutable std::atomic<double> last_query_time_ms_{0.0};
    mutable std::mutex performance_mutex_;
};

/**
 * @brief Mock time-series database for testing
 */
class MockTimeSeriesDB : public TimeSeriesDB {
public:
    explicit MockTimeSeriesDB(const TSDBConfig& config = TSDBConfig{});

    bool connect() override { connected_ = true; return true; }
    bool disconnect() override { connected_ = false; return true; }
    bool is_connected() const override { return connected_; }
    bool ping() override { return connected_; }

    bool create_database(const std::string& /*name*/) override { return true; }
    bool drop_database(const std::string& /*name*/) override { return true; }
    std::vector<std::string> list_databases() override;

    bool write_metrics(const std::vector<MetricPoint>& metrics) override;
    bool write_events(const std::vector<PrettificationEvent>& events) override;

    std::vector<MetricPoint> query_metrics(const TSDBQueryBuilder& query) override;
    std::vector<PrettificationEvent> query_events(const TSDBQueryBuilder& query) override;
    std::vector<MetricStatistics> query_aggregations(
        const TSDBQueryBuilder& query,
        const std::vector<std::string>& aggregations) override;

    bool create_retention_policy(const std::string& /*name*/,
                                const std::chrono::hours& /*duration*/,
                                int /*replication_factor*/,
                                bool /*default_policy*/) override { return true; }
    bool drop_retention_policy(const std::string& /*name*/) override { return true; }
    std::vector<std::string> list_retention_policies() override;

    bool create_continuous_query(const std::string& /*name*/, const std::string& /*query*/) override { return true; }
    bool drop_continuous_query(const std::string& /*name*/) override { return true; }
    std::vector<std::string> list_continuous_queries() override;

    nlohmann::json get_status() const override;
    double get_query_performance_ms() const override { return 0.1; }

    // Mock access methods
    const std::vector<MetricPoint>& get_metrics() const { return metrics_; }
    const std::vector<PrettificationEvent>& get_events() const { return events_; }
    void clear_data();

private:
    bool write_metrics_sync(const std::vector<MetricPoint>& /*metrics*/) override { return true; }
    bool write_events_sync(const std::vector<PrettificationEvent>& /*events*/) override { return true; }

    std::vector<MetricPoint> metrics_;
    std::vector<PrettificationEvent> events_;
    std::vector<std::string> databases_;
    std::vector<std::string> retention_policies_;
    mutable std::mutex mock_mutex_;
};

/**
 * @brief Factory for creating time-series database instances
 */
class TimeSeriesDBFactory {
public:
    static std::unique_ptr<TimeSeriesDB> create(const std::string& backend_type,
                                               const TSDBConfig& config);

    // Register custom backends
    using BackendFactory = std::function<std::unique_ptr<TimeSeriesDB>(const TSDBConfig&)>;
    static void register_backend(const std::string& name, BackendFactory factory);

    static std::vector<std::string> list_available_backends();

private:
    static std::unordered_map<std::string, BackendFactory> backends_;
};

} // namespace metrics
} // namespace aimux