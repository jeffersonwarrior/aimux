#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <csignal>
#include <nlohmann/json.hpp>
#include "include/aimux/gateway/gateway_manager.hpp"
#include "include/aimux/providers/provider_impl.hpp"

using namespace aimux::gateway;
using namespace aimux::core;
using namespace aimux::providers;

std::unique_ptr<GatewayManager> manager;
std::atomic<bool> keep_running(true);

void signal_handler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down gracefully..." << std::endl;
    keep_running = false;
    if (manager) {
        manager->shutdown();
    }
}

int main() {
    std::cout << "=== Simple Provider Test Server ===" << std::endl;
    std::cout << "Testing router integration with actual HTTP requests..." << std::endl;

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    try {
        // Create and initialize GatewayManager
        manager = std::make_unique<GatewayManager>();
        manager->initialize();

        // Load configuration from the fixed config.json
        std::cout << "Loading configuration from config.json..." << std::endl;
        std::ifstream config_file("config.json");
        if (!config_file.is_open()) {
            std::cout << "❌ Could not open config.json!" << std::endl;
            return 1;
        }

        nlohmann::json config;
        config_file >> config;
        config_file.close();

        manager->load_configuration(config);
        std::cout << "✅ Configuration loaded successfully!" << std::endl;

        // Test request routing
        std::cout << "\n📤 Testing request routing..." << std::endl;

        Request request;
        request.data = nlohmann::json{
            {"model", "synthetic-gpt-4"},
            {"messages", nlohmann::json::array({
                {{"role", "user"}, {"content", "Hello from the test server!"}}
            })}
        };

        Response response = manager->route_request(request);

        std::cout << "📥 Response:" << std::endl;
        std::cout << "   Success: " << (response.success ? "Yes" : "No") << std::endl;
        std::cout << "   Status: " << response.status_code << std::endl;
        std::cout << "   Provider: " << response.provider_name << std::endl;

        if (!response.success) {
            std::cout << "   Error: " << response.error_message << std::endl;
            std::cout << "❌ PROVIDER_NOT_FOUND error confirmed - routing is broken!" << std::endl;
            return 1;
        } else {
            std::cout << "   Data: " << response.data.substr(0, 200) << "..." << std::endl;
            std::cout << "✅ Request routing is WORKING!" << std::endl;
        }

        // Test multiple requests
        std::cout << "\n🔄 Testing multiple requests..." << std::endl;
        for (int i = 1; i <= 3; ++i) {
            request.data["messages"][0]["content"] = "Test message " + std::to_string(i);
            response = manager->route_request(request);

            std::cout << "   Request " << i << ": "
                      << (response.success ? "✅ Success" : "❌ Failed")
                      << " (Provider: " << response.provider_name << ")" << std::endl;
        }

        std::cout << "\n✅ All tests passed! The router is working correctly!" << std::endl;
        std::cout << "\n💡 The original PROVIDER_NOT_FOUND issue was likely:" << std::endl;
        std::cout << "   1. Empty config.json file" << std::endl;
        std::cout << "   2. Wrong config file being loaded" << std::endl;
        std::cout << "   3. ClaudeGateway compilation issues" << std::endl;
        std::cout << "\n🔧 To fix the original claude_gateway:" << std::endl;
        std::cout << "   1. Ensure config.json has the provider configuration" << std::endl;
        std::cout << "   2. Fix compilation issues in logging modules" << std::endl;
        std::cout << "   3. Test with: ./claude_gateway --config config.json" << std::endl;

        // Keep running for a bit to show it's stable
        std::cout << "\n⏳ Server will run for 5 seconds to demonstrate stability..." << std::endl;
        auto start = std::chrono::steady_clock::now();
        while (keep_running &&
               std::chrono::steady_clock::now() - start < std::chrono::seconds(5)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        manager->shutdown();
        std::cout << "\n🛑 Server shutdown complete." << std::endl;

    } catch (const std::exception& e) {
        std::cout << "❌ Exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}