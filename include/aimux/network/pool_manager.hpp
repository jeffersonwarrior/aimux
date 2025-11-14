#pragma once

#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <unordered_map>
#include <thread>
#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <nlohmann/json.hpp>

// Forward declarations for curl
struct CURL;
struct curl_slist;

namespace aimux {
namespace network {

/**
 * @brief Pooled connection with metadata
 */
class PooledConnection {
public:
    explicit PooledConnection(const std::string& host);
    ~PooledConnection();

    // Non-copyable but movable
    PooledConnection(const PooledConnection&) = delete;
    PooledConnection& operator=(const PooledConnection&) = delete;
    PooledConnection(PooledConnection&&) = default;
    PooledConnection& operator=(PooledConnection&&) = default;

    // Connection operations
    bool connect();
    void disconnect();
    bool isConnected() const;

    // HTTP operations
    struct Response {
        int status_code = 0;
        std::string body;
        std::unordered_map<std::string, std::string> headers;
        std::chrono::milliseconds response_time{0};
        bool success = false;
    };

    Response performRequest(
        const std::string& method,
        const std::string& url,
        const std::string& body = "",
        const std::unordered_map<std::string, std::string>& headers = {}
    );

    // Connection metadata
    std::chrono::steady_clock::time_point last_used;
    std::chrono::steady_clock::time_point created_at;
    size_t request_count = 0;
    bool is_healthy = true;

private:
    CURL* curl_;
    std::string host_;
    std::string error_buffer_;

    void resetConnection();
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp);
    static size_t HeaderCallback(void* contents, size_t size, size_t nmemb, void* userp);
};

/**
 * @brief Connection pool configuration
 */
struct PoolConfig {
    size_t min_connections = 2;
    size_t max_connections = 20;
    std::chrono::seconds connection_timeout{30};
    std::chrono::seconds request_timeout{60};
    std::chrono::seconds idle_timeout{300}; // 5 minutes
    std::chrono::seconds health_check_interval{30};
    size_t max_request_count_per_connection = 1000;
    bool enable_keepalive = true;
    bool enable_compression = true;
    double health_check_failure_threshold = 0.5;
};

/**
 * @brief Advanced connection pool manager
 */
class PoolManager {
public:
    explicit PoolManager(const PoolConfig& config = PoolConfig{});
    ~PoolManager();

    // Non-copyable but movable
    PoolManager(const PoolManager&) = delete;
    PoolManager& operator=(const PoolManager&) = delete;
    PoolManager(PoolManager&&) = delete;
    PoolManager& operator=(PoolManager&&) = delete;

    /**
     * @brief Get connection from pool
     */
    std::unique_ptr<PooledConnection> getConnection(const std::string& host);

    /**
     * @brief Return connection to pool
     */
    void returnConnection(const std::string& host, std::unique_ptr<PooledConnection> conn);

    /**
     * @brief Execute request with automatic connection management
     */
    PooledConnection::Response executeRequest(
        const std::string& method,
        const std::string& url,
        const std::string& body = "",
        const std::unordered_map<std::string, std::string>& headers = {}
    );

    // Pool management
    void start();
    void stop();
    void cleanup();
    void healthCheck();

    // Statistics
    struct PoolStats {
        size_t total_connections = 0;
        size_t active_connections = 0;
        size_t idle_connections = 0;
        size_t failed_connections = 0;
        size_t total_requests = 0;
        std::chrono::milliseconds avg_response_time{0};
        double success_rate = 0.0;
        std::chrono::milliseconds uptime{0};
    };

    PoolStats getStats(const std::string& host = "") const;
    void resetStats();

    // Configuration
    void updateConfig(const PoolConfig& config);
    PoolConfig getConfig() const { return config_; }

private:
    struct ConnectionPool {
        std::queue<std::unique_ptr<PooledConnection>> available;
        std::queue<std::unique_ptr<PooledConnection>> in_use;
        mutable std::mutex mutex;
        std::condition_variable condition;

        // Statistics
        std::atomic<size_t> created_count_{0};
        std::atomic<size_t> destroyed_count_{0};
        std::atomic<size_t> request_count_{0};
        std::atomic<size_t> success_count_{0};
        std::chrono::steady_clock::time_point last_health_check;
        std::atomic<bool> is_healthy_{true};
    };

    PoolConfig config_;
    std::unordered_map<std::string, std::unique_ptr<ConnectionPool>> pools_;
    mutable std::mutex pools_mutex_;

    // Background tasks
    std::atomic<bool> running_{false};
    std::thread cleanup_thread_;
    std::thread health_check_thread_;

    // Global statistics
    std::chrono::steady_clock::time_point start_time_;
    mutable std::atomic<size_t> global_requests_{0};
    mutable std::atomic<size_t> global_successes_{0};

    // Internal methods
    ConnectionPool& getOrCreatePool(const std::string& host);
    std::unique_ptr<PooledConnection> createConnection(const std::string& host);
    void cleanupTask();
    void healthCheckTask();
    void performPoolHealthCheck(ConnectionPool& pool, const std::string& host);
    std::string extractHost(const std::string& url) const;
};

/**
 * @brief Smart connection factory
 */
class ConnectionFactory {
public:
    static std::unique_ptr<PooledConnection> create(
        const std::string& host,
        const PoolConfig& config
    );

    // Default headers for all requests
    static std::unordered_map<std::string, std::string> getDefaultHeaders();

private:
    static void configureCurlSSL(CURL* curl);
    static void configureCurlTimeouts(CURL* curl, const PoolConfig& config);
};

/**
 * @brief Request builder for convenient HTTP operations
 */
class RequestBuilder {
public:
    explicit RequestBuilder(PoolManager& manager) : manager_(manager) {}

    RequestBuilder& method(const std::string& method);
    RequestBuilder& url(const std::string& url);
    RequestBuilder& body(const std::string& body);
    RequestBuilder& body(const nlohmann::json& json);
    RequestBuilder& header(const std::string& key, const std::string& value);
    RequestBuilder& timeout(std::chrono::milliseconds timeout);

    PooledConnection::Response execute();
    std::future<PooledConnection::Response> executeAsync();

private:
    PoolManager& manager_;
    std::string method_;
    std::string url_;
    std::string body_;
    std::unordered_map<std::string, std::string> headers_;
    std::optional<std::chrono::milliseconds> timeout_;
};

/**
 * @brief Circuit breaker for failing hosts
 */
class CircuitBreaker {
public:
    struct Config {
        int failure_threshold = 5;
        std::chrono::seconds recovery_timeout{60};
        int success_threshold = 3;
        double expected_success_rate = 0.8;
    };

    explicit CircuitBreaker(const Config& config = Config());

    enum class State {
        CLOSED,    // Normal operation
        OPEN,      // Failing, reject requests
        HALF_OPEN  // Testing recovery
    };

    bool canExecute() const;
    void recordSuccess();
    void recordFailure();
    State getState() const { return state_; }

private:
    Config config_;
    State state_{State::CLOSED};
    int failure_count_ = 0;
    int success_count_ = 0;
    std::chrono::steady_clock::time_point last_failure_time_;
    mutable std::mutex mutex_;

    void setState(State new_state);
};

} // namespace network
} // namespace aimux