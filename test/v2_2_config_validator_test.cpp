/**
 * Test suite for v2.2 ConfigValidator Class
 * Tests the configuration validation logic for prettifier settings
 */

#include <gtest/gtest.h>
#include <memory>
#include "aimux/webui/config_validator.hpp"

using namespace aimux::webui;

class ConfigValidatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        validator_ = std::make_unique<ConfigValidator>();
    }

    std::unique_ptr<ConfigValidator> validator_;
};

// =============================================================================
// Buffer Size Validation Tests
// =============================================================================

TEST_F(ConfigValidatorTest, ValidBufferSize_MinimumBoundary) {
    auto result = validator_->validate_buffer_size(256);
    EXPECT_TRUE(result.valid);
    EXPECT_TRUE(result.error_message.empty());
}

TEST_F(ConfigValidatorTest, ValidBufferSize_MaximumBoundary) {
    auto result = validator_->validate_buffer_size(8192);
    EXPECT_TRUE(result.valid);
    EXPECT_TRUE(result.error_message.empty());
}

TEST_F(ConfigValidatorTest, ValidBufferSize_MiddleRange) {
    auto result = validator_->validate_buffer_size(1024);
    EXPECT_TRUE(result.valid);
    EXPECT_TRUE(result.error_message.empty());
}

TEST_F(ConfigValidatorTest, InvalidBufferSize_BelowMinimum) {
    auto result = validator_->validate_buffer_size(255);
    EXPECT_FALSE(result.valid);
    EXPECT_FALSE(result.error_message.empty());
    EXPECT_EQ(result.invalid_field, "max_buffer_size_kb");
}

TEST_F(ConfigValidatorTest, InvalidBufferSize_AboveMaximum) {
    auto result = validator_->validate_buffer_size(8193);
    EXPECT_FALSE(result.valid);
    EXPECT_FALSE(result.error_message.empty());
    EXPECT_EQ(result.invalid_field, "max_buffer_size_kb");
}

TEST_F(ConfigValidatorTest, InvalidBufferSize_Zero) {
    auto result = validator_->validate_buffer_size(0);
    EXPECT_FALSE(result.valid);
}

TEST_F(ConfigValidatorTest, InvalidBufferSize_Negative) {
    auto result = validator_->validate_buffer_size(-100);
    EXPECT_FALSE(result.valid);
}

// =============================================================================
// Timeout Validation Tests
// =============================================================================

TEST_F(ConfigValidatorTest, ValidTimeout_MinimumBoundary) {
    auto result = validator_->validate_timeout(1000);
    EXPECT_TRUE(result.valid);
    EXPECT_TRUE(result.error_message.empty());
}

TEST_F(ConfigValidatorTest, ValidTimeout_MaximumBoundary) {
    auto result = validator_->validate_timeout(60000);
    EXPECT_TRUE(result.valid);
    EXPECT_TRUE(result.error_message.empty());
}

TEST_F(ConfigValidatorTest, ValidTimeout_MiddleRange) {
    auto result = validator_->validate_timeout(5000);
    EXPECT_TRUE(result.valid);
    EXPECT_TRUE(result.error_message.empty());
}

TEST_F(ConfigValidatorTest, InvalidTimeout_BelowMinimum) {
    auto result = validator_->validate_timeout(999);
    EXPECT_FALSE(result.valid);
    EXPECT_FALSE(result.error_message.empty());
    EXPECT_EQ(result.invalid_field, "timeout_ms");
}

TEST_F(ConfigValidatorTest, InvalidTimeout_AboveMaximum) {
    auto result = validator_->validate_timeout(60001);
    EXPECT_FALSE(result.valid);
    EXPECT_FALSE(result.error_message.empty());
    EXPECT_EQ(result.invalid_field, "timeout_ms");
}

TEST_F(ConfigValidatorTest, InvalidTimeout_Zero) {
    auto result = validator_->validate_timeout(0);
    EXPECT_FALSE(result.valid);
}

TEST_F(ConfigValidatorTest, InvalidTimeout_Negative) {
    auto result = validator_->validate_timeout(-1000);
    EXPECT_FALSE(result.valid);
}

// =============================================================================
// Format Preference Validation Tests
// =============================================================================

TEST_F(ConfigValidatorTest, ValidFormatPreference_Anthropic_JSON) {
    auto result = validator_->validate_format_preference("anthropic", "json-tool-use");
    EXPECT_TRUE(result.valid);
}

TEST_F(ConfigValidatorTest, ValidFormatPreference_Anthropic_XML) {
    auto result = validator_->validate_format_preference("anthropic", "xml-tool-calls");
    EXPECT_TRUE(result.valid);
}

TEST_F(ConfigValidatorTest, ValidFormatPreference_Anthropic_ThinkingBlocks) {
    auto result = validator_->validate_format_preference("anthropic", "thinking-blocks");
    EXPECT_TRUE(result.valid);
}

TEST_F(ConfigValidatorTest, ValidFormatPreference_Anthropic_ReasoningTraces) {
    auto result = validator_->validate_format_preference("anthropic", "reasoning-traces");
    EXPECT_TRUE(result.valid);
}

TEST_F(ConfigValidatorTest, ValidFormatPreference_OpenAI_ChatCompletion) {
    auto result = validator_->validate_format_preference("openai", "chat-completion");
    EXPECT_TRUE(result.valid);
}

TEST_F(ConfigValidatorTest, ValidFormatPreference_OpenAI_FunctionCalling) {
    auto result = validator_->validate_format_preference("openai", "function-calling");
    EXPECT_TRUE(result.valid);
}

TEST_F(ConfigValidatorTest, ValidFormatPreference_OpenAI_StructuredOutput) {
    auto result = validator_->validate_format_preference("openai", "structured-output");
    EXPECT_TRUE(result.valid);
}

TEST_F(ConfigValidatorTest, ValidFormatPreference_Cerebras_SpeedOptimized) {
    auto result = validator_->validate_format_preference("cerebras", "speed-optimized");
    EXPECT_TRUE(result.valid);
}

TEST_F(ConfigValidatorTest, ValidFormatPreference_Cerebras_Standard) {
    auto result = validator_->validate_format_preference("cerebras", "standard");
    EXPECT_TRUE(result.valid);
}

TEST_F(ConfigValidatorTest, InvalidFormatPreference_UnknownProvider) {
    auto result = validator_->validate_format_preference("unknown_provider", "some-format");
    EXPECT_FALSE(result.valid);
    EXPECT_FALSE(result.error_message.empty());
}

TEST_F(ConfigValidatorTest, InvalidFormatPreference_UnknownFormat) {
    auto result = validator_->validate_format_preference("anthropic", "unknown-format");
    EXPECT_FALSE(result.valid);
    EXPECT_FALSE(result.error_message.empty());
}

// =============================================================================
// Full Config Validation Tests
// =============================================================================

TEST_F(ConfigValidatorTest, ValidConfig_Complete) {
    nlohmann::json config = {
        {"enabled", true},
        {"format_preferences", {
            {"anthropic", "json-tool-use"},
            {"openai", "function-calling"},
            {"cerebras", "speed-optimized"}
        }},
        {"streaming_enabled", true},
        {"max_buffer_size_kb", 2048},
        {"timeout_ms", 10000}
    };

    auto result = validator_->validate_config(config);
    EXPECT_TRUE(result.valid);
    EXPECT_TRUE(result.error_message.empty());
}

TEST_F(ConfigValidatorTest, ValidConfig_Minimal) {
    nlohmann::json config = {
        {"enabled", true}
    };

    auto result = validator_->validate_config(config);
    EXPECT_TRUE(result.valid);
}

TEST_F(ConfigValidatorTest, InvalidConfig_BufferSizeTooSmall) {
    nlohmann::json config = {
        {"enabled", true},
        {"max_buffer_size_kb", 100},
        {"timeout_ms", 5000}
    };

    auto result = validator_->validate_config(config);
    EXPECT_FALSE(result.valid);
    EXPECT_EQ(result.invalid_field, "max_buffer_size_kb");
}

TEST_F(ConfigValidatorTest, InvalidConfig_TimeoutTooLarge) {
    nlohmann::json config = {
        {"enabled", true},
        {"max_buffer_size_kb", 1024},
        {"timeout_ms", 70000}
    };

    auto result = validator_->validate_config(config);
    EXPECT_FALSE(result.valid);
    EXPECT_EQ(result.invalid_field, "timeout_ms");
}

TEST_F(ConfigValidatorTest, InvalidConfig_InvalidFormatPreference) {
    nlohmann::json config = {
        {"enabled", true},
        {"format_preferences", {
            {"anthropic", "invalid-format"}
        }}
    };

    auto result = validator_->validate_config(config);
    EXPECT_FALSE(result.valid);
}

TEST_F(ConfigValidatorTest, InvalidConfig_WrongTypeForEnabled) {
    nlohmann::json config = {
        {"enabled", "yes"}  // Should be boolean
    };

    auto result = validator_->validate_config(config);
    EXPECT_FALSE(result.valid);
}

TEST_F(ConfigValidatorTest, InvalidConfig_WrongTypeForBufferSize) {
    nlohmann::json config = {
        {"max_buffer_size_kb", "1024"}  // Should be number
    };

    auto result = validator_->validate_config(config);
    EXPECT_FALSE(result.valid);
}

// =============================================================================
// Cross-Field Compatibility Validation Tests
// =============================================================================

TEST_F(ConfigValidatorTest, ValidCompatibility_StreamingWithReasonableTimeout) {
    nlohmann::json config = {
        {"streaming_enabled", true},
        {"timeout_ms", 5000}
    };

    auto result = validator_->validate_compatibility(config);
    EXPECT_TRUE(result.valid);
}

TEST_F(ConfigValidatorTest, InvalidCompatibility_StreamingWithVeryLowTimeout) {
    nlohmann::json config = {
        {"streaming_enabled", true},
        {"timeout_ms", 500}
    };

    auto result = validator_->validate_compatibility(config);
    EXPECT_FALSE(result.valid);
    EXPECT_FALSE(result.error_message.empty());
}

TEST_F(ConfigValidatorTest, ValidCompatibility_NoStreamingWithLowTimeout) {
    nlohmann::json config = {
        {"streaming_enabled", false},
        {"timeout_ms", 500}
    };

    // This should be valid since streaming is disabled
    auto result = validator_->validate_compatibility(config);
    EXPECT_TRUE(result.valid);
}

TEST_F(ConfigValidatorTest, ValidCompatibility_LargeBufferWithLongTimeout) {
    nlohmann::json config = {
        {"max_buffer_size_kb", 8192},
        {"timeout_ms", 60000}
    };

    auto result = validator_->validate_compatibility(config);
    EXPECT_TRUE(result.valid);
}

// =============================================================================
// Edge Cases and Error Handling
// =============================================================================

TEST_F(ConfigValidatorTest, EmptyConfig) {
    nlohmann::json config = nlohmann::json::object();

    auto result = validator_->validate_config(config);
    // Empty config should be valid (will use defaults)
    EXPECT_TRUE(result.valid);
}

TEST_F(ConfigValidatorTest, NullConfig) {
    nlohmann::json config = nullptr;

    auto result = validator_->validate_config(config);
    EXPECT_FALSE(result.valid);
}

TEST_F(ConfigValidatorTest, ArrayInsteadOfObject) {
    nlohmann::json config = nlohmann::json::array({1, 2, 3});

    auto result = validator_->validate_config(config);
    EXPECT_FALSE(result.valid);
}

TEST_F(ConfigValidatorTest, ExtraUnknownFields) {
    nlohmann::json config = {
        {"enabled", true},
        {"unknown_field", "value"},
        {"another_unknown", 123}
    };

    // Should still validate the known fields successfully
    auto result = validator_->validate_config(config);
    EXPECT_TRUE(result.valid);
}

TEST_F(ConfigValidatorTest, MultipleProviderFormatPreferences) {
    nlohmann::json config = {
        {"format_preferences", {
            {"anthropic", "json-tool-use"},
            {"openai", "chat-completion"},
            {"cerebras", "speed-optimized"},
            {"synthetic", "diagnostic"}
        }}
    };

    auto result = validator_->validate_config(config);
    // Should accept synthetic provider if implementation supports it
    EXPECT_TRUE(result.valid);
}

// =============================================================================
// Performance Tests
// =============================================================================

TEST_F(ConfigValidatorTest, PerformanceTest_RapidValidation) {
    nlohmann::json config = {
        {"enabled", true},
        {"max_buffer_size_kb", 1024},
        {"timeout_ms", 5000}
    };

    auto start = std::chrono::high_resolution_clock::now();

    const int iterations = 10000;
    for (int i = 0; i < iterations; ++i) {
        auto result = validator_->validate_config(config);
        EXPECT_TRUE(result.valid);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Validation should be fast: 10000 validations in < 1 second
    EXPECT_LT(duration.count(), 1000);
}

// Main function for running tests
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
