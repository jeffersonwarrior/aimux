#include "aimux/gateway/gateway_manager.hpp"
#include "aimux/logging/logger.hpp"
#include <crow.h>
#include <thread>
#include <chrono>
#include <sstream>
#include <iomanip>

namespace aimux {
namespace gateway {

/**
 * @brief V3 Unified Gateway - Single endpoint with intelligent routing
 *
 * This implements the V3 architecture where a single /anthropic endpoint
 * intelligently routes requests to the best provider based on content analysis.
 */
class V3UnifiedGateway {
public:
    struct Config {
        int port = 8080;
        std::string bind_address = "0.0.0.0";
        std::string log_level = "info";
        bool enable_metrics = true;
        bool enable_cors = true;
        int max_concurrent_requests = 100;
        std::chrono::seconds request_timeout{300};

        nlohmann::json to_json() const;
        static Config from_json(const nlohmann::json& j);
    };

    explicit V3UnifiedGateway(const Config& config = Config{});
    ~V3UnifiedGateway();

    // Lifecycle
    bool start();
    void stop();
    bool is_running() const { return running_.load(); }

    // Configuration
    void update_config(const Config& config);
    Config get_config() const { return config_; }

    // Gateway manager access
    GatewayManager* get_gateway_manager() { return gateway_manager_.get(); }

    // Health and metrics
    nlohmann::json get_status() const;
    nlohmann::json get_metrics() const;

private:
    Config config_;
    std::atomic<bool> running_{false};

    // Core components
    std::unique_ptr<GatewayManager> gateway_manager_;
    std::unique_ptr<crow::SimpleApp> app_;
    std::thread server_thread_;

    // Request tracking
    struct RequestTracker {
        std::string request_id;
        std::chrono::steady_clock::time_point start_time;
        std::string client_ip;
        std::string user_agent;
    };

    mutable std::mutex requests_mutex_;
    std::unordered_map<std::string, RequestTracker> active_requests_;

    // Server setup
    void setup_routes();
    void setup_cors();

    // Route handlers
    crow::response handle_anthropic_request(const crow::request& req);
    crow::response handle_health_check(const crow::request& req);
    crow::response handle_metrics(const crow::request& req);
    crow::response handle_providers(const crow::request& req);
    crow::response handle_config(const crow::request& req);

    // Request processing
    crow::response process_request(const crow::request& req);
    core::Response create_aimux_request(const nlohmann::json& anthropic_request);
    crow::response convert_to_anthropic_response(const core::Response& aimux_response, const RequestTracker& tracker);

    // Utilities
    std::string generate_request_id();
    RequestTracker create_tracker(const crow::request& req);
    std::string get_client_ip(const crow::request& req);
    bool validate_anthropic_request(const nlohmann::json& request);
    nlohmann::json create_error_response(const std::string& error_code, const std::string& message);

    // Authentication and validation
    bool authenticate_request(const crow::request& req);
    bool has_valid_api_key(const crow::request& req);
};

// ============================================================================
// Config Implementation
// ============================================================================

nlohmann::json V3UnifiedGateway::Config::to_json() const {
    nlohmann::json j;
    j["port"] = port;
    j["bind_address"] = bind_address;
    j["log_level"] = log_level;
    j["enable_metrics"] = enable_metrics;
    j["enable_cors"] = enable_cors;
    j["max_concurrent_requests"] = max_concurrent_requests;
    j["request_timeout"] = request_timeout.count();
    return j;
}

V3UnifiedGateway::Config V3UnifiedGateway::Config::from_json(const nlohmann::json& j) {
    Config config;
    config.port = j.value("port", 8080);
    config.bind_address = j.value("bind_address", "0.0.0.0");
    config.log_level = j.value("log_level", "info");
    config.enable_metrics = j.value("enable_metrics", true);
    config.enable_cors = j.value("enable_cors", true);
    config.max_concurrent_requests = j.value("max_concurrent_requests", 100);
    config.request_timeout = std::chrono::seconds(j.value("request_timeout", 300));
    return config;
}

// ============================================================================
// V3UnifiedGateway Implementation
// ============================================================================

V3UnifiedGateway::V3UnifiedGateway(const Config& config)
    : config_(config),
      gateway_manager_(std::make_unique<GatewayManager>()),
      app_(std::make_unique<crow::SimpleApp>()) {

    aimux::info("V3UnifiedGateway: Initializing V3 unified gateway");

    // Initialize gateway manager
    try {
        gateway_manager_->initialize();
        aimux::info("V3UnifiedGateway: Gateway manager initialized");
    } catch (const std::exception& e) {
        aimux::error("V3UnifiedGateway: Failed to initialize gateway manager: " + std::string(e.what()));
        throw;
    }

    setup_routes();
}

V3UnifiedGateway::~V3UnifiedGateway() {
    stop();
}

bool V3UnifiedGateway::start() {
    if (running_.load()) {
        aimux::warn("V3UnifiedGateway: Already running");
        return true;
    }

    try {
        // Start the server in a separate thread
        server_thread_ = std::thread([this]() {
            try {
                app_->bindaddr(config_.bind_address).port(config_.port).multithreaded().run();
            } catch (const std::exception& e) {
                aimux::error("V3UnifiedGateway: Server error: " + std::string(e.what()));
            }
        });

        // Wait a moment to see if server starts successfully
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        running_.store(true);
        aimux::info("V3UnifiedGateway: Started on " + config_.bind_address + ":" + std::to_string(config_.port));
        return true;

    } catch (const std::exception& e) {
        aimux::error("V3UnifiedGateway: Failed to start: " + std::string(e.what()));
        return false;
    }
}

void V3UnifiedGateway::stop() {
    if (!running_.load()) {
        return;
    }

    aimux::info("V3UnifiedGateway: Stopping server...");

    // Stop the app
    if (app_) {
        app_->stop();
    }

    // Wait for server thread to finish
    if (server_thread_.joinable()) {
        server_thread_.join();
    }

    running_.store(false);
    aimux::info("V3UnifiedGateway: Stopped successfully");
}

void V3UnifiedGateway::update_config(const Config& config) {
    config_ = config;
    aimux::info("V3UnifiedGateway: Configuration updated");
}

nlohmann::json V3UnifiedGateway::get_status() const {
    nlohmann::json status;
    status["running"] = running_.load();
    status["version"] = "3.1";
    status["architecture"] = "unified-gateway";
    status["endpoint"] = "http://" + config_.bind_address + ":" + std::to_string(config_.port) + "/anthropic";

    // Gateway manager status
    if (gateway_manager_) {
        status["gateway_manager"] = gateway_manager_->get_configuration();
        status["healthy_providers"] = gateway_manager_->get_healthy_providers();
        status["unhealthy_providers"] = gateway_manager_->get_unhealthy_providers();
    }

    // Active requests
    std::lock_guard<std::mutex> lock(requests_mutex_);
    status["active_requests"] = active_requests_.size();

    return status;
}

nlohmann::json V3UnifiedGateway::get_metrics() const {
    nlohmann::json metrics;

    if (gateway_manager_) {
        metrics = gateway_manager_->get_metrics();
    }

    // Gateway-specific metrics
    metrics["gateway"] = nlohmann::json{
        {"active_requests", active_requests_.size()},
        {"uptime_seconds", std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - std::chrono::steady_clock::time_point{}).count()},
        {"max_concurrent_requests", config_.max_concurrent_requests}
    };

    return metrics;
}

// ============================================================================
// Server Setup
// ============================================================================

void V3UnifiedGateway::setup_routes() {
    CROW_ROUTE(*app_, "/anthropic").methods("POST"_method)([this](const crow::request& req) {
        return handle_anthropic_request(req);
    });

    CROW_ROUTE(*app_, "/anthropic/v1/messages").methods("POST"_method)([this](const crow::request& req) {
        return handle_anthropic_request(req);
    });

    CROW_ROUTE(*app_, "/anthropic/v1/models").methods("GET"_method)([this](const crow::request& req) {
        return handle_models("anthropic");
    });

    // OpenAI compatibility endpoint
    CROW_ROUTE(*app_, "/v1/chat/completions").methods("POST"_method)([this](const crow::request& req) {
        return handle_openai_request(req);
    });

    CROW_ROUTE(*app_, "/v1/models").methods("GET"_method)([this](const crow::request& req) {
        return handle_models("openai");
    });

    // Management endpoints
    CROW_ROUTE(*app_, "/health").methods("GET"_method)([this](const crow::request& req) {
        return handle_health_check(req);
    });

    CROW_ROUTE(*app_, "/metrics").methods("GET"_method)([this](const crow::request& req) {
        return handle_metrics(req);
    });

    CROW_ROUTE(*app_, "/providers").methods("GET"_method)([this](const crow::request& req) {
        return handle_providers(req);
    });

    CROW_ROUTE(*app_, "/config").methods("GET"_method, "POST"_method)([this](const crow::request& req) {
        return handle_config(req);
    });

    // Enable CORS if configured
    if (config_.enable_cors) {
        setup_cors();
    }
}

void V3UnifiedGateway::setup_cors() {
    // Global CORS headers
    CROW_ALL_MIDDLEWARES(*app_, 0) = [this](const crow::request& req, crow::response& res, std::function<void()> next) {
        res.add_header("Access-Control-Allow-Origin", "*");
        res.add_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        res.add_header("Access-Control-Allow-Headers", "Content-Type, Authorization, x-api-key");
        res.add_header("Access-Control-Max-Age", "86400");

        if (req.method == "OPTIONS"_method) {
            res.code = 204;
            return;
        }

        next();
    };
}

// ============================================================================
// Route Handlers
// ============================================================================

crow::response V3UnifiedGateway::handle_anthropic_request(const crow::request& req) {
    return process_request(req);
}

crow::response V3UnifiedGateway::handle_openai_request(const crow::request& req) {
    // For now, treat OpenAI requests the same as Anthropic requests
    // In the future, we could add OpenAI-specific transformations
    return process_request(req);
}

crow::response V3UnifiedGateway::handle_health_check(const crow::request& req) {
    try {
        nlohmann::json health = {
            {"status", "healthy"},
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()},
            {"version", "3.1"},
            {"architecture", "unified-gateway"}
        };

        if (gateway_manager_) {
            health["providers"] = nlohmann::json{
                {"healthy_count", gateway_manager_->get_healthy_providers().size()},
                {"total_count", gateway_manager_->get_provider_configs().size()}
            };
        }

        return crow::response(200, health.dump());

    } catch (const std::exception& e) {
        aimux::error("Health check failed: " + std::string(e.what()));
        return crow::response(500, R"({"status": "unhealthy", "error": "Internal server error"})");
    }
}

crow::response V3UnifiedGateway::handle_metrics(const crow::request& req) {
    try {
        nlohmann::json metrics = get_metrics();
        return crow::response(200, metrics.dump());

    } catch (const std::exception& e) {
        aimux::error("Metrics request failed: " + std::string(e.what()));
        return crow::response(500, R"({"error": "Failed to retrieve metrics"})");
    }
}

crow::response V3UnifiedGateway::handle_providers(const crow::request& req) {
    try {
        if (!gateway_manager_) {
            return crow::response(503, R"({"error": "Gateway manager not available"})");
        }

        nlohmann::json response;
        response["healthy"] = gateway_manager_->get_healthy_providers();
        response["unhealthy"] = gateway_manager_->get_unhealthy_providers();
        response["configs"] = gateway_manager_->get_provider_configs();

        return crow::response(200, response.dump());

    } catch (const std::exception& e) {
        aimux::error("Providers endpoint failed: " + std::string(e.what()));
        return crow::response(500, R"({"error": "Failed to retrieve provider information"})");
    }
}

crow::response V3UnifiedGateway::handle_config(const crow::request& req) {
    try {
        if (req.method == "GET"_method) {
            if (gateway_manager_) {
                return crow::response(200, gateway_manager_->get_configuration().dump());
            }
            return crow::response(503, R"({"error": "Gateway manager not available"})");

        } else if (req.method == "POST"_method) {
            // Update configuration
            auto json_body = nlohmann::json::parse(req.body);

            // This would update the gateway manager configuration
            // For now, just return success
            return crow::response(200, R"({"success": true, "message": "Configuration updated"})");
        }

    } catch (const std::exception& e) {
        aimux::error("Config endpoint failed: " + std::string(e.what()));
        return crow::response(500, R"({"error": "Configuration operation failed"})");
    }

    return crow::response(405, R"({"error": "Method not allowed"})");
}

crow::response V3UnifiedGateway::handle_models(const std::string& format) {
    try {
        if (!gateway_manager_) {
            return crow::response(503, R"({"error": "Gateway manager not available"})");
        }

        // Get models from all healthy providers and combine them
        nlohmann::json models_response;
        models_response["object"] = "list";
        models_response["data"] = nlohmann::json::array();

        auto healthy_providers = gateway_manager_->get_healthy_providers();

        for (const auto& provider_name : healthy_providers) {
            // In a real implementation, we would query each provider for their models
            // For now, return some common models
            if (format == "anthropic") {
                models_response["data"].push_back({
                    {"id", "claude-3-sonnet-20240229"},
                    {"object", "model"},
                    {"created", 1708497600},
                    {"created_by", provider_name}
                });
                models_response["data"].push_back({
                    {"id", "claude-3-haiku-20240307"},
                    {"object", "model"},
                    {"created", 1708497600},
                    {"created_by", provider_name}
                });
            } else {
                models_response["data"].push_back({
                    {"id", "gpt-4"},
                    {"object", "model"},
                    {"created", 1678608800},
                    {"owned_by", provider_name}
                });
                models_response["data"].push_back({
                    {"id", "gpt-3.5-turbo"},
                    {"object", "model"},
                    {"created", 1678608800},
                    {"owned_by", provider_name}
                });
            }
        }

        return crow::response(200, models_response.dump());

    } catch (const std::exception& e) {
        aimux::error("Models endpoint failed: " + std::string(e.what()));
        return crow::response(500, R"({"error": "Failed to retrieve models"})");
    }
}

// ============================================================================
// Request Processing
// ============================================================================

crow::response V3UnifiedGateway::process_request(const crow::request& req) {
    auto tracker = create_tracker(req);

    try {
        // Parse request body
        nlohmann::json request_json;
        try {
            request_json = nlohmann::json::parse(req.body);
        } catch (const nlohmann::json::parse_error& e) {
            return crow::response(400, create_error_response("INVALID_JSON", e.what()).dump());
        }

        // Validate request
        if (!validate_anthropic_request(request_json)) {
            return crow::response(400, create_error_response("INVALID_REQUEST", "Invalid request format").dump());
        }

        // Convert to aimux format
        core::Request aimux_request = create_aimux_request(request_json);

        // Route through gateway manager
        core::Response response;
        if (gateway_manager_) {
            response = gateway_manager_->route_request(aimux_request);
        } else {
            response.success = false;
            response.error_message = "Gateway manager not available";
            response.status_code = 503;
        }

        // Convert back to Anthropic format
        return convert_to_anthropic_response(response, tracker);

    } catch (const std::exception& e) {
        aimux::error("Request processing failed: " + std::string(e.what()));
        return crow::response(500, create_error_response("INTERNAL_ERROR", e.what()).dump());
    } finally {
        // Clean up request tracker
        std::lock_guard<std::mutex> lock(requests_mutex_);
        active_requests_.erase(tracker.request_id);
    }
}

core::Request V3UnifiedGateway::create_aimux_request(const nlohmann::json& anthropic_request) {
    core::Request aimux_request;

    // Extract model
    if (anthropic_request.contains("model")) {
        aimux_request.model = anthropic_request["model"];
    }

    // Extract method and data
    aimux_request.method = "POST";
    aimux_request.data = anthropic_request;

    return aimux_request;
}

crow::response V3UnifiedGateway::convert_to_anthropic_response(const core::Response& aimux_response, const RequestTracker& tracker) {
    int status_code = aimux_response.success ? 200 : aimux_response.status_code;

    if (!aimux_response.success) {
        nlohmann::json error_response = create_error_response("PROVIDER_ERROR", aimux_response.error_message);
        error_response["provider"] = aimux_response.provider_name;
        error_response["request_id"] = tracker.request_id;
        return crow::response(status_code, error_response.dump());
    }

    try {
        // Parse provider response and convert to Anthropic format
        nlohmann::json response_data = nlohmann::json::parse(aimux_response.data);

        // Add metadata
        response_data["id"] = tracker.request_id;
        response_data["model"] = aimux_response.provider_name;

        return crow::response(status_code, response_data.dump());

    } catch (const nlohmann::json::parse_error& e) {
        aimux::error("Failed to parse provider response: " + std::string(e.what()));
        return crow::response(500, create_error_response("RESPONSE_PARSE_ERROR", "Invalid response from provider").dump());
    }
}

// ============================================================================
// Utility Functions
// ============================================================================

std::string V3UnifiedGateway::generate_request_id() {
    // Generate UUID-like request ID
    static std::atomic<uint64_t> counter{0};
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

    std::ostringstream oss;
    oss << "req_" << timestamp << "_" << counter.fetch_add(1);
    return oss.str();
}

V3UnifiedGateway::RequestTracker V3UnifiedGateway::create_tracker(const crow::request& req) {
    RequestTracker tracker;
    tracker.request_id = generate_request_id();
    tracker.start_time = std::chrono::steady_clock::now();
    tracker.client_ip = get_client_ip(req);

    auto user_agent_header = req.get_header_value("User-Agent");
    tracker.user_agent = std::string(user_agent_header);

    // Store in active requests
    std::lock_guard<std::mutex> lock(requests_mutex_);
    active_requests_[tracker.request_id] = tracker;

    return tracker;
}

std::string V3UnifiedGateway::get_client_ip(const crow::request& req) {
    // Try various headers for real IP
    auto x_forwarded_for = req.get_header_value("X-Forwarded-For");
    if (!x_forwarded_for.empty()) {
        // Take the first IP in the list
        std::string ips = std::string(x_forwarded_for);
        size_t comma_pos = ips.find(',');
        if (comma_pos != std::string::npos) {
            return ips.substr(0, comma_pos);
        }
        return ips;
    }

    auto x_real_ip = req.get_header_value("X-Real-IP");
    if (!x_real_ip.empty()) {
        return std::string(x_real_ip);
    }

    return req.remote_ip_address;
}

bool V3UnifiedGateway::validate_anthropic_request(const nlohmann::json& request) {
    // Basic validation for Anthropic-style requests

    // Check required fields
    if (!request.contains("model") || !request["model"].is_string()) {
        return false;
    }

    if (!request.contains("messages") || !request["messages"].is_array() || request["messages"].empty()) {
        return false;
    }

    // Validate message structure
    for (const auto& message : request["messages"]) {
        if (!message.contains("role") || !message.contains("content")) {
            return false;
        }

        std::string role = message["role"];
        if (role != "user" && role != "assistant") {
            return false;
        }
    }

    return true;
}

nlohmann::json V3UnifiedGateway::create_error_response(const std::string& error_code, const std::string& message) {
    return nlohmann::json{
        {"type", "error"},
        {"error", {
            {"type", error_code},
            {"message", message}
        }},
        {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()}
    };
}

bool V3UnifiedGateway::authenticate_request(const crow::request& req) {
    // For now, just check for any authentication requirements
    // In production, this would validate API keys, tokens, etc.
    return true;
}

bool V3UnifiedGateway::has_valid_api_key(const crow::request& req) {
    auto api_key_header = req.get_header_value("x-api-key");
    auto auth_header = req.get_header_value("Authorization");

    // For now, accept any request (development mode)
    // In production, validate against stored keys

    return true;
}

} // namespace gateway
} // namespace aimux