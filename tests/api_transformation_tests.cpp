/**
 * @file api_transformation_tests.cpp
 * @brief Comprehensive tests for API format transformation functionality
 *
 * This test program validates:
 * - Bidirectional request transformation (Anthropic ↔ OpenAI)
 * - Bidirectional response transformation (Anthropic ↔ OpenAI)
 * - Model name mapping between formats
 * - Message structure conversion
 * - Parameter mapping and default values
 * - Error handling and edge cases
 * - Content preserving transformations
 */

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <cassert>
#include <iomanip>
#include <chrono>
#include <nlohmann/json.hpp>
#include "aimux/gateway/api_transformer.hpp"
#include "aimux/gateway/format_detector.hpp"

using namespace aimux::gateway;
using json = nlohmann::json;

// Test data structures
struct TransformTestCase {
    std::string name;
    std::string description;
    json source_data;
    APIFormat source_format;
    APIFormat target_format;
    json expected_data;
    std::vector<std::string> expected_warnings;
    bool should_succeed;
};

struct TransformTestResults {
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
        std::cout << "API TRANSFORMATION TEST SUMMARY" << std::endl;
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

// Helper function to compare JSON with tolerance for optional fields
bool json_compare_relaxed(const json& a, const json& b, const std::vector<std::string>& allowed_fields = {}) {
    // Simple deep comparison for now - can be enhanced later
    return a == b;
}

// Helper function to transform a known OpenAI request to Anthropic format
json transform_openai_to_anthropic_expected(const json& openai_req) {
    json anthropic_req;

    // Map model
    if (openai_req.contains("model")) {
        std::string model = openai_req["model"];
        if (model == "gpt-4-turbo") anthropic_req["model"] = "claude-3-5-sonnet-20241022";
        else if (model == "gpt-4o-mini") anthropic_req["model"] = "claude-3-5-haiku-20241022";
        else anthropic_req["model"] = model; // fallback
    }

    // Copy messages
    if (openai_req.contains("messages")) {
        anthropic_req["messages"] = openai_req["messages"];
    }

    // Map common parameters
    if (openai_req.contains("max_tokens")) {
        anthropic_req["max_tokens"] = openai_req["max_tokens"];
    }
    if (openai_req.contains("temperature")) {
        anthropic_req["temperature"] = openai_req["temperature"];
    }
    if (openai_req.contains("top_p")) {
        anthropic_req["top_p"] = openai_req["top_p"];
    }

    return anthropic_req;
}

// Helper function to transform a known Anthropic request to OpenAI format
json transform_anthropic_to_openai_expected(const json& anthropic_req) {
    json openai_req;

    // Map model
    if (anthropic_req.contains("model")) {
        std::string model = anthropic_req["model"];
        if (model == "claude-3-5-sonnet-20241022") openai_req["model"] = "gpt-4-turbo";
        else if (model == "claude-3-5-haiku-20241022") openai_req["model"] = "gpt-4o-mini";
        else openai_req["model"] = model; // fallback
    }

    // Copy messages
    if (anthropic_req.contains("messages")) {
        openai_req["messages"] = anthropic_req["messages"];
    }

    // Map common parameters
    if (anthropic_req.contains("max_tokens")) {
        openai_req["max_tokens"] = anthropic_req["max_tokens"];
    }
    if (anthropic_req.contains("temperature")) {
        openai_req["temperature"] = anthropic_req["temperature"];
    }
    if (anthropic_req.contains("top_p")) {
        openai_req["top_p"] = anthropic_req["top_p"];
    }

    // Add OpenAI-specific defaults
    if (!openai_req.contains("frequency_penalty")) {
        openai_req["frequency_penalty"] = 0.0;
    }
    if (!openai_req.contains("presence_penalty")) {
        openai_req["presence_penalty"] = 0.0;
    }

    return openai_req;
}

// Test request transformation: Anthropic to OpenAI
void test_anthropic_to_openai_requests(ApiTransformer& transformer, TransformTestResults& results) {
    std::cout << "\n=== ANTHROPIC TO OPENAI REQUEST TRANSFORMATION TESTS ===" << std::endl;

    // Basic Anthropic request
    json anthropic_basic = {
        {"model", "claude-3-5-sonnet-20241022"},
        {"messages", json::array({
            {{"role", "user"}, {"content", "Hello, Claude!"}}
        })},
        {"max_tokens", 1024},
        {"temperature", 0.7}
    };

    TransformResult result = transformer.transform_request(
        anthropic_basic, APIFormat::ANTHROPIC, APIFormat::OPENAI
    );

    bool success = result.success &&
                   result.transformed_data.contains("model") &&
                   result.transformed_data["model"] == "gpt-4-turbo" &&
                   result.transformed_data.contains("messages") &&
                   result.transformed_data.contains("frequency_penalty") &&
                   result.transformed_data.contains("presence_penalty");

    results.add_result(
        success,
        "anthropic_to_openai_basic",
        "Basic Anthropic to OpenAI transformation with model mapping"
    );

    // Anthropic request with only essential fields
    json anthropic_minimal = {
        {"model", "claude-3-opus-20240229"},
        {"messages", json::array({
            {{"role", "user"}, {"content", "Minimal request"}}
        })}
    };

    result = transformer.transform_request(
        anthropic_minimal, APIFormat::ANTHROPIC, APIFormat::OPENAI
    );

    success = result.success &&
              result.transformed_data.contains("model") &&
              result.transformed_data.contains("messages") &&
              result.transformed_data.contains("max_tokens"); // Should add default

    results.add_result(
        success,
        "anthropic_to_openai_minimal",
        "Minimal Anthropic request with default value injection"
    );

    // Anthropic request with unknown model (should preserve model name)
    json anthropic_unknown_model = {
        {"model", "unknown-claude-model"},
        {"messages", json::array({
            {{"role", "user"}, {"content", "Test"}}
        })}
    };

    result = transformer.transform_request(
        anthropic_unknown_model, APIFormat::ANTHROPIC, APIFormat::OPENAI
    );

    success = result.success &&
              result.transformed_data.contains("model") &&
              result.transformed_data["model"] == "unknown-claude-model";

    results.add_result(
        success,
        "anthropic_to_openai_unknown_model",
        "Unknown model preservation in transformation"
    );
}

// Test request transformation: OpenAI to Anthropic
void test_openai_to_anthropic_requests(ApiTransformer& transformer, TransformTestResults& results) {
    std::cout << "\n=== OPENAI TO ANTHROPIC REQUEST TRANSFORMATION TESTS ===" << std::endl;

    // Basic OpenAI request
    json openai_basic = {
        {"model", "gpt-4-turbo"},
        {"messages", json::array({
            {{"role", "user"}, {"content", "Hello, GPT!"}}
        })},
        {"max_tokens", 1024},
        {"temperature", 0.7},
        {"frequency_penalty", 0.1},
        {"presence_penalty", 0.2}
    };

    TransformResult result = transformer.transform_request(
        openai_basic, APIFormat::OPENAI, APIFormat::ANTHROPIC
    );

    bool success = result.success &&
                   result.transformed_data.contains("model") &&
                   result.transformed_data["model"] == "claude-3-5-sonnet-20241022" &&
                   result.transformed_data.contains("messages") &&
                   !result.transformed_data.contains("frequency_penalty") &&
                   !result.transformed_data.contains("presence_penalty");

    results.add_result(
        success,
        "openai_to_anthropic_basic",
        "Basic OpenAI to Anthropic transformation with parameter filtering"
    );

    // OpenAI request with only essential fields
    json openai_minimal = {
        {"model", "gpt-3.5-turbo"},
        {"messages", json::array({
            {{"role", "user"}, {"content", "Minimal request"}}
        })}
    };

    result = transformer.transform_request(
        openai_minimal, APIFormat::OPENAI, APIFormat::ANTHROPIC
    );

    success = result.success &&
              result.transformed_data.contains("model") &&
              result.transformed_data.contains("messages") &&
              result.transformed_data.contains("max_tokens"); // Should add default

    results.add_result(
        success,
        "openai_to_anthropic_minimal",
        "Minimal OpenAI request with default value injection"
    );
}

// Test response transformation
void test_response_transformation(ApiTransformer& transformer, TransformTestResults& results) {
    std::cout << "\n=== RESPONSE TRANSFORMATION TESTS ===" << std::endl;

    // Anthropic-style response (should be converted to OpenAI format)
    json anthropic_response = {
        {"id", "msg_123"},
        {"type", "message"},
        {"role", "assistant"},
        {"content", json::array({
            {{"type", "text"}, {"text", "Hello from Claude!"}}
        })},
        {"model", "claude-3-5-sonnet-20241022"},
        {"stop_reason", "end_turn"},
        {"stop_sequence", nullptr},
        {"usage", {
            {"input_tokens", 10},
            {"output_tokens", 15}
        }}
    };

    TransformResult result = transformer.transform_response(
        anthropic_response, APIFormat::OPENAI, APIFormat::ANTHROPIC
    );

    bool success = result.success &&
                   result.transformed_data.contains("choices") &&
                   result.transformed_data.contains("usage");

    results.add_result(
        success,
        "anthropic_to_openai_response",
        "Anthropic response to OpenAI format conversion"
    );

    // OpenAI-style response (should be converted to Anthropic format)
    json openai_response = {
        {"id", "chatcmpl-123"},
        {"object", "chat.completion"},
        {"created", 1677652288},
        {"model", "gpt-4-turbo"},
        {"choices", json::array({
            {
                {"index", 0},
                {"message", {
                    {"role", "assistant"},
                    {"content", "Hello from GPT!"}
                }},
                {"finish_reason", "stop"}
            }
        })},
        {"usage", {
            {"prompt_tokens", 10},
            {"completion_tokens", 15},
            {"total_tokens", 25}
        }}
    };

    result = transformer.transform_response(
        openai_response, APIFormat::ANTHROPIC, APIFormat::OPENAI
    );

    success = result.success &&
              result.transformed_data.contains("content") &&
              result.transformed_data.contains("usage");

    results.add_result(
        success,
        "openai_to_anthropic_response",
        "OpenAI response to Anthropic format conversion"
    );
}

// Test model mapping
void test_model_mapping(ApiTransformer& transformer, TransformTestResults& results) {
    std::cout << "\n=== MODEL MAPPING TESTS ===" << std::endl;

    // Test known model mappings
    std::vector<std::pair<std::string, std::string>> known_mappings = {
        {"claude-3-5-sonnet-20241022", "gpt-4-turbo"},
        {"claude-3-5-haiku-20241022", "gpt-4o-mini"},
        {"claude-3-opus-20240229", "gpt-4-turbo"},
        {"claude-3-sonnet-20240229", "gpt-4-turbo"},
        {"claude-3-haiku-20240307", "gpt-3.5-turbo"}
    };

    for (const auto& [anthropic_model, expected_openai_model] : known_mappings) {
        std::string mapped = transformer.map_model(anthropic_model, APIFormat::ANTHROPIC, APIFormat::OPENAI);
        bool success = (mapped == expected_openai_model);
        results.add_result(
            success,
            "model_mapping_anthropic_to_openai_" + anthropic_model.substr(0, 10),
            anthropic_model + " -> " + mapped + " (expected: " + expected_openai_model + ")"
        );

        // Test reverse mapping
        std::string reverse_mapped = transformer.map_model(expected_openai_model, APIFormat::OPENAI, APIFormat::ANTHROPIC);
        bool reverse_success = (reverse_mapped == anthropic_model);
        results.add_result(
            reverse_success,
            "model_mapping_openai_to_anthropic_" + expected_openai_model.substr(0, 10),
            expected_openai_model + " -> " + reverse_mapped + " (expected: " + anthropic_model + ")"
        );
    }

    // Test unknown model preservation
    std::string unknown_model = "unknown-custom-model";
    std::string mapped_unknown = transformer.map_model(unknown_model, APIFormat::ANTHROPIC, APIFormat::OPENAI);
    results.add_result(
        mapped_unknown == unknown_model,
        "model_mapping_unknown_preservation",
        "Unknown model should be preserved: " + mapped_unknown
    );
}

// Test error handling
void test_error_handling(ApiTransformer& transformer, TransformTestResults& results) {
    std::cout << "\n=== ERROR HANDLING TESTS ===" << std::endl;

    // Test invalid JSON (empty object)
    json empty_request;
    TransformResult result = transformer.transform_request(
        empty_request, APIFormat::ANTHROPIC, APIFormat::OPENAI
    );

    results.add_result(
        !result.success,
        "empty_request_handling",
        "Empty request should fail gracefully: " + result.error_message
    );

    // Test invalid format combination (same format)
    json valid_request = {
        {"model", "claude-3-5-sonnet"},
        {"messages", json::array()}
    };

    result = transformer.transform_request(
        valid_request, APIFormat::ANTHROPIC, APIFormat::ANTHROPIC
    );

    results.add_result(
        result.success, // Should succeed (no transformation needed)
        "same_format_transformation",
        "Same format transformation should succeed"
    );

    // Test malformed messages array
    json malformed_messages = {
        {"model", "claude-3-5-sonnet"},
        {"messages", "invalid_messages"} // Should be array
    };

    result = transformer.transform_request(
        malformed_messages, APIFormat::ANTHROPIC, APIFormat::OPENAI
    );

    results.add_result(
        !result.success,
        "malformed_messages_handling",
        "Malformed messages should fail: " + result.error_message
    );
}

// Test transformation quality and content preservation
void test_content_preservation(ApiTransformer& transformer, TransformTestResults& results) {
    std::cout << "\n=== CONTENT PRESERVATION TESTS ===" << std::endl;

    // Test complex message structure
    json complex_anthropic = {
        {"model", "claude-3-5-sonnet-20241022"},
        {"messages", json::array({
            {{"role", "user"}, {"content", "First message"}},
            {{"role", "assistant"}, {"content", "First response"}},
            {{"role", "user"}, {"content", "Follow up question"}}
        })},
        {"max_tokens", 2048},
        {"temperature", 0.5},
        {"top_p", 0.9}
    };

    TransformResult result = transformer.transform_request(
        complex_anthropic, APIFormat::ANTHROPIC, APIFormat::OPENAI
    );

    bool content_preserved = result.success &&
                            result.transformed_data.contains("messages") &&
                            result.transformed_data["messages"].size() == 3 &&
                            result.transformed_data["max_tokens"] == 2048 &&
                            result.transformed_data["temperature"] == 0.5 &&
                            result.transformed_data["top_p"] == 0.9;

    results.add_result(
        content_preserved,
        "complex_message_preservation",
        "Complex message structure should be preserved"
    );

    // Test special characters handling
    json special_chars_request = {
        {"model", "claude-3-5-sonnet"},
        {"messages", json::array({
            {{"role", "user"}, {"content", "Hello 世界! 🚀 Testing special chars: ñáéíóú"}}
        })},
        {"max_tokens", 100}
    };

    result = transformer.transform_request(
        special_chars_request, APIFormat::ANTHROPIC, APIFormat::OPENAI
    );

    content_preserved = result.success &&
                       result.transformed_data.contains("messages") &&
                       result.transformed_data["messages"][0]["content"] == "Hello 世界! 🚀 Testing special chars: ñáéíóú";

    results.add_result(
        content_preserved,
        "special_characters_preservation",
        "Special characters should be preserved in transformation"
    );
}

// Test transformer configuration
void test_transformer_configuration(TransformTestResults& results) {
    std::cout << "\n=== TRANSFORMER CONFIGURATION TESTS ===" << std::endl;

    // Test default configuration
    auto default_transformer = TransformerFactory::create_transformer("production");
    results.add_result(
        default_transformer != nullptr,
        "default_transformer_creation",
        "Should create transformer with default configuration"
    );

    // Test custom configuration
    TransformConfig custom_config;
    custom_config.anthropic_defaults["max_tokens"] = 2048;
    custom_config.openai_defaults["temperature"] = 0.5;

    auto custom_transformer = TransformerFactory::create_transformer(custom_config);
    results.add_result(
        custom_transformer != nullptr,
        "custom_transformer_creation",
        "Should create transformer with custom configuration"
    );

    // Test configuration application
    if (custom_transformer) {
        json test_request = {
            {"model", "claude-3-5-sonnet"},
            {"messages", json::array({{"role", "user"}, {"content", "test"}}})}
        };

        TransformResult result = custom_transformer->transform_request(
            test_request, APIFormat::ANTHROPIC, APIFormat::OPENAI
        );

        bool defaults_applied = result.success &&
                               result.transformed_data.contains("temperature") &&
                               result.transformed_data["temperature"] == 0.5;

        results.add_result(
            defaults_applied,
            "custom_defaults_application",
            "Custom defaults should be applied in transformation"
        );
    }
}

// Performance test for transformation
void test_transformation_performance(ApiTransformer& transformer, TransformTestResults& results) {
    std::cout << "\n=== TRANSFORMATION PERFORMANCE TESTS ===" << std::endl;

    const int num_iterations = 1000;
    json test_request = {
        {"model", "claude-3-5-sonnet-20241022"},
        {"messages", json::array({
            {{"role", "user"}, {"content", "Performance test message"}}
        })},
        {"max_tokens", 1024},
        {"temperature", 0.7}
    };

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_iterations; ++i) {
        TransformResult result = transformer.transform_request(
            test_request, APIFormat::ANTHROPIC, APIFormat::OPENAI
        );
        // Prevent compiler optimization
        assert(result.success);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    double avg_time_us = static_cast<double>(duration.count()) / num_iterations;
    double avg_time_ms = avg_time_us / 1000.0;

    bool performance_acceptable = avg_time_ms < 0.5;  // Should be less than 0.5ms per transformation
    results.add_result(
        performance_acceptable,
        "transformation_performance_benchmark",
        "Average transformation time: " + std::to_string(avg_time_ms) + "ms per request (" +
        std::to_string(num_iterations) + " iterations)"
    );
}

// Main test function
int main() {
    std::cout << "=== AIMUX2 API TRANSFORMATION TEST SUITE ===" << std::endl;
    std::cout << "Testing comprehensive API format transformation functionality" << std::endl;

    TransformTestResults results;

    try {
        // Initialize API transformer
        ApiTransformer transformer;

        // Run all test categories
        test_anthropic_to_openai_requests(transformer, results);
        test_openai_to_anthropic_requests(transformer, results);
        test_response_transformation(transformer, results);
        test_model_mapping(transformer, results);
        test_error_handling(transformer, results);
        test_content_preservation(transformer, results);
        test_transformer_configuration(results);
        test_transformation_performance(transformer, results);

    } catch (const std::exception& e) {
        std::cerr << "Test suite crashed with exception: " << e.what() << std::endl;
        return 1;
    }

    // Print final results
    results.print_summary();

    // Return exit code based on test results
    return (results.failed_tests == 0) ? 0 : 1;
}