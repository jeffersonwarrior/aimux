#include <iostream>
#include <nlohmann/json.hpp>
#include "include/aimux/gateway/gateway_manager.hpp"
#include "include/aimux/providers/provider_impl.hpp"

using namespace aimux::gateway;
using namespace aimux::core;
using namespace aimux::providers;

int main() {
    std::cout << "Testing Gateway Manager Integration..." << std::endl;

    try {
        // Create GatewayManager
        GatewayManager manager;
        manager.initialize();
        std::cout << "✅ GatewayManager initialized!" << std::endl;

        // Load configuration (should use config.json)
        nlohmann::json config = {
            {"default_provider", "synthetic"},
            {"thinking_provider", "synthetic"},
            {"vision_provider", "synthetic"},
            {"tools_provider", "synthetic"},
            {"providers", {
                {"synthetic", {
                    {"name", "synthetic"},
                    {"api_key", "synthetic-key"},
                    {"base_url", "http://localhost:9999"},
                    {"models", std::vector<std::string>{"synthetic-gpt-4", "synthetic-claude"}},
                    {"supports_thinking", true},
                    {"supports_vision", false},
                    {"supports_tools", false},
                    {"supports_streaming", false},
                    {"avg_response_time_ms", 2000},
                    {"success_rate", 0.98},
                    {"max_concurrent_requests", 10},
                    {"cost_per_output_token", 0.0001},
                    {"health_check_interval", 60},
                    {"max_failures", 5},
                    {"recovery_delay", 300},
                    {"priority_score", 100},
                    {"enabled", true}
                }}
            }}
        };

        manager.load_configuration(config);
        std::cout << "✅ Configuration loaded!" << std::endl;

        // Check if provider was added
        if (manager.provider_exists("synthetic")) {
            std::cout << "✅ Synthetic provider exists in GatewayManager!" << std::endl;
        } else {
            std::cout << "❌ Synthetic provider NOT found in GatewayManager!" << std::endl;
            return 1;
        }

        // Test routing
        Request request;
        request.data = nlohmann::json{
            {"model", "synthetic-gpt-4"},
            {"messages", nlohmann::json::array({
                {{"role", "user"}, {"content", "Hello, gateway test!"}}
            })}
        };

        std::cout << "📤 Testing request routing..." << std::endl;
        Response response = manager.route_request(request);

        std::cout << "📤 Response received!" << std::endl;
        std::cout << "Success: " << (response.success ? "Yes" : "No") << std::endl;
        std::cout << "Status: " << response.status_code << std::endl;
        std::cout << "Provider: " << response.provider_name << std::endl;

        if (!response.success) {
            std::cout << "Error: " << response.error_message << std::endl;
            return 1;
        } else {
            std::cout << "Data: " << response.data.substr(0, 100) << "..." << std::endl;
        }

        manager.shutdown();
        std::cout << "\n✅ Gateway Manager integration test passed!" << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cout << "❌ Exception during gateway test: " << e.what() << std::endl;
        return 1;
    }
}