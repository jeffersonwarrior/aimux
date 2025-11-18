/**
 * @file format_detection_tests.cpp
 * @brief Comprehensive tests for API format detection functionality
 *
 * This test program validates:
 * - Format detection accuracy for various request formats
 * - Header-based detection
 * - Body content-based detection
 * - Model name pattern detection
 * - Endpoint pattern detection
 * - Confidence score calculation
 * - Edge cases and error handling
 */

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <cassert>
#include <iomanip>
#include <chrono>
#include <nlohmann/json.hpp>
#include "aimux/gateway/format_detector.hpp"

using namespace aimux::gateway;
using json = nlohmann::json;

// Test data structures
struct TestCase {
    std::string name;
    std::string description;
    json request_body;
    std::map<std::string, std::string> headers;
    std::string endpoint;
    APIFormat expected_format;
    double min_confidence;
};

struct TestResults {
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
        std::cout << "FORMAT DETECTION TEST SUMMARY" << std::endl;
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

// Create test cases for format detection
std::vector<TestCase> create_test_cases() {
    std::vector<TestCase> cases;

    // Anthropic format test cases
    cases.push_back({
        "anthropic_basic",
        "Basic Anthropic message format",
        json{
            {"model", "claude-3-5-sonnet-20241022"},
            {"messages", json::array({
                {{"role", "user"}, {"content", "Hello, Claude!"}}
            })},
            {"max_tokens", 1024}
        },
        {
            {"anthropic-version", "2023-06-01"},
            {"x-api-key", "sk-ant-api03-test"},
            {"content-type", "application/json"}
        },
        "/v1/messages",
        APIFormat::ANTHROPIC,
        0.7
    });

    // OpenAI format test cases
    cases.push_back({
        "openai_basic",
        "Basic OpenAI chat format",
        json{
            {"model", "gpt-4-turbo"},
            {"messages", json::array({
                {{"role", "user"}, {"content", "Hello, GPT!"}}
            })},
            {"max_tokens", 1024},
            {"temperature", 0.7}
        },
        {
            {"authorization", "Bearer sk-test-key"},
            {"content-type", "application/json"}
        },
        "/v1/chat/completions",
        APIFormat::OPENAI,
        0.7
    });

    // Model-based detection cases
    cases.push_back({
        "anthropic_model_only",
        "Model name indicates Anthropic format",
        json{
            {"model", "claude-3-opus-20240229"},
            {"messages", json::array({
                {{"role", "user"}, {"content", "Test"}}
            })}
        },
        {},
        "",
        APIFormat::ANTHROPIC,
        0.2
    });

    cases.push_back({
        "openai_model_only",
        "Model name indicates OpenAI format",
        json{
            {"model", "gpt-3.5-turbo"},
            {"messages", json::array({
                {{"role", "user"}, {"content", "Test"}}
            })}
        },
        {},
        "",
        APIFormat::OPENAI,
        0.2
    });

    // Endpoint-based detection cases
    cases.push_back({
        "anthropic_endpoint",
        "Endpoint indicates Anthropic format",
        json{},
        {},
        "/v1/messages",
        APIFormat::ANTHROPIC,
        0.35
    });

    cases.push_back({
        "openai_endpoint",
        "Endpoint indicates OpenAI format",
        json{},
        {},
        "/v1/chat/completions",
        APIFormat::OPENAI,
        0.35
    });

    // Header-based detection cases
    cases.push_back({
        "anthropic_headers",
        "Headers indicate Anthropic format",
        json{},
        {
            {"anthropic-version", "2023-06-01"},
            {"x-api-key", "sk-ant-test"}
        },
        "",
        APIFormat::ANTHROPIC,
        0.3
    });

    cases.push_back({
        "openai_headers",
        "Headers indicate OpenAI format",
        json{},
        {
            {"authorization", "Bearer sk-test"},
            {"openai-organization", "org-test"}
        },
        "",
        APIFormat::OPENAI,
        0.3
    });

    // Edge cases
    cases.push_back({
        "empty_request",
        "Empty request should return UNKNOWN",
        json{},
        {},
        "",
        APIFormat::UNKNOWN,
        0.0
    });

    cases.push_back({
        "conflicting_signals",
        "Conflicting model and endpoint should reduce confidence",
        json{
            {"model", "claude-3-5-sonnet"},
            {"messages", json::array()}
        },
        {},
        "/v1/chat/completions",  // OpenAI endpoint with Anthropic model
        APIFormat::OPENAI,  // Endpoint takes precedence
        0.35  // Endpoint-based confidence
    });

    return cases;
}

// Test format detection accuracy
void test_format_detection_accuracy(FormatDetector& detector, TestResults& results) {
    std::cout << "\n=== FORMAT DETECTION ACCURACY TESTS ===" << std::endl;

    auto test_cases = create_test_cases();

    for (const auto& test_case : test_cases) {
        DetectionResult result = detector.detect_format(
            test_case.request_body,
            test_case.headers,
            test_case.endpoint
        );

        bool format_correct = (result.format == test_case.expected_format);
        bool confidence_adequate = (result.confidence >= test_case.min_confidence);
        bool test_passed = format_correct && confidence_adequate;

        std::string details = "Format: " + format_to_string(result.format) +
                             " (expected: " + format_to_string(test_case.expected_format) +
                             "), Confidence: " + std::to_string(result.confidence) +
                             " (min: " + std::to_string(test_case.min_confidence) + ")";

        if (!result.reasoning.empty()) {
            details += ", Reasoning: " + result.reasoning;
        }

        results.add_result(test_passed, test_case.name, details);
    }
}

// Test format detection with different configurations
void test_format_detection_configurations(TestResults& results) {
    std::cout << "\n=== FORMAT DETECTION CONFIGURATION TESTS ===" << std::endl;

    // Test with default configuration
    FormatDetector default_detector;
    DetectionResult result1 = default_detector.detect_format(
        json{{"model", "claude-3-5-sonnet"}},
        {},
        ""
    );
    results.add_result(
        result1.format == APIFormat::ANTHROPIC && result1.confidence > 0.1,
        "default_config_anthropic",
        "Should detect Claude model with default config"
    );

    // Test with custom configuration
    FormatDetectionConfig custom_config;
    custom_config.anthropic_model_patterns.push_back("custom-claude");
    custom_config.openai_model_patterns.push_back("custom-gpt");

    FormatDetector custom_detector(custom_config);
    DetectionResult result2 = custom_detector.detect_format(
        json{{"model", "custom-claude"}},
        {},
        ""
    );
    results.add_result(
        result2.format == APIFormat::ANTHROPIC,
        "custom_config_detection",
        "Should detect custom model pattern"
    );
}

// Test confidence score calculation
void test_confidence_calculation(TestResults& results) {
    std::cout << "\n=== CONFIDENCE CALCULATION TESTS ===" << std::endl;

    FormatDetector detector;

    // Test high confidence case (multiple indicators)
    DetectionResult high_conf = detector.detect_format(
        json{
            {"model", "claude-3-5-sonnet"},
            {"messages", {{"role", "user"}, {"content", "test"}}}
        },
        {
            {"anthropic-version", "2023-06-01"},
            {"x-api-key", "sk-ant-test"}
        },
        "/v1/messages"
    );
    results.add_result(
        high_conf.confidence >= 0.6,
        "high_confidence_multiple_signals",
        "Multiple strong signals should give high confidence: " + std::to_string(high_conf.confidence)
    );

    // Test medium confidence case (single indicator)
    DetectionResult med_conf = detector.detect_format(
        json{{"model", "claude-3-opus"}},
        {},
        ""
    );
    results.add_result(
        med_conf.confidence >= 0.1 && med_conf.confidence < 0.5,
        "medium_confidence_single_signal",
        "Single signal should give medium confidence: " + std::to_string(med_conf.confidence)
    );

    // Test low confidence case
    DetectionResult low_conf = detector.detect_format(
        json{},
        {},
        ""
    );
    results.add_result(
        low_conf.confidence < 0.3,
        "low_confidence_no_signals",
        "No signals should give low confidence: " + std::to_string(low_conf.confidence)
    );
}

// Test edge cases and error handling
void test_edge_cases(FormatDetector& detector, TestResults& results) {
    std::cout << "\n=== EDGE CASES AND ERROR HANDLING TESTS ===" << std::endl;

    // Test malformed JSON
    try {
        json malformed = json::parse("{\"invalid\": json}");
        DetectionResult result = detector.detect_format(malformed, {}, "");
        results.add_result(false, "malformed_json_parsing", "Should not reach here - JSON parse should fail");
    } catch (const json::parse_error&) {
        results.add_result(true, "malformed_json_parsing", "Correctly handles malformed JSON");
    }

    // Test invalid model field type
    DetectionResult result1 = detector.detect_format(
        json{{"model", 123}},  // model should be string
        {},
        ""
    );
    results.add_result(
        result1.format == APIFormat::UNKNOWN,
        "invalid_model_type",
        "Invalid model type should result in UNKNOWN format"
    );

    // Test extremely long endpoint
    std::string long_endpoint(1000, 'a');
    DetectionResult result2 = detector.detect_format(
        json{},
        {},
        long_endpoint
    );
    results.add_result(
        true,  // Should not crash
        "long_endpoint_handling",
        "Should handle long endpoints gracefully"
    );

    // Test special characters in headers
    std::map<std::string, std::string> special_headers = {
        {"x-api-key", "sk-ant-test-特殊字符-🚀"},
        {"user-agent", "测试客户端"}
    };
    DetectionResult result3 = detector.detect_format(
        json{{"model", "claude-3"}},
        special_headers,
        ""
    );
    results.add_result(
        result3.format == APIFormat::ANTHROPIC,
        "special_characters_headers",
        "Should handle special characters in headers"
    );
}

// Test quick detection method
void test_quick_detection(FormatDetector& detector, TestResults& results) {
    std::cout << "\n=== QUICK DETECTION TESTS ===" << std::endl;

    // Test endpoint-based quick detection
    APIFormat format1 = detector.detect_format_quick(
        "/v1/messages",
        {}
    );
    results.add_result(
        format1 == APIFormat::ANTHROPIC,
        "quick_detection_anthropic_endpoint",
        "Quick detection should identify Anthropic endpoint"
    );

    APIFormat format2 = detector.detect_format_quick(
        "/v1/chat/completions",
        {}
    );
    results.add_result(
        format2 == APIFormat::OPENAI,
        "quick_detection_openai_endpoint",
        "Quick detection should identify OpenAI endpoint"
    );

    // Test header-based quick detection
    APIFormat format3 = detector.detect_format_quick(
        "",
        {{"anthropic-version", "2023-06-01"}}
    );
    results.add_result(
        format3 == APIFormat::ANTHROPIC,
        "quick_detection_anthropic_headers",
        "Quick detection should identify Anthropic headers"
    );

    APIFormat format4 = detector.detect_format_quick(
        "",
        {{"authorization", "Bearer sk-test"}}
    );
    results.add_result(
        format4 == APIFormat::OPENAI,
        "quick_detection_openai_headers",
        "Quick detection should identify OpenAI headers"
    );
}

// Performance test for format detection
void test_performance(FormatDetector& detector, TestResults& results) {
    std::cout << "\n=== PERFORMANCE TESTS ===" << std::endl;

    const int num_iterations = 1000;
    json test_request = {
        {"model", "claude-3-5-sonnet"},
        {"messages", json::array({{{"role", "user"}, {"content", "test"}}})},
        {"max_tokens", 1024}
    };
    std::map<std::string, std::string> test_headers = {
        {"anthropic-version", "2023-06-01"},
        {"x-api-key", "sk-ant-test"}
    };

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_iterations; ++i) {
        DetectionResult result = detector.detect_format(test_request, test_headers, "/v1/messages");
        // Prevent compiler optimization
        assert(result.format == APIFormat::ANTHROPIC);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    double avg_time_us = static_cast<double>(duration.count()) / num_iterations;
    double avg_time_ms = avg_time_us / 1000.0;

    bool performance_acceptable = avg_time_ms < 0.1;  // Should be less than 0.1ms per detection
    results.add_result(
        performance_acceptable,
        "performance_benchmark",
        "Average detection time: " + std::to_string(avg_time_ms) + "ms per request (" +
        std::to_string(num_iterations) + " iterations)"
    );
}

// Main test function
int main() {
    std::cout << "=== AIMUX2 FORMAT DETECTION TEST SUITE ===" << std::endl;
    std::cout << "Testing comprehensive API format detection functionality" << std::endl;

    TestResults results;

    try {
        // Initialize format detector
        FormatDetector detector;

        // Run all test categories
        test_format_detection_accuracy(detector, results);
        test_format_detection_configurations(results);
        test_confidence_calculation(results);

        // Create new detector for edge case tests
        FormatDetector edge_detector;
        test_edge_cases(edge_detector, results);
        test_quick_detection(edge_detector, results);
        test_performance(edge_detector, results);

    } catch (const std::exception& e) {
        std::cerr << "Test suite crashed with exception: " << e.what() << std::endl;
        return 1;
    }

    // Print final results
    results.print_summary();

    // Return exit code based on test results
    return (results.failed_tests == 0) ? 0 : 1;
}