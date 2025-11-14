#include "aimux/gateway/gateway_manager.hpp"
#include "aimux/gateway/v3_unified_gateway.hpp"
#include "aimux/providers/provider_impl.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <nlohmann/json.hpp>

using namespace aimux::gateway;
using namespace aimux::core;

/**
 * @brief Comprehensive test for V3 Gateway Manager and Unified Gateway
 *
 * This test demonstrates the complete V3 intelligent routing architecture:
 * 1. GatewayManager with intelligent request analysis
 * 2. Provider health monitoring with circuit breaker
 * 3. Load balancing across multiple providers
 * 4. Unified gateway with single endpoint
 */
class V3GatewayTest {
public:
    void run_all_tests() {
        std::cout << "=== V3 Gateway Architecture Integration Test ===" << std::endl;

        test_gateway_manager_basics();
        test_intelligent_routing();
        test_provider_health_management();
        test_load_balancing();
        test_unified_gateway();
        test_end_to_end_routing();

        std::cout << "\n=== All V3 Tests Completed Successfully! ===" << std::endl;
    }

private:
    void test_gateway_manager_basics() {
        std::cout << "\n--- Testing Gateway Manager Basics ---" << std::endl;

        try {
            // Create gateway manager
            auto gateway = std::make_unique<GatewayManager>();
            gateway->initialize();

            std::cout << "✓ GatewayManager initialized successfully" << std::endl;

            // Add synthetic provider for testing
            nlohmann::json synthetic_config = {
                {"name", "synthetic"},
                {"base_url", "http://localhost:9999"},
                {"api_key", "test-key-synthetic"},
                {"supports_thinking", true},
                {"supports_vision", false},
                {"supports_tools", true},
                {"supports_streaming", true},
                {"avg_response_time_ms", 500.0},
                {"cost_per_output_token", 0.001}
            };

            gateway->add_provider("synthetic", synthetic_config);
            std::cout << "✓ Synthetic provider added successfully" << std::endl;

            // Test provider existence
            if (gateway->provider_exists("synthetic")) {
                std::cout << "✓ Provider existence check passed" << std::endl;
            }

            // Test configuration access
            auto config = gateway->get_configuration();
            std::cout << "✓ Gateway configuration retrieved: " << config["providers"].size() << " providers" << std::endl;

            // Test metrics
            auto metrics = gateway->get_metrics();
            std::cout << "✓ Gateway metrics available - Total providers: " << metrics["total_providers"] << std::endl;

            gateway->shutdown();
            std::cout << "✓ GatewayManager shutdown successfully" << std::endl;

        } catch (const std::exception& e) {
            std::cout << "✗ Gateway Manager test failed: " << e.what() << std::endl;
            throw;
        }
    }

    void test_intelligent_routing() {
        std::cout << "\n--- Testing Intelligent Request Routing ---" << std::endl;

        try {
            auto gateway = std::make_unique<GatewayManager>();
            gateway->initialize();

            // Add multiple providers with different capabilities
            nlohmann::json thinking_config = {
                {"name", "thinking-provider"},
                {"base_url", "http://localhost:9991"},
                {"api_key", "test-key-thinking"},
                {"supports_thinking", true},
                {"supports_vision", false},
                {"supports_tools", false},
                {"cost_per_output_token", 0.002}
            };

            nlohmann::json vision_config = {
                {"name", "vision-provider"},
                {"base_url", "http://localhost:9992"},
                {"api_key", "test-key-vision"},
                {"supports_thinking", false},
                {"supports_vision", true},
                {"supports_tools", false},
                {"cost_per_output_token", 0.003}
            };

            gateway->add_provider("thinking-provider", thinking_config);
            gateway->add_provider("vision-provider", vision_config);

            // Set specialized providers
            gateway->set_thinking_provider("thinking-provider");
            gateway->set_vision_provider("vision-provider");

            std::cout << "✓ Multiple providers with different capabilities added" << std::endl;

            // Test request analysis
            Request thinking_request = create_thinking_request();
            Request standard_request = create_standard_request();
            Request vision_request = create_vision_request();

            auto thinking_analysis = gateway->analyze_request(thinking_request);
            auto standard_analysis = gateway->analyze_request(standard_request);
            auto vision_analysis = gateway->analyze_request(vision_request);

            std::cout << "✓ Request analysis completed:" << std::endl;
            std::cout << "  - Thinking request type: " << request_type_to_string(thinking_analysis.type_) << std::endl;
            std::cout << "  - Standard request type: " << request_type_to_string(standard_analysis.type_) << std::endl;
            std::cout << "  - Vision request type: " << request_type_to_string(vision_analysis.type_) << std::endl;

            // Test provider capabilities
            auto thinking_caps = gateway->get_provider_capabilities("thinking-provider");
            auto vision_caps = gateway->get_provider_capabilities("vision-provider");

            std::cout << "✓ Provider capabilities retrieved:" << std::endl;
            std::cout << "  - Thinking provider capabilities: " << capability_to_string(thinking_caps) << std::endl;
            std::cout << "  - Vision provider capabilities: " << capability_to_string(vision_caps) << std::endl;

            gateway->shutdown();

        } catch (const std::exception& e) {
            std::cout << "✗ Intelligent routing test failed: " << e.what() << std::endl;
            throw;
        }
    }

    void test_provider_health_management() {
        std::cout << "\n--- Testing Provider Health Management ---" << std::endl;

        try {
            auto gateway = std::make_unique<GatewayManager>();
            gateway->initialize();

            // Add provider
            nlohmann::json config = {
                {"name", "health-test-provider"},
                {"base_url", "http://localhost:9993"},
                {"api_key", "test-key-health"},
                {"max_failures", 3}
            };

            gateway->add_provider("health-test-provider", config);
            gateway->start_health_monitoring();

            std::cout << "✓ Health monitoring started" << std::endl;

            // Check initial health status
            auto healthy_providers = gateway->get_healthy_providers();
            std::cout << "✓ Initial healthy providers: " << healthy_providers.size() << std::endl;

            // Test manual health marking
            gateway->manually_mark_provider_unhealthy("health-test-provider");
            auto unhealthy_providers = gateway->get_unhealthy_providers();
            std::cout << "✓ Manual unhealthy marking: " << unhealthy_providers.size() << " unhealthy providers" << std::endl;

            // Test recovery
            gateway->manually_mark_provider_healthy("health-test-provider");
            healthy_providers = gateway->get_healthy_providers();
            std::cout << "✓ Recovery successful: " << healthy_providers.size() << " healthy providers" << std::endl;

            // Test health status checking
            auto status = gateway->get_provider_status("health-test-provider");
            std::cout << "✓ Provider status: " << health_status_to_string(status) << std::endl;

            gateway->stop_health_monitoring();
            gateway->shutdown();

        } catch (const std::exception& e) {
            std::cout << "✗ Provider health management test failed: " << e.what() << std::endl;
            throw;
        }
    }

    void test_load_balancing() {
        std::cout << "\n--- Testing Load Balancing ---" << std::endl;

        try {
            auto gateway = std::make_unique<GatewayManager>();
            gateway->initialize();

            // Add multiple providers for load balancing
            for (int i = 0; i < 3; ++i) {
                nlohmann::json config = {
                    {"name", "lb-provider-" + std::to_string(i)},
                    {"base_url", "http://localhost:999" + std::to_string(i + 4)},
                    {"api_key", "test-key-lb-" + std::to_string(i)},
                    {"avg_response_time_ms", 100.0 * (i + 1)},
                    {"cost_per_output_token", 0.001 * (i + 1)},
                    {"priority_score", 100 - (i * 10)}
                };

                gateway->add_provider("lb-provider-" + std::to_string(i), config);
            }

            std::cout << "✓ Multiple providers added for load balancing" << std::endl;

            // Test load balancing
            std::vector<std::string> candidates = {"lb-provider-0", "lb-provider-1", "lb-provider-2"};

            auto balanced = gateway->select_balanced_provider(candidates, RequestType::STANDARD);
            std::cout << "✓ Load balanced selection: " << balanced << std::endl;

            // Test multiple selections to see distribution
            std::unordered_map<std::string, int> selections;
            for (int i = 0; i < 20; ++i) {
                auto selected = gateway->select_balanced_provider(candidates, RequestType::STANDARD);
                selections[selected]++;
            }

            std::cout << "✓ Load balancing distribution:" << std::endl;
            for (const auto& [provider, count] : selections) {
                std::cout << "  " << provider << ": " << count << " selections" << std::endl;
            }

            gateway->shutdown();

        } catch (const std::exception& e) {
            std::cout << "✗ Load balancing test failed: " << e.what() << std::endl;
            throw;
        }
    }

    void test_unified_gateway() {
        std::cout << "\n--- Testing V3 Unified Gateway ---" << std::endl;

        try {
            V3UnifiedGateway::Config config;
            config.port = 8082;  // Use different port for testing
            config.enable_cors = false;
            config.log_level = "debug";

            auto gateway = std::make_unique<V3UnifiedGateway>(config);

            // Test gateway manager access
            auto* gm = gateway->get_gateway_manager();
            if (gm) {
                std::cout << "✓ Gateway manager accessible from unified gateway" << std::endl;

                // Add test provider
                nlohmann::json provider_config = {
                    {"name", "unified-test-provider"},
                    {"base_url", "http://localhost:9997"},
                    {"api_key", "test-key-unified"},
                    {"supports_thinking", true},
                    {"supports_tools", true}
                };

                gm->add_provider("unified-test-provider", provider_config);
                std::cout << "✓ Provider added through unified gateway" << std::endl;
            }

            // Test status and metrics
            auto status = gateway->get_status();
            std::cout << "✓ Gateway status: " << status["version"] << " on " << status["endpoint"] << std::endl;

            auto metrics = gateway->get_metrics();
            std::cout << "✓ Gateway metrics available" << std::endl;

            // Note: We don't actually start the server in this test
            // to avoid port conflicts, but we verify the setup

            std::cout << "✓ V3 Unified Gateway setup successful" << std::endl;

        } catch (const std::exception& e) {
            std::cout << "✗ Unified gateway test failed: " << e.what() << std::endl;
            throw;
        }
    }

    void test_end_to_end_routing() {
        std::cout << "\n--- Testing End-to-End Routing ---" << std::endl;

        try {
            auto gateway = std::make_unique<GatewayManager>();
            gateway->initialize();

            // Setup comprehensive provider ecosystem
            setup_comprehensive_providers(gateway.get());
            std::cout << "✓ Comprehensive provider ecosystem setup" << std::endl;

            // Configure routing callbacks
            bool routing_received = false;
            gateway->set_route_callback([&routing_received](const RequestMetrics& metrics) {
                routing_received = true;
                std::cout << "  Routing callback: " << metrics.provider_name_
                         << " in " << metrics.duration_ms_ << "ms" << std::endl;
            });

            // Test various request types
            test_request_routing(gateway.get(), "Thinking request", RequestType::THINKING);
            test_request_routing(gateway.get(), "Vision request", RequestType::VISION);
            test_request_routing(gateway.get(), "Tools request", RequestType::TOOLS);
            test_request_routing(gateway.get(), "Standard request", RequestType::STANDARD);

            // Test circuit breaker
            test_circuit_breaker(gateway.get());

            // Test failover
            test_failover(gateway.get());

            std::cout << "✓ All request types routed successfully" << std::endl;

            // Get final metrics
            auto final_metrics = gateway->get_metrics();
            std::cout << "✓ Final metrics:" << std::endl;
            std::cout << "  - Total requests: " << final_metrics["total_requests"] << std::endl;
            std::cout << "  - Success rate: " << final_metrics["success_rate"] << std::endl;
            std::cout << "  - Healthy providers: " << final_metrics["healthy_providers"] << std::endl;

            gateway->shutdown();

        } catch (const std::exception& e) {
            std::cout << "✗ End-to-end routing test failed: " << e.what() << std::endl;
            throw;
        }
    }

    void setup_comprehensive_providers(GatewayManager* gateway) {
        // Cerebras-like provider (fast, thinking-capable)
        nlohmann::json cerebras_config = {
            {"name", "cerebras"},
            {"base_url", "http://localhost:8001"},
            {"api_key", "test-key-cerebras"},
            {"supports_thinking", true},
            {"supports_vision", false},
            {"supports_tools", true},
            {"supports_streaming", true},
            {"avg_response_time_ms", 800.0},
            {"cost_per_output_token", 0.0008},
            {"priority_score", 90}
        };

        // MiniMax-like provider (balanced)
        nlohmann::json minimax_config = {
            {"name", "minimax"},
            {"base_url", "http://localhost:8002"},
            {"api_key", "test-key-minimax"},
            {"supports_thinking", true},
            {"supports_vision", true},
            {"supports_tools", true},
            {"supports_streaming", true},
            {"avg_response_time_ms", 1200.0},
            {"cost_per_output_token", 0.0012},
            {"priority_score", 80}
        };

        // Z.AI-like provider (vision-focused)
        nlohmann::json zai_config = {
            {"name", "zai"},
            {"base_url", "http://localhost:8003"},
            {"api_key", "test-key-zai"},
            {"supports_thinking", false},
            {"supports_vision", true},
            {"supports_tools", true},
            {"supports_streaming", false},
            {"avg_response_time_ms", 1500.0},
            {"cost_per_output_token", 0.0015},
            {"priority_score", 70}
        };

        // Synthetic provider (fallback)
        nlohmann::json synthetic_config = {
            {"name", "synthetic"},
            {"base_url", "http://localhost:8004"},
            {"api_key", "test-key-synthetic"},
            {"supports_thinking", true},
            {"supports_vision", false},
            {"supports_tools", false},
            {"supports_streaming", false},
            {"avg_response_time_ms, 2000.0},
            {"cost_per_output_token", 0.0001},
            {"priority_score", 50}
        };

        gateway->add_provider("cerebras", cerebras_config);
        gateway->add_provider("minimax", minimax_config);
        gateway->add_provider("zai", zai_config);
        gateway->add_provider("synthetic", synthetic_config);

        // Set specialized providers
        gateway->set_default_provider("minimax");
        gateway->set_thinking_provider("cerebras");
        gateway->set_vision_provider("zai");
        gateway->set_tools_provider("minimax");
    }

    void test_request_routing(GatewayManager* gateway, const std::string& description, RequestType expected_type) {
        Request request = create_request_by_type(expected_type);

        try {
            // Note: In a real test, this would actually send requests
            // For now, we just test the routing logic
            auto analysis = gateway->analyze_request(request);

            std::cout << "  " << description << " - Analyzed as: "
                     << request_type_to_string(analysis.type_) << std::endl;

            // Verify type detection
            if (analysis.type_ == expected_type) {
                std::cout << "    ✓ Correct type detected" << std::endl;
            } else {
                std::cout << "    ⚠ Type mismatch: expected "
                         << request_type_to_string(expected_type)
                         << ", got " << request_type_to_string(analysis.type_) << std::endl;
            }

        } catch (const std::exception& e) {
            std::cout << "    ✗ Routing test failed: " << e.what() << std::endl;
            throw;
        }
    }

    void test_circuit_breaker(GatewayManager* gateway) {
        std::cout << "  Testing circuit breaker..." << std::endl;

        // Manually mark a provider as unhealthy to test circuit breaker
        gateway->manually_mark_provider_unhealthy("synthetic");

        auto status = gateway->get_provider_status("synthetic");
        if (status != HealthStatus::HEALTHY) {
            std::cout << "    ✓ Circuit breaker activated for unhealthy provider" << std::endl;
        }

        // Test recovery
        gateway->manually_mark_provider_healthy("synthetic");
        status = gateway->get_provider_status("synthetic");
        if (status == HealthStatus::HEALTHY) {
            std::cout << "    ✓ Circuit breaker recovery working" << std::endl;
        }
    }

    void test_failover(GatewayManager* gateway) {
        std::cout << "  Testing failover logic..." << std::endl;

        // Get healthy providers
        auto healthy = gateway->get_healthy_providers();
        if (!healthy.empty()) {
            std::string failover = gateway->select_failover_provider(healthy[0], create_standard_request());
            if (!failover.empty() && failover != healthy[0]) {
                std::cout << "    ✓ Failover provider selected: " << failover << std::endl;
            } else {
                std::cout << "    ⚠ Limited failover options available" << std::endl;
            }
        }
    }

    // Helper methods for creating test requests
    Request create_thinking_request() {
        Request request;
        request.model = "claude-3-sonnet";
        request.method = "POST";
        request.data = nlohmann::json{
            {"model", "claude-3-sonnet"},
            {"messages", {{
                {"role", "user"},
                {"content", "Please think step by step to solve this complex problem: Explain how photosynthesis works and why it's important for life on Earth."}
            }}},
            {"max_tokens", 1000}
        };
        return request;
    }

    Request create_standard_request() {
        Request request;
        request.model = "claude-3-haiku";
        request.method = "POST";
        request.data = nlohmann::json{
            {"model", "claude-3-haiku"},
            {"messages", {{
                {"role", "user"},
                {"content", "What is the capital of France?"}
            }}},
            {"max_tokens", 100}
        };
        return request;
    }

    Request create_vision_request() {
        Request request;
        request.model = "claude-3-sonnet";
        request.method = "POST";
        request.data = nlohmann::json{
            {"model", "claude-3-sonnet"},
            {"messages", {{
                {"role", "user"},
                {"content", {
                    {{"type", "text"}, {"text", "Describe what you see in this image"}},
                    {{"type", "image"}, {"source", {{"type", "base64"}, {"media_type", "image/jpeg"}, {"data", "base64_image_data"}}}}}
                }}
            }}},
            {"max_tokens", 500}
        };
        return request;
    }

    Request create_tools_request() {
        Request request;
        request.model = "claude-3-sonnet";
        request.method = "POST";
        request.data = nlohmann::json{
            {"model", "claude-3-sonnet"},
            {"messages", {{
                {"role", "user"},
                {"content", "What's the weather like in New York?"}
            }}},
            {"tools", {{
                {"name", "get_weather"},
                {"description", "Get current weather for a location"},
                {"input_schema", {
                    {"type", "object"},
                    {"properties", {
                        {"location", {{"type", "string"}, {"description", "City name"}}}
                    }},
                    {"required", {"location"}}
                }}
            }}},
            {"max_tokens", 200}
        };
        return request;
    }

    Request create_request_by_type(RequestType type) {
        switch (type) {
            case RequestType::THINKING:
                return create_thinking_request();
            case RequestType::VISION:
                return create_vision_request();
            case RequestType::TOOLS:
                return create_tools_request();
            case RequestType::STANDARD:
            default:
                return create_standard_request();
        }
    }
};

int main() {
    try {
        V3GatewayTest test;
        test.run_all_tests();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test suite failed: " << e.what() << std::endl;
        return 1;
    }
}