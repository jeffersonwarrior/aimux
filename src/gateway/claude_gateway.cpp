#include "aimux/gateway/claude_gateway.hpp"
#include "aimux/core/bridge.hpp"
#include "aimux/providers/provider_impl.hpp"
#include <sstream>
#include <fstream>
#include <regex>
#include <chrono>
#include <iomanip>

namespace aimux {
namespace gateway {

// ============================================================================
// ClaudeGatewayMetrics Implementation
// ============================================================================

// Copy constructor
ClaudeGatewayMetrics::ClaudeGatewayMetrics(const ClaudeGatewayMetrics& other)
    : total_requests(other.total_requests.load()),
      successful_requests(other.successful_requests.load()),
      failed_requests(other.failed_requests.load()),
      total_response_time_ms(other.total_response_time_ms.load()),
      start_time(other.start_time) {
}

// Copy assignment operator
ClaudeGatewayMetrics& ClaudeGatewayMetrics::operator=(const ClaudeGatewayMetrics& other) {
    if (this != &other) {
        total_requests.store(other.total_requests.load());
        successful_requests.store(other.successful_requests.load());
        failed_requests.store(other.failed_requests.load());
        total_response_time_ms.store(other.total_response_time_ms.load());
        start_time = other.start_time;
    }
    return *this;
}

// Move constructor
ClaudeGatewayMetrics::ClaudeGatewayMetrics(ClaudeGatewayMetrics&& other) noexcept
    : total_requests(other.total_requests.load()),
      successful_requests(other.successful_requests.load()),
      failed_requests(other.failed_requests.load()),
      total_response_time_ms(other.total_response_time_ms.load()),
      start_time(std::move(other.start_time)) {
}

// Move assignment operator
ClaudeGatewayMetrics& ClaudeGatewayMetrics::operator=(ClaudeGatewayMetrics&& other) noexcept {
    if (this != &other) {
        total_requests.store(other.total_requests.load());
        successful_requests.store(other.successful_requests.load());
        failed_requests.store(other.failed_requests.load());
        total_response_time_ms.store(other.total_response_time_ms.load());
        start_time = std::move(other.start_time);
    }
    return *this;
}

nlohmann::json ClaudeGatewayMetrics::to_json() const {
    nlohmann::json j;
    j["total_requests"] = total_requests.load();
    j["successful_requests"] = successful_requests.load();
    j["failed_requests"] = failed_requests.load();
    j["success_rate"] = get_success_rate();
    j["average_response_time_ms"] = get_average_response_time();
    j["uptime_seconds"] = get_uptime_seconds();
    j["start_time_iso"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        start_time.time_since_epoch()).count();
    return j;
}

// ============================================================================
// ClaudeGatewayConfig Implementation
// ============================================================================

nlohmann::json ClaudeGatewayConfig::to_json() const {
    nlohmann::json j;
    j["bind_address"] = bind_address;
    j["port"] = port;
    j["log_level"] = log_level;
    j["enable_metrics"] = enable_metrics;
    j["enable_cors"] = enable_cors;
    j["cors_origin"] = cors_origin;
    j["request_logging"] = request_logging;
    j["max_request_size_mb"] = max_request_size_mb;
    j["request_timeout_seconds"] = request_timeout.count();
    return j;
}

ClaudeGatewayConfig ClaudeGatewayConfig::from_json(const nlohmann::json& j) {
    ClaudeGatewayConfig config;
    config.bind_address = j.value("bind_address", "127.0.0.1");
    config.port = j.value("port", 8080);
    config.log_level = j.value("log_level", "info");
    config.enable_metrics = j.value("enable_metrics", true);
    config.enable_cors = j.value("enable_cors", true);
    config.cors_origin = j.value("cors_origin", "*");
    config.request_logging = j.value("request_logging", false);
    config.max_request_size_mb = j.value("max_request_size_mb", 10U);
    config.request_timeout = std::chrono::seconds(j.value("request_timeout_seconds", 60));
    return config;
}

// ============================================================================
// ClaudeGateway Implementation
// ============================================================================

ClaudeGateway::ClaudeGateway() {
    manager_ = std::make_unique<GatewayManager>();
    aimux::info("ClaudeGateway: Service instance created");
}

ClaudeGateway::~ClaudeGateway() {
    if (is_running()) {
        shutdown();
    }
    aimux::info("ClaudeGateway: Service instance destroyed");
}

void ClaudeGateway::initialize(const ClaudeGatewayConfig& config) {
    if (initialized_.load()) {
        aimux::warn("ClaudeGateway: Already initialized");
        return;
    }

    config_ = config;
    bind_address_ = config.bind_address;
    port_ = config.port;

    // Validate configuration
    if (!validate_configuration()) {
        throw std::runtime_error("Invalid ClaudeGateway configuration");
    }

    try {
        // Initialize GatewayManager
        manager_->initialize();

        // Load provider configuration if available
        try {
            load_provider_config();
        } catch (const std::exception& e) {
            aimux::warn("Could not load provider config: " + std::string(e.what()));
        }

        // Setup Crow routes
        setup_routes();

        initialized_.store(true);
        aimux::info("ClaudeGateway: Initialized successfully on " + bind_address_ + ":" + std::to_string(port_));

    } catch (const std::exception& e) {
        aimux::error("ClaudeGateway initialization failed: " + std::string(e.what()));
        throw;
    }
}

void ClaudeGateway::start(const std::string& bind_address, int port) {
    if (running_.load()) {
        aimux::warn("ClaudeGateway: Already running");
        return;
    }

    if (!initialized_.load()) {
        ClaudeGatewayConfig config = config_;
        config.bind_address = bind_address.empty() ? bind_address_ : bind_address;
        config.port = port > 0 ? port : port_;
        initialize(config);
    }

    bind_address_ = bind_address.empty() ? bind_address_ : bind_address;
    port_ = port > 0 ? port : port_;

    try {
        shutdown_requested_.store(false);

        // Start server thread
        server_thread_ = std::thread(&ClaudeGateway::server_thread_func, this);

        // Wait for server to start
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        if (running_.load()) {
            aimux::info("ClaudeGateway: Started successfully on http://" + bind_address_ + ":" + std::to_string(port_));
        } else {
            throw std::runtime_error("Server failed to start");
        }

    } catch (const std::exception& e) {
        aimux::error("ClaudeGateway start failed: " + std::string(e.what()));
        throw;
    }
}

void ClaudeGateway::stop() {
    if (!running_.load()) {
        aimux::warn("ClaudeGateway: Not running");
        return;
    }

    aimux::info("ClaudeGateway: Stopping service...");
    shutdown_requested_.store(true);

    graceful_shutdown();

    if (server_thread_.joinable()) {
        server_thread_.join();
    }

    running_.store(false);
    aimux::info("ClaudeGateway: Service stopped");
}

void ClaudeGateway::shutdown() {
    if (running_.load()) {
        stop();
    }

    if (manager_) {
        manager_->shutdown();
    }

    initialized_.store(false);
    aimux::info("ClaudeGateway: Service shutdown complete");
}

void ClaudeGateway::update_config(const ClaudeGatewayConfig& config) {
    if (running_.load()) {
        aimux::warn("Cannot update configuration while service is running");
        return;
    }

    config_ = config;
    bind_address_ = config.bind_address;
    port_ = config.port;

    // Re-initialize with new configuration
    if (initialized_.load()) {
        initialized_.store(false);
        initialize(config);
    }
}

ClaudeGatewayMetrics ClaudeGateway::get_metrics() const {
    ClaudeGatewayMetrics result;
    result.total_requests.store(metrics_.total_requests.load());
    result.successful_requests.store(metrics_.successful_requests.load());
    result.failed_requests.store(metrics_.failed_requests.load());
    result.total_response_time_ms.store(metrics_.total_response_time_ms.load());
    result.start_time = metrics_.start_time;
    return result;
}

void ClaudeGateway::reset_metrics() {
    metrics_.total_requests.store(0);
    metrics_.successful_requests.store(0);
    metrics_.failed_requests.store(0);
    metrics_.total_response_time_ms.store(0.0);
    metrics_.start_time = std::chrono::steady_clock::now();
    aimux::info("ClaudeGateway: Metrics reset");
}

nlohmann::json ClaudeGateway::get_detailed_metrics() const {
    nlohmann::json detailed = metrics_.to_json();

    // Add gateway manager metrics
    if (manager_) {
        detailed["gateway_metrics"] = manager_->get_metrics();
        detailed["provider_configs"] = manager_->get_provider_configs();
    }

    detailed["service_status"] = nlohmann::json::object({
        {"initialized", initialized_.load()},
        {"running", running_.load()},
        {"bind_address", bind_address_},
        {"port", port_}
    });

    detailed["configuration"] = config_.to_json();

    return detailed;
}

void ClaudeGateway::load_provider_config(const std::string& config_file) {
    std::ifstream file(config_file);
    if (!file.good()) {
        aimux::warn("Config file not found: " + config_file);
        return;
    }

    try {
        nlohmann::json config = nlohmann::json::parse(file);
        manager_->load_configuration(config);
        aimux::info("ClaudeGateway: Loaded provider configuration from " + config_file);
    } catch (const std::exception& e) {
        aimux::error("Failed to load provider config: " + std::string(e.what()));
        throw;
    }
}

void ClaudeGateway::save_config(const std::string& config_file) {
    try {
        nlohmann::json config = manager_->get_configuration();
        config["claude_gateway"] = config_.to_json();

        std::ofstream file(config_file);
        file << config.dump(4);
        aimux::info("ClaudeGateway: Saved configuration to " + config_file);
    } catch (const std::exception& e) {
        aimux::error("Failed to save config: " + std::string(e.what()));
        throw;
    }
}

void ClaudeGateway::setup_routes() {
    aimux::debug("ClaudeGateway: Setting up routes");

    // Main Anthropic-compatible endpoint
    setup_anthropic_routes();

    // Management and monitoring routes
    setup_management_routes();

    // Health check routes
    setup_health_routes();

    aimux::debug("ClaudeGateway: Route setup complete");
}

void ClaudeGateway::setup_anthropic_routes() {
    // Main messages endpoint - Claude Code compatible
    app_.route_dynamic("/anthropic/v1/messages")
        .methods("POST"_method)
        ([this](const crow::request& req) {
            return handle_messages_endpoint(req);
        });

    // Models endpoint
    app_.route_dynamic("/anthropic/v1/models")
        .methods("GET"_method)
        ([this](const crow::request& /* req */) {
        try {
            // Return available models from all providers
            nlohmann::json response;
            response["object"] = "list";

            auto provider_configs = manager_->get_provider_configs();
            nlohmann::json models;

            for (const auto& [provider_name, config] : provider_configs.items()) {
                for (const auto& model : config["models"]) {
                    nlohmann::json model_info;
                    model_info["id"] = model.get<std::string>();
                    model_info["object"] = "model";
                    model_info["created"] = std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count();
                    model_info["owned_by"] = provider_name;
                    models.push_back(model_info);
                }
            }

            response["data"] = models;

            crow::response resp(200, response.dump());
            setup_cors_headers(resp);
            return resp;

        } catch (const std::exception& e) {
            return create_error_response(500, "MODELS_ERROR", e.what());
        }
    });

    aimux::debug("ClaudeGateway: Anthropic routes configured");
}

void ClaudeGateway::setup_management_routes() {
    // Metrics endpoint
    app_.route_dynamic("/metrics")
        .methods("GET"_method)
        ([this](const crow::request& req) {
            return handle_metrics_request(req);
        });

    // Configuration endpoint
    app_.route_dynamic("/config")
        .methods("GET"_method, "POST"_method)
        ([this](const crow::request& req) {
            return handle_config_request(req);
        });

    // Providers endpoint
    app_.route_dynamic("/providers")
        .methods("GET"_method)
        ([this](const crow::request& req) {
            return handle_providers_request(req);
        });

    aimux::debug("ClaudeGateway: Management routes configured");
}

void ClaudeGateway::setup_health_routes() {
    // Basic health check
    app_.route_dynamic("/health")
        .methods("GET"_method)
        ([this](const crow::request& req) {
            return handle_health_request(req);
        });

    // Detailed health check
    app_.route_dynamic("/health/detailed")
        .methods("GET"_method)
        ([this](const crow::request& /* req */) {
        try {
            nlohmann::json health;
            health["status"] = "healthy";
            health["service"] = "ClaudeGateway";
            health["version"] = "3.2.0";
            health["uptime"] = metrics_.get_uptime_seconds();
            health["total_requests"] = metrics_.total_requests.load();

            if (manager_) {
                health["gateway_status"] = manager_->is_initialized() ? "initialized" : "not_initialized";
                health["healthy_providers"] = manager_->get_healthy_providers().size();
                health["unhealthy_providers"] = manager_->get_unhealthy_providers().size();
            }

            crow::response resp(200, health.dump());
            setup_cors_headers(resp);
            return resp;

        } catch (const std::exception& e) {
            return create_error_response(500, "HEALTH_ERROR", e.what());
        }
    });

    aimux::debug("ClaudeGateway: Health routes configured");
}

crow::response ClaudeGateway::handle_messages_endpoint(const crow::request& req) {
    auto start_time = std::chrono::high_resolution_clock::now();

    try {
        // Validate request
        std::string validation_error;
        if (!validate_request(req, validation_error)) {
            metrics_.failed_requests++;
            return create_error_response(400, "INVALID_REQUEST", validation_error);
        }

        // Convert to core request
        core::Request core_req = convert_crow_request(req);

        // Route through gateway manager
        core::Response core_resp = manager_->route_request(core_req);

        // Calculate duration
        auto end_time = std::chrono::high_resolution_clock::now();
        double duration_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();

        // Update metrics
        update_metrics(core_resp, duration_ms);

        // Log request if enabled
        if (config_.request_logging) {
            log_request(req, core_resp, duration_ms);
        }

        // Convert response
        crow::response resp = convert_core_response(core_resp);

        // Set CORS headers
        setup_cors_headers(resp);

        // Trigger callback if set
        if (request_callback_) {
            request_callback_(core_req, core_resp, duration_ms);
        }

        return resp;

    } catch (const std::exception& e) {
        metrics_.failed_requests++;
        log_error("MESSAGES_ENDPOINT", e.what());

        if (error_callback_) {
            error_callback_("MESSAGES_ENDPOINT", e.what());
        }

        return create_error_response(500, "INTERNAL_ERROR", e.what());
    }
}

crow::response ClaudeGateway::handle_metrics_request(const crow::request& /* req */) {
    try {
        crow::response resp(200, get_detailed_metrics().dump());
        setup_cors_headers(resp);
        return resp;
    } catch (const std::exception& e) {
        return create_error_response(500, "METRICS_ERROR", e.what());
    }
}

crow::response ClaudeGateway::handle_health_request(const crow::request& /* req */) {
    try {
        bool healthy = running_.load() && manager_ && manager_->is_initialized();

        nlohmann::json health;
        health["status"] = healthy ? "healthy" : "unhealthy";
        health["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        crow::response resp(healthy ? 200 : 503, health.dump());
        setup_cors_headers(resp);
        return resp;
    } catch (const std::exception& e) {
        return create_error_response(500, "HEALTH_ERROR", e.what());
    }
}

crow::response ClaudeGateway::handle_config_request(const crow::request& req) {
    try {
        if (req.method == "GET"_method) {
            nlohmann::json config;
            config["claude_gateway"] = config_.to_json();
            if (manager_) {
                config["gateway"] = manager_->get_configuration();
            }

            crow::response resp(200, config.dump());
            setup_cors_headers(resp);
            return resp;
        } else if (req.method == "POST"_method) {
            // Update configuration (implement as needed)
            crow::response resp(501, "Configuration updates not implemented");
            setup_cors_headers(resp);
            return resp;
        }
    } catch (const std::exception& e) {
        return create_error_response(500, "CONFIG_ERROR", e.what());
    }

    return create_error_response(405, "METHOD_NOT_ALLOWED", "Only GET and POST supported");
}

crow::response ClaudeGateway::handle_providers_request(const crow::request& /* req */) {
    try {
        nlohmann::json providers_info;

        if (manager_) {
            providers_info["providers"] = manager_->get_provider_configs();
            providers_info["healthy"] = manager_->get_healthy_providers();
            providers_info["unhealthy"] = manager_->get_unhealthy_providers();
            providers_info["metrics"] = manager_->get_metrics();
        }

        crow::response resp(200, providers_info.dump());
        setup_cors_headers(resp);
        return resp;
    } catch (const std::exception& e) {
        return create_error_response(500, "PROVIDERS_ERROR", e.what());
    }
}

core::Request ClaudeGateway::convert_crow_request(const crow::request& req) {
    core::Request request;

    // Extract model from request body
    try {
        nlohmann::json body = nlohmann::json::parse(req.body);
        request.model = body.value("model", "claude-3-sonnet-20240229");
        request.method = "POST";
        request.data = body;
    } catch (const std::exception& e) {
        // If JSON parsing fails, create basic request
        request.model = "claude-3-sonnet-20240229";
        request.method = "POST";
        request.data = nlohmann::json::object();
    }

    return request;
}

crow::response ClaudeGateway::convert_core_response(const core::Response& resp) {
    crow::response crow_resp;

    if (resp.success) {
        crow_resp.code = 200;
        crow_resp.body = resp.data;

        // Set content type for JSON responses
        if (!resp.data.empty() && resp.data[0] == '{') {
            crow_resp.set_header("Content-Type", "application/json");
        }
    } else {
        // Parse error data for proper HTTP status
        try {
            nlohmann::json error_data = nlohmann::json::parse(resp.data);
            int status_code = error_data.value("error", nlohmann::json::object()).value("status", 500);
            crow_resp.code = status_code;
        } catch (...) {
            crow_resp.code = resp.status_code > 0 ? resp.status_code : 500;
        }

        crow_resp.body = resp.data;
        crow_resp.set_header("Content-Type", "application/json");
    }

    // Add headers from provider response
    if (!resp.provider_name.empty()) {
        crow_resp.set_header("X-Aimux-Provider", resp.provider_name);
    }
    if (resp.response_time_ms > 0) {
        crow_resp.set_header("X-Aimux-Response-Time", std::to_string(resp.response_time_ms));
    }

    return crow_resp;
}

crow::response ClaudeGateway::create_error_response(int status, const std::string& code, const std::string& message) {
    nlohmann::json error;
    error["error"]["type"] = "api_error";
    error["error"]["code"] = code;
    error["error"]["message"] = message;

    crow::response resp(status, error.dump());
    resp.set_header("Content-Type", "application/json");
    setup_cors_headers(resp);
    return resp;
}

bool ClaudeGateway::validate_request(const crow::request& req, std::string& error_msg) {
    // Check request size
    if (!is_request_size_valid(req)) {
        error_msg = "Request too large (max " + std::to_string(config_.max_request_size_mb) + "MB)";
        return false;
    }

    // Check content type
    auto content_type = req.get_header_value("Content-Type");
    if (content_type.empty() || content_type.find("application/json") == std::string::npos) {
        error_msg = "Content-Type must be application/json";
        return false;
    }

    // Validate JSON structure
    try {
        nlohmann::json body = nlohmann::json::parse(req.body);

        // Check required fields
        if (!body.contains("messages") || !body["messages"].is_array()) {
            error_msg = "Missing or invalid 'messages' field";
            return false;
        }

        if (body["messages"].empty()) {
            error_msg = "'messages' array cannot be empty";
            return false;
        }

    } catch (const nlohmann::json::exception& e) {
        error_msg = "Invalid JSON: " + std::string(e.what());
        return false;
    }

    return true;
}

bool ClaudeGateway::is_request_size_valid(const crow::request& req) {
    size_t max_size = config_.max_request_size_mb * 1024 * 1024;
    return req.body.length() <= max_size;
}

void ClaudeGateway::setup_cors_headers(crow::response& resp) {
    if (config_.enable_cors) {
        resp.set_header("Access-Control-Allow-Origin", config_.cors_origin);
        resp.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        resp.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization, X-Requested-With");
        resp.set_header("Access-Control-Max-Age", "86400");
    }
}

void ClaudeGateway::log_request(const crow::request& req, const core::Response& resp, double duration_ms) {
    aimux::info("Request: " + crow::method_name(req.method) + " " + std::string(req.url) +
                " -> " + std::string(resp.success ? "SUCCESS" : "FAILED") +
                " (" + std::to_string(resp.status_code) + ") in " +
                std::to_string(duration_ms) + "ms via " + resp.provider_name);
}

void ClaudeGateway::log_error(const std::string& type, const std::string& message) {
    aimux::error("ClaudeGateway[" + type + "]: " + message);
}

void ClaudeGateway::update_metrics(const core::Response& resp, double duration_ms) {
    if (!config_.enable_metrics) {
        return;
    }

    metrics_.total_requests++;
    metrics_.total_response_time_ms += duration_ms;

    if (resp.success) {
        metrics_.successful_requests++;
    } else {
        metrics_.failed_requests++;
    }
}

void ClaudeGateway::server_thread_func() {
    try {
        aimux::info("ClaudeGateway server thread starting on " + bind_address_ + ":" + std::to_string(port_));

        running_.store(true);

        // Configure and run the app
        app_.bindaddr(bind_address_).port(port_).multithreaded().run();

    } catch (const std::exception& e) {
        running_.store(false);
        aimux::error("ClaudeGateway server thread error: " + std::string(e.what()));
        handle_gateway_error("SERVER_THREAD", e.what());
    }
}

void ClaudeGateway::graceful_shutdown() {
    try {
        // Stop accepting new connections
        app_.stop();

        // Give existing connections time to complete
        std::this_thread::sleep_for(std::chrono::seconds(5));

        aimux::info("ClaudeGateway: Graceful shutdown completed");
    } catch (const std::exception& e) {
        aimux::error("ClaudeGateway graceful shutdown error: " + std::string(e.what()));
    }
}

bool ClaudeGateway::validate_configuration() {
    // Validate port
    if (port_ < 1 || port_ > 65535) {
        aimux::error("Invalid port: " + std::to_string(port_));
        return false;
    }

    // Validate bind address
    static const std::regex ip_regex(R"(^(\d{1,3}\.){3}\d{1,3}$|^[a-zA-Z0-9.-]+$)");
    if (!std::regex_match(bind_address_, ip_regex)) {
        aimux::error("Invalid bind address: " + bind_address_);
        return false;
    }

    // Validate other config parameters
    if (config_.max_request_size_mb < 1 || config_.max_request_size_mb > 100) {
        aimux::error("Invalid max request size: " + std::to_string(config_.max_request_size_mb) + "MB");
        return false;
    }

    return true;
}

void ClaudeGateway::handle_route_error(const std::string& route, const std::exception& e) {
    log_error("ROUTE_ERROR", route + ": " + e.what());
    metrics_.failed_requests++;
}

void ClaudeGateway::handle_gateway_error(const std::string& operation, const std::string& error) {
    log_error("GATEWAY_ERROR", operation + ": " + error);
    metrics_.failed_requests++;
}

} // namespace gateway
} // namespace aimux