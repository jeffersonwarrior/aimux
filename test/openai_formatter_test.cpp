#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <chrono>
#include <memory>

#include "aimux/prettifier/openai_formatter.hpp"

using namespace aimux::prettifier;
using namespace aimux::core;
using ::testing::Contains;
using ::testing::Lt;

class OpenAIFormatterTest : public ::testing::Test {
protected:
    void SetUp() override {
        formatter_ = std::make_shared<OpenAIFormatter>();

        test_context_.provider_name = "openai";
        test_context_.model_name = "gpt-4";
        test_context_.original_format = "json";
        test_context_.processing_start = std::chrono::system_clock::now();
    }

    std::shared_ptr<OpenAIFormatter> formatter_;
    ProcessingContext test_context_;
};

TEST_F(OpenAIFormatterTest, BasicFunctionality_FunctionCallingSupport) {
    Response response;
    response.data = R"({
        "choices":[{
            "message":{
                "content":"Response with function calls",
                "tool_calls":[{
                    "id":"call_123",
                    "type":"function",
                    "function":{
                        "name":"get_weather",
                        "arguments":"{\"location\":\"New York\",\"units\":\"metric\"}"
                    }
                }]
            },
            "finish_reason":"tool_calls"
        }]
    })";

    auto result = formatter_->postprocess_response(response, test_context_);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.extracted_tool_calls.size(), 1);

    const auto& tool_call = result.extracted_tool_calls[0];
    EXPECT_EQ(tool_call.name, "get_weather");
    EXPECT_EQ(tool_call.id, "call_123");
    EXPECT_EQ(tool_call.status, "completed");
    EXPECT_TRUE(tool_call.parameters.contains("location"));
}

TEST_F(OpenAIFormatterTest, StructuredOutput_Validation) {
    nlohmann::json config = {
        {"enable_structured_outputs", true},
        {"validate_tool_schemas", false} // Disable schema validation for this test
    };
    formatter_->configure(config);

    Response response;
    response.data = R"({
        "choices":[{
            "message":{
                "content": null,
                "refusal": null
            }
        }],
        "created": 1234567890,
        "model": "gpt-4-turbo",
        "object": "chat.completion"
    })";

    // Simulate structured output by setting as JSON
    auto test_json = R"({"name": "John", "age": 30, "city": "New York"})";
    response.data = test_json;

    auto result = formatter_->postprocess_response(response, test_context_);

    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.processed_content.empty());
}

TEST_F(OpenAIFormatterTest, LegacyFormat_Compatibility) {
    nlohmann::json config = {
        {"support_legacy_formats", true}
    };
    formatter_->configure(config);

    Response response;
    response.data = R"({
        "id": "legacy-response",
        "object": "text_completion",
        "created": 1234567890,
        "model": "text-davinci-003",
        "choices":[{
            "text":"Legacy completion response",
            "index":0,
            "logprobs":null,
            "finish_reason":"stop"
        }]
    })";

    auto result = formatter_->postprocess_response(response, test_context_);

    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.processed_content.empty());
}

