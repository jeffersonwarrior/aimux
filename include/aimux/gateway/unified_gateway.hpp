#pragma once

#include <memory>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <map>
#include <vector>
#include <chrono>
#include <crow.h>
#include <nlohmann/json.hpp>

#include "aimux/gateway/format_detector.hpp"
#include "aimux/gateway/api_transformer.hpp"
#include "aimux/core/bridge.hpp"
#include "aimux/core/router.hpp"

namespace aimux {
namespace gateway {

/**
 * @brief Gateway server configuration
 */
struct GatewayConfig {
    // Server endpoints
    int anthropic_port = 8080;      // Port for Anthropic-compatible API
    int openai_port = 8081;         // Port for OpenAI-compatible API
    std::string bind_address = "0.0.0.0";

    // Provider selection
    std::vector<std::string> enabled_providers = {"synthetic"};
    std::map<std::string, std::string> provider_routing_rules;

    // Format handling
    bool enable_format_transformation = true;
    bool auto_detect_format = true;
    bool preserve_headers = true;

    // Performance settings
    int connection_timeout_ms = 30000;
    int max_concurrent_requests = 100;
    bool enable_request_caching = false;

    // Logging and monitoring
    bool enable_request_logging = true;
    bool enable_metrics_collection = true;
    std::string log_level = "info";

    // Security
    bool require_api_key = false;
    std::vector<std::string> allowed_api_keys;

    nlohmann::json to_json() const;
    static GatewayConfig from_json(const nlohmann::json& j);
};

/**
 * @brief Request context for tracking gateway operations
 */
struct RequestContext {
    std::string request_id;
    std::chrono::steady_clock::time_point start_time;
    APIFormat detected_format = APIFormat::UNKNOWN;
    APIFormat client_format = APIFormat::UNKNOWN;
    APIFormat provider_format = APIFormat::UNKNOWN;
    std::string selected_provider;
    std::string client_ip;
    std::string user_agent;

    // Timing information
    std::chrono::milliseconds format_detection_time{0};
    std::chrono::milliseconds transformation_time{0};
    std::chrono::milliseconds provider_time{0};
    std::chrono::milliseconds total_time{0};

    nlohmann::json to_json() const;
};

/**
 * @brief Gateway metrics for monitoring
 */
struct GatewayMetrics {
    std::atomic<size_t> total_requests{0};
    std::atomic<size_t> anthropic_requests{0};
    std::atomic<size_t> openai_requests{0};
    std::atomic<size_t> format_transformations{0};
    std::atomic<size_t> successful_requests{0};
    std::atomic<size_t> failed_requests{0};

    std::map<std::string, std::atomic<size_t>> provider_requests;
    std::map<std::string, std::atomic<double>> provider_response_times;

    std::chrono::steady_clock::time_point start_time;

    GatewayMetrics() : start_time(std::chrono::steady_clock::now()) {}

    nlohmann::json to_json() const;
};

/**
 * @brief Unified gateway supporting dual API endpoints
 */
class UnifiedGateway {
public:
    explicit UnifiedGateway(const GatewayConfig& config = GatewayConfig{});
    ~UnifiedGateway();

    // Server lifecycle
    bool start();
    void stop();
    bool is_running() const { return running_.load(); }

    // Configuration
    void update_config(const GatewayConfig& config);
    GatewayConfig get_config() const { return config_; }

    // Provider management
    void add_provider(const std::string& name, std::unique_ptr<aimux::core::Bridge> bridge);
    void remove_provider(const std::string& name);
    std::vector<std::string> get_available_providers() const;

    // Metrics and monitoring
    GatewayMetrics get_metrics() const;
    std::string get_health_status() const;

    // Configuration for components
    void set_format_detector_config(const FormatDetectionConfig& config);
    void set_transformer_config(const TransformConfig& config);

private:
    GatewayConfig config_;
    std::atomic<bool> running_{false};

    // Server instances
    std::unique_ptr<crow::SimpleApp> anthropic_app_;
    std::unique_ptr<crow::SimpleApp> openai_app_;
    std::thread anthropic_thread_;
    std::thread openai_thread_;

    // Gateway components
    std::unique_ptr<FormatDetector> format_detector_;
    std::unique_ptr<ApiTransformer> api_transformer_;

    // Provider management
    mutable std::mutex providers_mutex_;
    std::map<std::string, std::unique_ptr<aimux::core::Bridge>> providers_;

    // Metrics and monitoring
    mutable std::mutex metrics_mutex_;
    GatewayMetrics metrics_;

    // Request tracking
    mutable std::mutex requests_mutex_;
    std::map<std::string, RequestContext> active_requests_;

    // Server setup
    void setup_anthropic_server();
    void setup_openai_server();
    void setup_common_routes(crow::SimpleApp& app, APIFormat format);

    // Route handlers
    crow::response handle_api_request(const crow::request& req, APIFormat format);
    crow::response handle_health_check(const crow::request& req, APIFormat format);
    crow::response handle_metrics(const crow::request& req, APIFormat format);
    crow::response handle_models(const crow::request& req, APIFormat format);

    // Request processing pipeline
    struct ProcessedRequest {
        bool success = false;
        std::string error_message;
        std::string provider_name;
        nlohmann::json transformed_request;
        std::string request_id;
    };

    ProcessedRequest process_request(const crow::request& req, APIFormat format);
    crow::response create_error_response(
        const std::string& error,
        int status_code = 400,
        APIFormat format = APIFormat::UNKNOWN
    );

    // Provider selection and routing
    std::string select_provider(const nlohmann::json& request, APIFormat format);
    bool is_provider_available(const std::string& provider_name);
    aimux::core::Response forward_to_provider(
        const std::string& provider_name,
        const nlohmann::json& request,
        APIFormat provider_format
    );

    // Request handling utilities
    RequestContext create_request_context(const crow::request& req, APIFormat format);
    void update_metrics(const RequestContext& context, bool success);
    void log_request(const RequestContext& context, const std::string& details);

    // Response formatting
    crow::response format_response(
        const aimux::core::Response& provider_response,
        const RequestContext& context,
        APIFormat client_format
    );

    // Authentication and security
    bool authenticate_request(const crow::request& req);

    // Utility functions
    std::string generate_request_id();
    std::map<std::string, std::string> extract_headers(const crow::request& req);
    std::string get_client_ip(const crow::request& req);

    // Model information
    nlohmann::json get_available_models(APIFormat format);
    nlohmann::json transform_models_response(
        const nlohmann::json& provider_models,
        APIFormat target_format
    );

    // Error handling
    void handle_server_error(const std::string& error, APIFormat format);
};

/**
 * @brief Factory for creating gateway instances
 */
class GatewayFactory {
public:
    /**
     * @brief Create gateway with default configuration
     * @return Unique pointer to gateway
     */
    static std::unique_ptr<UnifiedGateway> create_gateway();

    /**
     * @brief Create gateway with custom configuration
     * @param config Gateway configuration
     * @return Unique pointer to gateway
     */
    static std::unique_ptr<UnifiedGateway> create_gateway(const GatewayConfig& config);

    /**
     * @brief Create gateway configuration preset
     * @param preset_name Configuration preset ("development", "production", "testing")
     * @return Gateway configuration
     */
    static GatewayConfig create_config(const std::string& preset_name = "production");
};

} // namespace gateway
} // namespace aimux