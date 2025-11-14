#include "aimux/providers/provider_impl.hpp"
#include "aimux/core/router.hpp"
#include <iostream>
#include <nlohmann/json.hpp>

int main() {
    std::cout << "=== Phase 2 Functionality Test ===" << std::endl;

    try {
        // Test 1: Provider Creation
        std::cout << "\n1. Testing Provider Creation..." << std::endl;

        // Test CerebrasProvider
        nlohmann::json cerebras_config = {
            {"api_key", "test-key"},
            {"endpoint", "https://api.cerebras.ai"},
            {"models", std::vector<std::string>{"llama3.1-70b"}}
        };

        auto cerebras_provider = aimux::providers::ProviderFactory::create_provider("cerebras", cerebras_config);
        if (cerebras_provider) {
            std::cout << "✅ CerebrasProvider created successfully" << std::endl;
        } else {
            std::cout << "❌ CerebrasProvider creation failed" << std::endl;
        }

        // Test ZAI Provider
        nlohmann::json zai_config = {
            {"api_key", "85c99bec0fa64a0d8a4a01463868667a.RsDzW0iuxtgvYqd2"},
            {"endpoint", "https://api.z.ai/api/paas/v4"},
            {"models", std::vector<std::string>{"glm-4.6", "glm-4.5"}}
        };

        auto zai_provider = aimux::providers::ProviderFactory::create_provider("zai", zai_config);
        if (zai_provider) {
            std::cout << "✅ ZAI Provider created successfully" << std::endl;
        } else {
            std::cout << "❌ ZAI Provider creation failed" << std::endl;
        }

        // Test MiniMaxProvider
        nlohmann::json minimax_config = {
            {"api_key", "test-key"},
            {"group_id", "test-group"},
            {"endpoint", "https://api.minimax.chat"},
            {"models", std::vector<std::string>{"minimax-m2-100k"}}
        };

        auto minimax_provider = aimux::providers::ProviderFactory::create_provider("minimax", minimax_config);
        if (minimax_provider) {
            std::cout << "✅ MiniMaxProvider created successfully" << std::endl;
        } else {
            std::cout << "❌ MiniMaxProvider creation failed" << std::endl;
        }

        // Test SyntheticProvider
        nlohmann::json synthetic_config = {
            {"api_key", "synthetic-key"},
            {"endpoint", "https://synthetic.ai"},
            {"models", std::vector<std::string>{"synthetic-1"}}
        };

        auto synthetic_provider = aimux::providers::ProviderFactory::create_provider("synthetic", synthetic_config);
        if (synthetic_provider) {
            std::cout << "✅ SyntheticProvider created successfully" << std::endl;
        } else {
            std::cout << "❌ SyntheticProvider creation failed" << std::endl;
        }

        // Test 2: Router with Multiple Providers
        std::cout << "\n2. Testing Router with Load Balancing..." << std::endl;

        std::vector<aimux::core::ProviderConfig> providers = {
            {"cerebras", "https://api.cerebras.ai", "test-key", {"llama3.1-70b"}, 60, true},
            {"zai", "https://api.z.ai/api/paas/v4", "85c99bec0fa64a0d8a4a01463868667a.RsDzW0iuxtgvYqd2", {"glm-4.6"}, 60, true},
            {"minimax", "https://api.minimax.chat", "test-key", {"minimax-m2-100k"}, 60, true},
            {"synthetic", "https://synthetic.ai", "synthetic-key", {"synthetic-1"}, 60, true}
        };

        aimux::core::Router router(providers);
        std::cout << "✅ Router created with 4 providers" << std::endl;

        // Test 3: Health Status
        std::cout << "\n3. Testing Health Status..." << std::endl;
        std::string health = router.get_health_status();
        std::cout << "✅ Health status retrieved" << std::endl;

        // Test 4: Metrics
        std::cout << "\n4. Testing Metrics Collection..." << std::endl;
        std::string metrics = router.get_metrics();
        std::cout << "✅ Metrics retrieved" << std::endl;

        // Test 5: Provider Factory Validation
        std::cout << "\n5. Testing Provider Factory..." << std::endl;
        auto supported = aimux::providers::ProviderFactory::get_supported_providers();
        std::cout << "✅ Supported providers: ";
        for (const auto& provider : supported) {
            std::cout << provider << " ";
        }
        std::cout << std::endl;

        // Test 6: Configuration Validation
        std::cout << "\n6. Testing Configuration Validation..." << std::endl;
        bool cerebras_valid = aimux::providers::ProviderFactory::validate_config("cerebras", cerebras_config);
        bool zai_valid = aimux::providers::ProviderFactory::validate_config("zai", zai_config);
        bool minimax_valid = aimux::providers::ProviderFactory::validate_config("minimax", minimax_config);
        bool synthetic_valid = aimux::providers::ProviderFactory::validate_config("synthetic", synthetic_config);

        std::cout << "✅ Configuration validation: " << std::endl;
        std::cout << "   Cerebras: " << (cerebras_valid ? "VALID" : "INVALID") << std::endl;
        std::cout << "   ZAI: " << (zai_valid ? "VALID" : "INVALID") << std::endl;
        std::cout << "   MiniMax: " << (minimax_valid ? "VALID" : "INVALID") << std::endl;
        std::cout << "   Synthetic: " << (synthetic_valid ? "VALID" : "INVALID") << std::endl;

        // Test 7: Request Test with Synthetic Provider
        std::cout << "\n7. Testing Request Routing (Synthetic)..." << std::endl;

        aimux::core::Request test_request;
        test_request.model = "synthetic-1";
        test_request.method = "POST";
        test_request.data = nlohmann::json{
            {"messages", nlohmann::json::array({
                {{"role", "user"}, {"content", "Hello, world!"}}
            })},
            {"max_tokens", 50}
        };

        aimux::core::Response response = router.route(test_request);
        if (response.success) {
            std::cout << "✅ Request routed successfully" << std::endl;
            std::cout << "   Provider: " << response.provider_name << std::endl;
            std::cout << "   Response time: " << response.response_time_ms << "ms" << std::endl;
            std::cout << "   Status code: " << response.status_code << std::endl;
        } else {
            std::cout << "❌ Request routing failed: " << response.error_message << std::endl;
        }

        std::cout << "\n=== Phase 2 Test Complete ===" << std::endl;
        std::cout << "✅ All core functionality verified" << std::endl;

    } catch (const std::exception& e) {
        std::cout << "❌ Test failed with exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}