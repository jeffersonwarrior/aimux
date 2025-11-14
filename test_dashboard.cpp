#include <iostream>
#include <memory>
#include <thread>
#include <chrono>

// Crow framework
#include <crow.h>
#include <nlohmann/json.hpp>

// Resource loader
#include "aimux/webui/resource_loader.hpp"

int main() {
    std::cout << "Testing Aimux2 Dashboard..." << std::endl;

    // Initialize resource loader
    aimux::webui::ResourceLoader::getInstance().initialize();

    // Create Crow app
    crow::SimpleApp app;

    // Dashboard route
    CROW_ROUTE(app, "/dashboard.html")([](const crow::request& /*req*/, crow::response& res) {
        auto& loader = aimux::webui::ResourceLoader::getInstance();
        const auto* resource = loader.getResource("/dashboard.html");

        if (resource) {
            res.write(resource->data);
            res.set_header("Content-Type", resource->content_type);
            res.end();
        } else {
            res.code = 404;
            res.end("Dashboard not found");
        }
    });

    // CSS route
    CROW_ROUTE(app, "/dashboard.css")([](const crow::request& /*req*/, crow::response& res) {
        auto& loader = aimux::webui::ResourceLoader::getInstance();
        const auto* resource = loader.getResource("/dashboard.css");

        if (resource) {
            res.write(resource->data);
            res.set_header("Content-Type", resource->content_type);
            res.end();
        } else {
            res.code = 404;
            res.end("CSS not found");
        }
    });

    // JS route
    CROW_ROUTE(app, "/dashboard.js")([](const crow::request& /*req*/, crow::response& res) {
        auto& loader = aimux::webui::ResourceLoader::getInstance();
        const auto* resource = loader.getResource("/dashboard.js");

        if (resource) {
            res.write(resource->data);
            res.set_header("Content-Type", resource->content_type);
            res.end();
        } else {
            res.code = 404;
            res.end("JavaScript not found");
        }
    });

    // API route for testing
    CROW_ROUTE(app, "/api/metrics")([]{
        nlohmann::json metrics = {
            {"total_requests", 150},
            {"successful_requests", 142},
            {"failed_requests", 8},
            {"providers", {
                {{"name", "synthetic"}, {"status", "healthy"}, {"response_time", 245}},
                {{"name", "cerebras"}, {"status", "degraded"}, {"response_time", 389}}
            }},
            {"uptime_seconds", 3600},
            {"memory_usage_mb", 45},
            {"cpu_usage_percent", 12.5}
        };
        return crow::response(200, metrics.dump());
    });

    // WebSocket route for real-time updates
    CROW_WEBSOCKET_ROUTE(app, "/ws")
    .onaccept([](const crow::request&, void**) {
        return true; // Accept all connections
    })
    .onopen([](crow::websocket::connection& conn) {
        std::cout << "WebSocket connection opened" << std::endl;

        // Send initial data
        nlohmann::json initial_data = {
            {"type", "dashboard_update"},
            {"data", {
                {"providers", {
                    {{"name", "synthetic"}, {"status", "healthy"}, {"response_time", 245}, {"requests", 89}},
                    {{"name", "cerebras"}, {"status", "degraded"}, {"response_time", 389}, {"requests", 61}}
                }},
                {"metrics", {
                    {"total_requests", 150},
                    {"success_rate", 94.7},
                    {"uptime_hours", 1.0},
                    {"memory_usage_mb", 45},
                    {"cpu_usage_percent", 12.5}
                }}
            }}
        };

        conn.send_text(initial_data.dump());
    })
    .onmessage([](crow::websocket::connection&, const std::string& data, bool) {
        std::cout << "WebSocket message received: " << data << std::endl;
    })
    .onclose([](crow::websocket::connection&, const std::string& reason, short unsigned int) {
        std::cout << "WebSocket connection closed: " << reason << std::endl;
    });

    std::cout << "Starting dashboard test server on http://localhost:18080" << std::endl;
    std::cout << "Open http://localhost:18080/dashboard.html in your browser" << std::endl;

    // Start server
    app.port(18080).multithreaded().run();

    return 0;
}