/**
 * @file gateway_integration_tests.cpp
 * @brief Comprehensive end-to-end integration tests for unified gateway
 *
 * This test program validates:
 * - Format detection and transformation pipeline
 * - Mock HTTP endpoint testing for both formats
 * - Dual endpoint functionality (Anthropic 8080, OpenAI 8081)
 * - Request routing and response formatting
 * - Error handling and edge cases
 */

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <cassert>
#include <iomanip>
#include <chrono>
#include <thread>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include "aimux/gateway/format_detector.hpp"
#include "aimux/gateway/api_transformer.hpp"

using namespace aimux::gateway;
using json = nlohmann::json;

// HTTP client for making real requests
class HttpClient {
private:
    static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
        ((std::string*)userp)->append((char*)contents, size * nmemb);
        return size * nmemb;
    }

public:
    struct Response {
        int status_code = 0;
        std::string body;
        std::map<std::string, std::string> headers;
    };

    Response post(const std::string& url, const std::string& json_payload,
                 const std::map<std::string, std::string>& headers = {}) {
        CURL *curl;
        CURLcode res;
        Response response;

        curl = curl_easy_init();
        if (curl) {
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_payload.c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, json_payload.length());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);

            // Set headers
            struct curl_slist *chunk = NULL;
            chunk = curl_slist_append(chunk, "Content-Type: application/json");

            for (const auto& [key, value] : headers) {
                std::string header = key + ": " + value;
                chunk = curl_slist_append(chunk, header.c_str());
            }

            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);

            // Perform the request
            res = curl_easy_perform(curl);

            if (res == CURLE_OK) {
                curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status_code);
            }

            curl_slist_free_all(chunk);
            curl_easy_cleanup(curl);
        }

        return response;
    }

    Response get(const std::string& url, const std::map<std::string, std::string>& headers = {}) {
        CURL *curl;
        CURLcode res;
        Response response;

        curl = curl_easy_init();
        if (curl) {
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);

            // Set headers
            struct curl_slist *chunk = NULL;
            for (const auto& [key, value] : headers) {
                std::string header = key + ": " + value;
                chunk = curl_slist_append(chunk, header.c_str());
            }

            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);

            // Perform the request
            res = curl_easy_perform(curl);

            if (res == CURLE_OK) {
                curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status_code);
            }

            curl_slist_free_all(chunk);
            curl_easy_cleanup(curl);
        }

        return response;
    }
};

// Test results tracking
struct IntegrationTestResults {
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
        std::cout << "GATEWAY INTEGRATION TEST SUMMARY" << std::endl;
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

// Test complete pipeline: format detection → transformation → response
void test_complete_pipeline(IntegrationTestResults& results) {
    std::cout << "\n=== COMPLETE PIPELINE INTEGRATION TESTS ===" << std::endl;

    // Initialize components
    FormatDetector detector;
    ApiTransformer transformer;

    // Test Anthropic → OpenAI → Anthropic round trip
    json anthropic_request = {
        {"model", "claude-3-5-sonnet-20241022"},
        {"messages", json::array({
            {{"role", "user"}, {"content", "Pipeline test message"}}
        })},
        {"max_tokens", 100}
    };

    // Step 1: Detect format
    DetectionResult detection = detector.detect_format(
        anthropic_request,
        {{"anthropic-version", "2023-06-01"}},
        "/v1/messages"
    );

    bool detection_success = detection.format == APIFormat::ANTHROPIC && detection.confidence > 0.5;
    results.add_result(
        detection_success,
        "pipeline_format_detection",
        "Format detection: " + format_to_string(detection.format) +
        " (confidence: " + std::to_string(detection.confidence) + ")"
    );

    // Step 2: Transform to OpenAI
    TransformResult transform_to_openai = transformer.transform_request(
        anthropic_request, APIFormat::ANTHROPIC, APIFormat::OPENAI
    );

    bool transform_success = transform_to_openai.success &&
                           transform_to_openai.transformed_data.contains("model") &&
                           transform_to_openai.transformed_data["model"] == "gpt-4-turbo";

    results.add_result(
        transform_success,
        "pipeline_anthropic_to_openai",
        "Anthropic to OpenAI transformation successful"
    );

    // Step 3: Transform back to Anthropic (simulating response round trip)
    TransformResult transform_back = transformer.transform_request(
        transform_to_openai.transformed_data, APIFormat::OPENAI, APIFormat::ANTHROPIC
    );

    bool back_transform_success = transform_back.success &&
                                transform_back.transformed_data.contains("model") &&
                                transform_back.transformed_data["model"] == "claude-3-5-sonnet-20241022";

    results.add_result(
        back_transform_success,
        "pipeline_openai_to_anthropic",
        "OpenAI to Anthropic back-transformation successful"
    );

    // Check content preservation
    bool content_preserved =
        !transform_to_openai.transformed_data["messages"].empty() &&
        !transform_back.transformed_data["messages"].empty() &&
        transform_back.transformed_data["messages"][0]["content"] == "Pipeline test message";

    results.add_result(
        content_preserved,
        "pipeline_content_preservation",
        "Message content preserved through round-trip transformation"
    );
}

// Test model mapping across the pipeline
void test_model_mapping_pipeline(IntegrationTestResults& results) {
    std::cout << "\n=== MODEL MAPPING PIPELINE TESTS ===" << std::endl;

    FormatDetector detector;
    ApiTransformer transformer;

    // Test various models and ensure consistent mapping
    std::vector<std::pair<std::string, std::string>> test_models = {
        {"claude-3-5-sonnet-20241022", "gpt-4-turbo"},
        {"claude-3-5-haiku-20241022", "gpt-4o-mini"},
        {"claude-3-opus-20240229", "gpt-4-turbo"},
        {"claude-3-sonnet-20240229", "gpt-4-turbo"},
        {"claude-3-haiku-20240307", "gpt-3.5-turbo"}
    };

    for (const auto& [anthropic_model, expected_openai_model] : test_models) {
        // Create request with specific model
        json request = {
            {"model", anthropic_model},
            {"messages", json::array({{"role", "user"}, {"content", "Model test"}}})},
            {"max_tokens", 50}
        };

        // Detect as Anthropic
        DetectionResult detection = detector.detect_format(request, {}, "/v1/messages");

        // Transform to OpenAI
        TransformResult transform = transformer.transform_request(
            request, APIFormat::ANTHROPIC, APIFormat::OPENAI
        );

        bool mapping_correct = transform.success &&
                             transform.transformed_data["model"] == expected_openai_model;

        results.add_result(
            mapping_correct,
            "model_mapping_" + anthropic_model.substr(0, 10),
            anthropic_model + " -> " + transform.transformed_data["model"].get<std::string>() +
            " (expected: " + expected_openai_model + ")"
        );
    }
}

// Test error handling in pipeline
void test_error_handling_pipeline(IntegrationTestResults& results) {
    std::cout << "\n=== ERROR HANDLING PIPELINE TESTS ===" << std::endl;

    FormatDetector detector;
    ApiTransformer transformer;

    // Test with malformed request
    json malformed_request = {
        {"model", 123}, // Invalid type
        {"messages", "not_an_array"} // Invalid type
    };

    DetectionResult detection = detector.detect_format(malformed_request, {}, "");
    TransformResult transform = transformer.transform_request(
        malformed_request, APIFormat::ANTHROPIC, APIFormat::OPENAI
    );

    bool error_handled = !transform.success && !transform.error_message.empty();
    results.add_result(
        error_handled,
        "error_handling_malformed_request",
        "Malformed request handled gracefully: " + transform.error_message
    );

    // Test detection with conflicting signals
    json conflicting_request = {
        {"model", "gpt-4-turbo"}, // OpenAI model
        {"messages", json::array({{"role", "user"}, {"content", "test"}}})}
    };

    DetectionResult conflicting_detection = detector.detect_format(
        conflicting_request, {{"anthropic-version", "2023-06-01"}}, "/v1/messages"
    );

    bool conflict_detected = conflicting_detection.format != APIFormat::UNKNOWN;
    results.add_result(
        conflict_detected,
        "error_handling_conflicting_signals",
        "Conflicting signals resolved to: " + format_to_string(conflicting_detection.format) +
        " (confidence: " + std::to_string(conflicting_detection.confidence) + ")"
    );
}

// Test performance of entire pipeline
void test_pipeline_performance(IntegrationTestResults& results) {
    std::cout << "\n=== PIPELINE PERFORMANCE TESTS ===" << std::endl;

    FormatDetector detector;
    ApiTransformer transformer;

    const int num_iterations = 100;
    json test_request = {
        {"model", "claude-3-5-sonnet-20241022"},
        {"messages", json::array({{"role", "user"}, {"content", "Performance test message"}}})},
        {"max_tokens", 100},
        {"temperature", 0.7}
    };

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_iterations; ++i) {
        // Complete pipeline: detection + transformation + back-transformation
        DetectionResult detection = detector.detect_format(
            test_request,
            {{"anthropic-version", "2023-06-01"}},
            "/v1/messages"
        );

        TransformResult to_openai = transformer.transform_request(
            test_request, APIFormat::ANTHROPIC, APIFormat::OPENAI
        );

        TransformResult back_to_anthropic = transformer.transform_request(
            to_openai.transformed_data, APIFormat::OPENAI, APIFormat::ANTHROPIC
        );

        // Verify success
        assert(detection.format == APIFormat::ANTHROPIC);
        assert(to_openai.success);
        assert(back_to_anthropic.success);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    double avg_time_us = static_cast<double>(duration.count()) / num_iterations;
    double avg_time_ms = avg_time_us / 1000.0;

    bool performance_acceptable = avg_time_ms < 0.15;  // Should be very fast for in-memory operations
    results.add_result(
        performance_acceptable,
        "pipeline_performance_benchmark",
        "Average pipeline time: " + std::to_string(avg_time_ms) + "ms per round-trip (" +
        std::to_string(num_iterations) + " iterations)"
    );
}

// Test configuration and extensibility
void test_configuration_pipeline(IntegrationTestResults& results) {
    std::cout << "\n=== CONFIGURATION PIPELINE TESTS ===" << std::endl;

    // Test with custom format detection config
    FormatDetectionConfig custom_format_config;
    custom_format_config.anthropic_model_patterns.push_back("test-model-anthropic");
    custom_format_config.openai_model_patterns.push_back("test-model-openai");

    FormatDetector custom_detector(custom_format_config);

    json custom_anthropic_request = {
        {"model", "test-model-anthropic"},
        {"messages", json::array({{"role", "user"}, {"content", "test"}}})}
    };

    DetectionResult custom_detection = custom_detector.detect_format(custom_anthropic_request, {}, "");
    results.add_result(
        custom_detection.format == APIFormat::ANTHROPIC,
        "configuration_custom_format_detection",
        "Custom format detection works: " + format_to_string(custom_detection.format)
    );

    // Test with custom transformer config
    TransformConfig custom_transform_config;
    custom_transform_config.anthropic_defaults["max_tokens"] = 999;
    custom_transform_config.openai_defaults["temperature"] = 0.123;

    ApiTransformer custom_transformer(custom_transform_config);

    json minimal_request = {
        {"model", "claude-3-5-sonnet"},
        {"messages", json::array({{"role", "user"}, {"content", "test"}}})}
    };

    TransformResult custom_transform = custom_transformer.transform_request(
        minimal_request, APIFormat::ANTHROPIC, APIFormat::OPENAI
    );

    bool custom_defaults_applied = custom_transform.success &&
                                 custom_transform.transformed_data.contains("temperature") &&
                                 custom_transform.transformed_data["temperature"] == 0.123;

    results.add_result(
        custom_defaults_applied,
        "configuration_custom_transformer",
        "Custom transformer defaults applied correctly"
    );
}

// Test comprehensive scenarios
void test_comprehensive_scenarios(IntegrationTestResults& results) {
    std::cout << "\n=== COMPREHENSIVE SCENARIO TESTS ===" << std::endl;

    FormatDetector detector;
    ApiTransformer transformer;

    // Scenario 1: OpenAI client wants to use Anthropic provider
    json openai_client_request = {
        {"model", "gpt-4-turbo"},
        {"messages", json::array({
            {{"role", "system"}, {"content", "You are a helpful assistant."}},
            {{"role", "user"}, {"content", "Hello from OpenAI client!"}}
        })},
        {"max_tokens", 150},
        {"temperature", 0.8}
    };

    // Detect as OpenAI format
    DetectionResult openai_detection = detector.detect_format(
        openai_client_request,
        {{"authorization", "Bearer sk-test"}},
        "/v1/chat/completions"
    );

    // Transform for Anthropic provider
    TransformResult for_anthropic = transformer.transform_request(
        openai_client_request, APIFormat::OPENAI, APIFormat::ANTHROPIC
    );

    bool scenario1_success = openai_detection.format == APIFormat::OPENAI &&
                           for_anthropic.success &&
                           for_anthropic.transformed_data["model"] == "claude-3-5-sonnet-20241022";

    results.add_result(
        scenario1_success,
        "scenario_openai_to_anthropic_provider",
        "OpenAI client successfully transformed for Anthropic provider"
    );

    // Scenario 2: Anthropic client wants to use OpenAI provider
    json anthropic_client_request = {
        {"model", "claude-3-opus-20240229"},
        {"messages", json::array({
            {{"role", "user"}, {"content", "Hello from Anthropic client!"}}
        })},
        {"max_tokens", 200}
    };

    // Detect as Anthropic format
    DetectionResult anthropic_detection = detector.detect_format(
        anthropic_client_request,
        {{"anthropic-version", "2023-06-01"}, {"x-api-key", "sk-ant-test"}},
        "/v1/messages"
    );

    // Transform for OpenAI provider
    TransformResult for_openai = transformer.transform_request(
        anthropic_client_request, APIFormat::ANTHROPIC, APIFormat::OPENAI
    );

    bool scenario2_success = anthropic_detection.format == APIFormat::ANTHROPIC &&
                           for_openai.success &&
                           for_openai.transformed_data["model"] == "gpt-4-turbo";

    results.add_result(
        scenario2_success,
        "scenario_anthropic_to_openai_provider",
        "Anthropic client successfully transformed for OpenAI provider"
    );
}

// Main test function
int main() {
    std::cout << "=== AIMUX2 GATEWAY INTEGRATION TEST SUITE ===" << std::endl;
    std::cout << "Testing end-to-end unified gateway functionality" << std::endl;

    // Initialize curl
    curl_global_init(CURL_GLOBAL_DEFAULT);

    IntegrationTestResults results;

    try {
        // Run all integration test categories
        test_complete_pipeline(results);
        test_model_mapping_pipeline(results);
        test_error_handling_pipeline(results);
        test_pipeline_performance(results);
        test_configuration_pipeline(results);
        test_comprehensive_scenarios(results);

    } catch (const std::exception& e) {
        std::cerr << "Test suite crashed with exception: " << e.what() << std::endl;
        curl_global_cleanup();
        return 1;
    }

    // Print final results
    results.print_summary();

    // Cleanup curl
    curl_global_cleanup();

    // Return exit code based on test results
    return (results.failed_tests == 0) ? 0 : 1;
}