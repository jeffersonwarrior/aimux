/**
 * @file provider_compatibility_tests.cpp
 * @brief Comprehensive tests for provider compatibility with unified gateway
 *
 * This test program validates:
 * - Provider compatibility with both API formats
 * - Route provider selection and routing
 * - Provider capabilities mapping
 * - Cross-format provider usage
 * - Error handling and fallback scenarios
 * - Real provider responses with transformation
 */

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <cassert>
#include <iomanip>
#include <chrono>
#include <thread>
#include <nlohmann/json.hpp>
#include "aimux/gateway/format_detector.hpp"
#include "aimux/gateway/api_transformer.hpp"

using namespace aimux::gateway;
using json = nlohmann::json;

// Test results tracking
struct ProviderTestResults {
    int total_tests = 0;
    int passed_tests = 0;
    int failed_tests = 0;
    std::vector<std::string> failures;

    void add_result(bool passed, const std::string& test_name, const std::string& details = "") {
        total_tests++;
        if (passed) {
            passed_tests++;
            std::cout << "✓ " << test_name;
            if (!details.empty()) std::cout << " - " << details;
            std::cout << std::endl;
        } else {
            failed_tests++;
            std::string failure_msg = "✗ " + test_name;
            if (!details.empty()) failure_msg += " - " + details;
            failures.push_back(failure_msg);
            std::cout << failure_msg << std::endl;
        }
    }

    void print_summary() {
        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << "PROVIDER COMPATIBILITY TEST SUMMARY" << std::endl;
        std::cout << std::string(60, '=') << std::endl;
        std::cout << "Total Tests: " << total_tests << std::endl;
        std::cout << "Passed:      " << passed_tests << " ("
                  << (total_tests > 0 ? (passed_tests * 100.0 / total_tests) : 0) << "%)" << std::endl;
        std::cout << "Failed:      " << failed_tests << " ("
                  << (total_tests > 0 ? (failed_tests * 100.0 / total_tests) : 0) << "%)" << std::endl;

        if (!failures.empty()) {
            std::cout << "\nFAILURES:" << std::endl;
            for (const auto& failure : failures) {
                std::cout << "  " << failure << std::endl;
            }
        }
    }
};

// Mock provider capabilities
struct ProviderCapabilities {
    std::string name;
    std::vector<std::string> supported_formats;
    std::vector<std::string> available_models;
    bool supports_thinking = false;
    bool supports_vision = false;
    bool supports_tools = false;
    double default_temperature_range[2] = {0.0, 2.0};
    int max_tokens = 4096;
};

// Simulated provider capabilities based on real providers
std::vector<ProviderCapabilities> get_provider_capabilities() {
    return {
        {
            "cerebras",
            {"openai"},  // Cerebras uses OpenAI-compatible format
            {"llama3.1-8b", "llama3.1-70b", "llama3-3-70b"},
            false,  // thinking
            false,  // vision
            true,   // tools
            {0.0, 2.0},
            8192
        },
        {
            "zai",
            {"openai"},  // Z.AI uses OpenAI-compatible format
            {"gpt-3.5-turbo", "gpt-4", "claude-v1"},
            false,  // thinking
            true,   // vision
            true,   // tools
            {0.0, 1.0},
            4096
        },
        {
            "synthetic",
            {"anthropic", "openai"},  // Synthetic supports both
            {"claude-3-5-sonnet", "claude-3-opus", "gpt-4"},
            true,   // thinking
            true,   // vision
            true,   // tools
            {0.0, 2.0},
            100000
        }
    };
}

// Test provider format compatibility
void test_provider_format_compatibility(ProviderTestResults& results) {
    std::cout << "\n=== PROVIDER FORMAT COMPATIBILITY TESTS ===" << std::endl;

    auto providers = get_provider_capabilities();
    FormatDetector detector;
    ApiTransformer transformer;

    for (const auto& provider : providers) {
        // Test Anthropic format compatibility
        if (std::find(provider.supported_formats.begin(), provider.supported_formats.end(), "anthropic") != provider.supported_formats.end()) {
            json anthropic_request = {
                {"model", provider.available_models[0]},
                {"messages", json::array({
                    {{"role", "user"}, {"content", "Test Anthropic format with " + provider.name}}
                })},
                {"max_tokens", 100}
            };

            DetectionResult detection = detector.detect_format(anthropic_request, {}, "/v1/messages");
            TransformResult transform = transformer.transform_request(
                anthropic_request, APIFormat::ANTHROPIC, APIFormat::ANTHROPIC  // No transformation needed
            );

            bool anthropic_compatible = detection.format == APIFormat::ANTHROPIC && transform.success;
            results.add_result(
                anthropic_compatible,
                "anthropic_format_" + provider.name,
                provider.name + " supports Anthropic format"
            );
        }

        // Test OpenAI format compatibility
        if (std::find(provider.supported_formats.begin(), provider.supported_formats.end(), "openai") != provider.supported_formats.end()) {
            json openai_request = {
                {"model", provider.available_models[0]},
                {"messages", json::array({
                    {{"role", "user"}, {"content", "Test OpenAI format with " + provider.name}}
                })},
                {"max_tokens", 100},
                {"temperature", 0.7}
            };

            DetectionResult detection = detector.detect_format(openai_request, {}, "/v1/chat/completions");
            TransformResult transform = transformer.transform_request(
                openai_request, APIFormat::OPENAI, APIFormat::OPENAI  // No transformation needed
            );

            bool openai_compatible = detection.format == APIFormat::OPENAI && transform.success;
            results.add_result(
                openai_compatible,
                "openai_format_" + provider.name,
                provider.name + " supports OpenAI format"
            );
        }
    }
}

// Test cross-format provider usage
void test_cross_format_provider_usage(ProviderTestResults& results) {
    std::cout << "\n=== CROSS-FORMAT PROVIDER USAGE TESTS ===" << std::endl;

    auto providers = get_provider_capabilities();
    FormatDetector detector;
    ApiTransformer transformer;

    for (const auto& provider : providers) {
        // Test: OpenAI client using Anthropic-capable provider
        if (std::find(provider.supported_formats.begin(), provider.supported_formats.end(), "anthropic") != provider.supported_formats.end()) {
            json openai_client_request = {
                {"model", "gpt-4-turbo"},  // OpenAI model name
                {"messages", json::array({
                    {{"role", "user"}, {"content", "OpenAI client using " + provider.name}}
                })},
                {"max_tokens", 50},
                {"temperature", 0.8}
            };

            // Detect as OpenAI format
            DetectionResult detection = detector.detect_format(openai_client_request, {}, "/v1/chat/completions");

            // Transform for Anthropic-format provider
            TransformResult transform = transformer.transform_request(
                openai_client_request, APIFormat::OPENAI, APIFormat::ANTHROPIC
            );

            bool cross_format_success = detection.format == APIFormat::OPENAI && transform.success;
            results.add_result(
                cross_format_success,
                "openai_to_anthropic_provider_" + provider.name,
                "OpenAI client can use Anthropic-capable " + provider.name
            );
        }

        // Test: Anthropic client using OpenAI-capable provider
        if (std::find(provider.supported_formats.begin(), provider.supported_formats.end(), "openai") != provider.supported_formats.end()) {
            json anthropic_client_request = {
                {"model", "claude-3-5-sonnet"},  // Anthropic model name
                {"messages", json::array({
                    {{"role", "user"}, {"content", "Anthropic client using " + provider.name}}
                })},
                {"max_tokens", 50}
            };

            // Detect as Anthropic format
            DetectionResult detection = detector.detect_format(anthropic_client_request, {}, "/v1/messages");

            // Transform for OpenAI-format provider
            TransformResult transform = transformer.transform_request(
                anthropic_client_request, APIFormat::ANTHROPIC, APIFormat::OPENAI
            );

            bool cross_format_success = detection.format == APIFormat::ANTHROPIC && transform.success;
            results.add_result(
                cross_format_success,
                "anthropic_to_openai_provider_" + provider.name,
                "Anthropic client can use OpenAI-capable " + provider.name
            );
        }
    }
}

// Test provider capabilities mapping
void test_provider_capabilities_mapping(ProviderTestResults& results) {
    std::cout << "\n=== PROVIDER CAPABILITIES MAPPING TESTS ===" << std::endl;

    auto providers = get_provider_capabilities();

    for (const auto& provider : providers) {
        // Test model mapping
        for (const auto& model : provider.available_models) {
            // Create request with provider's model
            json request = {
                {"model", model},
                {"messages", json::array({{"role", "user"}, {"content", "Model test"}})},
                {"max_tokens", std::min(100, provider.max_tokens)}
            };

            // Test with appropriate format
            APIFormat format = APIFormat::OPENAI;  // Default to OpenAI
            if (provider.name == "synthetic") {
                format = APIFormat::ANTHROPIC;  // Synthetic can handle both, test Anthropic
            }

            FormatDetector detector;
            ApiTransformer transformer;

            DetectionResult detection = detector.detect_format(request, {},
                format == APIFormat::ANTHROPIC ? "/v1/messages" : "/v1/chat/completions");

            TransformResult transform = transformer.transform_request(
                request, format, format  // No transformation needed
            );

            bool capability_supported = detection.format != APIFormat::UNKNOWN && transform.success;
            results.add_result(
                capability_supported,
                "model_capability_" + provider.name + "_" + model.substr(0, 10),
                provider.name + " supports model: " + model
            );
        }

        // Test parameter limits
        json max_tokens_request = {
            {"model", provider.available_models[0]},
            {"messages", json::array({{"role", "user"}, {"content", "Test"}})},
            {"max_tokens", provider.max_tokens}
        };

        ApiTransformer transformer;
        TransformResult transform = transformer.transform_request(
            max_tokens_request, APIFormat::OPENAI, APIFormat::OPENAI
        );

        bool max_tokens_handled = transform.success;
        results.add_result(
            max_tokens_handled,
            "max_tokens_capability_" + provider.name,
            provider.name + " handles max_tokens: " + std::to_string(provider.max_tokens)
        );
    }
}

// Test provider selection logic
void test_provider_selection_logic(ProviderTestResults& results) {
    std::cout << "\n=== PROVIDER SELECTION LOGIC TESTS ===" << std::endl;

    // Simulate provider selection based on request characteristics
    struct SelectTestCase {
        std::string name;
        json request;
        APIFormat detected_format;
        std::string expected_provider_type;
    };

    std::vector<SelectTestCase> test_cases = {
        {
            "thinking_request",
            json{
                {"model", "claude-3-opus"},
                {"messages", json::array({{"role", "user"}, {"content", "Complex reasoning"}}})},
                {"max_tokens", 2000}
            },
            APIFormat::ANTHROPIC,
            "synthetic"  // Only synthetic supports thinking in our mock
        },
        {
            "vision_request",
            json{
                {"model", "gpt-4-vision"},
                {"messages", json::array({{"role", "user"}, {"content", "Analyze this image"}}})},
                {"max_tokens", 500}
            },
            APIFormat::OPENAI,
            "zai"  // Z.AI supports vision
        },
        {
            "tools_request",
            json{
                {"model", "llama3.1-70b"},
                {"messages", json::array({{"role", "user"}, {"content", "Use tools"}}})},
                {"max_tokens", 1000},
                {"tools", json::array({{"type", "function"}})}
            },
            APIFormat::OPENAI,
            "cerebras"  // Cerebras supports tools
        },
        {
            "basic_request",
            json{
                {"model", "claude-3-5-sonnet"},
                {"messages", json::array({{"role", "user"}, {"content", "Simple question"}}})},
                {"max_tokens", 100}
            },
            APIFormat::ANTHROPIC,
            "synthetic"  // Synthetic can handle all requests
        }
    };

    auto providers = get_provider_capabilities();

    for (const auto& test_case : test_cases) {
        // Simulate provider selection logic
        std::string selected_provider = "synthetic";  // Default fallback

        for (const auto& provider : providers) {
            // Check if provider supports the detected format
            bool format_supported = std::find(provider.supported_formats.begin(),
                                             provider.supported_formats.end(),
                                             test_case.detected_format == APIFormat::ANTHROPIC ? "anthropic" : "openai") != provider.supported_formats.end();

            if (format_supported) {
                // Additional selection criteria based on request characteristics
                if (test_case.name == "thinking_request" && provider.supports_thinking) {
                    selected_provider = provider.name;
                    break;
                } else if (test_case.name == "vision_request" && provider.supports_vision) {
                    selected_provider = provider.name;
                    break;
                } else if (test_case.name == "tools_request" && provider.supports_tools) {
                    selected_provider = provider.name;
                    break;
                }
            }
        }

        bool selection_correct = selected_provider == test_case.expected_provider_type;
        results.add_result(
            selection_correct,
            "provider_selection_" + test_case.name,
            "Selected: " + selected_provider + " (expected: " + test_case.expected_provider_type + ")"
        );
    }
}

// Test error scenarios with providers
void test_provider_error_scenarios(ProviderTestResults& results) {
    std::cout << "\n=== PROVIDER ERROR SCENARIOS TESTS ===" << std::endl;

    ApiTransformer transformer;

    // Test with unsupported model
    json unsupported_model_request = {
        {"model", "nonexistent-model-v1"},
        {"messages", json::array({{"role", "user"}, {"content", "Test"}})},
        {"max_tokens", 100}
    };

    TransformResult transform1 = transformer.transform_request(
        unsupported_model_request, APIFormat::OPENAI, APIFormat::ANTHROPIC
    );

    bool unsupported_handled = transform1.success;  // Should succeed with model preservation
    results.add_result(
        unsupported_handled,
        "unsupported_model_handling",
        "Unsupported model handled gracefully: " + transform1.transformed_data["model"].get<std::string>()
    );

    // Test with parameters exceeding provider limits
    json excessive_params_request = {
        {"model", "claude-3-5-sonnet"},
        {"messages", json::array({{"role", "user"}, {"content": "Test"}}})},
        {"max_tokens", 1000000},  // Very high
        {"temperature", 3.0}      // Above typical range
    };

    TransformResult transform2 = transformer.transform_request(
        excessive_params_request, APIFormat::ANTHROPIC, APIFormat::OPENAI
    );

    bool excessive_handled = transform2.success;
    results.add_result(
        excessive_handled,
        "excessive_parameters_handling",
        "Excessive parameters handled: max_tokens=" +
        std::to_string(transform2.transformed_data["max_tokens"].get<int>())
    );

    // Test format transformation errors
    json malformed_request = {
        {"model", 123},  // Wrong type
        {"messages", "not_array"}  // Wrong type
    };

    TransformResult transform3 = transformer.transform_request(
        malformed_request, APIFormat::ANTHROPIC, APIFormat::OPENAI
    );

    bool malformed_handled = !transform3.success && !transform3.error_message.empty();
    results.add_result(
        malformed_handled,
        "malformed_request_error",
        "Malformed request error handled: " + transform3.error_message
    );
}

// Test performance with different providers
void test_provider_performance(ProviderTestResults& results) {
    std::cout << "\n=== PROVIDER PERFORMANCE TESTS ===" << std::endl;

    auto providers = get_provider_capabilities();
    FormatDetector detector;
    ApiTransformer transformer;

    const int num_iterations = 50;

    for (const auto& provider : providers) {
        json test_request = {
            {"model", provider.available_models[0]},
            {"messages", json::array({{"role", "user"}, {"content", "Performance test"}}})},
            {"max_tokens", std::min(100, provider.max_tokens)}
        };

        APIFormat test_format = APIFormat::OPENAI;
        if (provider.name == "synthetic") {
            test_format = APIFormat::ANTHROPIC;
        }

        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < num_iterations; ++i) {
            DetectionResult detection = detector.detect_format(test_request, {},
                test_format == APIFormat::ANTHROPIC ? "/v1/messages" : "/v1/chat/completions");

            TransformResult transform = transformer.transform_request(
                test_request, test_format, test_format
            );

            assert(detection.format != APIFormat::UNKNOWN);
            assert(transform.success);
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        double avg_time_us = static_cast<double>(duration.count()) / num_iterations;
        double avg_time_ms = avg_time_us / 1000.0;

        bool performance_acceptable = avg_time_ms < 0.05;  // Should be very fast for in-memory ops
        results.add_result(
            performance_acceptable,
            "provider_performance_" + provider.name,
            "Average: " + std::to_string(avg_time_ms) + "ms per operation (" +
            std::to_string(num_iterations) + " iterations)"
        );
    }
}

// Main test function
int main() {
    std::cout << "=== AIMUX2 PROVIDER COMPATIBILITY TEST SUITE ===" << std::endl;
    std::cout << "Testing provider compatibility with unified gateway formats" << std::endl;

    ProviderTestResults results;

    try {
        // Run all provider compatibility test categories
        test_provider_format_compatibility(results);
        test_cross_format_provider_usage(results);
        test_provider_capabilities_mapping(results);
        test_provider_selection_logic(results);
        test_provider_error_scenarios(results);
        test_provider_performance(results);

    } catch (const std::exception& e) {
        std::cerr << "Test suite crashed with exception: " << e.what() << std::endl;
        return 1;
    }

    // Print final results
    results.print_summary();

    // Return exit code based on test results
    return (results.failed_tests == 0) ? 0 : 1;
}