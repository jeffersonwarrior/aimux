#include <iostream>
#include <nlohmann/json.hpp>
#include "include/aimux/providers/provider_impl.hpp"
#include "include/aimux/core/bridge.hpp"

using namespace aimux::providers;
using namespace aimux::core;

int main() {
    std::cout << "Testing Provider Factory..." << std::endl;

    // Test 1: Check if supported providers list works
    auto supported = ProviderFactory::get_supported_providers();
    std::cout << "Supported providers: ";
    for (const auto& provider : supported) {
        std::cout << provider << " ";
    }
    std::cout << std::endl;

    // Test 2: Create synthetic provider
    nlohmann::json synthetic_config = {
        {"api_key", "synthetic-key"},
        {"endpoint", "http://localhost:9999"},
        {"models", std::vector<std::string>{"synthetic-gpt-4"}}
    };

    try {
        auto synthetic = ProviderFactory::create_provider("synthetic", synthetic_config);
        if (synthetic) {
            std::cout << "✅ Synthetic provider created successfully!" << std::endl;
            std::cout << "Provider name: " << synthetic->get_provider_name() << std::endl;
            std::cout << "Provider healthy: " << (synthetic->is_healthy() ? "Yes" : "No") << std::endl;

            // Test sending a request
            Request request;
            request.data = nlohmann::json{
                {"model", "synthetic-gpt-4"},
                {"messages", nlohmann::json::array({
                    {{"role", "user"}, {"content", "Hello, test message!"}}
                })}
            };

            Response response = synthetic->send_request(request);
            std::cout << "📤 Response received!" << std::endl;
            std::cout << "Success: " << (response.success ? "Yes" : "No") << std::endl;
            std::cout << "Status: " << response.status_code << std::endl;
            if (!response.success) {
                std::cout << "Error: " << response.error_message << std::endl;
            } else {
                std::cout << "Data: " << response.data.substr(0, 100) << "..." << std::endl;
            }
        } else {
            std::cout << "❌ Failed to create synthetic provider!" << std::endl;
            return 1;
        }
    } catch (const std::exception& e) {
        std::cout << "❌ Exception creating synthetic provider: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "\n✅ Provider Factory test passed!" << std::endl;
    return 0;
}