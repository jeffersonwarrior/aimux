#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <map>
#include <set>
#include <tuple>
#include <chrono>
#include <memory>
#include <crow.h>
#include <nlohmann/json.hpp>

namespace aimux {
namespace core {
    struct Request;
    struct Response;
    class Bridge;
}}

namespace aimux {
namespace config {
    struct WebUIConfig;
}}

namespace aimux {
namespace webui {

/**
 * @brief Real-time metrics data structure
 */
struct WebMetrics {
    size_t total_requests = 0;
    size_t successful_requests = 0;
    size_t failed_requests = 0;
    std::map<std::string, double> provider_response_times;
    std::map<std::string, bool> provider_health;
    std::chrono::steady_clock::time_point start_time;
    
    nlohmann::json to_json() const {
        nlohmann::json j;
        j["total_requests"] = total_requests;
        j["successful_requests"] = successful_requests;
        j["failed_requests"] = failed_requests;
        j["provider_response_times"] = provider_response_times;
        j["provider_health"] = provider_health;
        
        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - start_time);
        j["uptime_seconds"] = uptime.count();
        
        return j;
    }
};

/**
 * @brief Simple HTTP response
 */
struct HttpResponse {
    int status_code = 200;
    std::string content_type = "application/json";
    std::string body;
    
    HttpResponse(int code = 200, const std::string& ct = "application/json", const std::string& b = "")
        : status_code(code), content_type(ct), body(b) {}
};

/**
 * @brief Professional HTTP server using Crow framework
 */
class WebServer {
public:
    explicit WebServer(int port = 8080);
    explicit WebServer(const aimux::config::WebUIConfig& config);
    ~WebServer();
    
    // Start server
    void start();
    
    // Stop server
    void stop();
    
    // Check if server is running
    bool is_running() const { return running_.load(); }
    
    // Update metrics
    void update_provider_metrics(const std::string& provider_name, double response_time_ms, bool success);

    // Get current metrics
    WebMetrics get_metrics() const;

    // Network information utilities
    struct NetworkInfo {
        std::string bind_address;
        std::string resolved_bind_address;
        int port;
        std::string zerotier_ip;
        std::vector<std::string> available_ips;
        bool zerotier_available;
    };

    NetworkInfo get_network_info() const;

private:
    int port_;
    std::string bind_address_;
    std::string resolved_bind_address_;
    std::atomic<bool> running_{false};
    std::thread server_thread_;

    // Crow framework app
    std::unique_ptr<crow::SimpleApp> app_;

    // Metrics (thread-safe)
    mutable std::mutex metrics_mutex_;
    WebMetrics metrics_;

    // Provider management (thread-safe)
    mutable std::mutex providers_mutex_;
    std::map<std::string, std::unique_ptr<aimux::core::Bridge>> providers_;

    // WebSocket connection management
    mutable std::mutex ws_mutex_;
    std::set<crow::websocket::connection*> ws_connections_;
    std::map<crow::websocket::connection*, std::string> ws_connection_ids_; // Maps connections to MetricsStreamer IDs
    std::thread ws_broadcast_thread_;
    std::atomic<bool> ws_broadcast_running_{false};

    // Initialization
    void initialize_providers();

    // Helper method to get provider type
    std::string get_provider_type(const std::string& provider_name);

    // Route setup
    void setup_routes();

    // Server loop (Crow handles this internally)
    void server_loop();

    // Crow route handlers (return Crow response)
    crow::response handle_crow_root();
    crow::response handle_crow_metrics();
    crow::response handle_crow_health();
    crow::response handle_crow_providers();
    crow::response handle_crow_status();

    // New REST API endpoints (Crow versions)
    crow::response handle_crow_get_provider(const std::string& provider_name);
    crow::response handle_crow_create_provider(const std::string& body);
    crow::response handle_crow_update_provider(const std::string& provider_name, const std::string& body);
    crow::response handle_crow_delete_provider(const std::string& provider_name);
    crow::response handle_crow_get_config();
    crow::response handle_crow_update_config(const std::string& body);
    crow::response handle_crow_test_provider(const std::string& body);
    crow::response handle_crow_api_info();

    // Original handlers (kept for backwards compatibility)
    HttpResponse handle_root();
    HttpResponse handle_metrics();
    HttpResponse handle_health();
    HttpResponse handle_providers();
    HttpResponse handle_status();
    HttpResponse handle_get_provider(const std::string& provider_name);
    HttpResponse handle_create_provider(const std::string& body);
    HttpResponse handle_update_provider(const std::string& provider_name, const std::string& body);
    HttpResponse handle_delete_provider(const std::string& provider_name);
    HttpResponse handle_get_config();
    HttpResponse handle_update_config(const std::string& body);
    HttpResponse handle_test_provider(const std::string& body);
    HttpResponse handle_api_info();

    // Utility methods
    crow::response convert_to_crow_response(const HttpResponse& response);
    std::string generate_html_response(const std::string& title, const std::string& body);

    // Resource serving
    void serve_embedded_resource(const std::string& path, crow::response& res);

    // WebSocket methods for real-time updates
    void send_dashboard_update(crow::websocket::connection& conn);
    void broadcast_dashboard_update();
    nlohmann::json create_dashboard_data();
    void start_websocket_broadcast();
    void stop_websocket_broadcast();
    void websocket_broadcast_loop();
};

} // namespace webui
} // namespace aimux