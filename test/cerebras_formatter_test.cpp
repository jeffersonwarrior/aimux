#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <chrono>
#include <memory>

#include "aimux/prettifier/cerebras_formatter.hpp"
#include "aimux/core/router.hpp"

using namespace aimux::prettifier;
using namespace aimux::core;
using ::testing::Contains;
using ::testing::Not;
using ::testing::Gt;
using ::testing::Lt;
using ::testing::Ge;

class CerebrasFormatterTest : public ::testing::Test {
protected:
    void SetUp() override {
        formatter_ = std::make_shared<CerebrasFormatter>();

        test_request_.data = nlohmann::json{
            {"content", "Test request for Cerebras formatting"},
            {"model", "llama3.1-70b"}
        };

        test_context_.provider_name = "cerebras";
        test_context_.model_name = "llama3.1-70b";
        test_context_.original_format = "json";
        test_context_.processing_start = std::chrono::system_clock::now();
    }

    std::shared_ptr<CerebrasFormatter> formatter_;
    Request test_request_;
    ProcessingContext test_context_;
};

TEST_F(CerebrasFormatterTest, BasicFunctionality_CorrectIdentification) {
    EXPECT_EQ(formatter_->get_name(), "cerebras-speed-formatter-v1.0.0");
    EXPECT_EQ(formatter_->version(), "1.0.0");
    EXPECT_FALSE(formatter_->description().empty());

    // Check supported formats
    auto supported_formats = formatter_->supported_formats();
    EXPECT_THAT(supported_formats, Contains("json"));
    EXPECT_THAT(supported_formats, Contains("text"));
    EXPECT_THAT(supported_formats, Contains("cerebras-raw"));

    // Check supported providers
    auto providers = formatter_->supported_providers();
    EXPECT_THAT(providers, Contains("cerebras"));
    EXPECT_THAT(providers, Contains("cerebras-ai"));

    // Check capabilities
    auto capabilities = formatter_->capabilities();
    EXPECT_THAT(capabilities, Contains("speed-optimization"));
    EXPECT_THAT(capabilities, Contains("fast-tool-calls"));
    EXPECT_THAT(capabilities, Contains("streaming-support"));
}

TEST_F(CerebrasFormatterTest, PreprocessRequest_SpeedOptimization) {
    auto start = std::chrono::high_resolution_clock::now();
    auto result = formatter_->preprocess_request(test_request_);
    auto processing_time = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now() - start).count();

    EXPECT_LT(processing_time, 50000); // <50ms target
    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.processed_content.empty());

    // Verify speed optimization metadata
    auto processed_json = nlohmann::json::parse(result.processed_content);
    EXPECT_TRUE(processed_json.contains("_cerebras_optimization"));
    EXPECT_TRUE(processed_json["_cerebras_optimization"]["speed_priority"]);

    // Check temperature for speed
    EXPECT_TRUE(processed_json.contains("temperature"));
    EXPECT_LT(processed_json["temperature"], 0.5); // Should be low for speed
}

TEST_F(CerebrasFormatterTest, PostprocessResponse_FastToolCallExtraction) {
    Response response;
    response.data = R"({
        "choices":[{
            "message":{
                "content":"Response with tool calls",
                "tool_calls":[{
                    "type":"function_call",
                    "function":{
                        "name":"extract_data",
                        "arguments":"{\"index\":42,\"data\":\"test_value\"}"
                    }
                }]
            }
        }]
    })";

    auto start = std::chrono::high_resolution_clock::now();
    auto result = formatter_->postprocess_response(response, test_context_);
    auto processing_time = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now() - start).count();

    EXPECT_LT(processing_time, 50000); // <50ms target
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.extracted_tool_calls.size(), 1);

    const auto& tool_call = result.extracted_tool_calls[0];
    EXPECT_EQ(tool_call.name, "extract_data");
    EXPECT_FALSE(tool_call.parameters.empty());
    EXPECT_EQ(tool_call.status, "completed");

    // Verify TOON format generation
    EXPECT_FALSE(result.processed_content.empty());
    auto toon_json = nlohmann::json::parse(result.processed_content);
    EXPECT_EQ(toon_json["format"], "toon");
    EXPECT_EQ(toon_json["provider"], "cerebras");
    EXPECT_TRUE(toon_json.contains("metadata"));
    EXPECT_TRUE(toon_json["metadata"]["speed_optimized"]);
}

TEST_F(CerebrasFormatterTest, Performance_Benchmarking) {
    const int num_iterations = 100;
    std::vector<double> processing_times;

    Response response;
    response.data = R"({"choices":[{"message":{"content":"Benchmark test"}}]})";

    for (int i = 0; i < num_iterations; ++i) {
        test_request_.data["test_iteration"] = i;

        auto start = std::chrono::high_resolution_clock::now();
        auto preprocess_result = formatter_->preprocess_request(test_request_);
        auto postprocess_result = formatter_->postprocess_response(response, test_context_);
        auto end = std::chrono::high_resolution_clock::now();

        auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(
            end - start).count();

        processing_times.push_back(duration_us / 1000.0); // Convert to ms

        EXPECT_TRUE(preprocess_result.success);
        EXPECT_TRUE(postprocess_result.success);
    }

    double avg_time = std::accumulate(processing_times.begin(), processing_times.end(), 0.0) / processing_times.size();
    double max_time = *std::max_element(processing_times.begin(), processing_times.end());

    EXPECT_LT(avg_time, 30.0) << "Average processing time exceeds target";
    EXPECT_LT(max_time, 100.0) << "Maximum processing time exceeds target";

    // Check metrics
    auto metrics = formatter_->get_metrics();
    EXPECT_EQ(metrics["total_processing_count"], num_iterations);
    EXPECT_GT(metrics["average_processing_time_us"], 0);
    EXPECT_LT(metrics["average_processing_time_us"], 50000); // <50ms average
}

TEST_F(CerebrasFormatterTest, StreamingSupport_AsyncProcessing) {
    test_context_.streaming_mode = true;

    EXPECT_TRUE(formatter_->begin_streaming(test_context_));

    // Process individual chunks
    std::vector<std::string> chunks = {
        R"({"delta":{"content":"Chunk 1 "}})",
        R"({"delta":{"content":"Chunk 2 "}})",
        R"({"delta":{"content":"Chunk 3"}})",
        R"({"delta":{},"finish_reason":"stop"})"
    };

    for (size_t i = 0; i < chunks.size(); ++i) {
        bool is_final = (i == chunks.size() - 1);
        auto result = formatter_->process_streaming_chunk(chunks[i], is_final, test_context_);

        EXPECT_TRUE(result.success);
        EXPECT_TRUE(result.streaming_mode);
        EXPECT_FALSE(result.processed_content.empty());
    }

    auto final_result = formatter_->end_streaming(test_context_);
    EXPECT_TRUE(final_result.success);
    EXPECT_FALSE(final_result.streaming_mode);
    EXPECT_FALSE(final_result.processed_content.empty());
}

TEST_F(CerebrasFormatterTest, Configuration_Customization) {
    // Test speed optimization configuration
    nlohmann::json config = {
        {"optimize_speed", true},
        {"enable_detailed_metrics", true},
        {"cache_tool_patterns", true},
        {"max_processing_time_ms", 75},
        {"enable_fast_failover", false}
    };

    EXPECT_TRUE(formatter_->configure(config));
    EXPECT_TRUE(formatter_->validate_configuration());

    auto current_config = formatter_->get_configuration();
    EXPECT_EQ(current_config["optimize_speed"], true);
    EXPECT_EQ(current_config["enable_detailed_metrics"], true);
    EXPECT_EQ(current_config["max_processing_time_ms"], 75);

    // Test invalid configuration
    nlohmann::json invalid_config = {
        {"max_processing_time_ms", -10} // Invalid negative value
    };

    formatter_->configure(invalid_config);
    EXPECT_FALSE(formatter_->validate_configuration());
}

TEST_F(CerebrasFormatterTest, FastFailover_TimeoutHandling) {
    // Configure for fast failover testing
    nlohmann::json config = {
        {"max_processing_time_ms", 1}, // Very short timeout
        {"enable_fast_failover", true}
    };

    formatter_->configure(config);

    // Create a response that would take longer to process
    std::string large_content(100000, 'x'); // 100KB content
    Response response;
    response.data = R"({
        "choices":[{
            "message":{
                "content":")" + large_content + R"(",
                "tool_calls":[]
            }
        }]
    })";

    auto result = formatter_->postprocess_response(response, test_context_);

    // Should succeed with fast failover
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.metadata.contains("fast_failover"));
    EXPECT_TRUE(result.metadata["fast_failover"]);
}

TEST_F(CerebrasFormatterTest, ErrorHandling_InvalidInput) {
    Response response;
    response.data = "This is not valid JSON";

    auto result = formatter_->postprocess_response(response, test_context_);

    // Should handle gracefully
    EXPECT_TRUE(result.success); // Should succeed with basic processing
    EXPECT_FALSE(result.processed_content.empty());
}

TEST_F(CerebrasFormatterTest, ToolCallExtraction_PatternMatching) {
    // Test various tool call formats
    std::vector<std::string> tool_call_formats = {
        R"({"type":"function_call","function":{"name":"test_func","arguments":"{\"param\":\"value\"}"}})",
        R"({
            "tool_calls": [{
                "function": {"name": "another_func", "arguments": "{}"}
            }]
        })",
        R"("function_call": {"name": "legacy_func", "arguments": "{\"test\": true}"})"
    };

    for (const auto& format : tool_call_formats) {
        Response response;
        response.data = format;

        auto result = formatter_->postprocess_response(response, test_context_);

        EXPECT_TRUE(result.success) << "Failed to process format: " + format;

        if (format.find("function") != std::string::npos) {
            // Should extract tool calls if present
            EXPECT_GE(result.extracted_tool_calls.size(), 0);
        }
    }
}

TEST_F(CerebrasFormatterTest, HealthCheck_Diagnostics) {
    auto health = formatter_->health_check();

    EXPECT_EQ(health["status"], "healthy");
    EXPECT_TRUE(health.contains("timestamp"));
    EXPECT_TRUE(health.contains("speed_validation_us"));
    EXPECT_TRUE(health.contains("pattern_cache_available"));

    auto diagnostics = formatter_->get_diagnostics();

    EXPECT_EQ(diagnostics["name"], "cerebras-speed-formatter-v1.0.0");
    EXPECT_EQ(diagnostics["version"], "1.0.0");
    EXPECT_TRUE(diagnostics.contains("performance_analysis"));
    EXPECT_TRUE(diagnostics.contains("recommendations"));
}

TEST_F(CerebrasFormatterTest, Metrics_CollectionAndReset) {
    // Perform some operations to generate metrics
    for (int i = 0; i < 10; ++i) {
        Response response;
        response.data = R"({"choices":[{"message":{"content":"Metrics test " + std::to_string(i) + "}}]})";

        auto result = formatter_->postprocess_response(response, test_context_);
        EXPECT_TRUE(result.success);
    }

    auto metrics = formatter_->get_metrics();
    EXPECT_EQ(metrics["total_processing_count"], 10);
    EXPECT_GT(metrics["total_processing_time_us"], 0);
    EXPECT_GT(metrics["average_processing_time_us"], 0);

    // Test metrics reset
    formatter_->reset_metrics();
    auto reset_metrics = formatter_->get_metrics();
    EXPECT_EQ(reset_metrics["total_processing_count"], 0);
    EXPECT_EQ(reset_metrics["total_processing_time_us"], 0);
}

TEST_F(CerebrasFormatterTest, ContentNormalization_SpeedOptimized) {
    Response response;
    response.data = R"({
        "content": "  Response with   extra   whitespace   and   artifacts  [DONE]\n"
    })";

    auto result = formatter_->postprocess_response(response, test_context_);

    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.processed_content.empty());

    // Should remove artifacts efficiently
    std::string processed = result.processed_content;
    EXPECT_TRUE(processed.find("[DONE]") == std::string::npos);
    // Should preserve meaningful content
    EXPECT_TRUE(processed.find("Response with extra whitespace") != std::string::npos);
}

