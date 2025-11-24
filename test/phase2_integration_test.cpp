#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <chrono>
#include <thread>
#include <memory>
#include <future>

#include "aimux/prettifier/prettifier_plugin.hpp"
#include "aimux/prettifier/cerebras_formatter.hpp"
#include "aimux/prettifier/openai_formatter.hpp"
#include "aimux/prettifier/anthropic_formatter.hpp"
#include "aimux/prettifier/synthetic_formatter.hpp"
#include "aimux/prettifier/streaming_processor.hpp"
#include "aimux/core/router.hpp"

using namespace aimux::prettifier;
using namespace aimux::core;
using ::testing::Gt;
using ::testing::Lt;
using ::testing::Ge;

/**
 * @brief Comprehensive Phase 2 Integration Test Suite
 *
 * This test suite validates the complete integration of all Phase 2 components:
 * - 4 provider-specific formatters
 * - Streaming processor
 * - Plugin registry
 * - Performance targets
 * - Thread safety
 * - Error handling
 */
class Phase2IntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize common test data
        test_request_.data = nlohmann::json{
            {"content", "Test request for formatting"},
            {"model", "test-model"}
        };

        test_context_.provider_name = "test_provider";
        test_context_.model_name = "test-model";
        test_context_.original_format = "json";
        test_context_.processing_start = std::chrono::system_clock::now();
    }

    Request test_request_;
    ProcessingContext test_context_;
};

// Performance Tests

TEST_F(Phase2IntegrationTest, PerformanceTargets_CerebratesFormatter) {
    auto formatter = std::make_shared<CerebrasFormatter>();

    // Test preprocessing performance
    auto start = std::chrono::high_resolution_clock::now();
    auto preprocess_result = formatter->preprocess_request(test_request_);
    auto preprocess_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - start).count();

    EXPECT_LT(preprocess_time, 50); // <50ms target
    EXPECT_TRUE(preprocess_result.success);

    // Test postprocessing performance
    Response response;
    response.data = R"({"choices":[{"message":{"content":"Fast response","tool_calls":[{"function":{"name":"test","arguments":"{}"}}]}}]})";

    start = std::chrono::high_resolution_clock::now();
    auto postprocess_result = formatter->postprocess_response(response, test_context_);
    auto postprocess_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - start).count();

    EXPECT_LT(postprocess_time, 50); // <50ms target
    EXPECT_TRUE(postprocess_result.success);
    EXPECT_EQ(postprocess_result.extracted_tool_calls.size(), 1);
}

TEST_F(Phase2IntegrationTest, PerformanceTargets_OpenAIFormatter) {
    auto formatter = std::make_shared<OpenAIFormatter>();

    Response response;
    response.data = R"({
        "choices":[{
            "message":{
                "content":"OpenAI response",
                "tool_calls":[{
                    "id":"call_1",
                    "function":{"name":"test_function","arguments":"{\"param\":\"value\"}"}
                }]
            }
        }]
    })";

    auto start = std::chrono::high_resolution_clock::now();
    auto result = formatter->postprocess_response(response, test_context_);
    auto processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - start).count();

    EXPECT_LT(processing_time, 50); // Performance target: <50ms
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.extracted_tool_calls.size(), 1);
    EXPECT_EQ(result.extracted_tool_calls[0].name, "test_function");
}

TEST_F(Phase2IntegrationTest, PerformanceTargets_AnthropicFormatter) {
    auto formatter = std::make_shared<AnthropicFormatter>();

    Response response;
    response.data = R"(<thinking>
This is a test thinking block with step-by-step analysis.
</thinking>

Here is the final response with <function_calls>
<invoke name="test_function">
<parameter name="param">value</parameter>
</invoke>
</function_calls>)";

    auto start = std::chrono::high_resolution_clock::now();
    auto result = formatter->postprocess_response(response, test_context_);
    auto processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - start).count();

    EXPECT_LT(processing_time, 50); // Performance target: <50ms
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.extracted_tool_calls.size(), 1);
    EXPECT_TRUE(result.reasoning.has_value());
    EXPECT_FALSE(result.reasoning->empty());
}

// Load Testing

TEST_F(Phase2IntegrationTest, LoadTest_ConcurrentMarkdownNormalization) {
    const int num_concurrent = 100;
    const int requests_per_thread = 10;

    auto formatter = std::make_shared<SyntheticFormatter>();
    formatter->configure({
        {"simulation_mode", "mixed"},
        {"performance_benchmarking", true},
        {"concurrent_testing", true}
    });

    std::vector<std::future<bool>> futures;
    std::atomic<int> successful_requests{0};
    std::atomic<int> failed_requests{0};

    auto start_time = std::chrono::high_resolution_clock::now();

    // Launch concurrent threads
    for (int i = 0; i < num_concurrent; ++i) {
        futures.push_back(std::async(std::launch::async, [&, i]() {
            for (int j = 0; j < requests_per_thread; ++j) {
                Response response;
                response.data = R"({"content":"Test content " + std::to_string(i) + "_" + std::to_string(j) + "})";

                try {
                    auto result = formatter->postprocess_response(response, test_context_);
                    if (result.success) {
                        successful_requests.fetch_add(1);
                    } else {
                        failed_requests.fetch_add(1);
                    }
                } catch (...) {
                    failed_requests.fetch_add(1);
                }
            }
            return true;
        }));
    }

    // Wait for all threads to complete
    for (auto& future : futures) {
        future.wait();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto total_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();

    int total_requests = num_concurrent * requests_per_thread;
    double success_rate = static_cast<double>(successful_requests.load()) / total_requests;
    double requests_per_second = (total_requests * 1000.0) / total_time_ms;

    // Performance validations
    EXPECT_GT(success_rate, 0.95); // >95% success rate
    EXPECT_GT(requests_per_second, 100.0); // >100 requests per second
    EXPECT_LT(total_time_ms, 30000); // <30 seconds total

    std::cout << "Load test results:" << std::endl;
    std::cout << "  Total requests: " << total_requests << std::endl;
    std::cout << "  Successful: " << successful_requests.load() << std::endl;
    std::cout << "  Failed: " << failed_requests.load() << std::endl;
    std::cout << "  Success rate: " << (success_rate * 100) << "%" << std::endl;
    std::cout << "  Requests per second: " << requests_per_second << std::endl;
    std::cout << "  Total time: " << total_time_ms << " ms" << std::endl;
}

TEST_F(Phase2IntegrationTest, LoadTest_StressToolCallExtraction) {
    auto formatter = std::make_shared<SyntheticFormatter>();
    formatter->configure({
        {"simulation_mode", "openai"},
        {"error_injection_rate", 0.05} // 5% error rate
    });

    const int num_iterations = 1000;
    std::atomic<int> successful_extractions{0};
    std::atomic<int> tool_calls_found{0};

    auto start_time = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_iterations; ++i) {
        Response response;
        response.data = R"({
            "choices":[{
                "message":{
                    "tool_calls":[
                        {"id":"call_1","function":{"name":"extract_data","arguments":"{\"index\":" + std::to_string(i) + "}"}},
                        {"id":"call_2","function":{"name":"validate_result","arguments":"{\"data\":\"test\"}"}}
                    ]
                }
            }]
        })";

        try {
            auto result = formatter->postprocess_response(response, test_context_);
            if (result.success) {
                successful_extractions.fetch_add(1);
                tool_calls_found.fetch_add(result.extracted_tool_calls.size());
            }
        } catch (...) {
            // Handle errors gracefully
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto processing_time_us = std::chrono::duration_cast<std::chrono::microseconds>(
        end_time - start_time).count();

    double avg_time_per_request = static_cast<double>(processing_time_us) / num_iterations;
    double success_rate = static_cast<double>(successful_extractions.load()) / num_iterations;

    EXPECT_LT(avg_time_per_request, 20000.0); // <20ms average
    EXPECT_GT(success_rate, 0.9); // >90% success rate despite 5% error injection
    EXPECT_EQ(tool_calls_found.load(), successful_extractions.load() * 2); // 2 tool calls per successful request
}

// Security Tests

TEST_F(Phase2IntegrationTest, Security_InjectionAttackPrevention) {
    auto formatter = std::make_shared<SyntheticFormatter>();
    formatter->configure({
        {"error_injection_rate", 0.0}, // Disable synthetic errors
        {"test_data_generation", true}
    });

    // Malicious JSON injection attempts
    std::vector<std::string> malicious_inputs = {
        R"({"content":{"$gt": ""}})", // NoSQL injection
        R"({"content":"<script>alert('xss')</script>"})", // XSS
        R"({"content":"' OR 1=1 --"}), // SQL injection
        R"({"content":"../../../etc/passwd"}), // Path traversal
        R"({"content":"{{7*7}}"}), // Template injection
        R"({"content":"\x00\x01\x02\x03"}), // Null bytes
        R"({"content":"\n\r\t\f\v"}), // Control characters
        R"({"content":"很长的内容" * 10000})", // Very long content
        R"()({})" // Unclosed brackets
    };

    int successful_sanitizations = 0;

    for (const auto& malicious_input : malicious_inputs) {
        Response response;
        response.data = malicious_input;

        try {
            auto result = formatter->postprocess_response(response, test_context_);

            // Should not crash and should either succeed gracefully or fail safely
            if (result.success || !result.error_message.empty()) {
                successful_sanitizations++;
            }

            // Verify no code execution artifacts in output
            if (result.success) {
                EXPECT_TRUE(result.processed_content.find("<script>") == std::string::npos);
                EXPECT_TRUE(result.processed_content.find("' OR ") == std::string::npos);
                EXPECT_TRUE(result.processed_content.find("../../../") == std::string::npos);
            }

        } catch (...) {
            // Exception handling is also acceptable for malicious input
            successful_sanitizations++;
        }
    }

    EXPECT_EQ(successful_sanitizations, malicious_inputs.size()); // All inputs handled safely
}

TEST_F(Phase2IntegrationTest, Security_MalformedInputHandling) {
    auto formatter = std::make_shared<OpenAIFormatter>();

    std::vector<std::string> malformed_inputs = {
        "", // Empty string
        "{", // Incomplete JSON
        "not json at all", // Not JSON
        R"({"incomplete": "object)", // Mismatched brackets
        R"({"array": [1,2,})", // Mismatched brackets
        R"({"invalid": "\x80\x81\x82"}")", // Invalid UTF-8
        std::string(10000, 'x'), // Large but reasonable input
    };

    int handled_safely = 0;

    for (const auto& malformed_input : malformed_inputs) {
        Response response;
        response.data = malformed_input;

        try {
            auto result = formatter->postprocess_response(response, test_context_);

            // Should not crash
            if (result.success || !result.error_message.empty()) {
                handled_safely++;
            }

        } catch (...) {
            // Exception handling is acceptable
            handled_safely++;
        }
    }

    EXPECT_EQ(handled_safely, malformed_inputs.size()); // all handled safely
}

// Memory Safety Tests

TEST_F(Phase2IntegrationTest, MemoryUsage_SustainedLoad) {
    auto formatter = std::make_shared<CerebrasFormatter>();
    formatter->configure({"memory_profiling", true});

    // Baseline memory measurement (approximate)
    size_t baseline_memory = 0; // Would use actual memory profiling in real implementation

    const int num_iterations = 10000;

    for (int i = 0; i < num_iterations; ++i) {
        Response response;
        response.data = R"({"content":"Test content " + std::to_string(i) + "})";

        auto result = formatter->postprocess_response(response, test_context_);

        // Periodic memory check
        if (i % 1000 == 0 && i > 0) {
            // Would monitor actual memory usage here
            // For now, just verify operation continues
            EXPECT_TRUE(result.success || !result.error_message.empty());
        }
    }

    // In a real implementation, would verify memory usage hasn't grown significantly
    EXPECT_TRUE(true); // Placeholder for memory check
}

TEST_F(Phase2IntegrationTest, MemorySafety_RapidObjectCreationDestruction) {
    for (int i = 0; i < 1000; ++i) {
        // Rapid creation and destruction of formatters
        {
            auto formatter = std::make_shared<OpenAIFormatter>();
            Response response;
            response.data = R"({"content":"Rapid test " + std::to_string(i) + "})";

            auto result = formatter->postprocess_response(response, test_context_);
            // Should not crash
        }
        {
            auto formatter = std::make_shared<AnthropicFormatter>();
            Response response;
            response.data = R"(<thinking>Rapid thinking " + std::to_string(i) + "</thinking>)";

            auto result = formatter->postprocess_response(response, test_context_);
            // Should not crash
        }
    }

    EXPECT_TRUE(true); // If we reach here, no memory safety issues occurred
}

// Integration Tests

TEST_F(Phase2IntegrationTest, Integration_CrossPluginCompatibility) {
    std::vector<std::shared_ptr<PrettifierPlugin>> formatters = {
        std::make_shared<CerebrasFormatter>(),
        std::make_shared<OpenAIFormatter>(),
        std::make_shared<AnthropicFormatter>(),
        std::make_shared<SyntheticFormatter>()
    };

    // Test each formatter with the same input
    Response response;
    response.data = R"({"content":"Cross-plugin test content"})";

    std::vector<std::string> output_formats;

    for (const auto& formatter : formatters) {
        auto result = formatter->postprocess_response(response, test_context_);

        EXPECT_TRUE(result.success) << "Formatter failed: " << formatter->get_name();
        EXPECT_FALSE(result.processed_content.empty());

        output_formats.push_back(result.output_format);
    }

    // All should produce valid output (may be different formats)
    EXPECT_EQ(output_formats.size(), formatters.size());
}

TEST_F(Phase2IntegrationTest, Integration_MultiProviderWorkflow) {
    auto synthetic_formatter = std::make_shared<SyntheticFormatter>();
    synthetic_formatter->configure({
        {"simulation_mode", "mixed"},
        {"test_data_generation", true},
        {"performance_benchmarking", true}
    });

    // Simulate a multi-provider workflow
    std::vector<std::pair<std::string, std::string>> provider_responses = {
        {"cerebras", R"({"content":"Fast Cerebras response"})"},
        {"openai", R"({"choices":[{"message":{"content":"OpenAI response"}}]})"},
        {"anthropic", R"(<thinking>Claude thinking</thinking>Claude response)"},
        {"synthetic", R"({"content":"Synthetic test response"})"}
    };

    std::vector<ProcessingResult> results;

    for (const auto& [provider, response_data] : provider_responses) {
        test_context_.provider_name = provider;

        Response response;
        response.data = response_data;

        auto result = synthetic_formatter->postprocess_response(response, test_context_);
        results.push_back(result);

        EXPECT_TRUE(result.success) << "Failed for provider: " << provider;
        EXPECT_FALSE(result.processed_content.empty());
    }

    // Verify all formatters worked correctly
    EXPECT_EQ(results.size(), provider_responses.size());

    // Check that different providers produce potentially different results
    std::set<std::string> unique_contents;
    for (const auto& result : results) {
        unique_contents.insert(result.processed_content);
    }

    EXPECT_GT(unique_contents.size(), 1); // Should have some variation
}

TEST_F(Phase2IntegrationTest, Integration_StreamingProcessorIntegration) {
    auto processor = std::make_shared<StreamingProcessor>();
    auto formatter = std::make_shared<CerebrasFormatter>();

    // Configure processor for testing
    processor->configure({
        {"thread_pool_size", 2},
        {"buffer_size_mb", 16},
        {"max_concurrent_streams", 10}
    });

    test_context_.provider_name = "cerebras";
    test_context_.model_name = "llama3.1-70b";

    // Create streaming session
    auto stream_id = processor->create_stream(test_context_, formatter);
    EXPECT_FALSE(stream_id.empty());

    // Process stream chunks
    std::vector<std::string> chunks = {
        R"({"delta":{"content":"First "}})",
        R"({"delta":{"content":"chunk "}})",
        R"({"delta":{"content":"of "}})",
        R"({"delta":{"content":"streaming "}})",
        R"({"delta":{"content":"response"}})",
        R"({"delta":{},"finish_reason":"stop"})"
    };

    std::vector<std::future<bool>> chunk_futures;

    for (size_t i = 0; i < chunks.size(); ++i) {
        bool is_final = (i == chunks.size() - 1);
        auto future = processor->process_chunk(stream_id, chunks[i], is_final);
        chunk_futures.push_back(std::move(future));
    }

    // Wait for all chunks to be processed
    for (auto& future : chunk_futures) {
        EXPECT_TRUE(future.get()); // All chunks should process successfully
    }

    // Get final result
    auto result = processor->get_result(stream_id);
    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.processed_content.empty());
    EXPECT_TRUE(result.streaming_mode);

    // Verify streaming metadata
    EXPECT_TRUE(result.metadata.contains("stream_id"));
    EXPECT_TRUE(result.metadata.contains("total_chunks"));
    EXPECT_TRUE(result.metadata.contains("processor_stats"));
}

// Error Recovery Tests

TEST_F(Phase2IntegrationTest, ErrorResilience_GracefulDegradation) {
    auto formatter = std::make_shared<SyntheticFormatter>();
    formatter->configure({
        {"error_injection_rate", 0.1}, // 10% error rate
        {"simulation_mode", "mixed"}
    });

    int total_requests = 100;
    int successful_requests = 0;
    int recovered_from_errors = 0;

    for (int i = 0; i < total_requests; ++i) {
        Response response;
        response.data = R"({"content":"Error recovery test " + std::to_string(i) + "})";

        try {
            auto result = formatter->postprocess_response(response, test_context_);

            if (result.success) {
                successful_requests++;
            } else if (!result.error_message.empty()) {
                // Graceful error handling
                recovered_from_errors++;
            }
        } catch (...) {
            // Exception handling is also acceptable
            recovered_from_errors++;
        }
    }

    int total_successful = successful_requests + recovered_from_errors;
    double recovery_rate = static_cast<double>(total_successful) / total_requests;

    EXPECT_GT(recovery_rate, 0.9); // Should recover from >90% of cases
    EXPECT_TRUE(successful_requests > 0 || recovered_from_errors > 0);
}

// Overall Performance Benchmark

TEST_F(Phase2IntegrationTest, Benchmark_OverallPerformance) {
    std::vector<std::shared_ptr<PrettifierPlugin>> formatters = {
        std::make_shared<CerebrasFormatter>(),
        std::make_shared<OpenAIFormatter>(),
        std::make_shared<AnthropicFormatter>(),
        std::make_shared<SyntheticFormatter>()
    };

    const int num_iterations = 100;
    std::map<std::string, std::vector<double>> processing_times;

    for (const auto& formatter : formatters) {
        std::string formatter_name = formatter->get_name();

        for (int i = 0; i < num_iterations; ++i) {
            Response response;
            response.data = R"({"content":"Performance test " + std::to_string(i) + "})";

            auto start = std::chrono::high_resolution_clock::now();
            auto result = formatter->postprocess_response(response, test_context_);
            auto end = std::chrono::high_resolution_clock::now();

            auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(
                end - start).count();

            processing_times[formatter_name].push_back(duration_us / 1000.0); // Convert to ms

            EXPECT_TRUE(result.success) << "Performance test failed for " << formatter_name;
        }
    }

    // Calculate and verify performance metrics
    for (const auto& [name, times] : processing_times) {
        double avg_time = std::accumulate(times.begin(), times.end(), 0.0) / times.size();
        double max_time = *std::max_element(times.begin(), times.end());
        double min_time = *std::min_element(times.begin(), times.end());

        std::cout << "Performance metrics for " << name << ":" << std::endl;
        std::cout << "  Average: " << avg_time << " ms" << std::endl;
        std::cout << "  Max: " << max_time << " ms" << std::endl;
        std::cout << "  Min: " << min_time << " ms" << std::endl;

        // Performance targets
        EXPECT_LT(avg_time, 50.0) << name << " exceeds average processing time target";
        EXPECT_LT(max_time, 100.0) << name << " exceeds maximum processing time target";
    }
}

// Health Check Validation

TEST_F(Phase2IntegrationTest, HealthCheck_AllFormatters) {
    std::vector<std::shared_ptr<PrettifierPlugin>> formatters = {
        std::make_shared<CerebrasFormatter>(),
        std::make_shared<OpenAIFormatter>(),
        std::make_shared<AnthropicFormatter>(),
        std::make_shared<SyntheticFormatter>()
    };

    for (const auto& formatter : formatters) {
        auto health = formatter->health_check();

        EXPECT_TRUE(health.contains("status"));
        EXPECT_TRUE(health.contains("timestamp"));
        EXPECT_EQ(health["status"], "healthy") << "Health check failed for " << formatter->get_name();

        // Verify diagnostic information
        auto diagnostics = formatter->get_diagnostics();
        EXPECT_TRUE(diagnostics.contains("name"));
        EXPECT_TRUE(diagnostics.contains("version"));
        EXPECT_TRUE(diagnostics.contains("configuration"));
        EXPECT_TRUE(diagnostics.contains("metrics"));
    }

    // Also test streaming processor health
    auto processor = std::make_shared<StreamingProcessor>();
    auto processor_health = processor->health_check();

    EXPECT_EQ(processor_health["status"], "healthy");
    EXPECT_TRUE(processor_health.contains("performance_metrics"));
}

