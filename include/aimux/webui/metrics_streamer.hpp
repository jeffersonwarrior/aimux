#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <chrono>
#include <memory>
#include <queue>
#include <deque>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <crow.h>
#include <nlohmann/json.hpp>

namespace aimux {
namespace webui {

/**
 * @brief Advanced provider metrics with detailed statistics
 */
struct ProviderMetrics {
    std::string name;
    std::string status;  // healthy, degraded, unhealthy, offline

    // Request metrics
    uint64_t requests_last_minute = 0;
    uint64_t requests_last_hour = 0;
    uint64_t total_requests = 0;
    double requests_per_second = 0.0;

    // Response time metrics (milliseconds)
    double avg_response_time_ms = 0.0;
    double p50_response_time_ms = 0.0;
    double p95_response_time_ms = 0.0;
    double p99_response_time_ms = 0.0;
    std::chrono::steady_clock::time_point last_request_time;

    // Success and error metrics
    double success_rate = 100.0;
    std::unordered_map<std::string, uint64_t> error_breakdown;  // rate_limit, network, auth, server_error, etc.

    // Cost and token metrics
    double cost_per_hour = 0.0;
    double total_cost = 0.0;
    struct TokenUsage {
        uint64_t input = 0;
        uint64_t output = 0;
        uint64_t total = 0;
    } tokens_used;

    // Rate limiting
    bool rate_limited = false;
    std::chrono::steady_clock::time_point rate_limit_until;

    nlohmann::json to_json() const {
        nlohmann::json j;
        j["name"] = name;
        j["status"] = status;
        j["requests_last_min"] = requests_last_minute;
        j["requests_last_hour"] = requests_last_hour;
        j["total_requests"] = total_requests;
        j["requests_per_second"] = requests_per_second;
        j["avg_response_time_ms"] = avg_response_time_ms;
        j["p50_response_time_ms"] = p50_response_time_ms;
        j["p95_response_time_ms"] = p95_response_time_ms;
        j["p99_response_time_ms"] = p99_response_time_ms;

        auto now = std::chrono::steady_clock::now();
        if (last_request_time != std::chrono::steady_clock::time_point{}) {
            auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - last_request_time);
            j["last_request"] = std::to_string(duration.count()) + "s ago";
        } else {
            j["last_request"] = "never";
        }

        j["success_rate"] = success_rate;
        j["error_breakdown"] = error_breakdown;
        j["cost_per_hour"] = cost_per_hour;
        j["total_cost"] = total_cost;
        j["tokens_used"] = {
            {"input", tokens_used.input},
            {"output", tokens_used.output},
            {"total", tokens_used.total}
        };
        j["rate_limited"] = rate_limited;

        return j;
    }
};

/**
 * @brief System performance metrics
 */
struct SystemMetrics {
    // CPU metrics
    struct CPU {
        double current_percent = 0.0;
        double avg_1min = 0.0;
        double avg_5min = 0.0;
        double avg_15min = 0.0;
        uint32_t cores = 0;
        std::string load;  // light, moderate, heavy
    } cpu;

    // Memory metrics
    struct Memory {
        uint64_t used_mb = 0;
        uint64_t available_mb = 0;
        uint64_t total_mb = 0;
        double percent = 0.0;
        double usage_trend = 0.0;  // bytes per second
    } memory;

    // Network metrics
    struct Network {
        uint32_t connections = 0;
        uint64_t bytes_sent = 0;
        uint64_t bytes_received = 0;
        double bytes_per_sec_sent = 0.0;
        double bytes_per_sec_received = 0.0;
        std::chrono::steady_clock::time_point last_activity;
    } network;

    // Disk metrics
    struct Disk {
        uint64_t used_mb = 0;
        uint64_t available_mb = 0;
        uint64_t total_mb = 0;
        double percent = 0.0;
        double read_throughput = 0.0;  // MB/s
        double write_throughput = 0.0; // MB/s
    } disk;

    // General system metrics
    uint64_t uptime_seconds = 0;
    double requests_per_second = 0.0;
    std::chrono::steady_clock::time_point start_time;

    nlohmann::json to_json() const {
        nlohmann::json j;

        j["cpu"] = {
            {"current", cpu.current_percent},
            {"avg_1min", cpu.avg_1min},
            {"avg_5min", cpu.avg_5min},
            {"avg_15min", cpu.avg_15min},
            {"cores", cpu.cores}
        };

        j["memory"] = {
            {"used_mb", memory.used_mb},
            {"available_mb", memory.available_mb},
            {"total_mb", memory.total_mb},
            {"percent", memory.percent},
            {"usage_trend", memory.usage_trend}
        };

        j["network"] = {
            {"connections", network.connections},
            {"bytes_sent", network.bytes_sent},
            {"bytes_received", network.bytes_received},
            {"bytes_per_sec_sent", network.bytes_per_sec_sent},
            {"bytes_per_sec_received", network.bytes_per_sec_received}
        };

        if (disk.used_mb > 0) {
            j["disk"] = {
                {"used_mb", disk.used_mb},
                {"available_mb", disk.available_mb},
                {"total_mb", disk.total_mb},
                {"percent", disk.percent},
                {"read_throughput", disk.read_throughput},
                {"write_throughput", disk.write_throughput}
            };
        }

        j["uptime"] = uptime_seconds;
        j["requests_per_second"] = requests_per_second;

        return j;
    }
};

/**
 * @brief Historical data points for trend analysis
 */
struct HistoricalData {
    std::deque<double> response_times;      // Last 60 points
    std::deque<double> success_rates;       // Last 60 points
    std::deque<uint64_t> requests_per_min;  // Last 60 points
    std::deque<double> cpu_usage;           // Last 60 points
    std::deque<double> memory_usage;        // Last 60 points

    static constexpr size_t MAX_HISTORY_POINTS = 60;

    void add_response_time(double time_ms) {
        response_times.push_back(time_ms);
        if (response_times.size() > MAX_HISTORY_POINTS) {
            response_times.pop_front();
        }
    }

    void add_success_rate(double rate) {
        success_rates.push_back(rate);
        if (success_rates.size() > MAX_HISTORY_POINTS) {
            success_rates.pop_front();
        }
    }

    void add_requests_per_min(uint64_t rpm) {
        requests_per_min.push_back(rpm);
        if (requests_per_min.size() > MAX_HISTORY_POINTS) {
            requests_per_min.pop_front();
        }
    }

    void add_cpu_usage(double percent) {
        cpu_usage.push_back(percent);
        if (cpu_usage.size() > MAX_HISTORY_POINTS) {
            cpu_usage.pop_front();
        }
    }

    void add_memory_usage(double percent) {
        memory_usage.push_back(percent);
        if (memory_usage.size() > MAX_HISTORY_POINTS) {
            memory_usage.pop_front();
        }
    }

    nlohmann::json to_json() const {
        nlohmann::json j;
        j["response_times"] = std::vector<double>(response_times.begin(), response_times.end());
        j["success_rates"] = std::vector<double>(success_rates.begin(), success_rates.end());
        j["requests_per_min"] = std::vector<uint64_t>(requests_per_min.begin(), requests_per_min.end());
        j["cpu_usage"] = std::vector<double>(cpu_usage.begin(), cpu_usage.end());
        j["memory_usage"] = std::vector<double>(memory_usage.begin(), memory_usage.end());
        return j;
    }
};

/**
 * @brief WebSocket connection information
 */
struct WebSocketConnection {
    crow::websocket::connection* connection;
    std::string connection_id;
    std::chrono::steady_clock::time_point connect_time;
    std::chrono::steady_clock::time_point last_ping;
    uint64_t messages_sent = 0;
    uint64_t messages_received = 0;
    bool authenticated = false;
    std::string client_info;

    WebSocketConnection(crow::websocket::connection* conn, const std::string& id)
        : connection(conn), connection_id(id), connect_time(std::chrono::steady_clock::now()),
          last_ping(std::chrono::steady_clock::now()) {}
};

/**
 * @brief Configuration for MetricsStreamer
 */
struct MetricsStreamerConfig {
    uint32_t update_interval_ms = 1000;         // Data collection interval
    uint32_t broadcast_interval_ms = 2000;      // WebSocket broadcast interval
    uint32_t max_connections = 100;             // Max concurrent WebSocket connections
    uint32_t connection_timeout_ms = 30000;     // Connection timeout
    uint32_t max_message_queue_size = 1000;     // Max queued messages per connection
    bool enable_delta_compression = true;       // Use delta compression for updates
    bool enable_authentication = false;         // Require WebSocket authentication
    std::string auth_token = "";               // Authentication token
    uint32_t history_retention_minutes = 60;    // Historical data retention
    bool enable_performance_monitoring = true;  // Track internal performance
};

/**
 * @brief Professional real-time metrics streaming system
 *
 * This class provides comprehensive real-time monitoring with:
 * - Thread-safe metrics collection and aggregation
 * - Advanced WebSocket connection management
 * - Historical data buffering for trend analysis
 * - Configurable performance optimization
 * - Professional error handling and recovery
 */
class MetricsStreamer {
public:
    /**
     * @brief Get singleton instance
     */
    static MetricsStreamer& getInstance();

    /**
     * @brief Initialize the metrics streamer
     */
    bool initialize(const MetricsStreamerConfig& config = {});

    /**
     * @brief Stop the metrics streamer
     */
    void shutdown();

    /**
     * @brief Check if the streamer is running
     */
    bool is_running() const { return running_.load(); }

    /**
     * @brief Register a new WebSocket connection
     */
    std::string register_connection(crow::websocket::connection* conn, const std::string& client_info = "");

    /**
     * @brief Unregister a WebSocket connection
     */
    void unregister_connection(const std::string& connection_id);

    /**
     * @brief Handle WebSocket message
     */
    void handle_message(const std::string& connection_id, const std::string& message);

    /**
     * @brief Update provider metrics (thread-safe)
     */
    void update_provider_metrics(const std::string& provider_name,
                                double response_time_ms,
                                bool success,
                                const std::string& error_type = "",
                                uint64_t input_tokens = 0,
                                uint64_t output_tokens = 0,
                                double cost = 0.0);

    /**
     * @brief Get current comprehensive metrics
     */
    nlohmann::json get_comprehensive_metrics() const;

    /**
     * @brief Get metrics for specific provider
     */
    ProviderMetrics get_provider_metrics(const std::string& provider_name) const;

    /**
     * @brief Get system metrics
     */
    SystemMetrics get_system_metrics() const;

    /**
     * @brief Get historical data
     */
    HistoricalData get_historical_data() const;

    /**
     * @brief Configuration access
     */
    const MetricsStreamerConfig& get_config() const { return config_; }

    /**
     * @brief Performance statistics
     */
    struct PerformanceStats {
        uint64_t total_updates = 0;
        uint64_t total_broadcasts = 0;
        double avg_update_time_ms = 0.0;
        double avg_broadcast_time_ms = 0.0;
        size_t current_connections = 0;
        uint64_t peak_connections = 0;
        uint64_t failed_connections = 0;
        uint64_t messages_sent = 0;
        uint64_t messages_dropped = 0;
    };

    PerformanceStats get_performance_stats() const;

private:
    MetricsStreamer() = default;
    ~MetricsStreamer() = default;
    MetricsStreamer(const MetricsStreamer&) = delete;
    MetricsStreamer& operator=(const MetricsStreamer&) = delete;

    // Configuration
    MetricsStreamerConfig config_;

    // Runtime state
    std::atomic<bool> running_{false};
    std::thread metrics_thread_;
    std::thread broadcast_thread_;

    // Data structures (thread-safe)
    mutable std::shared_mutex providers_mutex_;
    std::unordered_map<std::string, ProviderMetrics> provider_metrics_;

    mutable std::shared_mutex system_mutex_;
    SystemMetrics system_metrics_;

    mutable std::shared_mutex history_mutex_;
    HistoricalData historical_data_;

    mutable std::shared_mutex connections_mutex_;
    std::unordered_map<std::string, std::unique_ptr<WebSocketConnection>> connections_;

    // Performance tracking
    mutable std::mutex performance_mutex_;
    PerformanceStats performance_stats_;

    // Synchronization
    std::condition_variable stop_cv_;
    std::mutex stop_mutex_;

    // Internal methods
    void metrics_collection_loop();
    void websocket_broadcast_loop();
    void update_system_metrics();
    void broadcast_to_all_connections(const nlohmann::json& data);
    void send_to_connection(const std::string& connection_id, const nlohmann::json& data);
    void cleanup_stale_connections();

    // Data processing
    double calculate_percentile(const std::deque<double>& values, double percentile) const;
    void update_historical_data();
    nlohmann::json create_comprehensive_message(uint64_t sequence_num) const;
    nlohmann::json create_delta_message(const nlohmann::json& previous_data, const nlohmann::json& current_data) const;

    // Connection management
    std::string generate_connection_id() const;
    bool authenticate_connection(const std::string& connection_id, const std::string& auth_token) const;
    void handle_connection_request(const std::string& connection_id, const nlohmann::json& message);
    void handle_ping_pong(const std::string& connection_id, const nlohmann::json& message);

    // Utility methods
    uint64_t get_current_timestamp() const;
    std::string format_timestamp(uint64_t timestamp) const;
    double calculate_cpu_usage() const;
    uint64_t get_memory_usage() const;
    double calculate_network_throughput() const;
};

} // namespace webui
} // namespace aimux