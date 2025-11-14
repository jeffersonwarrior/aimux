#ifndef AIMUX_METRICS_H
#define AIMUX_METRICS_H

#include <string>
#include <map>
#include <vector>
#include <memory>
#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <nlohmann/json.hpp>

namespace aimux {
namespace monitoring {

/**
 * System metrics and monitoring for production deployments
 * Tracks performance, availability, and operational metrics
 */

// Metric types
enum class MetricType {
    COUNTER,      // Cumulative counter
    GAUGE,        // Current value
    HISTOGRAM,    // Distribution of values
    TIMER         // Duration measurements
};

// Data point structure
struct DataPoint {
    double value;
    std::chrono::system_clock::time_point timestamp;
    std::map<std::string, std::string> tags;

    DataPoint(double val, const std::map<std::string, std::string>& t = {})
        : value(val), timestamp(std::chrono::system_clock::now()), tags(t) {}
};

// Histogram buckets
struct HistogramBuckets {
    std::vector<double> boundaries;
    std::vector<uint64_t> counts;

    HistogramBuckets(const std::vector<double>& bounds = {1.0, 5.0, 10.0, 50.0, 100.0, 500.0, 1000.0})
        : boundaries(bounds), counts(bounds.size(), 0) {}
};

// Base metric interface
class Metric {
public:
    virtual ~Metric() = default;
    virtual nlohmann::json toJson() const = 0;
    virtual void reset() = 0;
    virtual MetricType getType() const = 0;
    virtual const std::string& getName() const = 0;

protected:
    std::string name_;
    std::map<std::string, std::string> tags_;
};

// Counter metric
class Counter : public Metric {
public:
    Counter(const std::string& name, const std::map<std::string, std::string>& tags = {});

    void increment(double delta = 1.0);
    double getValue() const;
    nlohmann::json toJson() const override;
    void reset() override;
    MetricType getType() const override { return MetricType::COUNTER; }
    const std::string& getName() const override;

private:
    std::atomic<double> value_{0.0};
};

// Gauge metric
class Gauge : public Metric {
public:
    Gauge(const std::string& name, const std::map<std::string, std::string>& tags = {});

    void set(double value);
    void increment(double delta = 1.0);
    void decrement(double delta = 1.0);
    double getValue() const;
    nlohmann::json toJson() const override;
    void reset() override;
    MetricType getType() const override { return MetricType::GAUGE; }
    const std::string& getName() const override;

private:
    std::atomic<double> value_{0.0};
};

// Histogram metric
class Histogram : public Metric {
public:
    Histogram(const std::string& name, const std::vector<double>& buckets = {},
              const std::map<std::string, std::string>& tags = {});

    void observe(double value);
    double getSum() const;
    uint64_t getCount() const;
    std::vector<double> getBuckets() const;
    std::vector<uint64_t> getBucketCounts() const;
    nlohmann::json toJson() const override;
    void reset() override;
    MetricType getType() const override { return MetricType::HISTOGRAM; }
    const std::string& getName() const override;

private:
    std::atomic<uint64_t> count_{0};
    std::atomic<double> sum_{0.0};
    HistogramBuckets buckets_;
    mutable std::mutex mutex_;
};

// Timer metric
class Timer : public Metric {
public:
    Timer(const std::string& name, const std::map<std::string, std::string>& tags = {});

    void observe(double durationMs);
    std::chrono::milliseconds timeFunction(const std::function<void()>& func);
    double getAverageTime() const;
    double getMinTime() const;
    double getMaxTime() const;
    uint64_t getCount() const;
    nlohmann::json toJson() const override;
    void reset() override;
    MetricType getType() const override { return MetricType::TIMER; }
    const std::string& getName() const override;

private:
    std::atomic<uint64_t> count_{0};
    std::atomic<double> sum_{0.0};
    std::atomic<double> min_{std::numeric_limits<double>::infinity()};
    std::atomic<double> max_{0.0};
};

// Metrics registry
class MetricsRegistry {
public:
    static MetricsRegistry& getInstance();

    // Metric registration
    std::shared_ptr<Counter> registerCounter(const std::string& name, const std::map<std::string, std::string>& tags = {});
    std::shared_ptr<Gauge> registerGauge(const std::string& name, const std::map<std::string, std::string>& tags = {});
    std::shared_ptr<Histogram> registerHistogram(const std::string& name, const std::vector<double>& buckets = {},
                                                 const std::map<std::string, std::string>& tags = {});
    std::shared_ptr<Timer> registerTimer(const std::string& name, const std::map<std::string, std::string>& tags = {});

    // Metric retrieval
    std::shared_ptr<Metric> getMetric(const std::string& name);
    std::vector<std::shared_ptr<Metric>> getAllMetrics();

    // Export functionality
    nlohmann::json exportToJson() const;
    std::string exportToPrometheus() const;

    // Management
    void resetAllMetrics();
    void cleanup();

private:
    MetricsRegistry() = default;
    ~MetricsRegistry() = default;
    MetricsRegistry(const MetricsRegistry&) = delete;
    MetricsRegistry& operator=(const MetricsRegistry&) = delete;

    std::map<std::string, std::shared_ptr<Metric>> metrics_;
    mutable std::shared_mutex mutex_;
};

// System metrics collector
class SystemMetricsCollector {
public:
    SystemMetricsCollector();
    ~SystemMetricsCollector();

    void startCollection(std::chrono::seconds interval = std::chrono::seconds(30));
    void stopCollection();
    nlohmann::json getCurrentMetrics() const;

    // Specific system metrics
    struct SystemMetrics {
        double cpu_usage_percent;
        double memory_usage_mb;
        double memory_usage_percent;
        uint64_t disk_used_mb;
        uint64_t disk_total_mb;
        uint64_t network_bytes_sent;
        uint64_t network_bytes_received;
        uint32_t process_count;
        uint32_t thread_count;
        double load_average_1m;
        double load_average_5m;
        double load_average_15m;
        std::chrono::system_clock::time_point timestamp;
    };

private:
    void collectLoop();
    SystemMetrics collectSystemMetrics() const;

    std::atomic<bool> running_{false};
    std::thread collectorThread_;
    std::chrono::seconds collectionInterval_;
    SystemMetrics currentMetrics_;
    mutable std::mutex metricsMutex_;
};

// Health check system
class HealthChecker {
public:
    struct HealthStatus {
        bool healthy;
        std::string status;
        std::map<std::string, bool> checks;
        std::chrono::system_clock::time_point timestamp;

        nlohmann::json toJson() const {
            nlohmann::json j;
            j["healthy"] = healthy;
            j["status"] = status;
            j["checks"] = checks;
            j["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
                timestamp.time_since_epoch()).count();
            return j;
        }
    };

    static HealthChecker& getInstance();

    void registerCheck(const std::string& name, std::function<bool()> checker);
    void unregisterCheck(const std::string& name);
    HealthStatus checkHealth() const;

private:
    HealthChecker() = default;
    ~HealthChecker() = default;
    HealthChecker(const HealthChecker&) = delete;
    HealthChecker& operator=(const HealthChecker&) = delete;

    std::map<std::string, std::function<bool()>> checks_;
    mutable std::shared_mutex mutex_;
};

// Alerting system
class AlertManager {
public:
    struct Alert {
        std::string id;
        std::string name;
        std::string severity; // "critical", "warning", "info"
        std::string message;
        std::chrono::system_clock::time_point timestamp;
        bool resolved;
        std::map<std::string, std::string> labels;

        nlohmann::json toJson() const {
            nlohmann::json j;
            j["id"] = id;
            j["name"] = name;
            j["severity"] = severity;
            j["message"] = message;
            j["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
                timestamp.time_since_epoch()).count();
            j["resolved"] = resolved;
            j["labels"] = labels;
            return j;
        }
    };

    enum class Severity { CRITICAL, WARNING, INFO };

    static AlertManager& getInstance();

    void raiseAlert(const std::string& name, const std::string& message,
                   Severity severity = Severity::WARNING,
                   const std::map<std::string, std::string>& labels = {});
    void resolveAlert(const std::string& id);
    std::vector<Alert> getActiveAlerts() const;
    std::vector<Alert> getAllAlerts() const;
    void clearResolved(std::chrono::hours olderThan = std::chrono::hours(24));

private:
    AlertManager() = default;
    ~AlertManager() = default;
    AlertManager(const AlertManager&) = delete;
    AlertManager& operator=(const AlertManager&) = delete;

    std::string generateAlertId() const;
    std::string severityToString(Severity severity) const;

    std::map<std::string, Alert> alerts_;
    mutable std::shared_mutex mutex_;
};

// Utility macros for easy metric usage
#define AIMUX_COUNTER(name) \
    aimux::monitoring::MetricsRegistry::getInstance().registerCounter(name)

#define AIMUX_GAUGE(name) \
    aimux::monitoring::MetricsRegistry::getInstance().registerGauge(name)

#define AIMUX_HISTOGRAM(name, buckets) \
    aimux::monitoring::MetricsRegistry::getInstance().registerHistogram(name, buckets)

#define AIMUX_TIMER(name) \
    aimux::monitoring::MetricsRegistry::getInstance().registerTimer(name)

#define AIMUX_TIMED_BLOCK(timer_name) \
    auto _timer_start = std::chrono::high_resolution_clock::now(); \
    auto _timer = AIMUX_TIMER(timer_name); \
    (void)_timer; // suppress unused variable warning

#define AIMUX_TIMED_BLOCK_END() \
    do { \
        auto _timer_end = std::chrono::high_resolution_clock::now(); \
        auto _duration = std::chrono::duration_cast<std::chrono::milliseconds>( \
            _timer_end - _timer_start).count(); \
        _timer->observe(_duration); \
    } while(0)

} // namespace monitoring
} // namespace aimux

#endif // AIMUX_METRICS_H