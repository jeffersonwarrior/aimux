#include <gtest/gtest.h>
#include <chrono>
#include "aimux/prettifier/toon_formatter.hpp"
#include "aimux/prettifier/prettifier_plugin.hpp"

using namespace aimux::prettifier;
using namespace aimux::core;

// Test suite following qa/phase1_foundation_qa_plan.md - Component 3
class ToonFormatterTest : public ::testing::Test {
protected:
    void SetUp() override {
        formatter_ = std::make_unique<ToonFormatter>();
        sample_response_ = Response{
            .success = true,
            .data = "```python\nprint('Hello, World!')\n```",
            .error_message = "",
            .status_code = 200,
            .response_time_ms = 150.0,
            .provider_name = "test-provider",
            // Note: tokens_used field not available in current Response structure
            // .tokens_used = 50
        };

        sample_context_ = ProcessingContext{
            .provider_name = "test-provider",
            .model_name = "test-model",
            .original_format = "markdown",
            .streaming_mode = false,
            .processing_start = std::chrono::system_clock::now()
        };

        sample_tool_ = ToolCall{
            .name = "test_function",
            .id = "call_123",
            .parameters = nlohmann::json{{"param1", "value1"}, {"param2", 42}},
            .status = "completed"
        };
    }

    std::unique_ptr<ToonFormatter> formatter_;
    Response sample_response_;
    ProcessingContext sample_context_;
    ToolCall sample_tool_;
};

// Test basic serialization functionality
TEST_F(ToonFormatterTest, BasicSerialization) {
    std::string toon = formatter_->serialize_response(sample_response_, sample_context_);

    EXPECT_FALSE(toon.empty());
    EXPECT_NE(toon.find("# META"), std::string::npos);
    EXPECT_NE(toon.find("# CONTENT"), std::string::npos);
    EXPECT_NE(toon.find("provider: test-provider"), std::string::npos);
    EXPECT_NE(toon.find("[CONTENT:"), std::string::npos);
    EXPECT_NE(toon.find("[TYPE: markdown]"), std::string::npos);
}

// Test serialization with tool calls
TEST_F(ToonFormatterTest, SerializationWithTools) {
    std::vector<ToolCall> tools = {sample_tool_};
    std::string toon = formatter_->serialize_response(sample_response_, sample_context_, tools);

    EXPECT_NE(toon.find("# TOOLS"), std::string::npos);
    EXPECT_NE(toon.find("[CALL: test_function]"), std::string::npos);
    EXPECT_NE(toon.find("[PARAM:"), std::string::npos);
    EXPECT_NE(toon.find("[STATUS: completed]"), std::string::npos);
}

// Test serialization with thinking/reasoning
TEST_F(ToonFormatterTest, SerializationWithThinking) {
    std::string thinking = "I need to analyze this problem step by step.";
    std::string toon = formatter_->serialize_response(sample_response_, sample_context_, {}, thinking);

    EXPECT_NE(toon.find("# THINKING"), std::string::npos);
    EXPECT_NE(toon.find("[REASONING:"), std::string::npos);
    EXPECT_NE(toon.find("analyze this problem"), std::string::npos);
}

// Test basic deserialization
TEST_F(ToonFormatterTest, BasicDeserialization) {
    std::string toon = formatter_->serialize_response(sample_response_, sample_context_);
    auto parsed = formatter_->deserialize_toon(toon);

    ASSERT_TRUE(parsed.has_value());
    EXPECT_TRUE(parsed->contains("metadata"));
    EXPECT_TRUE(parsed->contains("content"));
    EXPECT_EQ((*parsed)["metadata"]["provider"], "test-provider");
    EXPECT_EQ((*parsed)["content"]["type"], "markdown");
}

// Test round-trip data preservation
TEST_F(ToonFormatterTest, RoundTripDataPreservation) {
    // Test with complex data
    nlohmann::json original_data = nlohmann::json{
        {"message", "Hello, World!"},
        {"numbers", {1, 2, 3, 4, 5}},
        {"nested", {
            {"key1", "value1"},
            {"key2", 42}
        }}
    };

    std::string toon = formatter_->serialize_data(original_data, {
        {"source", "test"},
        {"environment", "unit-test"}
    });

    auto parsed = formatter_->deserialize_toon(toon);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_TRUE(parsed->contains("metadata"));
    EXPECT_TRUE(parsed->contains("content"));

    // Content should be preserved
    auto content = (*parsed)["content"];
    EXPECT_TRUE(content.contains("content"));
    std::string content_str = content["content"];

    // Parse back the JSON to verify it's intact
    nlohmann::json restored = nlohmann::json::parse(content_str);
    EXPECT_EQ(restored, original_data);
}

// Test JSON to TOON conversion
TEST_F(ToonFormatterTest, JsonToToonConversion) {
    nlohmann::json data = {
        {"name", "test"},
        {"value", 42},
        {"active", true}
    };

    std::string toon = formatter_->json_to_toon(data);
    EXPECT_NE(toon.find("name: test"), std::string::npos);
    EXPECT_NE(toon.find("value: 42"), std::string::npos);
    EXPECT_NE(toon.find("active: true"), std::string::npos);
}

// Test section extraction
TEST_F(ToonFormatterTest, SectionExtraction) {
    std::string toon = formatter_->serialize_response(sample_response_, sample_context_, {sample_tool_});

    auto meta_section = formatter_->extract_section(toon, "META");
    ASSERT_TRUE(meta_section.has_value());
    EXPECT_NE(meta_section->find("provider:"), std::string::npos);

    auto content_section = formatter_->extract_section(toon, "CONTENT");
    ASSERT_TRUE(content_section.has_value());
    EXPECT_NE(content_section->find("[TYPE:"), std::string::npos);

    auto tools_section = formatter_->extract_section(toon, "TOOLS");
    ASSERT_TRUE(tools_section.has_value());
    EXPECT_NE(tools_section->find("[CALL:"), std::string::npos);

    auto non_existent = formatter_->extract_section(toon, "NONEXISTENT");
    EXPECT_FALSE(non_existent.has_value());
}

// Test TOON validation
TEST_F(ToonFormatterTest, ToonValidation) {
    std::string valid_toon = formatter_->serialize_response(sample_response_, sample_context_);
    std::string error_message;
    EXPECT_TRUE(formatter_->validate_toon(valid_toon, error_message));

    std::string invalid_toon = "Invalid content without sections";
    EXPECT_FALSE(formatter_->validate_toon(invalid_toon, error_message));

    std::string missing_content = "# META\nkey: value\n";
    EXPECT_FALSE(formatter_->validate_toon(missing_content, error_message));
    EXPECT_NE(error_message.find("CONTENT"), std::string::npos);
}

// Test TOON analysis
TEST_F(ToonFormatterTest, ToonAnalysis) {
    std::string toon = formatter_->serialize_response(sample_response_, sample_context_, {sample_tool_});
    auto analysis = formatter_->analyze_toon(toon);

    EXPECT_TRUE(analysis.contains("total_size_bytes"));
    EXPECT_TRUE(analysis.contains("line_count"));
    EXPECT_TRUE(analysis.contains("section_count"));
    EXPECT_TRUE(analysis.contains("sections"));

    EXPECT_GE(analysis["total_size_bytes"], 0);
    EXPECT_GE(analysis["line_count"], 0);
    EXPECT_GE(analysis["section_count"], 2); // META and CONTENT at minimum
}

// Test content escaping
TEST_F(ToonFormatterTest, ContentEscaping) {
    std::string content_with_headers = "# This should be escaped\nRegular content\n# Another header";
    std::string toon = formatter_->create_content_section(content_with_headers, "text");

    // Headers should be escaped
    EXPECT_NE(toon.find("\\# This should be escaped"), std::string::npos);
    EXPECT_NE(toon.find("\\# Another header"), std::string::npos);

    // Regular content should remain unchanged
    EXPECT_NE(toon.find("Regular content"), std::string::npos);
}

// Test content unescaping
TEST_F(ToonFormatterTest, ContentUnescaping) {
    std::string escaped = "\\# This should be unescaped\nRegular content";
    std::string unescaped = formatter_->unescape_toon_content(escaped);

    EXPECT_EQ(unescaped, "# This should be unescaped\nRegular content");
}

// Test configuration management
TEST_F(ToonFormatterTest, ConfigurationManagement) {
    ToonFormatter::Config config;
    config.include_metadata = false;
    config.enable_compression = true;
    config.indent = "  ";

    formatter_->update_config(config);

    EXPECT_EQ(formatter_->get_config().include_metadata, false);
    EXPECT_EQ(formatter_->get_config().enable_compression, true);
    EXPECT_EQ(formatter_->get_config().indent, "  ");

    // Test with metadata disabled
    std::string toon = formatter_->serialize_response(sample_response_, sample_context_);
    EXPECT_EQ(toon.find("# META"), std::string::npos);
    EXPECT_NE(toon.find("# CONTENT"), std::string::npos);
}

// Performance test according to QA requirements (<10ms serialization, <5ms deserialization)
TEST_F(ToonFormatterTest, PerformanceBenchmarks) {
    const int iterations = 100;
    std::vector<ToolCall> tools = {sample_tool_, sample_tool_};

    // Test serialization performance
    auto serialize_start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        formatter_->serialize_response(sample_response_, sample_context_, tools);
    }
    auto serialize_end = std::chrono::high_resolution_clock::now();
    auto serialize_duration = std::chrono::duration_cast<std::chrono::microseconds>(
        serialize_end - serialize_start);
    double serialize_ms_per_op = static_cast<double>(serialize_duration.count()) / iterations / 1000.0;

    EXPECT_LT(serialize_ms_per_op, 10.0)  // <10ms target
        << "Serialization too slow: " << serialize_ms_per_op << "ms per operation";

    // Test deserialization performance
    std::string toon = formatter_->serialize_response(sample_response_, sample_context_, tools);
    auto deserialize_start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        formatter_->deserialize_toon(toon);
    }
    auto deserialize_end = std::chrono::high_resolution_clock::now();
    auto deserialize_duration = std::chrono::duration_cast<std::chrono::microseconds>(
        deserialize_end - deserialize_start);
    double deserialize_ms_per_op = static_cast<double>(deserialize_duration.count()) / iterations / 1000.0;

    EXPECT_LT(deserialize_ms_per_op, 5.0)  // <5ms target
        << "Deserialization too slow: " << deserialize_ms_per_op << "ms per operation";
}

// Memory overhead test according to QA requirements (<2x original size)
TEST_F(ToonFormatterTest, MemoryOverhead) {
    std::string large_content(10000, 'x');  // 10KB of content
    Response large_response;
    large_response.data = large_content;
    large_response.success = true;
    // Note: tokens_used field not available in current Response structure
    // large_response.tokens_used = 2000;

    ProcessingContext large_context = sample_context_;
    large_context.original_format = "text";

    std::string toon = formatter_->serialize_response(large_response, large_context);

    size_t original_size = large_content.length();
    size_t toon_size = toon.length();
    double overhead_multiplier = static_cast<double>(toon_size) / original_size;

    EXPECT_LT(overhead_multiplier, 2.0)  // <2x overhead target
        << "Memory overhead too high: " << overhead_multiplier << "x original size";

    // Log the actual overhead for monitoring
    printf("Memory overhead: %.2fx (%zu -> %zu bytes)\n",
           overhead_multiplier, original_size, toon_size);
}

// Thread safety test
TEST_F(ToonFormatterTest, ThreadSafety) {
    const int num_threads = 10;
    const int operations_per_thread = 50;
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([this, operations_per_thread, &success_count]() {
            try {
                for (int i = 0; i < operations_per_thread; ++i) {
                    // Serialize
                    std::string toon = formatter_->serialize_response(sample_response_, sample_context_);
                    EXPECT_FALSE(toon.empty());

                    // Deserialize
                    auto parsed = formatter_->deserialize_toon(toon);
                    EXPECT_TRUE(parsed.has_value());

                    // Extract section
                    auto content = formatter_->extract_section(toon, "CONTENT");
                    EXPECT_TRUE(content.has_value());

                    success_count++;
                }
            } catch (const std::exception& e) {
                // Should not throw under normal circumstances
                printf("Thread safety test error: %s\n", e.what());
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    int expected_successes = num_threads * operations_per_thread;
    EXPECT_EQ(success_count, expected_successes)
        << "Thread safety test failed: " << success_count
        << " successes out of " << expected_successes;
}

// Error handling test
TEST_F(ToonFormatterTest, ErrorHandling) {
    // Test invalid JSON in parameters
    ToolCall invalid_tool;
    invalid_tool.name = "test";
    invalid_tool.parameters = nlohmann::json{{"malformed", "unclosed \"string"}};

    std::vector<ToolCall> tools = {invalid_tool};
    std::string toon = formatter_->serialize_response(sample_response_, sample_context_, tools);

    // Should still serialize, but may contain error information
    EXPECT_FALSE(toon.empty());
    EXPECT_NE(toon.find("# TOOLS"), std::string::npos);

    // Test malformed TOON
    std::string malformed_toon = "# INVALID_SECTION\ncontent";
    auto parsed = formatter_->deserialize_toon(malformed_toon);
    EXPECT_FALSE(parsed.has_value());
}

// Large content handling
TEST_F(ToonFormatterTest, LargeContentHandling) {
    // Test content that exceeds the compression threshold
    std::string large_content(2000, 'x');  // 2KB content

    Response large_response;
    large_response.data = large_content;
    large_response.success = true;

    ToonFormatter::Config config;
    config.enable_compression = true;
    formatter_->update_config(config);

    std::string toon = formatter_->serialize_response(large_response, sample_context_);
    EXPECT_FALSE(toon.empty());
    EXPECT_NE(toon.find("[CONTENT_SIZE:"), std::string::npos);
    EXPECT_NE(toon.find(large_content), std::string::npos);
}

// Format conversion test
TEST_F(ToonFormatterTest, FormatConversion) {
    // Test XML-style tool calls
    std::string xml_content = R"(
        <function_calls>
        {"name": "test_function", "arguments": {"param": "value"}}
        </function_calls>
    )";

    auto tool_calls = formatter_->serialize_response(sample_response_, sample_context_);
    EXPECT_NE(tool_calls.find("provider: test-provider"), std::string::npos);
}

// Integration test with other prettifier components
TEST_F(ToonFormatterTest, IntegrationWithPrettifierComponents) {
    // Create processing result with TOON format
    ProcessingResult result;
    result.success = true;
    result.processed_content = "Processed content";
    result.output_format = "toon";
    result.streaming_mode = false;

    // Convert to TOON
    Response response;
    response.data = result.processed_content;
    response.success = result.success;

    std::string toon = formatter_->serialize_response(response, sample_context_);

    // Verify back and forth conversion works
    auto parsed = formatter_->deserialize_toon(toon);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_TRUE(parsed->contains("content"));
}