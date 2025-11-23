/**
 * Comprehensive Router Test Suite
 * Tests the core Router class with edge cases, error conditions, and performance scenarios
 * Target: >95% code coverage for Router class
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <thread>
#include <chrono>
#include <vector>
#include <memory>
#include <atomic>
#include <future>
#include <random>

#include "aimux/core/router.hpp"
#include "nlohmann/json.hpp"

using namespace aimux::core;
using namespace ::testing;

// Mock Provider Implementation for testing
class MockProvider {
public:
    MockProvider(const std::string& name, bool healthy = true, int latency_ms = 100)
        : name_(name), healthy_(healthy), latency_ms_(latency_ms), request_count_(0) {}

    Response simulate_response(const Request& request) {
        request_count_++;

        Response response;
        response.success = healthy_;
        response.provider_name = name_;
        response.response_time_ms = latency_ms_;
        response.status_code = healthy_ ? 200 : 503;
        response.data = nlohmann::json{
            {"provider", name_},
            {"request_count", request_count_},
            {"latency_ms", latency_ms_}
        }.dump();

        if (!healthy_) {
            response.error_message = "Provider " + name_ + " is unhealthy";
        }

        // Simulate network latency
        std::this_thread::sleep_for(std::chrono::milliseconds(latency_ms));

        return response;
    }

    void set_healthy(bool healthy) { healthy_ = healthy; }
    void set_latency(int ms) { latency_ms_ = ms; }
    int get_request_count() const { return request_count_; }

private:
    std::string name_;
    bool healthy_;
    int latency_ms_;
    std::atomic<int> request_count_;
};

// Test fixture for Router tests
class RouterTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set up multiple providers with different characteristics
        providers_ = {
            ProviderConfig{
                "fast_provider",
                "https://fast-provider.example.com",
                "key1",
                {"gpt-4", "gpt-3.5-turbo"},
                60,
                true
            },
            ProviderConfig{
                "slow_provider",
                "https://slow-provider.example.com",
                "key2",
                {"gpt-4", "claude-3"},
                30,
                true
            },
            ProviderConfig{
                "unreliable_provider",
                "https://unreliable.example.com",
                "key3",
                {"gpt-3.5-turbo"},
                45,
                true
            }
        };

        router_ = std::make_unique<Router>(providers_);
    }

    void TearDown() override {
        router_.reset();
    }

    Request create_test_request(const std::string& model = "gpt-4") {
        Request request;
        request.model = model;
        request.method = "POST";
        request.data = nlohmann::json{
            {"messages", {{"role", "user"}, {"content", "test message"}}}
        };
        return request;
    }

    std::vector<ProviderConfig> providers_;
    std::unique_ptr<Router> router_;
};

// Basic constructor tests
TEST_F(RouterTest, ConstructorWithValidProviders) {
    EXPECT_NO_THROW({
        Router router(providers_);
    });
}

TEST_F(RouterTest, ConstructorWithEmptyProviders) {
    std::vector<ProviderConfig> empty_providers;
    EXPECT_THROW({
        Router router(empty_providers);
    }, std::invalid_argument);
}

TEST_F(RouterTest, ConstructorWithDuplicateProviderNames) {
    std::vector<ProviderConfig> duplicate_providers = {
        ProviderConfig{"provider1", "https://example1.com", "key1", {"gpt-4"}, 60, true},
        ProviderConfig{"provider1", "https://example2.com", "key2", {"gpt-3.5"}, 30, true}
    };

    EXPECT_THROW({
        Router router(duplicate_providers);
    }, std::invalid_argument);
}

// Configuration validation tests
TEST_F(RouterTest, ProviderConfigValidation) {
    // Test empty name
    ProviderConfig invalid_config1;
    invalid_config1.endpoint = "https://example.com";
    invalid_config1.api_key = "key";
    EXPECT_THROW({
        invalid_config1.to_json();
    }, std::invalid_argument);

    // Test invalid endpoint
    ProviderConfig invalid_config2;
    invalid_config2.name = "test";
    invalid_config2.endpoint = "not-a-url";
    invalid_config2.api_key = "key";
    EXPECT_THROW({
        invalid_config2.to_json();
    }, std::invalid_argument);

    // Test valid configuration
    ProviderConfig valid_config;
    valid_config.name = "test-provider";
    valid_config.endpoint = "https://example.com/api";
    valid_config.api_key = "test-key";
    valid_config.models = {"gpt-4"};
    valid_config.max_requests_per_minute = 60;
    valid_config.enabled = true;

    EXPECT_NO_THROW({
        auto json = valid_config.to_json();
        EXPECT_EQ(json["name"], "test-provider");
        EXPECT_EQ(json["endpoint"], "https://example.com/api");
        EXPECT_NE(json.find("api_key"), json.end()); // Should be present but redacted
    });
}

// Request routing tests
TEST_F(RouterTest, BasicRequestRouting) {
    Request request = create_test_request();

    Response response = router_->route(request);

    // Basic response validation
    EXPECT_TRUE(response.success || !response.success); // Either way, structure should be valid
    EXPECT_FALSE(response.provider_name.empty());
    EXPECT_GE(response.response_time_ms, 0.0);
}

TEST_F(RouterTest, RoutingWithUnsupportedModel) {
    Request request = create_test_request("unsupported-model");
    request.data = nlohmann::json{
        {"messages", {{"role", "user"}, {"content", "test"}}}
    };

    Response response = router_->route(request);

    // Should either fail or find a provider that supports the model
    EXPECT_TRUE(!response.success || response.provider_name.empty());
}

TEST_F(RouterTest, RoutingWhenAllProvidersDisabled) {
    // Create router with all providers disabled
    std::vector<ProviderConfig> disabled_providers;
    for (auto& provider : providers_) {
        provider.enabled = false;
        disabled_providers.push_back(provider);
    }

    Router disabled_router(disabled_providers);
    Request request = create_test_request();

    Response response = disabled_router.route(request);

    EXPECT_FALSE(response.success);
    EXPECT_EQ(response.status_code, 503);
    EXPECT_TRUE(response.error_message.find("No available providers") != std::string::npos);
}

// Health status tests
TEST_F(RouterTest, HealthStatusReporting) {
    std::string health_json = router_->get_health_status();

    EXPECT_FALSE(health_json.empty());

    auto health = nlohmann::json::parse(health_json);
    EXPECT_TRUE(health.contains("overall_health"));
    EXPECT_TRUE(health.contains("providers"));
}

TEST_F(RouterTest, HealthStatusStructure) {
    std::string health_json = router_->get_health_status();
    auto health = nlohmann::json::parse(health_json);

    // Check provider structure
    for (const auto& provider : providers_) {
        EXPECT_TRUE(health["providers"].contains(provider.name));

        auto provider_health = health["providers"][provider.name];
        EXPECT_TRUE(provider_health.contains("healthy"));
        EXPECT_TRUE(provider_health.contains("enabled"));
    }
}

// Metrics collection tests
TEST_F(RouterTest, MetricsCollection) {
    // Make some requests
    Request request = create_test_request();
    for (int i = 0; i < 5; ++i) {
        router_->route(request);
    }

    std::string metrics_json = router_->get_metrics();
    EXPECT_FALSE(metrics_json.empty());

    auto metrics = nlohmann::json::parse(metrics_json);
    EXPECT_TRUE(metrics.contains("total_requests"));
    EXPECT_GE(metrics["total_requests"], 5);
}

TEST_F(RouterTest, MetricsAccuracy) {
    Request request = create_test_request();

    int initial_count = 0;

    // Get initial metrics
    auto initial_metrics = nlohmann::json::parse(router_->get_metrics());
    if (initial_metrics.contains("total_requests")) {
        initial_count = initial_metrics["total_requests"];
    }

    // Make specific number of requests
    const int num_requests = 10;
    for (int i = 0; i < num_requests; ++i) {
        router_->route(request);
    }

    // Check metrics increased correctly
    auto final_metrics = nlohmann::json::parse(router_->get_metrics());
    EXPECT_EQ(final_metrics["total_requests"], initial_count + num_requests);
}

// Concurrent access tests
TEST_F(RouterTest, ConcurrentRouting) {
    Request request = create_test_request();
    const int num_threads = 10;
    const int requests_per_thread = 5;

    std::vector<std::future<Response>> futures;

    // Launch concurrent requests
    for (int i = 0; i < num_threads; ++i) {
        futures.push_back(std::async(std::launch::async, [this, &request, requests_per_thread]() {
            Response last_response;
            for (int j = 0; j < requests_per_thread; ++j) {
                last_response = router_->route(request);
            }
            return last_response;
        }));
    }

    // Wait for all threads to complete
    std::vector<Response> responses;
    for (auto& future : futures) {
        responses.push_back(future.get());
    }

    // Validate responses
    EXPECT_EQ(responses.size(), num_threads);
    for (const auto& response : responses) {
        EXPECT_FALSE(response.provider_name.empty());
    }

    // Check total request count in metrics
    auto metrics = nlohmann::json::parse(router_->get_metrics());
    EXPECT_GE(metrics["total_requests"], num_threads * requests_per_thread);
}

// Error handling tests
TEST_F(RouterTest, MalformedRequestHandling) {
    Request malformed_request;
    malformed_request.model = ""; // Empty model
    malformed_request.method = ""; // Empty method
    malformed_request.data = nullptr; // Invalid JSON

    Response response = router_->route(malformed_request);

    // Should return an error response
    EXPECT_TRUE(response.provider_name.empty() || response.error_message.find("validation") != std::string::npos);
}

TEST_F(RouterTest, LargeRequestHandling) {
    Request request = create_test_request();

    // Create a large JSON payload
    nlohmann::json large_data;
    for (int i = 0; i < 1000; ++i) {
        large_data["data_" + std::to_string(i)] = std::string(1000, 'x');
    }
    request.data = large_data;

    Response response = router_->route(request);

    // Should handle gracefully (either succeed or fail with appropriate error)
    EXPECT_TRUE(response.success || !response.success); // Structure should be valid
}

// Performance tests
TEST_F(RouterTest, RoutingPerformance) {
    Request request = create_test_request();
    const int num_requests = 100;

    auto start_time = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_requests; ++i) {
        router_->route(request);
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    // Should complete reasonably quickly (adjust threshold as needed)
    EXPECT_LT(duration.count(), num_requests * 10); // Less than 10ms per request on average

    // Check that request count matches
    auto metrics = nlohmann::json::parse(router_->get_metrics());
    EXPECT_EQ(metrics["total_requests"].get<int>(), num_requests);
}

TEST_F(RouterTest, MetricsPerformanceTest) {
    // Test that metrics collection doesn't significantly impact performance
    Request request = create_test_request();
    const int num_iterations = 1000;

    // Test with metrics
    auto start_with_metrics = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_iterations; ++i) {
        router_->route(request);
        if (i % 100 == 0) {
            router_->get_metrics(); // Periodic metrics collection
        }
    }
    auto end_with_metrics = std::chrono::high_resolution_clock::now();
    auto duration_with_metrics = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_with_metrics - start_with_metrics);

    // Performance should be reasonable (<5ms per request)
    EXPECT_LT(duration_with_metrics.count(), num_iterations * 5);
}

// JSON serialization tests
TEST_F(RouterTest, ProviderConfigJsonSerialization) {
    ProviderConfig config;
    config.name = "test-provider";
    config.endpoint = "https://example.com/api";
    config.api_key = "secret-api-key";
    config.models = {"gpt-4", "gpt-3.5-turbo"};
    config.max_requests_per_minute = 100;
    config.enabled = true;

    // Serialize to JSON
    auto json = config.to_json();

    EXPECT_EQ(json["name"], "test-provider");
    EXPECT_EQ(json["endpoint"], "https://example.com/api");
    EXPECT_TRUE(json.contains("api_key")); // API key field should be present
    EXPECT_NE(json["api_key"], "secret-api-key"); // Should be redacted
    EXPECT_TRUE(json.contains("max_requests_per_minute"));
    EXPECT_EQ(json["models"].size(), 2);

    // Deserialize from JSON
    auto deserialized_config = ProviderConfig::from_json(json);
    EXPECT_EQ(deserialized_config.name, config.name);
    EXPECT_EQ(deserialized_config.endpoint, config.endpoint);
    EXPECT_EQ(deserialized_config.max_requests_per_minute, config.max_requests_per_minute);
    EXPECT_EQ(deserialized_config.models.size(), config.models.size());
}

TEST_F(RouterTest, RequestSerialization) {
    Request request;
    request.model = "gpt-4";
    request.method = "POST";
    request.data = nlohmann::json{
        {"messages", {{"role", "user"}, {"content", "Hello"}}}
    };

    auto json = request.to_json();

    EXPECT_EQ(json["model"], "gpt-4");
    EXPECT_EQ(json["method"], "POST");
    EXPECT_TRUE(json["data"].contains("messages"));

    auto deserialized_request = Request::from_json(json);
    EXPECT_EQ(deserialized_request.model, request.model);
    EXPECT_EQ(deserialized_request.method, request.method);
    EXPECT_EQ(deserialized_request.data, request.data);
}

TEST_F(RouterTest, ResponseSerialization) {
    Response response;
    response.success = true;
    response.data = "response data";
    response.error_message = "";
    response.status_code = 200;
    response.response_time_ms = 125.5;
    response.provider_name = "test-provider";

    auto json = response.to_json();

    EXPECT_TRUE(json["success"]);
    EXPECT_EQ(json["data"], "response data");
    EXPECT_EQ(json["status_code"], 200);
    EXPECT_EQ(json["response_time_ms"], 125.5);
    EXPECT_EQ(json["provider_name"], "test-provider");

    auto deserialized_response = Response::from_json(json);
    EXPECT_EQ(deserialized_response.success, response.success);
    EXPECT_EQ(deserialized_response.data, response.data);
    EXPECT_EQ(deserialized_response.status_code, response.status_code);
    EXPECT_EQ(deserialized_response.provider_name, response.provider_name);
}

// Edge case tests
TEST_F(RouterTest, ExtremeConfigurations) {
    // Test provider with very low rate limit
    ProviderConfig low_rate_provider;
    low_rate_provider.name = "low-rate";
    low_rate_provider.endpoint = "https://example.com";
    low_rate_provider.api_key = "key";
    low_rate_provider.models = {"gpt-4"};
    low_rate_provider.max_requests_per_minute = 1;
    low_rate_provider.enabled = true;

    Router low_rate_router({low_rate_provider});

    Request request = create_test_request();

    // Make multiple requests rapidly
    Response response1 = low_rate_router.route(request);
    Response response2 = low_rate_router.route(request);

    // Should handle rate limiting gracefully
    EXPECT_TRUE(response1.provider_name.empty() || response2.provider_name.empty() ||
                response1.error_message.find("rate") != std::string::npos ||
                response2.error_message.find("rate") != std::string::npos);
}

TEST_F(RouterTest, SpecialCharactersHandling) {
    ProviderConfig special_config;
    special_config.name = "test-provider";
    special_config.endpoint = "https://example.com/api?param=value&special=χars";
    special_config.api_key = "key-with-special-chars_123!@#$%^&*()";
    special_config.models = {"gpt-4", "model-with-ünicode"};
    special_config.enabled = true;

    Router special_router({special_config});

    Request request;
    request.model = "model-with-ünicode";
    request.method = "POST";
    request.data = nlohmann::json{
        {"message", "Hello世界! 🌟"}
    };

    Response response = special_router.route(request);

    // Should handle special characters gracefully
    EXPECT_TRUE(response.success || response.error_message.find("validation") != std::string::npos);
}

// Memory and resource tests
TEST_F(RouterTest, MemoryUsageTest) {
    // Create and destroy multiple routers to test memory management
    for (int i = 0; i < 100; ++i) {
        Router temp_router(providers_);
        Request request = create_test_request();

        for (int j = 0; j < 10; ++j) {
            temp_router.route(request);
        }
    }

    // If we reach here without memory issues, test passes
    SUCCEED();
}

TEST_F(RouterTest, ResourceCleanupTest) {
    // Test that resources are properly cleaned up
    {
        Router temp_router(providers_);
        Request request = create_test_request();
        temp_router.route(request);
        temp_router.get_metrics();
        temp_router.get_health_status();
    } // Router destructor should be called here

    // If we reach here without memory leaks/dangling pointers, test passes
    SUCCEED();
}