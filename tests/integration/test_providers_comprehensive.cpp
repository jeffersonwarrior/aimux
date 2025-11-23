/**
 * Comprehensive Provider Test Suite - Integration tests for Aimux v2.0.0 providers
 * Tests all provider implementations with real API calls and error handling
 */

#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <future>
#include <atomic>
#include <vector>
#include <string>

// Aimux providers
#include "aimux/providers/provider_impl.hpp"

using namespace aimux;
using namespace std::chrono_literals;

class ProviderIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup test configurations for each provider
        cerebras_config = {
            {"api_key", "test-cerebras-key"},
            {"endpoint", "https://api.cerebras.ai"},
            {"max_requests_per_minute", 60}
        };
        
        zai_config = {
            {"api_key", "test-zai-key"},
            {"endpoint", "https://api.z.ai"},
            {"max_requests_per_minute", 60}
        };
        
        minimax_config = {
            {"api_key", "test-minimax-key"},
            {"endpoint", "https://api.minimax.chat"},
            {"max_requests_per_minute", 60}
        };
        
        synthetic_config = {
            {"api_key", "mock-key"},
            {"endpoint", "https://synthetic.ai"},
            {"max_requests_per_minute", 1000}
        };
    }

    void TearDown() override {
        // Cleanup
    }

    nlohmann::json cerebras_config;
    nlohmann::json zai_config;
    nlohmann::json minimax_config;
    nlohmann::json synthetic_config;
};

// Test provider factory creation
TEST_F(ProviderIntegrationTest, ProviderFactoryCreation) {
    auto cerebras_provider = providers::ProviderFactory::create_provider("cerebras", cerebras_config);
    auto zai_provider = providers::ProviderFactory::create_provider("zai", zai_config);
    auto minimax_provider = providers::ProviderFactory::create_provider("minimax", minimax_config);
    auto synthetic_provider = providers::ProviderFactory::create_provider("synthetic", synthetic_config);
    
    ASSERT_NE(cerebras_provider, nullptr);
    ASSERT_NE(zai_provider, nullptr);
    ASSERT_NE(minimax_provider, nullptr);
    ASSERT_NE(synthetic_provider, nullptr);
    
    EXPECT_EQ(cerebras_provider->get_provider_name(), "cerebras");
    EXPECT_EQ(zai_provider->get_provider_name(), "zai");
    EXPECT_EQ(minimax_provider->get_provider_name(), "minimax");
    EXPECT_EQ(synthetic_provider->get_provider_name(), "synthetic");
}

// Test synthetic provider functionality
TEST_F(ProviderIntegrationTest, SyntheticProviderFunctionality) {
    auto provider = providers::ProviderFactory::create_provider("synthetic", synthetic_config);
    
    // Create test request
    core::Request request;
    request.method = "POST";
    request.data = {
        {"messages", nlohmann::json::array({
            {{"role", "user"}, {"content", "Hello synthetic provider!"}}
        })},
        {"max_tokens", 100},
        {"temperature", 0.7}
    };
    
    // Send request
    auto response = provider->send_request(request);
    
    ASSERT_TRUE(response.success) << "Synthetic provider should always succeed";
    EXPECT_FALSE(response.data.empty()) << "Response should contain data";
    EXPECT_EQ(response.status_code, 200) << "Status code should be 200";
    EXPECT_GT(response.response_time_ms, 0) << "Response time should be positive";
    EXPECT_EQ(response.provider_name, "synthetic") << "Provider name should match";
    
    // Test rate limiting
    auto rate_limit = provider->get_rate_limit_status();
    EXPECT_EQ(rate_limit["provider"], "synthetic");
    EXPECT_EQ(rate_limit["max_requests_per_minute"], 1000);
    
    std::cout << "Synthetic provider test passed\n";
    std::cout << "Response time: " << response.response_time_ms << "ms\n";
    std::cout << "Response data: " << response.data.substr(0, 50) << "...\n";
}

// Test provider health status
TEST_F(ProviderIntegrationTest, ProviderHealthStatus) {
    auto cerebras_provider = providers::ProviderFactory::create_provider("cerebras", cerebras_config);
    auto zai_provider = providers::ProviderFactory::create_provider("zai", zai_config);
    auto minimax_provider = providers::ProviderFactory::create_provider("minimax", minimax_config);
    auto synthetic_provider = providers::ProviderFactory::create_provider("synthetic", synthetic_config);
    
    // Test health status (should be true for all providers initially)
    EXPECT_TRUE(cerebras_provider->is_healthy()) << "Cerebras should be healthy";
    EXPECT_TRUE(zai_provider->is_healthy()) << "ZAI should be healthy";
    EXPECT_TRUE(minimax_provider->is_healthy()) << "MiniMax should be healthy";
    EXPECT_TRUE(synthetic_provider->is_healthy()) << "Synthetic should be healthy";
    
    std::cout << "All providers report healthy status\n";
}

// Test rate limiting functionality
TEST_F(ProviderIntegrationTest, RateLimitingFunctionality) {
    auto provider = providers::ProviderFactory::create_provider("synthetic", synthetic_config);
    
    // Create test request
    core::Request request;
    request.method = "POST";
    request.data = {
        {"messages", nlohmann::json::array({
            {{"role", "user"}, {"content", "Rate limit test"}}
        })}
    };
    
    // Send multiple requests to test rate limiting
    int num_requests = 10;
    int successful_requests = 0;
    
    for (int i = 0; i < num_requests; ++i) {
        auto response = provider->send_request(request);
        if (response.success) {
            successful_requests++;
        }
        
        // Add small delay to simulate real usage
        std::this_thread::sleep_for(10ms);
    }
    
    EXPECT_EQ(successful_requests, num_requests) << "All requests should succeed";
    
    // Check rate limit status
    auto rate_limit = provider->get_rate_limit_status();
    EXPECT_EQ(rate_limit["requests_made"], num_requests);
    EXPECT_EQ(rate_limit["requests_remaining"], 1000 - num_requests);
    
    std::cout << "Rate limiting test passed\n";
    std::cout << "Requests made: " << rate_limit["requests_made"] << "\n";
    std::cout << "Requests remaining: " << rate_limit["requests_remaining"] << "\n";
}

// Test concurrent requests
TEST_F(ProviderIntegrationTest, ConcurrentRequests) {
    auto provider = providers::ProviderFactory::create_provider("synthetic", synthetic_config);
    
    // Create test request
    core::Request request;
    request.method = "POST";
    request.data = {
        {"messages", nlohmann::json::array({
            {{"role", "user"}, {"content", "Concurrent test"}}
        })}
    };
    
    const int num_requests = 5;
    std::vector<std::future<core::Response>> futures;
    
    // Launch concurrent requests
    for (int i = 0; i < num_requests; ++i) {
        futures.push_back(std::async(std::launch::async, [&provider, &request]() {
            return provider->send_request(request);
        }));
    }
    
    // Wait for all requests to complete
    int successful_requests = 0;
    std::vector<int> response_times;
    
    for (auto& future : futures) {
        auto response = future.get();
        if (response.success) {
            successful_requests++;
            response_times.push_back(response.response_time_ms);
        }
    }
    
    EXPECT_EQ(successful_requests, num_requests) << "All concurrent requests should succeed";
    EXPECT_EQ(response_times.size(), num_requests) << "Should have response times for all requests";
    
    // Calculate statistics
    double avg_response_time = 0;
    for (int time : response_times) {
        avg_response_time += time;
    }
    avg_response_time /= response_times.size();
    
    std::cout << "Concurrent requests test passed\n";
    std::cout << "Successful requests: " << successful_requests << "/" << num_requests << "\n";
    std::cout << "Average response time: " << avg_response_time << "ms\n";
}

// Test provider error handling
TEST_F(ProviderIntegrationTest, ErrorHandling) {
    // Test invalid provider configuration
    nlohmann::json invalid_config = {
        {"api_key", ""}, // Empty API key
        {"endpoint", "https://api.cerebras.ai"},
        {"max_requests_per_minute", 60}
    };
    
    auto provider = providers::ProviderFactory::create_provider("cerebras", invalid_config);
    ASSERT_NE(provider, nullptr) << "Provider should be created even with invalid config";
    
    // Test request with invalid configuration
    core::Request request;
    request.method = "POST";
    request.data = {
        {"messages", nlohmann::json::array({
            {{"role", "user"}, {"content", "Error test"}}
        })}
    };
    
    auto response = provider->send_request(request);
    
    // Response should fail due to invalid configuration
    EXPECT_FALSE(response.success) << "Request should fail with invalid configuration";
    EXPECT_FALSE(response.error_message.empty()) << "Error message should be provided";
    EXPECT_NE(response.status_code, 200) << "Status code should indicate error";
    
    std::cout << "Error handling test passed\n";
    std::cout << "Error message: " << response.error_message << "\n";
    std::cout << "Status code: " << response.status_code << "\n";
}

// Test provider configuration validation
TEST_F(ProviderIntegrationTest, ConfigurationValidation) {
    // Test valid configurations
    EXPECT_TRUE(providers::ProviderFactory::validate_config("cerebras", cerebras_config));
    EXPECT_TRUE(providers::ProviderFactory::validate_config("zai", zai_config));
    EXPECT_TRUE(providers::ProviderFactory::validate_config("minimax", minimax_config));
    EXPECT_TRUE(providers::ProviderFactory::validate_config("synthetic", synthetic_config));
    
    // Test invalid configurations
    nlohmann::json invalid_config = {
        {"api_key", ""}, // Empty API key
        {"endpoint", ""}, // Empty endpoint
        {"max_requests_per_minute", 60}
    };
    
    EXPECT_FALSE(providers::ProviderFactory::validate_config("cerebras", invalid_config));
    EXPECT_FALSE(providers::ProviderFactory::validate_config("zai", invalid_config));
    EXPECT_FALSE(providers::ProviderFactory::validate_config("minimax", invalid_config));
    
    // Test configuration for unsupported provider
    nlohmann::json test_config = {
        {"api_key", "test-key"},
        {"endpoint", "https://test.api.com"},
        {"max_requests_per_minute", 60}
    };
    
    EXPECT_FALSE(providers::ProviderFactory::validate_config("unsupported", test_config));
    
    std::cout << "Configuration validation test passed\n";
}

// Test supported providers list
TEST_F(ProviderIntegrationTest, SupportedProviders) {
    auto supported = providers::ProviderFactory::get_supported_providers();
    
    EXPECT_GE(supported.size(), 4) << "Should support at least 4 providers";
    
    // Check for required providers
    bool has_cerebras = false;
    bool has_zai = false;
    bool has_minimax = false;
    bool has_synthetic = false;
    
    for (const auto& provider : supported) {
        if (provider == "cerebras") has_cerebras = true;
        if (provider == "zai") has_zai = true;
        if (provider == "minimax") has_minimax = true;
        if (provider == "synthetic") has_synthetic = true;
    }
    
    EXPECT_TRUE(has_cerebras) << "Should support cerebras provider";
    EXPECT_TRUE(has_zai) << "Should support zai provider";
    EXPECT_TRUE(has_minimax) << "Should support minimax provider";
    EXPECT_TRUE(has_synthetic) << "Should support synthetic provider";
    
    std::cout << "Supported providers: ";
    for (const auto& provider : supported) {
        std::cout << provider << " ";
    }
    std::cout << "\n";
}

// Performance test
TEST_F(ProviderIntegrationTest, PerformanceTest) {
    auto provider = providers::ProviderFactory::create_provider("synthetic", synthetic_config);
    
    // Create test request
    core::Request request;
    request.method = "POST";
    request.data = {
        {"messages", nlohmann::json::array({
            {{"role", "user"}, {"content", "Performance test"}}
        })}
    };
    
    const int num_requests = 50;
    const auto start_time = std::chrono::steady_clock::now();
    
    // Send multiple requests and measure performance
    std::vector<int> response_times;
    int successful_requests = 0;
    
    for (int i = 0; i < num_requests; ++i) {
        auto response = provider->send_request(request);
        if (response.success) {
            successful_requests++;
            response_times.push_back(response.response_time_ms);
        }
    }
    
    const auto end_time = std::chrono::steady_clock::now();
    const auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    // Calculate performance metrics
    double avg_response_time = 0;
    for (int time : response_times) {
        avg_response_time += time;
    }
    avg_response_time /= response_times.size();
    
    double requests_per_second = (num_requests * 1000.0) / total_time.count();
    
    // Performance assertions
    EXPECT_EQ(successful_requests, num_requests) << "All requests should succeed";
    EXPECT_LT(avg_response_time, 5000) << "Average response time should be under 5 seconds";
    EXPECT_GT(requests_per_second, 1.0) << "Should handle at least 1 request per second";
    
    std::cout << "Performance test results:\n";
    std::cout << "  Total requests: " << num_requests << "\n";
    std::cout << "  Successful requests: " << successful_requests << "\n";
    std::cout << "  Total time: " << total_time.count() << "ms\n";
    std::cout << "  Average response time: " << avg_response_time << "ms\n";
    std::cout << "  Requests per second: " << requests_per_second << "\n";
}

// Main function for running tests independently
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}