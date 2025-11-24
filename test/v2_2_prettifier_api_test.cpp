/**
 * Test suite for v2.2 PrettifierAPI Class
 * Tests the REST API endpoints for prettifier status and configuration
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include "aimux/webui/prettifier_api.hpp"
#include "aimux/webui/config_validator.hpp"
#include "aimux/prettifier/prettifier_plugin.hpp"

using namespace aimux::webui;
using namespace aimux::prettifier;
using namespace aimux::core;
using namespace ::testing;

// Mock PrettifierPlugin for testing
class MockPrettifierPlugin : public PrettifierPlugin {
public:
    MOCK_METHOD(ProcessingResult, preprocess_request, (const Request& request), (override));
    MOCK_METHOD(ProcessingResult, postprocess_response, (const Response& response, const ProcessingContext& context), (override));
    MOCK_METHOD(std::string, get_name, (), (const, override));
    MOCK_METHOD(std::string, version, (), (const, override));
    MOCK_METHOD(std::string, description, (), (const, override));
    MOCK_METHOD(std::vector<std::string>, supported_formats, (), (const, override));
    MOCK_METHOD(std::vector<std::string>, output_formats, (), (const, override));
    MOCK_METHOD(std::vector<std::string>, supported_providers, (), (const, override));
    MOCK_METHOD(std::vector<std::string>, capabilities, (), (const, override));
};

class PrettifierAPITest : public ::testing::Test {
protected:
    void SetUp() override {
        mock_plugin_ = std::make_shared<MockPrettifierPlugin>();

        // Set up default mock expectations
        ON_CALL(*mock_plugin_, get_name()).WillByDefault(Return("test-prettifier"));
        ON_CALL(*mock_plugin_, version()).WillByDefault(Return("2.1.1"));
        ON_CALL(*mock_plugin_, supported_providers()).WillByDefault(Return(
            std::vector<std::string>{"anthropic", "openai", "cerebras", "synthetic"}
        ));
        ON_CALL(*mock_plugin_, supported_formats()).WillByDefault(Return(
            std::vector<std::string>{"json", "xml", "markdown"}
        ));

        api_ = std::make_unique<PrettifierAPI>(mock_plugin_);
    }

    std::shared_ptr<MockPrettifierPlugin> mock_plugin_;
    std::unique_ptr<PrettifierAPI> api_;
};

// =============================================================================
// Status Endpoint Tests (GET /api/prettifier/status)
// =============================================================================

TEST_F(PrettifierAPITest, HandleStatusRequest_Success) {
    auto response = api_->handle_status_request();

    ASSERT_TRUE(response.contains("status"));
    ASSERT_TRUE(response.contains("version"));
    ASSERT_TRUE(response.contains("supported_providers"));
}

TEST_F(PrettifierAPITest, HandleStatusRequest_ReturnsValidJSON) {
    auto response = api_->handle_status_request();

    // Response should be valid JSON object
    EXPECT_TRUE(response.is_object());
    EXPECT_FALSE(response.empty());
}

TEST_F(PrettifierAPITest, HandleStatusRequest_ContainsAllRequiredFields) {
    auto response = api_->handle_status_request();

    // Check all required fields from API spec
    EXPECT_TRUE(response.contains("status"));
    EXPECT_TRUE(response.contains("version"));
    EXPECT_TRUE(response.contains("supported_providers"));
    EXPECT_TRUE(response.contains("format_preferences"));
    EXPECT_TRUE(response.contains("performance_metrics"));
    EXPECT_TRUE(response.contains("configuration"));
}

TEST_F(PrettifierAPITest, HandleStatusRequest_StatusEnabled) {
    auto response = api_->handle_status_request();

    ASSERT_TRUE(response.contains("status"));
    EXPECT_EQ(response["status"], "enabled");
}

TEST_F(PrettifierAPITest, HandleStatusRequest_VersionFormat) {
    auto response = api_->handle_status_request();

    ASSERT_TRUE(response.contains("version"));
    std::string version = response["version"];

    // Should contain at least one dot (semantic versioning)
    EXPECT_NE(version.find('.'), std::string::npos);
}

TEST_F(PrettifierAPITest, HandleStatusRequest_SupportedProvidersArray) {
    auto response = api_->handle_status_request();

    ASSERT_TRUE(response.contains("supported_providers"));
    EXPECT_TRUE(response["supported_providers"].is_array());
    EXPECT_GT(response["supported_providers"].size(), 0);
}

TEST_F(PrettifierAPITest, HandleStatusRequest_PerformanceMetricsStructure) {
    auto response = api_->handle_status_request();

    ASSERT_TRUE(response.contains("performance_metrics"));
    auto metrics = response["performance_metrics"];

    EXPECT_TRUE(metrics.contains("avg_formatting_time_ms"));
    EXPECT_TRUE(metrics.contains("throughput_requests_per_second"));
    EXPECT_TRUE(metrics.contains("success_rate_percent"));
    EXPECT_TRUE(metrics.contains("uptime_seconds"));
}

TEST_F(PrettifierAPITest, HandleStatusRequest_ConfigurationStructure) {
    auto response = api_->handle_status_request();

    ASSERT_TRUE(response.contains("configuration"));
    auto config = response["configuration"];

    EXPECT_TRUE(config.contains("prettifier_enabled"));
    EXPECT_TRUE(config.contains("streaming_enabled"));
    EXPECT_TRUE(config.contains("security_hardening"));
    EXPECT_TRUE(config.contains("max_buffer_size_kb"));
    EXPECT_TRUE(config.contains("timeout_ms"));
}

TEST_F(PrettifierAPITest, HandleStatusRequest_FormatPreferencesStructure) {
    auto response = api_->handle_status_request();

    ASSERT_TRUE(response.contains("format_preferences"));
    auto prefs = response["format_preferences"];

    // Should contain provider-specific preferences
    EXPECT_TRUE(prefs.is_object());
    EXPECT_GT(prefs.size(), 0);
}

// =============================================================================
// Config Endpoint Tests (POST /api/prettifier/config)
// =============================================================================

TEST_F(PrettifierAPITest, HandleConfigRequest_ValidConfig_Success) {
    nlohmann::json config = {
        {"enabled", true},
        {"format_preferences", {
            {"anthropic", "json-tool-use"},
            {"openai", "function-calling"}
        }},
        {"streaming_enabled", true},
        {"max_buffer_size_kb", 2048},
        {"timeout_ms", 10000}
    };

    auto response = api_->handle_config_request(config);

    ASSERT_TRUE(response.contains("success"));
    EXPECT_TRUE(response["success"].get<bool>());
    EXPECT_TRUE(response.contains("message"));
    EXPECT_TRUE(response.contains("applied_config"));
}

TEST_F(PrettifierAPITest, HandleConfigRequest_InvalidBufferSize_Failure) {
    nlohmann::json config = {
        {"max_buffer_size_kb", 100}  // Below minimum (256)
    };

    auto response = api_->handle_config_request(config);

    ASSERT_TRUE(response.contains("success"));
    EXPECT_FALSE(response["success"].get<bool>());
    EXPECT_TRUE(response.contains("error"));
    EXPECT_TRUE(response.contains("details"));
}

TEST_F(PrettifierAPITest, HandleConfigRequest_InvalidTimeout_Failure) {
    nlohmann::json config = {
        {"timeout_ms", 500}  // Below minimum (1000)
    };

    auto response = api_->handle_config_request(config);

    ASSERT_TRUE(response.contains("success"));
    EXPECT_FALSE(response["success"].get<bool>());
}

TEST_F(PrettifierAPITest, HandleConfigRequest_InvalidFormatPreference_Failure) {
    nlohmann::json config = {
        {"format_preferences", {
            {"anthropic", "invalid-format"}
        }}
    };

    auto response = api_->handle_config_request(config);

    ASSERT_TRUE(response.contains("success"));
    EXPECT_FALSE(response["success"].get<bool>());
}

TEST_F(PrettifierAPITest, HandleConfigRequest_PartialConfig_Success) {
    nlohmann::json config = {
        {"streaming_enabled", false}
    };

    auto response = api_->handle_config_request(config);

    // Partial config should be valid
    ASSERT_TRUE(response.contains("success"));
    EXPECT_TRUE(response["success"].get<bool>());
}

TEST_F(PrettifierAPITest, HandleConfigRequest_EmptyConfig_Success) {
    nlohmann::json config = nlohmann::json::object();

    auto response = api_->handle_config_request(config);

    // Empty config should be valid (no changes)
    ASSERT_TRUE(response.contains("success"));
    EXPECT_TRUE(response["success"].get<bool>());
}

TEST_F(PrettifierAPITest, HandleConfigRequest_MultipleValidations_Failure) {
    nlohmann::json config = {
        {"max_buffer_size_kb", 50},     // Invalid
        {"timeout_ms", 100},            // Invalid
        {"streaming_enabled", "yes"}    // Wrong type
    };

    auto response = api_->handle_config_request(config);

    EXPECT_FALSE(response["success"].get<bool>());
    EXPECT_TRUE(response.contains("error"));
}

// =============================================================================
// Helper Method Tests
// =============================================================================

TEST_F(PrettifierAPITest, GetStatusJSON_ReturnsValidJSON) {
    auto status = api_->get_status_json();

    EXPECT_TRUE(status.is_object());
    EXPECT_FALSE(status.empty());
}

TEST_F(PrettifierAPITest, GetProviderFormats_Anthropic) {
    auto formats = api_->get_provider_formats("anthropic");

    EXPECT_TRUE(formats.is_object());
    EXPECT_TRUE(formats.contains("default_format"));
    EXPECT_TRUE(formats.contains("available_formats"));
}

TEST_F(PrettifierAPITest, GetProviderFormats_OpenAI) {
    auto formats = api_->get_provider_formats("openai");

    EXPECT_TRUE(formats.is_object());
    EXPECT_TRUE(formats.contains("default_format"));
    EXPECT_TRUE(formats.contains("available_formats"));
}

TEST_F(PrettifierAPITest, GetProviderFormats_Cerebras) {
    auto formats = api_->get_provider_formats("cerebras");

    EXPECT_TRUE(formats.is_object());
    EXPECT_TRUE(formats.contains("default_format"));
    EXPECT_TRUE(formats.contains("available_formats"));
}

TEST_F(PrettifierAPITest, GetProviderFormats_UnknownProvider) {
    auto formats = api_->get_provider_formats("unknown");

    // Should return empty or error structure
    EXPECT_TRUE(formats.is_object());
}

TEST_F(PrettifierAPITest, GetPerformanceMetrics_ValidStructure) {
    auto metrics = api_->get_performance_metrics();

    EXPECT_TRUE(metrics.is_object());
    EXPECT_TRUE(metrics.contains("avg_formatting_time_ms"));
    EXPECT_TRUE(metrics.contains("throughput_requests_per_second"));
    EXPECT_TRUE(metrics.contains("success_rate_percent"));
    EXPECT_TRUE(metrics.contains("uptime_seconds"));
}

TEST_F(PrettifierAPITest, GetPerformanceMetrics_ReasonableValues) {
    auto metrics = api_->get_performance_metrics();

    // Check that values are within reasonable ranges
    double avg_time = metrics["avg_formatting_time_ms"];
    EXPECT_GT(avg_time, 0.0);
    EXPECT_LT(avg_time, 1000.0);  // Should be under 1 second

    double success_rate = metrics["success_rate_percent"];
    EXPECT_GE(success_rate, 0.0);
    EXPECT_LE(success_rate, 100.0);
}

TEST_F(PrettifierAPITest, GetConfiguration_ValidStructure) {
    auto config = api_->get_configuration();

    EXPECT_TRUE(config.is_object());
    EXPECT_TRUE(config.contains("prettifier_enabled"));
    EXPECT_TRUE(config.contains("streaming_enabled"));
    EXPECT_TRUE(config.contains("security_hardening"));
}

// =============================================================================
// Thread Safety Tests
// =============================================================================

TEST_F(PrettifierAPITest, ConcurrentStatusRequests) {
    const int num_threads = 10;
    const int requests_per_thread = 100;

    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([this, &success_count, requests_per_thread]() {
            for (int j = 0; j < requests_per_thread; ++j) {
                try {
                    auto response = api_->handle_status_request();
                    if (response.contains("status")) {
                        success_count++;
                    }
                } catch (...) {
                    // Catch any exceptions
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // All requests should succeed
    EXPECT_EQ(success_count, num_threads * requests_per_thread);
}

TEST_F(PrettifierAPITest, ConcurrentConfigRequests) {
    const int num_threads = 5;

    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([this, &success_count]() {
            nlohmann::json config = {
                {"timeout_ms", 5000}
            };

            try {
                auto response = api_->handle_config_request(config);
                if (response.contains("success")) {
                    success_count++;
                }
            } catch (...) {
                // Catch any exceptions
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(success_count, num_threads);
}

// =============================================================================
// Performance Tests
// =============================================================================

TEST_F(PrettifierAPITest, PerformanceTest_StatusRequest) {
    auto start = std::chrono::high_resolution_clock::now();

    const int iterations = 1000;
    for (int i = 0; i < iterations; ++i) {
        auto response = api_->handle_status_request();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // 1000 status requests should complete in < 1 second
    EXPECT_LT(duration.count(), 1000);

    // Average time per request should be < 1ms
    double avg_time = duration.count() / static_cast<double>(iterations);
    EXPECT_LT(avg_time, 1.0);
}

TEST_F(PrettifierAPITest, PerformanceTest_ConfigRequest) {
    nlohmann::json config = {
        {"streaming_enabled", true},
        {"max_buffer_size_kb", 1024}
    };

    auto start = std::chrono::high_resolution_clock::now();

    const int iterations = 1000;
    for (int i = 0; i < iterations; ++i) {
        auto response = api_->handle_config_request(config);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // 1000 config requests should complete in < 2 seconds (includes validation)
    EXPECT_LT(duration.count(), 2000);
}

// =============================================================================
// Error Handling Tests
// =============================================================================

TEST_F(PrettifierAPITest, HandleConfigRequest_NullConfig) {
    nlohmann::json config = nullptr;

    auto response = api_->handle_config_request(config);

    EXPECT_FALSE(response["success"].get<bool>());
    EXPECT_TRUE(response.contains("error"));
}

TEST_F(PrettifierAPITest, HandleConfigRequest_ArrayInsteadOfObject) {
    nlohmann::json config = nlohmann::json::array({1, 2, 3});

    auto response = api_->handle_config_request(config);

    EXPECT_FALSE(response["success"].get<bool>());
}

TEST_F(PrettifierAPITest, HandleConfigRequest_LargeConfig) {
    nlohmann::json config = {
        {"enabled", true},
        {"format_preferences", {
            {"anthropic", "json-tool-use"},
            {"openai", "function-calling"},
            {"cerebras", "speed-optimized"}
        }},
        {"streaming_enabled", true},
        {"max_buffer_size_kb", 4096},
        {"timeout_ms", 30000},
        {"security_hardening", true}
    };

    auto response = api_->handle_config_request(config);

    EXPECT_TRUE(response["success"].get<bool>());
}

// Main function for running tests
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
