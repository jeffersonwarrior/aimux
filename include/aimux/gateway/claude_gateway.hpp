#pragma once

#include <memory>
#include <thread>
#include <atomic>
#include <chrono>
#include <functional>
#include <crow.h>
#include <nlohmann/json.hpp>
#include "aimux/gateway/gateway_manager.hpp"
#include "aimux/core/router.hpp"
#include "aimux/logging/logger.hpp"

namespace aimux {
namespace gateway {

/**
 * @brief Metrics for ClaudeGateway service operations
 */
struct ClaudeGatewayMetrics {
    std::atomic<uint64_t> total_requests{0};
    std::atomic<uint64_t> successful_requests{0};
    std::atomic<uint64_t> failed_requests{0};
    std::atomic<double> total_response_time_ms{0.0};
    std::chrono::steady_clock::time_point start_time;

    ClaudeGatewayMetrics() : start_time(std::chrono::steady_clock::now()) {}

    // Copy constructor and assignment operator
    ClaudeGatewayMetrics(const ClaudeGatewayMetrics& other);
    ClaudeGatewayMetrics& operator=(const ClaudeGatewayMetrics& other);

    // Move constructor and assignment operator
    ClaudeGatewayMetrics(ClaudeGatewayMetrics&& other) noexcept;
    ClaudeGatewayMetrics& operator=(ClaudeGatewayMetrics&& other) noexcept;

    double get_average_response_time() const {
        uint64_t total = total_requests.load();
        return total > 0 ? total_response_time_ms.load() / total : 0.0;
    }

    double get_success_rate() const {
        uint64_t total = total_requests.load();
        return total > 0 ? static_cast<double>(successful_requests.load()) / total : 0.0;
    }

    double get_uptime_seconds() const {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration<double>(now - start_time).count();
    }

    nlohmann::json to_json() const;
};

/**
 * @brief Configuration for ClaudeGateway service
 */
struct ClaudeGatewayConfig {
    std::string bind_address = "127.0.0.1";
    int port = 8080;
    std::string log_level = "info";
    bool enable_metrics = true;
    bool enable_cors = true;
    std::string cors_origin = "*";
    bool request_logging = false;
    size_t max_request_size_mb = 10;
    std::chrono::seconds request_timeout{60};

    nlohmann::json to_json() const;
    static ClaudeGatewayConfig from_json(const nlohmann::json& j);
};

/**
 * @brief V3.2 ClaudeGateway Service - Single unified endpoint for Claude Code compatibility
 *
 * This class implements the V3.2 unified gateway architecture that provides a single
 * `/anthropic/v1/messages` endpoint compatible with Claude Code. It leverages the
 * V3.1 GatewayManager for intelligent routing while providing a clean, production-ready
 * HTTP service interface.
 */
class ClaudeGateway {
public:
    using RequestCallback = std::function<void(const core::Request&, const core::Response&, double)>;
    using ErrorCallback = std::function<void(const std::string&, const std::string&)>;

    explicit ClaudeGateway();
    ~ClaudeGateway();

    // Service lifecycle management
    void initialize(const ClaudeGatewayConfig& config = ClaudeGatewayConfig{});
    void start(const std::string& bind_address = "127.0.0.1", int port = 8080);
    void stop();
    void shutdown();

    // Status and configuration
    bool is_running() const { return running_.load(); }
    bool is_initialized() const { return initialized_.load(); }

    std::string get_bind_address() const { return bind_address_; }
    int get_port() const { return port_; }

    ClaudeGatewayConfig get_config() const { return config_; }
    void update_config(const ClaudeGatewayConfig& config);

    // Gateway manager access
    GatewayManager* get_gateway_manager() { return manager_.get(); }
    const GatewayManager* get_gateway_manager() const { return manager_.get(); }

    // Metrics and monitoring
    ClaudeGatewayMetrics get_metrics() const;
    void reset_metrics();
    nlohmann::json get_detailed_metrics() const;

    // Callbacks for external monitoring
    void set_request_callback(RequestCallback callback) { request_callback_ = std::move(callback); }
    void set_error_callback(ErrorCallback callback) { error_callback_ = std::move(callback); }

    // Configuration management
    void load_provider_config(const std::string& config_file = "config.json");
    void save_config(const std::string& config_file = "claude_gateway_config.json");

private:
    // Core components
    std::unique_ptr<GatewayManager> manager_;
    crow::SimpleApp app_;
    std::thread server_thread_;

    // Service state
    std::atomic<bool> initialized_{false};
    std::atomic<bool> running_{false};
    std::atomic<bool> shutdown_requested_{false};

    // Configuration
    ClaudeGatewayConfig config_;
    std::string bind_address_;
    int port_;

    // Metrics
    mutable ClaudeGatewayMetrics metrics_;

    // Callbacks
    RequestCallback request_callback_;
    ErrorCallback error_callback_;

    // Route setup
    void setup_routes();
    void setup_anthropic_routes();
    void setup_management_routes();
    void setup_health_routes();

    // Request handling
    crow::response handle_anthropic_request(const crow::request& req);
    crow::response handle_messages_endpoint(const crow::request& req);

    // Management endpoints
    crow::response handle_metrics_request(const crow::request& req);
    crow::response handle_health_request(const crow::request& req);
    crow::response handle_config_request(const crow::request& req);
    crow::response handle_providers_request(const crow::request& req);

    // Utility methods
    core::Request convert_crow_request(const crow::request& req);
    crow::response convert_core_response(const core::Response& resp);
    crow::response create_error_response(int status, const std::string& code, const std::string& message);

    // Validation and security
    bool validate_request(const crow::request& req, std::string& error_msg);
    bool is_request_size_valid(const crow::request& req);
    void setup_cors_headers(crow::response& resp);

    // Logging and monitoring
    void log_request(const crow::request& req, const core::Response& resp, double duration_ms);
    void log_error(const std::string& type, const std::string& message);
    void update_metrics(const core::Response& resp, double duration_ms);

    // Service lifecycle helpers
    void server_thread_func();
    void graceful_shutdown();
    bool validate_configuration();

    // Error handling
    void handle_route_error(const std::string& route, const std::exception& e);
    void handle_gateway_error(const std::string& operation, const std::string& error);
};

} // namespace gateway
} // namespace aimux