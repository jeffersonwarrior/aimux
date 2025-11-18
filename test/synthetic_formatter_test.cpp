#include <gtest/gtest.h>
#include <memory>

#include "aimux/prettifier/synthetic_formatter.hpp"

using namespace aimux::prettifier;
using namespace aimux::core;

class SyntheticFormatterTest : public ::testing::Test {
protected:
    void SetUp() override {
        formatter_ = std::make_shared<SyntheticFormatter>();
        test_context_.provider_name = "synthetic";
        test_context_.model_name = "test-model";
        test_context_.processing_start = std::chrono::system_clock::now();
    }

    std::shared_ptr<SyntheticFormatter> formatter_;
    ProcessingContext test_context_;
};

TEST_F(SyntheticFormatterTest, BasicFunctionality_TestDataGeneration) {
    auto config = nlohmann::json{
        {"test_data_generation", true},
        {"simulation_mode", "synthetic"}
    };
    formatter_->configure(config);

    Response response;
    response.data = R"({"content":"Test response"})";

    auto result = formatter_->postprocess_response(response, test_context_);
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.processed_content.find("Synthetic response:") != std::string::npos);
}

TEST_F(SyntheticFormatterTest, ErrorInjection_RobustnessTesting) {
    auto config = nlohmann::json{
        {"error_injection_rate", 1.0}, // 100% error rate for testing
        {"simulation_mode", "synthetic"}
    };
    formatter_->configure(config);

    Response response;
    response.data = R"({"content":"Error test"})";

    auto result = formatter_->postprocess_response(response, test_context_);
    // Should handle gracefully with error recovery
    EXPECT_TRUE(result.processed_content.find("Synthetic response:") != std::string::npos);
}

