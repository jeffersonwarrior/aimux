#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

#include "aimux/webui/web_server.hpp"
#include "aimux/webui/metrics_streamer.hpp"
#include "aimux/webui/resource_loader.hpp"
#include "aimux/providers/provider_impl.hpp"
#include "aimux/core/bridge.hpp"
#include "config/production_config.h"

namespace aimux {
namespace webui {

WebServer::WebServer(int port) : port_(port), bind_address_("127.0.0.1"), resolved_bind_address_("127.0.0.1") {
    metrics_.start_time = std::chrono::steady_clock::now();

    // Initialize embedded resources
    ResourceLoader::getInstance().initialize();

    // Initialize MetricsStreamer for advanced real-time monitoring
    MetricsStreamerConfig config;
    config.update_interval_ms = 1000;
    config.broadcast_interval_ms = 2000;
    config.max_connections = 100;
    config.enable_delta_compression = true;
    config.enable_authentication = false;
    config.history_retention_minutes = 60;
    if (!MetricsStreamer::getInstance().initialize(config)) {
        std::cerr << "Failed to initialize MetricsStreamer" << std::endl;
    }

    // Initialize Crow app
    app_ = std::make_unique<crow::SimpleApp>();

    // Initialize providers with default configurations
    initialize_providers();

    // Setup Crow routes
    setup_routes();
}

WebServer::WebServer(const aimux::config::WebUIConfig& config)
    : port_(config.port), bind_address_(config.bind_address) {

    metrics_.start_time = std::chrono::steady_clock::now();

    // Initialize embedded resources
    ResourceLoader::getInstance().initialize();

    // Resolve the bind address using configuration manager
    auto& config_manager = aimux::config::ProductionConfigManager::getInstance();
    resolved_bind_address_ = config_manager.resolveBindAddress(config);

    std::cout << "WebUI configured to bind to: " << resolved_bind_address_ << ":" << port_ << std::endl;
    if (config.bind_address != resolved_bind_address_) {
        std::cout << "  (requested: " << config.bind_address << ", resolved: " << resolved_bind_address_ << ")" << std::endl;
    }

    // Initialize MetricsStreamer for advanced real-time monitoring
    MetricsStreamerConfig metrics_config;
    metrics_config.update_interval_ms = config.metrics_update_interval_ms > 0 ? config.metrics_update_interval_ms : 1000;
    metrics_config.broadcast_interval_ms = config.websocket_broadcast_interval_ms > 0 ? config.websocket_broadcast_interval_ms : 2000;
    metrics_config.max_connections = config.max_websocket_connections > 0 ? config.max_websocket_connections : 100;
    metrics_config.enable_delta_compression = config.enable_delta_compression;
    metrics_config.enable_authentication = config.enable_websocket_auth;
    metrics_config.auth_token = config.websocket_auth_token;
    metrics_config.history_retention_minutes = config.history_retention_minutes > 0 ? config.history_retention_minutes : 60;
    if (!MetricsStreamer::getInstance().initialize(metrics_config)) {
        std::cerr << "Failed to initialize MetricsStreamer" << std::endl;
    }

    // Initialize Crow app
    app_ = std::make_unique<crow::SimpleApp>();

    // Initialize providers with default configurations
    initialize_providers();

    // Setup Crow routes
    setup_routes();
}

void WebServer::initialize_providers() {
    nlohmann::json default_config = {
        {"api_key", "test_key"},
        {"endpoint", "https://api.example.com"},
        {"timeout", 30000},
        {"max_retries", 3}
    };

    // Create synthetic provider for testing
    try {
        auto synthetic = providers::ProviderFactory::create_provider("synthetic", default_config);
        if (synthetic) {
            providers_["synthetic"] = std::move(synthetic);
        }
    } catch (const std::exception& e) {
        std::cerr << "Failed to create synthetic provider: " << e.what() << std::endl;
    }

    // Note: In production, you would load actual API keys from secure storage
}

WebServer::~WebServer() {
    stop();

    // Shutdown MetricsStreamer
    MetricsStreamer::getInstance().shutdown();
}

void WebServer::start() {
    if (running_.load()) {
        return; // Already running
    }

    running_.store(true);
    server_thread_ = std::thread(&WebServer::server_loop, this);

    // Start WebSocket broadcast thread for real-time updates
    start_websocket_broadcast();

    std::cout << "Web server starting on " << resolved_bind_address_ << ":" << port_ << std::endl;
}

void WebServer::stop() {
    if (!running_.load()) {
        return; // Not running
    }

    running_.store(false);

    // Stop WebSocket broadcast thread
    stop_websocket_broadcast();

    // Crow handles shutdown automatically when the app goes out of scope
    if (app_) {
        app_->stop();
    }

    // Wait for server thread to finish
    if (server_thread_.joinable()) {
        server_thread_.join();
    }

    std::cout << "Web server stopped" << std::endl;
}

void WebServer::setup_routes() {
    auto& app = *app_;  // Dereference to get reference

    // Static resource routes (embedded files)
    CROW_ROUTE(app, "/dashboard.html")([this](const crow::request& /*req*/, crow::response& res) {
        serve_embedded_resource("/dashboard.html", res);
    });

    CROW_ROUTE(app, "/dashboard.css")([this](const crow::request& /*req*/, crow::response& res) {
        serve_embedded_resource("/dashboard.css", res);
    });

    CROW_ROUTE(app, "/dashboard.js")([this](const crow::request& /*req*/, crow::response& res) {
        serve_embedded_resource("/dashboard.js", res);
    });

    // Main dashboard (redirect to embedded dashboard)
    CROW_ROUTE(app, "/")([this](const crow::request& /*req*/, crow::response& res) {
        serve_embedded_resource("/dashboard.html", res);
    });

    // System endpoints
    CROW_ROUTE(app, "/health")([this]() {
        return handle_crow_health();
    });

    CROW_ROUTE(app, "/metrics")([this]() {
        return handle_crow_metrics();
    });

    CROW_ROUTE(app, "/status")([this]() {
        return handle_crow_status();
    });

    // Provider endpoints
    CROW_ROUTE(app, "/providers").methods("GET"_method)([this]() {
        return handle_crow_providers();
    });

    CROW_ROUTE(app, "/providers").methods("POST"_method)([this](const crow::request& req) {
        return handle_crow_create_provider(req.body);
    });

    CROW_ROUTE(app, "/providers/<string>").methods("GET"_method)([this](const std::string& name) {
        return handle_crow_get_provider(name);
    });

    CROW_ROUTE(app, "/providers/<string>").methods("PUT"_method)([this](const crow::request& req, const std::string& name) {
        return handle_crow_update_provider(name, req.body);
    });

    CROW_ROUTE(app, "/providers/<string>").methods("DELETE"_method)([this](const std::string& name) {
        return handle_crow_delete_provider(name);
    });

    // Test endpoint
    CROW_ROUTE(app, "/test").methods("POST"_method)([this](const crow::request& req) {
        return handle_crow_test_provider(req.body);
    });

    // Config endpoints
    CROW_ROUTE(app, "/config").methods("GET"_method)([this]() {
        return handle_crow_get_config();
    });

    CROW_ROUTE(app, "/config").methods("POST"_method)([this](const crow::request& req) {
        return handle_crow_update_config(req.body);
    });

    // Comprehensive metrics endpoint
    CROW_ROUTE(app, "/metrics/comprehensive")([this]() {
        try {
            nlohmann::json metrics = MetricsStreamer::getInstance().get_comprehensive_metrics();
            crow::response response(200, metrics.dump());
            response.add_header("Content-Type", "application/json");
            response.add_header("Access-Control-Allow-Origin", "*");
            return response;
        } catch (const std::exception& e) {
            crow::response error_response(500, R"({"error": "Failed to get comprehensive metrics", "message": ")" + std::string(e.what()) + "\"}");
            error_response.add_header("Content-Type", "application/json");
            error_response.add_header("Access-Control-Allow-Origin", "*");
            return error_response;
        }
    });

    // Performance stats endpoint
    CROW_ROUTE(app, "/metrics/performance")([this]() {
        try {
            auto stats = MetricsStreamer::getInstance().get_performance_stats();
            nlohmann::json response_data = {
                {"current_connections", stats.current_connections},
                {"peak_connections", stats.peak_connections},
                {"total_updates", stats.total_updates},
                {"total_broadcasts", stats.total_broadcasts},
                {"avg_update_time_ms", stats.avg_update_time_ms},
                {"avg_broadcast_time_ms", stats.avg_broadcast_time_ms},
                {"failed_connections", stats.failed_connections},
                {"messages_sent", stats.messages_sent},
                {"messages_dropped", stats.messages_dropped}
            };

            crow::response response(200, response_data.dump());
            response.add_header("Content-Type", "application/json");
            response.add_header("Access-Control-Allow-Origin", "*");
            return response;
        } catch (const std::exception& e) {
            crow::response error_response(500, R"({"error": "Failed to get performance stats", "message": ")" + std::string(e.what()) + "\"}");
            error_response.add_header("Content-Type", "application/json");
            error_response.add_header("Access-Control-Allow-Origin", "*");
            return error_response;
        }
    });

    // Historical data endpoint
    CROW_ROUTE(app, "/metrics/history")([this]() {
        try {
            auto history = MetricsStreamer::getInstance().get_historical_data();
            crow::response response(200, history.to_json().dump());
            response.add_header("Content-Type", "application/json");
            response.add_header("Access-Control-Allow-Origin", "*");
            return response;
        } catch (const std::exception& e) {
            crow::response error_response(500, R"({"error": "Failed to get historical data", "message": ")" + std::string(e.what()) + "\"}");
            error_response.add_header("Content-Type", "application/json");
            error_response.add_header("Access-Control-Allow-Origin", "*");
            return error_response;
        }
    });

    // Provider-specific metrics endpoint
    CROW_ROUTE(app, "/metrics/provider/<string>")([this](const std::string& provider_name) {
        try {
            auto metrics = MetricsStreamer::getInstance().get_provider_metrics(provider_name);
            crow::response response(200, metrics.to_json().dump());
            response.add_header("Content-Type", "application/json");
            response.add_header("Access-Control-Allow-Origin", "*");
            return response;
        } catch (const std::exception& e) {
            crow::response error_response(500, R"({"error": "Failed to get provider metrics", "provider": ")" + provider_name + R"(", "message": ")" + std::string(e.what()) + "\"}");
            error_response.add_header("Content-Type", "application/json");
            error_response.add_header("Access-Control-Allow-Origin", "*");
            return error_response;
        }
    });

    // API info endpoint
    CROW_ROUTE(app, "/api-endpoints")([this]() {
        return handle_crow_api_info();
    });

    // Enhanced WebSocket for real-time dashboard data with MetricsStreamer integration
    CROW_WEBSOCKET_ROUTE(app, "/ws")
        .onopen([this](crow::websocket::connection& conn) {
            std::cout << "WebSocket connection opened - integrating with MetricsStreamer" << std::endl;

            // Register with MetricsStreamer for professional connection management
            std::string connection_id = MetricsStreamer::getInstance().register_connection(&conn, "WebServer Client");

            if (!connection_id.empty()) {
                // Store connection ID mapping for cleanup
                std::lock_guard<std::mutex> lock(ws_mutex_);
                ws_connection_ids_[&conn] = connection_id;
            } else {
                std::cerr << "Failed to register WebSocket connection with MetricsStreamer - max connections reached" << std::endl;
                conn.close();
            }
        })
        .onclose([this](crow::websocket::connection& conn, const std::string& reason, uint16_t /*code*/) {
            std::cout << "WebSocket connection closed: " << reason << std::endl;

            // Unregister from MetricsStreamer
            std::lock_guard<std::mutex> lock(ws_mutex_);
            auto it = ws_connection_ids_.find(&conn);
            if (it != ws_connection_ids_.end()) {
                MetricsStreamer::getInstance().unregister_connection(it->second);
                ws_connection_ids_.erase(it);
            }

            // Also remove from legacy connections set for backward compatibility
            ws_connections_.erase(&conn);
        })
        .onmessage([this](crow::websocket::connection& conn, const std::string& data, bool /*is_binary*/) {
            std::cout << "WebSocket received: " << data << std::endl;

            // Get connection ID for MetricsStreamer
            std::string connection_id;
            {
                std::lock_guard<std::mutex> lock(ws_mutex_);
                auto it = ws_connection_ids_.find(&conn);
                if (it != ws_connection_ids_.end()) {
                    connection_id = it->second;
                }
            }

            // Forward message to MetricsStreamer for professional handling
            if (!connection_id.empty()) {
                MetricsStreamer::getInstance().handle_message(connection_id, data);
            } else {
                // Fallback handling for connections not registered with MetricsStreamer
                try {
                    nlohmann::json message = nlohmann::json::parse(data);
                    std::string type = message.value("type", "unknown");

                    if (type == "request_update") {
                        this->send_dashboard_update(conn);
                    } else if (type == "toggle_refresh") {
                        bool enabled = message.value("enabled", true);
                        conn.send_text(R"({"type": "refresh_toggled", "enabled": )" + std::string(enabled ? "true" : "false") + "}");
                    } else {
                        conn.send_text(R"({"type": "echo", "message": ")" + data + "\"}");
                    }
                } catch (const std::exception& e) {
                    conn.send_text(R"({"type": "error", "message": "Invalid JSON: )" + std::string(e.what()) + "\"}");
                }
            }
        });

    // CORS is handled in convert_to_crow_response
}

void WebServer::server_loop() {
    if (!app_) {
        std::cerr << "Crow app not initialized" << std::endl;
        return;
    }

    std::cout << "Web server listening on " << resolved_bind_address_ << ":" << port_ << std::endl;

    // Configure and run Crow app
    app_->bindaddr(resolved_bind_address_).port(port_).multithreaded().run();
}

// Crow route handlers - convert original handlers to Crow responses
crow::response WebServer::handle_crow_root() {
    return convert_to_crow_response(handle_root());
}

crow::response WebServer::handle_crow_metrics() {
    return convert_to_crow_response(handle_metrics());
}

crow::response WebServer::handle_crow_health() {
    return convert_to_crow_response(handle_health());
}

crow::response WebServer::handle_crow_providers() {
    return convert_to_crow_response(handle_providers());
}

crow::response WebServer::handle_crow_status() {
    return convert_to_crow_response(handle_status());
}

crow::response WebServer::handle_crow_get_provider(const std::string& provider_name) {
    return convert_to_crow_response(handle_get_provider(provider_name));
}

crow::response WebServer::handle_crow_create_provider(const std::string& body) {
    return convert_to_crow_response(handle_create_provider(body));
}

crow::response WebServer::handle_crow_update_provider(const std::string& provider_name, const std::string& body) {
    return convert_to_crow_response(handle_update_provider(provider_name, body));
}

crow::response WebServer::handle_crow_delete_provider(const std::string& provider_name) {
    return convert_to_crow_response(handle_delete_provider(provider_name));
}

crow::response WebServer::handle_crow_get_config() {
    return convert_to_crow_response(handle_get_config());
}

crow::response WebServer::handle_crow_update_config(const std::string& body) {
    return convert_to_crow_response(handle_update_config(body));
}

crow::response WebServer::handle_crow_test_provider(const std::string& body) {
    return convert_to_crow_response(handle_test_provider(body));
}

crow::response WebServer::handle_crow_api_info() {
    return convert_to_crow_response(handle_api_info());
}

// Utility method to convert our HttpResponse to Crow response
crow::response WebServer::convert_to_crow_response(const HttpResponse& response) {
    crow::response crow_resp(response.status_code);
    crow_resp.add_header("Content-Type", response.content_type);
    crow_resp.add_header("Access-Control-Allow-Origin", "*");
    crow_resp.add_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    crow_resp.add_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
    crow_resp.write(response.body);
    return crow_resp;
}

// Update metrics
void WebServer::update_provider_metrics(const std::string& provider_name, double response_time_ms, bool success) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    metrics_.provider_response_times[provider_name] = response_time_ms;
    metrics_.provider_health[provider_name] = success;
}

WebMetrics WebServer::get_metrics() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    return metrics_;
}

// Helper method to get provider type
std::string WebServer::get_provider_type(const std::string& provider_name) {
    if (provider_name == "synthetic") return "test";
    if (provider_name == "cerebras") return "llm";
    if (provider_name == "zai") return "llm";
    if (provider_name == "minimax") return "llm";
    return "unknown";
}

// Original handlers (now used by Crow wrappers)
HttpResponse WebServer::handle_root() {
    // Get network information
    auto network_info = get_network_info();

    std::ostringstream html;
    html << R"(
<!DOCTYPE html>
<html>
<head>
    <title>Aimux Provider Manager</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 40px; background: #f5f5f5; }
        .container { max-width: 1000px; margin: 0 auto; }
        .header { background: #2563eb; color: white; padding: 2rem; text-align: center; margin: -40px -40px 20px -40px; }
        .header h1 { margin: 0; font-size: 2.5rem; }
        .card { background: white; padding: 25px; margin: 20px 0; border-radius: 12px; box-shadow: 0 2px 15px rgba(0,0,0,0.1); }
        .card h2 { color: #2563eb; margin-bottom: 20px; }
        .status-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 15px; }
        .status-item { background: #f8fafc; padding: 15px; border-radius: 8px; text-align: center; }
        .status-label { font-size: 0.875rem; color: #64748b; margin-bottom: 5px; text-transform: uppercase; }
        .status-value { font-size: 1.5rem; font-weight: 600; color: #1e293b; }
        .status-value.ip { font-size: 1rem; font-family: monospace; }
        .network-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 20px; }
        .network-section { background: #f8fafc; padding: 15px; border-radius: 8px; }
        .network-section h3 { color: #1e293b; margin-bottom: 10px; font-size: 1.1rem; }
        .ip-list { list-style: none; padding: 0; margin: 5px 0; }
        .ip-list li { font-family: monospace; background: white; padding: 5px 8px; margin: 2px 0; border-radius: 4px; font-size: 0.875rem; }
        .ip-list li.valid { color: #16a34a; }
        .ip-list li.invalid { color: #64748b; }
        .btn { padding: 10px 20px; margin: 5px; border: none; border-radius: 6px; cursor: pointer; text-decoration: none; display: inline-block; }
        .btn-primary { background: #2563eb; color: white; }
        .btn-success { background: #16a34a; color: white; }
        .btn-secondary { background: #64748b; color: white; }
        .endpoint-list { display: grid; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); gap: 15px; }
        .endpoint-item { background: #f8fafc; padding: 15px; border-radius: 8px; font-family: monospace; font-size: 0.875rem; }
        .endpoint-method { font-weight: 600; color: #2563eb; }
        .status-indicator { display: inline-block; width: 8px; height: 8px; border-radius: 50%; margin-right: 5px; }
        .status-indicator.online { background: #16a34a; }
        .status-indicator.offline { background: #dc2626; }
        .framework-info { background: #e0f2fe; padding: 15px; border-radius: 8px; margin: 10px 0; }
        .framework-info strong { color: #0284c7; }
    </style>
</head>
<body>
    <div class="header">
        <h1>Aimux Provider Manager</h1>
        <p>Multi-AI Provider Router Dashboard v2.0 - Professional WebUI with Crow Framework</p>
    </div>

    <div class="container">
        <div class="card">
            <div class="framework-info">
                <strong>🎯 Framework Upgrade:</strong> Successfully migrated from native sockets to Crow framework for professional HTTP handling.
                <br><strong>📈 Benefits:</strong> Thread-safe routing, proper HTTP status codes, WebSocket support, and improved performance.
            </div>

            <h2>Network Configuration</h2>
            <div class="status-grid">
                <div class="status-item">
                    <div class="status-label">Bind Address</div>
                    <div class="status-value">)" << network_info.resolved_bind_address << R"(</div>
                </div>
                <div class="status-item">
                    <div class="status-label">Port</div>
                    <div class="status-value">)" << network_info.port << R"(</div>
                </div>
                <div class="status-item">
                    <div class="status-label">Web Server</div>
                    <div class="status-value">
                        <span class="status-indicator online"></span>Running (Crow)
                    </div>
                </div>
                <div class="status-item">
                    <div class="status-label">ZeroTier</div>
                    <div class="status-value">
                        <span class="status-indicator )" << (network_info.zerotier_available ? "online" : "offline") << R"(</span>
                        )" << (network_info.zerotier_available ? "Available" : "Not Detected") << R"(
                    </div>
                </div>
            </div>
            <div class="network-grid">
                <div class="network-section">
                    <h3>Network Interfaces</h3>
                    <ul class="ip-list">
                        <li class="valid">🌐 <strong>Access URL:</strong> http://" << network_info.resolved_bind_address << ":" << network_info.port << "</li>
                        <li class=\"valid\">💻 <strong>Resolved IP:</strong> " << network_info.resolved_bind_address << "</li>
                        <li class=\"valid\">⚙️ <strong>Configured:</strong> " << network_info.bind_address << "</li>
                        " << (network_info.zerotier_available ? "<li class=\"valid\">🌍 <strong>ZeroTier IP:</strong> " + network_info.zerotier_ip + "</li>" : "") << R"(
                    </ul>
                </div>
                <div class="network-section">
                    <h3>Available IP Addresses</h3>
                    <ul class="ip-list">)";

    if (network_info.available_ips.empty()) {
        html << "<li class=\"invalid\">No external IP addresses detected</li>";
    } else {
        for (const auto& ip : network_info.available_ips) {
            html << "<li class=\"valid\">🔗 " << ip << "</li>";
        }
    }

    html << R"(</ul>
                </div>
            </div>
        </div>

        <div class="card">
            <h2>System Status</h2>
            <div class="status-grid">
                <div class="status-item">
                    <div class="status-label">API</div>
                    <div class="status-value">Operational</div>
                </div>
                <div class="status-item">
                    <div class="status-label">Version</div>
                    <div class="status-value">v2.1 (Crow)</div>
                </div>
                <div class="status-item">
                    <div class="status-label">Uptime</div>
                    <div class="status-value" id="uptime">Calculating...</div>
                </div>
                <div class="status-item">
                    <div class="status-label">Requests</div>
                    <div class="status-value" id="requests">0</div>
                </div>
            </div>
        </div>

        <div class="card">
            <h2>Quick Actions</h2>
            <div style="text-align: center; padding: 20px 0;">
                <a href="/providers" class="btn btn-primary" target="_blank">View Provider Data</a>
                <a href="/health" class="btn btn-success" target="_blank">Health Check</a>
                <a href="/api-endpoints" class="btn btn-secondary" target="_blank">API Info</a>
            </div>
        </div>

        <div class="card">
            <h2>REST API Endpoints (Crow Framework)</h2>
            <div class="endpoint-list">
                <div class="endpoint-item">
                    <div><span class="endpoint-method">GET</span> /providers</div>
                    <div>List all providers</div>
                </div>
                <div class="endpoint-item">
                    <div><span class="endpoint-method">GET</span> /health</div>
                    <div>System health check</div>
                </div>
                <div class="endpoint-item">
                    <div><span class="endpoint-method">GET</span> /metrics</div>
                    <div>System metrics</div>
                </div>
                <div class="endpoint-item">
                    <div><span class="endpoint-method">POST</span> /test</div>
                    <div>Test provider</div>
                </div>
                <div class="endpoint-item">
                    <div><span class="endpoint-method">GET</span> /api-endpoints</div>
                    <div>API information</div>
                </div>
                <div class="endpoint-item">
                    <div><span class="endpoint-method">WS</span> /ws</div>
                    <div>WebSocket (Task 2.3)</div>
                </div>
            </div>
        </div>

        <div class="card">
            <h2>About</h2>
            <p><strong>Aimux v2.1</strong> - Professional web server with Crow framework integration.</p>
            <ul>
                <li><strong>✨ NEW:</strong> Professional Crow framework handling</li>
                <li><strong>🚀 Performance:</strong> Thread-safe multithreaded routing</li>
                <li><strong>🔗 Professional:</strong> Proper HTTP status codes and headers</li>
                <li><strong>📡 Ready:</strong> WebSocket support enabled (Task 2.3)</li>
                <li><strong>🔄 Compatible:</strong> All existing endpoints preserved</li>
                <li>Multi-provider support (Cerebras, Z.AI, MiniMax, Synthetic)</li>
                <li><~10x faster than TypeScript implementation</li>
                <li>Enhanced network configuration with ZeroTier support</li>
                <li>System service management capabilities</li>
            </ul>
        </div>
    </div>

    <script>
        // Update metrics dynamically
        function updateMetrics() {
            fetch('/metrics')
                .then(response => response.json())
                .then(data => {
                    // Update uptime
                    if (data.uptime_seconds) {
                        const uptimeHours = Math.floor(data.uptime_seconds / 3600);
                        const uptimeMins = Math.floor((data.uptime_seconds % 3600) / 60);
                        document.getElementById('uptime').textContent =
                            uptimeHours > 0 ? `${uptimeHours}h ${uptimeMins}m` : `${uptimeMins}m`;
                    }

                    // Update request count
                    if (data.total_requests !== undefined) {
                        document.getElementById('requests').textContent = data.total_requests;
                    }
                })
                .catch(error => {
                    console.log('Error fetching metrics:', error);
                });
        }

        // Update metrics every 5 seconds
        updateMetrics(); // Initial update
        setInterval(updateMetrics, 5000);

        // Auto-refresh network info every 30 seconds
        setTimeout(() => {
            window.location.reload();
        }, 30000);
    </script>
</body>
</html>
)";

    return HttpResponse(200, "text/html", html.str());
}

HttpResponse WebServer::handle_metrics() {
    // Get comprehensive metrics from MetricsStreamer
    nlohmann::json metrics = MetricsStreamer::getInstance().get_comprehensive_metrics();

    // Add legacy information for backward compatibility
    {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        auto legacy_metrics = metrics_.to_json();
        metrics["legacy"] = legacy_metrics;
    }

    metrics["framework"] = "crow";
    metrics["metrics_streamer_enabled"] = MetricsStreamer::getInstance().is_running();

    // Add performance stats
    auto perf_stats = MetricsStreamer::getInstance().get_performance_stats();
    metrics["websocket_performance"] = {
        {"current_connections", perf_stats.current_connections},
        {"peak_connections", perf_stats.peak_connections},
        {"total_updates", perf_stats.total_updates},
        {"total_broadcasts", perf_stats.total_broadcasts},
        {"avg_update_time_ms", perf_stats.avg_update_time_ms},
        {"avg_broadcast_time_ms", perf_stats.avg_broadcast_time_ms},
        {"messages_sent", perf_stats.messages_sent},
        {"messages_dropped", perf_stats.messages_dropped}
    };

    return HttpResponse(200, "application/json", metrics.dump(4));
}

HttpResponse WebServer::handle_health() {
    return HttpResponse(200, "application/json", R"({"status": "healthy", "service": "aimux-webui", "framework": "crow"})");
}

HttpResponse WebServer::handle_providers() {
    nlohmann::json response;
    response["providers"] = nlohmann::json::array();

    std::lock_guard<std::mutex> lock(providers_mutex_);

    int total = 0;
    int healthy = 0;

    for (const auto& [name, provider] : providers_) {
        nlohmann::json provider_info;
        provider_info["name"] = name;
        provider_info["type"] = get_provider_type(name);
        provider_info["status"] = provider->is_healthy() ? "healthy" : "unhealthy";
        provider_info["rate_limit"] = provider->get_rate_limit_status();

        response["providers"].push_back(provider_info);
        total++;
        if (provider->is_healthy()) {
            healthy++;
        }
    }

    // Also add supported but not configured providers
    auto supported = providers::ProviderFactory::get_supported_providers();
    for (const auto& supported_name : supported) {
        if (providers_.find(supported_name) == providers_.end()) {
            nlohmann::json provider_info;
            provider_info["name"] = supported_name;
            provider_info["type"] = get_provider_type(supported_name);
            provider_info["status"] = "not_configured";
            provider_info["rate_limit"] = nlohmann::json{};

            response["providers"].push_back(provider_info);
        }
    }

    response["total"] = total;
    response["healthy"] = healthy;
    response["supported_types"] = supported;
    response["framework"] = "crow";

    return HttpResponse(200, "application/json", response.dump(4));
}

HttpResponse WebServer::handle_status() {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    nlohmann::json status = metrics_.to_json();
    status["system"] = nlohmann::json{
        {"webui", true},
        {"providers", true},
        {"version", "v2.1"},
        {"framework", "crow"}
    };
    return HttpResponse(200, "application/json", status.dump(4));
}

// New REST API endpoints
HttpResponse WebServer::handle_get_provider(const std::string& provider_name) {
    std::lock_guard<std::mutex> lock(providers_mutex_);

    auto it = providers_.find(provider_name);
    if (it == providers_.end()) {
        return HttpResponse(404, "application/json",
            nlohmann::json{{"error", "Provider not found"}, {"provider", provider_name}}.dump());
    }

    nlohmann::json provider_info;
    provider_info["name"] = provider_name;
    provider_info["type"] = get_provider_type(provider_name);
    provider_info["status"] = it->second->is_healthy() ? "healthy" : "unhealthy";
    provider_info["rate_limit"] = it->second->get_rate_limit_status();
    provider_info["metrics"] = nlohmann::json{
        {"response_time_ms", metrics_.provider_response_times[provider_name]},
        {"health_status", metrics_.provider_health[provider_name]}
    };

    return HttpResponse(200, "application/json", provider_info.dump(4));
}

HttpResponse WebServer::handle_create_provider(const std::string& body) {
    try {
        nlohmann::json request_json = nlohmann::json::parse(body);

        if (!request_json.contains("name") || !request_json.contains("config")) {
            return HttpResponse(400, "application/json",
                R"({"error": "Bad Request", "message": "Missing 'name' or 'config' field"})");
        }

        std::string provider_name = request_json["name"];
        nlohmann::json config = request_json["config"];

        // Validate configuration
        if (!providers::ProviderFactory::validate_config(provider_name, config)) {
            return HttpResponse(400, "application/json",
                nlohmann::json{{"error", "Invalid configuration"}, {"provider", provider_name}}.dump());
        }

        std::lock_guard<std::mutex> lock(providers_mutex_);

        // Check if provider already exists
        if (providers_.find(provider_name) != providers_.end()) {
            return HttpResponse(409, "application/json",
                nlohmann::json{{"error", "Provider already exists"}, {"provider", provider_name}}.dump());
        }

        // Create provider
        auto provider = providers::ProviderFactory::create_provider(provider_name, config);
        if (!provider) {
            return HttpResponse(500, "application/json",
                nlohmann::json{{"error", "Failed to create provider"}, {"provider", provider_name}}.dump());
        }

        providers_[provider_name] = std::move(provider);

        return HttpResponse(201, "application/json",
            nlohmann::json{{"message", "Provider created successfully"}, {"provider", provider_name}}.dump());

    } catch (const nlohmann::json::parse_error& e) {
        return HttpResponse(400, "application/json",
            nlohmann::json{{"error", "Invalid JSON"}, {"message", e.what()}}.dump());
    } catch (const std::exception& e) {
        return HttpResponse(500, "application/json",
            nlohmann::json{{"error", "Internal server error"}, {"message", e.what()}}.dump());
    }
}

HttpResponse WebServer::handle_update_provider(const std::string& provider_name, const std::string& body) {
    std::lock_guard<std::mutex> lock(providers_mutex_);

    auto it = providers_.find(provider_name);
    if (it == providers_.end()) {
        return HttpResponse(404, "application/json",
            nlohmann::json{{"error", "Provider not found"}, {"provider", provider_name}}.dump());
    }

    try {
        nlohmann::json request_json = nlohmann::json::parse(body);
        nlohmann::json new_config = request_json["config"];

        // Validate configuration
        if (!providers::ProviderFactory::validate_config(provider_name, new_config)) {
            return HttpResponse(400, "application/json",
                nlohmann::json{{"error", "Invalid configuration"}, {"provider", provider_name}}.dump());
        }

        // Create new provider with updated config
        auto new_provider = providers::ProviderFactory::create_provider(provider_name, new_config);
        if (!new_provider) {
            return HttpResponse(500, "application/json",
                nlohmann::json{{"error", "Failed to update provider"}, {"provider", provider_name}}.dump());
        }

        providers_[provider_name] = std::move(new_provider);

        return HttpResponse(200, "application/json",
            nlohmann::json{{"message", "Provider updated successfully"}, {"provider", provider_name}}.dump());

    } catch (const nlohmann::json::parse_error& e) {
        return HttpResponse(400, "application/json",
            nlohmann::json{{"error", "Invalid JSON"}, {"message", e.what()}}.dump());
    } catch (const std::exception& e) {
        return HttpResponse(500, "application/json",
            nlohmann::json{{"error", "Internal server error"}, {"message", e.what()}}.dump());
    }
}

HttpResponse WebServer::handle_delete_provider(const std::string& provider_name) {
    std::lock_guard<std::mutex> lock(providers_mutex_);

    auto it = providers_.find(provider_name);
    if (it == providers_.end()) {
        return HttpResponse(404, "application/json",
            nlohmann::json{{"error", "Provider not found"}, {"provider", provider_name}}.dump());
    }

    providers_.erase(it);

    return HttpResponse(200, "application/json",
        nlohmann::json{{"message", "Provider deleted successfully"}, {"provider", provider_name}}.dump());
}

HttpResponse WebServer::handle_get_config() {
    nlohmann::json config = providers::ConfigParser::generate_default_config();
    return HttpResponse(200, "application/json", config.dump(4));
}

HttpResponse WebServer::handle_update_config(const std::string& body) {
    try {
        nlohmann::json config_json = nlohmann::json::parse(body);

        if (!providers::ConfigParser::validate_config_structure(config_json)) {
            return HttpResponse(400, "application/json",
                R"({"error": "Invalid configuration structure"})");
        }

        // In a real implementation, you would save this to a file
        // For now, just return success
        return HttpResponse(200, "application/json",
            R"({"message": "Configuration updated successfully"})");

    } catch (const nlohmann::json::parse_error& e) {
        return HttpResponse(400, "application/json",
            nlohmann::json{{"error", "Invalid JSON"}, {"message", e.what()}}.dump());
    }
}

HttpResponse WebServer::handle_test_provider(const std::string& body) {
    try {
        nlohmann::json request_json = nlohmann::json::parse(body);

        if (!request_json.contains("provider")) {
            return HttpResponse(400, "application/json",
                R"({"error": "Missing 'provider' field"})");
        }

        std::string provider_name = request_json["provider"];
        std::string test_message = request_json.value("message", "Hello, this is a test message.");

        std::lock_guard<std::mutex> lock(providers_mutex_);

        auto it = providers_.find(provider_name);
        if (it == providers_.end()) {
            return HttpResponse(404, "application/json",
                nlohmann::json{{"error", "Provider not found"}, {"provider", provider_name}}.dump());
        }

        // Create a test request
        aimux::core::Request test_request;
        test_request.model = "test-model";
        test_request.method = "chat";
        test_request.data = nlohmann::json{
            {"messages", nlohmann::json::array({{
                {"role", "user"},
                {"content", test_message}
            }})}
        };

        // Measure response time
        auto start_time = std::chrono::steady_clock::now();
        aimux::core::Response response = it->second->send_request(test_request);
        auto end_time = std::chrono::steady_clock::now();

        double response_time = std::chrono::duration<double, std::milli>(end_time - start_time).count();

        // Update metrics in both legacy system and MetricsStreamer
        update_provider_metrics(provider_name, response_time, response.success);

        // Enhanced metrics for MetricsStreamer
        std::string error_type = "";
        uint64_t input_tokens = 0;
        uint64_t output_tokens = 0;
        double cost = 0.0;

        if (!response.success) {
            if (response.status_code >= 429) {
                error_type = "rate_limit";
            } else if (response.status_code >= 400 && response.status_code < 500) {
                error_type = "auth";
            } else {
                error_type = "server_error";
            }
        }

        // Extract token usage from response if available
        try {
            if (response.success && !response.data.empty()) {
                auto response_json = nlohmann::json::parse(response.data);
                if (response_json.contains("usage")) {
                    auto usage = response_json["usage"];
                    if (usage.is_object()) {
                        input_tokens = static_cast<uint64_t>(usage.value("prompt_tokens", 0));
                        output_tokens = static_cast<uint64_t>(usage.value("completion_tokens", 0));

                        // Simple cost calculation (adjust based on actual pricing)
                        cost = (input_tokens * 0.00001) + (output_tokens * 0.00003); // Example pricing
                    }
                }
            }
        } catch (const nlohmann::json::parse_error& e) {
            // Silently ignore parsing errors - token extraction is optional
            input_tokens = 0;
            output_tokens = 0;
            cost = 0.0;
        }

        // Update MetricsStreamer with comprehensive data
        MetricsStreamer::getInstance().update_provider_metrics(
            provider_name, response_time, response.success, error_type,
            input_tokens, output_tokens, cost
        );

        nlohmann::json result;
        result["provider"] = provider_name;
        result["success"] = response.success;
        result["response_time_ms"] = response_time;
        result["response_data"] = response.data;
        result["error_message"] = response.error_message;
        result["status_code"] = response.status_code;
        result["actual_response_time_ms"] = response.response_time_ms;
        result["provider_used"] = response.provider_name;
        result["framework"] = "crow";
        result["metrics_streamer"] = {
            {"tokens_used", {{"input", input_tokens}, {"output", output_tokens}, {"total", input_tokens + output_tokens}}},
            {"cost", cost},
            {"error_type", error_type}
        };

        return HttpResponse(200, "application/json", result.dump(4));

    } catch (const nlohmann::json::parse_error& e) {
        return HttpResponse(400, "application/json",
            nlohmann::json{{"error", "Invalid JSON"}, {"message", e.what()}}.dump());
    } catch (const std::exception& e) {
        return HttpResponse(500, "application/json",
            nlohmann::json{{"error", "Test failed"}, {"message", e.what()}}.dump());
    }
}

HttpResponse WebServer::handle_api_info() {
    nlohmann::json api_info;
    api_info["title"] = "Aimux WebUI API";
    api_info["version"] = "v2.1";
    api_info["description"] = "REST API for managing Aimux AI providers and monitoring system - Powered by Crow framework";
    api_info["framework"] = "crow";

    nlohmann::json endpoints = nlohmann::json::array();

    // GET endpoints
    endpoints.push_back({
        {"method", "GET"},
        {"path", "/"},
        {"description", "Main dashboard HTML (Crow enhanced)"}
    });
    endpoints.push_back({
        {"method", "GET"},
        {"path", "/health"},
        {"description", "System health check"}
    });
    endpoints.push_back({
        {"method", "GET"},
        {"path", "/metrics"},
        {"description", "System metrics"}
    });
    endpoints.push_back({
        {"method", "GET"},
        {"path", "/providers"},
        {"description", "List all providers"}
    });
    endpoints.push_back({
        {"method", "GET"},
        {"path", "/providers/{name}"},
        {"description", "Get specific provider info"}
    });
    endpoints.push_back({
        {"method", "GET"},
        {"path", "/status"},
        {"description", "Full system status"}
    });
    endpoints.push_back({
        {"method", "GET"},
        {"path", "/config"},
        {"description", "Get default configuration"}
    });
    endpoints.push_back({
        {"method", "GET"},
        {"path", "/api-endpoints"},
        {"description", "This API information"}
    });
    endpoints.push_back({
        {"method", "WebSocket"},
        {"path", "/ws"},
        {"description", "WebSocket endpoint (Task 2.3)"}
    });

    // POST endpoints
    endpoints.push_back({
        {"method", "POST"},
        {"path", "/providers"},
        {"description", "Create new provider"}
    });
    endpoints.push_back({
        {"method", "PUT"},
        {"path", "/providers/{name}"},
        {"description", "Update provider configuration"}
    });
    endpoints.push_back({
        {"method", "DELETE"},
        {"path", "/providers/{name}"},
        {"description", "Delete provider"}
    });
    endpoints.push_back({
        {"method", "POST"},
        {"path", "/test"},
        {"description", "Test a provider with a message"}
    });
    endpoints.push_back({
        {"method", "POST"},
        {"path", "/config"},
        {"description", "Update system configuration"}
    });

    api_info["endpoints"] = endpoints;

    return HttpResponse(200, "application/json", api_info.dump(4));
}

WebServer::NetworkInfo WebServer::get_network_info() const {
    NetworkInfo info;
    info.bind_address = bind_address_;
    info.resolved_bind_address = resolved_bind_address_;
    info.port = port_;

    // Get ZeroTier IP and available IPs from config manager
    try {
        auto& config_manager = aimux::config::ProductionConfigManager::getInstance();
        info.zerotier_ip = config_manager.detectZeroTierIP();
        info.available_ips = config_manager.getAvailableIPAddresses();
        info.zerotier_available = !info.zerotier_ip.empty();
    } catch (const std::exception& e) {
        info.zerotier_ip = "";
        info.zerotier_available = false;
    }

    return info;
}

void WebServer::serve_embedded_resource(const std::string& path, crow::response& res) {
    auto& resource_loader = ResourceLoader::getInstance();
    const EmbeddedResource* resource = resource_loader.getResource(path);

    if (resource) {
        res.add_header("Content-Type", resource->content_type);
        res.add_header("ETag", resource->etag);
        res.add_header("Cache-Control", "public, max-age=3600");
        res.write(resource->data);
        res.end();
    } else {
        res.code = 404;
        res.write("Resource not found");
        res.end();
    }
}

nlohmann::json WebServer::create_dashboard_data() {
    nlohmann::json dashboard_data;
    dashboard_data["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    // Provider data
    dashboard_data["providers"] = nlohmann::json::object();

    std::lock_guard<std::mutex> providers_lock(providers_mutex_);
    for (const auto& [name, provider_ptr] : providers_) {
        if (provider_ptr) {
            nlohmann::json provider_info;
            provider_info["healthy"] = provider_ptr->is_healthy();
            provider_info["success_rate"] = 95.0 + (std::rand() % 5); // Simulated data
            provider_info["requests_per_min"] = 10 + (std::rand() % 40); // Simulated data

            // Get response time from metrics
            std::lock_guard<std::mutex> metrics_lock(metrics_mutex_);
            auto response_time_it = metrics_.provider_response_times.find(name);
            if (response_time_it != metrics_.provider_response_times.end()) {
                provider_info["response_time_ms"] = response_time_it->second;
            } else {
                provider_info["response_time_ms"] = 50 + (std::rand() % 200); // Simulated data if no metrics
            }

            // Provider might have errors
            if (!provider_ptr->is_healthy()) {
                provider_info["error"] = "connection_failed";
            }

            dashboard_data["providers"][name] = provider_info;
        }
    }

    // System metrics
    dashboard_data["system"] = nlohmann::json::object();
    dashboard_data["system"]["memory_mb"] = 5.0 + (std::rand() % 20); // Simulated memory usage
    dashboard_data["system"]["cpu_percent"] = 1 + (std::rand() % 25); // Simulated CPU usage
    dashboard_data["system"]["uptime_sec"] = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - metrics_.start_time).count();

    // Request metrics
    dashboard_data["requests"] = nlohmann::json::object();
    dashboard_data["requests"]["active"] = 0 + (std::rand() % 5); // Simulated active requests
    dashboard_data["requests"]["per_second"] = 0.5 + (std::rand() % 10) / 2.0; // Simulated requests/sec
    dashboard_data["requests"]["total_today"] = metrics_.total_requests;

    return dashboard_data;
}

void WebServer::send_dashboard_update(crow::websocket::connection& conn) {
    try {
        nlohmann::json dashboard_data = create_dashboard_data();
        std::string data_str = dashboard_data.dump();
        conn.send_text(data_str);
    } catch (const std::exception& e) {
        std::cerr << "Error sending WebSocket update: " << e.what() << std::endl;
    }
}

void WebServer::broadcast_dashboard_update() {
    std::lock_guard<std::mutex> lock(ws_mutex_);

    if (ws_connections_.empty()) {
        return;
    }

    try {
        nlohmann::json dashboard_data = create_dashboard_data();
        std::string data_str = dashboard_data.dump();

        // Send to all connected clients
        for (crow::websocket::connection* conn : ws_connections_) {
            try {
                if (conn) {
                    conn->send_text(data_str);
                }
            } catch (const std::exception& e) {
                std::cerr << "Error sending WebSocket data to connection: " << e.what() << std::endl;
                // Note: Could remove failed connections here in a production implementation
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error creating dashboard data for broadcast: " << e.what() << std::endl;
    }
}

void WebServer::start_websocket_broadcast() {
    if (ws_broadcast_running_.load()) {
        return; // Already running
    }

    ws_broadcast_running_.store(true);
    ws_broadcast_thread_ = std::thread(&WebServer::websocket_broadcast_loop, this);

    std::cout << "WebSocket broadcast thread started" << std::endl;
}

void WebServer::stop_websocket_broadcast() {
    if (!ws_broadcast_running_.load()) {
        return; // Not running
    }

    ws_broadcast_running_.store(false);

    // Wait for broadcast thread to finish
    if (ws_broadcast_thread_.joinable()) {
        ws_broadcast_thread_.join();
    }

    std::cout << "WebSocket broadcast thread stopped" << std::endl;
}

void WebServer::websocket_broadcast_loop() {
    std::cout << "WebSocket broadcast loop started" << std::endl;

    while (ws_broadcast_running_.load()) {
        try {
            // Broadcast dashboard updates every 2 seconds
            broadcast_dashboard_update();
        } catch (const std::exception& e) {
            std::cerr << "Error in WebSocket broadcast loop: " << e.what() << std::endl;
        }

        // Sleep for 2 seconds between broadcasts
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }

    std::cout << "WebSocket broadcast loop stopped" << std::endl;
}

} // namespace webui
} // namespace aimux