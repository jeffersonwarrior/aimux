/**
 * Provider Integration Tests for Aimux v2.0.0
 * Comprehensive testing of all providers, factory patterns, API specs, and configuration
 */

#include <iostream>
#include <chrono>
#include <memory>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <thread>
#include <future>

// Aimux includes
#include "aimux/providers/provider_impl.hpp"
#include "aimux/providers/api_specs.hpp"

using namespace aimux;
using namespace std::chrono;

// Test harness
struct TestResults {
    int total_tests = 0;
    int passed_tests = 0;
    int failed_tests = 0;
    std::vector<std::string> failures;

    void record_test(const std::string& test_name, bool passed, const std::string& message = "") {
        total_tests++;
        if (passed) {
            passed_tests++;
            std::cout << "[✓] " << test_name << std::endl;
        } else {
            failed_tests++;
            std::cout << "[✗] " << test_name;
            if (!message.empty()) {
                std::cout << " - " << message;
            }
            std::cout << std::endl;
            failures.push_back(test_name + ": " + message);
        }
    }

    void print_summary() {
        std::cout << "\n=== TEST SUMMARY ===" << std::endl;
        std::cout << "Total: " << total_tests << std::endl;
        std::cout << "Passed: " << passed_tests << std::endl;
        std::cout << "Failed: " << failed_tests << std::endl;
        std::cout << "Success Rate: " << std::fixed << std::setprecision(2)
                  << (100.0 * passed_tests / total_tests) << "%" << std::endl;

        if (!failures.empty()) {
            std::cout << "\nFAILURES:" << std::endl;
            for (const auto& failure : failures) {
                std::cout << "  - " << failure << std::endl;
            }
        }
    }
};

// Test configurations
nlohmann::json get_test_config(const std::string& provider_name) {
    if (provider_name == "cerebras") {
        return {
            {"api_key", "test-cerebras-key-123456789012345678901234567890"},
            {"endpoint", "https://api.cerebras.ai"},
            {"max_requests_per_minute", 60}
        };
    } else if (provider_name == "zai") {
        return {
            {"api_key", "test-zai-key-123456789012345678901234567890"},
            {"endpoint", "https://api.z.ai"},
            {"max_requests_per_minute", 100}
        };
    } else if (provider_name == "minimax") {
        return {
            {"api_key", "test-minimax-key-123456789012345678901234567890"},
            {"endpoint", "https://api.minimax.io"},
            {"group_id", "test-group-123"},
            {"max_requests_per_minute", 60}
        };
    } else if (provider_name == "synthetic") {
        return {
            {"api_key", "synthetic-test-key"},
            {"endpoint", "https://synthetic.ai"},
            {"max_requests_per_minute", 1000}
        };
    }
    return {};
}

// Test 1: Provider Factory Creation
void test_provider_factory(TestResults& results) {
    std::cout << "\n=== TEST 1: Provider Factory Creation ===" << std::endl;

    std::vector<std::string> providers = {"cerebras", "zai", "minimax", "synthetic"};

    for (const auto& provider_name : providers) {
        try {
            auto config = get_test_config(provider_name);
            auto provider = providers::ProviderFactory::create_provider(provider_name, config);

            bool success = (provider != nullptr);
            results.record_test("Create " + provider_name + " provider", success,
                               success ? "" : "Factory returned nullptr");

            if (success) {
                results.record_test(provider_name + " name correctness",
                                   provider->get_provider_name() == provider_name,
                                   "Expected " + provider_name + ", got " + provider->get_provider_name());
            }
        } catch (const std::exception& e) {
            results.record_test("Create " + provider_name + " provider", false,
                               "Exception: " + std::string(e.what()));
        }
    }

    // Test invalid provider
    try {
        auto invalid_provider = providers::ProviderFactory::create_provider("invalid", {});
        results.record_test("Invalid provider creation", invalid_provider == nullptr,
                           "Should fail for unknown provider");
    } catch (const std::exception& e) {
        results.record_test("Invalid provider creation", true,
                           "Correctly threw exception: " + std::string(e.what()));
    }

    // Test provider listing
    auto supported_providers = providers::ProviderFactory::get_supported_providers();
    bool has_all_providers = (supported_providers.size() == 4) &&
                            (std::find(supported_providers.begin(), supported_providers.end(), "cerebras") != supported_providers.end()) &&
                            (std::find(supported_providers.begin(), supported_providers.end(), "zai") != supported_providers.end()) &&
                            (std::find(supported_providers.begin(), supported_providers.end(), "minimax") != supported_providers.end()) &&
                            (std::find(supported_providers.begin(), supported_providers.end(), "synthetic") != supported_providers.end());

    results.record_test("Provider list completeness", has_all_providers,
                       "Expected 4 providers, got " + std::to_string(supported_providers.size()));
}

// Test 2: API Specs Integration
void test_api_specs_integration(TestResults& results) {
    std::cout << "\n=== TEST 2: API Specs Integration ===" << std::endl;

    // Test base endpoints - use direct constants instead of function calls
    results.record_test("Cerebras endpoint",
                       std::string(providers::api_specs::endpoints::CEREBRAS_BASE) == "https://api.cerebras.ai/v1",
                       "Expected https://api.cerebras.ai/v1");

    results.record_test("Z.AI endpoint",
                       std::string(providers::api_specs::endpoints::ZAI_BASE) == "https://api.z.ai/api/anthropic/v1",
                       "Expected https://api.z.ai/api/anthropic/v1");

    results.record_test("MiniMax endpoint",
                       std::string(providers::api_specs::endpoints::MINIMAX_BASE) == "https://api.minimax.io/anthropic",
                       "Expected https://api.minimax.io/anthropic");

    // Test rate limits
    results.record_test("Cerebras rate limit",
                       providers::api_specs::rate_limits::CEREBRAS_RPM == 100,
                       "Expected 100 RPM, got " + std::to_string(providers::api_specs::rate_limits::CEREBRAS_RPM));

    results.record_test("Z.AI rate limit",
                       providers::api_specs::rate_limits::ZAI_RPM == 100,
                       "Expected 100 RPM, got " + std::to_string(providers::api_specs::rate_limits::ZAI_RPM));

    results.record_test("MiniMax rate limit",
                       providers::api_specs::rate_limits::MINIMAX_RPM == 60,
                       "Expected 60 RPM, got " + std::to_string(providers::api_specs::rate_limits::MINIMAX_RPM));

    // Test model identifiers
    results.record_test("Cerebras model ID",
                       std::string(providers::api_specs::models::cerebras::LLAMA3_1_70B) == "llama3.1-70b",
                       "Expected llama3.1-70b");

    results.record_test("Z.AI model ID",
                       std::string(providers::api_specs::models::zai::CLAUDE_3_5_SONNET) == "claude-3-5-sonnet-20241022",
                       "Expected claude-3-5-sonnet-20241022");

    results.record_test("MiniMax model ID",
                       std::string(providers::api_specs::models::minimax::MINIMAX_M2_100K) == "minimax-m2-100k",
                       "Expected minimax-m2-100k");

    // Test capabilities
    auto cerebras_caps = providers::api_specs::capabilities::CEREBRAS_CAPS;
    results.record_test("Cerebras thinking capability", cerebras_caps.thinking,
                       "Cerebras should support thinking");
    results.record_test("Cerebras tools capability", cerebras_caps.tools,
                       "Cerebras should support tools");

    auto zai_caps = providers::api_specs::capabilities::ZAI_CAPS;
    results.record_test("Z.AI vision capability", zai_caps.vision,
                       "Z.AI should support vision");
    results.record_test("Z.AI tools capability", zai_caps.tools,
                       "Z.AI should support tools");

    auto minimax_caps = providers::api_specs::capabilities::MINIMAX_CAPS;
    results.record_test("MiniMax thinking capability", minimax_caps.thinking,
                       "MiniMax should support thinking");
    results.record_test("MiniMax tools capability", minimax_caps.tools,
                       "MiniMax should support tools");
}

// Test 3: Configuration Testing
void test_configuration(TestResults& results) {
    std::cout << "\n=== TEST 3: Configuration Testing ===" << std::endl;

    // Test valid configurations
    std::vector<std::string> providers = {"cerebras", "zai", "minimax", "synthetic"};
    for (const auto& provider_name : providers) {
        try {
            auto config = get_test_config(provider_name);
            bool is_valid = providers::ProviderFactory::validate_config(provider_name, config);

            results.record_test(provider_name + " config validation", is_valid,
                               "Config should be valid for " + provider_name);
        } catch (const std::exception& e) {
            results.record_test(provider_name + " config validation", false,
                               "Exception: " + std::string(e.what()));
        }
    }

    // Test default config generation
    auto default_config = providers::ConfigParser::generate_default_config();
    bool has_providers = default_config.contains("providers") &&
                        default_config["providers"].is_array() &&
                        default_config["providers"].size() > 0;

    results.record_test("Default config generation", has_providers,
                       "Default config should contain providers array");
}

// Test 4: Provider Functionality (using synthetic for safe testing)
void test_provider_functionality(TestResults& results) {
    std::cout << "\n=== TEST 4: Provider Functionality ===" << std::endl;

    try {
        auto config = get_test_config("synthetic");
        auto provider = providers::ProviderFactory::create_provider("synthetic", config);

        if (provider) {
            // Test health check
            bool is_healthy = provider->is_healthy();
            results.record_test("Synthetic provider health check", is_healthy,
                              "Synthetic provider should always be healthy");

            // Test basic request
            core::Request request;
            request.method = "POST";
            request.data = {
                {"messages", nlohmann::json::array({
                    {{"role", "user"}, {"content", "Hello, synthetic provider!"}}
                })},
                {"max_tokens", 100},
                {"temperature", 0.7}
            };

            auto start_time = high_resolution_clock::now();
            auto response = provider->send_request(request);
            auto end_time = high_resolution_clock::now();

            results.record_test("Synthetic request success", response.success,
                              "Synthetic provider should respond successfully");

            results.record_test("Synthetic response data validation",
                              !response.data.empty() && response.status_code == 200,
                              "Response should contain data with status 200");

            results.record_test("Synthetic response time sanity",
                              response.response_time_ms > 0 && response.response_time_ms < 10000,
                              "Response time should be reasonable (0-10s)");

            // Test rate limit status
            auto rate_limit_status = provider->get_rate_limit_status();
            results.record_test("Synthetic rate limit status", !rate_limit_status.empty(),
                              "Should return rate limit information");
        }
    } catch (const std::exception& e) {
        results.record_test("Synthetic provider functionality", false,
                          "Exception: " + std::string(e.what()));
    }
}

// Test 5: Error Handling
void test_error_handling(TestResults& results) {
    std::cout << "\n=== TEST 5: Error Handling ===" << std::endl;

    // Test creating providers with invalid configs
    try {
        nlohmann::json invalid_config = {{"invalid_key", "invalid_value"}};
        auto provider = providers::ProviderFactory::create_provider("cerebras", invalid_config);
        results.record_test("Invalid config handling", false, "Should throw exception for invalid config");
    } catch (const std::exception&) {
        results.record_test("Invalid config handling", true, "Correctly threw exception for invalid config");
    }

    // Test empty API key validation
    try {
        auto config = get_test_config("cerebras");
        config["api_key"] = "";
        bool is_valid = providers::ProviderFactory::validate_config("cerebras", config);
        results.record_test("Empty API key validation", !is_valid, "Should reject empty API key");
    } catch (const std::exception& e) {
        results.record_test("Empty API key validation", true,
                          "Correctly threw exception: " + std::string(e.what()));
    }
}

// Test 6: Performance Validation
void test_performance(TestResults& results) {
    std::cout << "\n=== TEST 6: Performance Validation ===" << std::endl;

    // Test provider instantiation performance
    std::vector<std::string> providers = {"cerebras", "zai", "minimax", "synthetic"};

    for (const auto& provider_name : providers) {
        try {
            auto config = get_test_config(provider_name);

            auto start_time = high_resolution_clock::now();
            auto provider = providers::ProviderFactory::create_provider(provider_name, config);
            auto end_time = high_resolution_clock::now();
            auto duration_ms = duration_cast<microseconds>(end_time - start_time).count() / 1000.0;

            bool is_fast_enough = duration_ms < 100.0; // Should instantiate in <100ms
            results.record_test(provider_name + " instantiation performance", is_fast_enough,
                              "Took " + std::to_string(duration_ms) + "ms (should be <100ms)");
        } catch (const std::exception& e) {
            results.record_test(provider_name + " instantiation performance", false,
                              "Exception: " + std::string(e.what()));
        }
    }

    // Test synthetic provider throughput (10 consecutive requests)
    try {
        auto config = get_test_config("synthetic");
        auto provider = providers::ProviderFactory::create_provider("synthetic", config);

        if (provider) {
            std::vector<std::future<core::Response>> futures;
            auto start_time = high_resolution_clock::now();

            // Send 10 requests concurrently
            for (int i = 0; i < 10; ++i) {
                core::Request request;
                request.method = "POST";
                request.data = {
                    {"messages", nlohmann::json::array({
                        {{"role", "user"}, {"content", "Test request " + std::to_string(i)}}
                    })},
                    {"max_tokens", 50}
                };

                futures.push_back(std::async(std::launch::async, [&provider, request]() {
                    return provider->send_request(request);
                }));
            }

            // Wait for all to complete
            int successful_requests = 0;
            for (auto& future : futures) {
                auto response = future.get();
                if (response.success) successful_requests++;
            }

            auto end_time = high_resolution_clock::now();
            auto duration_ms = duration_cast<milliseconds>(end_time - start_time).count();

            bool all_successful = (successful_requests == 10);
            bool reasonable_time = (duration_ms < 5000); // Should complete within 5 seconds

            results.record_test("Synthetic throughput (10 requests)", all_successful,
                              std::to_string(successful_requests) + "/10 successful");

            results.record_test("Synthetic throughput performance", reasonable_time,
                              "Took " + std::to_string(duration_ms) + "ms for 10 requests");
        }
    } catch (const std::exception& e) {
        results.record_test("Synthetic throughput performance", false,
                          "Exception: " + std::string(e.what()));
    }
}

// Main test runner
int main() {
    std::cout << "=== AIMUX v2.0.0 PROVIDER INTEGRATION TEST SUITE ===" << std::endl;
    std::cout << "Starting comprehensive provider testing..." << std::endl;

    TestResults results;

    auto overall_start_time = high_resolution_clock::now();

    test_provider_factory(results);
    test_api_specs_integration(results);
    test_configuration(results);
    test_provider_functionality(results);
    test_error_handling(results);
    test_performance(results);

    auto overall_end_time = high_resolution_clock::now();
    auto total_duration = duration_cast<milliseconds>(overall_end_time - overall_start_time);

    results.print_summary();

    std::cout << "\nTotal test execution time: " << total_duration.count() << "ms" << std::endl;

    if (results.failed_tests == 0) {
        std::cout << "\n🎉 ALL TESTS PASSED! Provider integration is working correctly." << std::endl;
        return 0;
    } else {
        std::cout << "\n❌ " << results.failed_tests << " tests failed. Review above for details." << std::endl;
        return 1;
    }
}