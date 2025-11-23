#include <gtest/gtest.h>
#include <memory>

#include "aimux/prettifier/anthropic_formatter.hpp"

using namespace aimux::prettifier;
using namespace aimux::core;

class AnthropicFormatterTest : public ::testing::Test {
protected:
    void SetUp() override {
        formatter_ = std::make_shared<AnthropicFormatter>();
        test_context_.provider_name = "anthropic";
        test_context_.model_name = "claude-3-sonnet";
        test_context_.processing_start = std::chrono::system_clock::now();
    }

    std::shared_ptr<AnthropicFormatter> formatter_;
    ProcessingContext test_context_;
};

TEST_F(AnthropicFormatterTest, BasicFunctionality_XmlToolUseSupport) {
    Response response;
    response.data = R"(<function_calls>
<invoke name="extract_data">
<parameter name="content">Test data</parameter>
</invoke>
</function_calls>)";

    auto result = formatter_->postprocess_response(response, test_context_);
    EXPECT_TRUE(result.success);
}

TEST_F(AnthropicFormatterTest, ThinkingBlocks_Extraction) {
    Response response;
    response.data = R"(<thinking>
This is step-by-step reasoning.
Step 1: Analyze the problem.
Step 2: Consider options.
Step 3: Choose solution.
</thinking>

Here is the final answer.)";

    auto result = formatter_->postprocess_response(response, test_context_);
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.reasoning.has_value());
}

