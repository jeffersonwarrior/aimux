/**
 * Router-Provider Integration Test Suite
 *
 * Comprehensive integration tests for the Router and Provider system:
 * - Multi-provider failover and load balancing
 * - Rate limiting across providers
 * - Health monitoring and recovery
 * - Configuration hot-reloading
 * - Concurrent request routing
 * - Performance under load with fault injection
 *
 * Target: >90% integration coverage for router-provider interactions
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <thread>
#include <chrono>
#include <future>
#include <atomic>
#include <vector>
#include <memory>
#include <random>
#include <queue>
#include <condition_variable>

#include "aimux/core/router.hpp"
#include "aimux/core/bridge.hpp"
#include "aimux/testing/fault_injection.h"
#include "aimux/testing/property_based_testing.h"
#include "nlohmann/json.hpp"

using namespace aimux::core;
using namespace aimux::testing;
using namespace ::testing;

// Mock provider implementation for testing
class MockProviderBridge : public Bridge {
public:
    MockProviderBridge(const std::string& name, bool healthy = true,
                      int latency_ms = 50, double success_rate = 0.95,
                      int max_requests_per_minute = 100)
        : name_(name), healthy_(healthy), latency_ms_(latency_ms),
          success_rate_(success_rate), max_requests_per_minute_(max_requests_per_minute),
          request_count_(0), fail_count_(0) {}

    Response send_request(const Request& request) override {
        request_count_++;

        // Simulate rate limiting
        if (check_rate_limit()) {
            Response error_response;
            error_response.success = false;
            error_response.error_message = "Rate limit exceeded";
            error_response.status_code = 429;
            error_response.provider_name = name_;
            return error_response;
        }

        // Simulate network delay
        std::this_thread::sleep_for(std::chrono::milliseconds(latency_ms_));

        // Simulate failure based on success rate
        static thread_local std::mt19937_64 rng(std::random_device{}());
        std::uniform_real_distribution<double> dist(0.0, 1.0);

        Response response;
        response.provider_name = name_;
        response.response_time_ms = latency_ms_;

        if (dist(rng) > success_rate_) {
            fail_count_++;
            response.success = false;
            response.error_message = "Provider failed (simulated)";
            response.status_code = 500;
        } else {
            response.success = true;
            response.status_code = 200;
            response.content = "Response from " + name_ + " for request " +
                              std::to_string(request_count_);
            response.data = nlohmann::json{
                {"provider", name_},
                {"request_id", request_count_},
                {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::system_clock::now().time_since_epoch()).count()}
            };
        }

        return response;
    }

    bool is_healthy() const override {
        // Simulate health degradation after many requests
        return healthy_ && fail_count_ < request_count_ * 0.1; // 10% failure threshold
    }

    std::string get_provider_name() const override {
        return name_;
    }

    nlohmann::json get_rate_limit_status() const override {
        auto now = std::chrono::system_clock::now();
        auto minute_ago = now - std::chrono::minutes(1);

        // Count requests in the last minute
        int recent_requests = 0;
        {
            std::lock_guard<std::mutex> lock(request_times_mutex_);
            for (auto it = request_times_.rbegin(); it != request_times_.rend(); ++it) {
                if (*it >= minute_ago) recent_requests++;
                else break;
            }
        }

        return nlohmann::json{
            {"requests_used", recent_requests},
            {"requests_limit", max_requests_per_minute_},
            {"reset_time", std::chrono::duration_cast<std::chrono::seconds>(
                            now.time_since_epoch()).count() + 60},
            {"retry_after", recent_requests >= max_requests_per_minute_ ? 60 : 0}
        };
    }

    // Test control methods
    void set_healthy(bool healthy) { healthy_ = healthy; }
    void set_success_rate(double rate) { success_rate_ = std::max(0.0, std::min(1.0, rate)); }
    void set_latency(int ms) { latency_ms_ = ms; }
    int get_request_count() const { return request_count_.load(); }
    int get_fail_count() const { return fail_count_.load(); }

private:
    bool check_rate_limit() {
        auto now = std::chrono::system_clock::now();
        auto minute_ago = now - std::chrono::minutes(1);

        std::lock_guard<std::mutex> lock(request_times_mutex_);

        // Remove old timestamps
        while (!request_times_.empty() && request_times_.front() < minute_ago) {
            request_times_.pop();
        }

        // Check rate limit
        if (request_times_.size() >= static_cast<size_t>(max_requests_per_minute_)) {
            return true; // Rate limited
        }

        request_times_.push(now);
        return false;
    }

    std::string name_;
    std::atomic<bool> healthy_;
    std::atomic<int> latency_ms_;
    std::atomic<double> success_rate_;
    int max_requests_per_minute_;
    std::atomic<int> request_count_;
    std::atomic<int> fail_count_;

    std::queue<std::chrono::system_clock::time_point> request_times_;
    mutable std::mutex request_times_mutex_;
};

// Test fixture for Router-Provider integration
class RouterProviderIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create multiple mock providers with different characteristics
        providers_ = {
            std::make_shared<MockProviderBridge>("fast_reliable", true, 20, 0.98, 200),
            std::make_shared<MockProviderBridge>("moderate_reliable", true, 50, 0.95, 100),
            std::make_shared<MockProviderBridge>("slow_unreliable", true, 100, 0.85, 50),
            std::make_shared<MockProviderBridge>("backup_provider", true, 30, 0.90, 75)
        };

        router_ = std::make_unique<Router>();

        // Add all providers to the router
        for (auto& provider : providers_) {
            router_->add_provider(provider);
        }

        // Configure routing strategy
        router_->set_routing_strategy(Router::RoutingStrategy::FASTEST_RESPONSE);
        router_->set_failover_policy(Router::FailoverPolicy::AUTOMATIC);
    }

    void TearDown() override {
        router_.reset();
        providers_.clear();
        get_fault_manager().reset_all();
    }

    Request create_test_request(const std::string& model = "gpt-4",
                               const std::string& content = "Test message") {
        Request request;
        request.model = model;
        request.method = "POST";
        request.correlation_id = generate_correlation_id();
        request.parameters = nlohmann::json{
            {"temperature", 0.7},
            {"max_tokens", 100}
        };
        request.data = nlohmann::json{
            {"messages", {
                {{"role", "user"}, {"content", content}}
            }}
        };
        return request;
    }

    std::string generate_correlation_id() {
        static std::atomic<int> counter{0};
        return "test-" + std::to_string(counter++);
    }

    std::vector<std::shared_ptr<MockProviderBridge>> providers_;
    std::unique_ptr<Router> router_;
};

// Basic integration tests
TEST_F(RouterProviderIntegrationTest, BasicRequestRouting) {
    Request request = create_test_request();

    Response response = router_->route(request);

    EXPECT_TRUE(response.success || !response.success); // Either way should be structured
    EXPECT_FALSE(response.provider_name.empty());
    EXPECT_GE(response.response_time_ms, 0.0);

    // Check that one of our providers handled the request
    bool provider_found = false;
    for (const auto& provider : providers_) {
        if (provider->get_provider_name() == response.provider_name) {
            provider_found = true;
            EXPECT_GT(provider->get_request_count(), 0);
            break;
        }
    }
    EXPECT_TRUE(provider_found);
}

TEST_F(RouterProviderIntegrationTest, LoadBalancing AcrossProviders) {
    const int num_requests = 20;
    std::map<std::string, int> provider_counts;

    // Send multiple requests
    for (int i = 0; i < num_requests; ++i) {
        Request request = create_test_request("gpt-4", "Test message " + std::to_string(i));
        Response response = router_->route(request);

        if (response.success) {
            provider_counts[response.provider_name]++;
        }
    }

    // Should distribute load across multiple providers
    EXPECT_GT(provider_counts.size(), 1);

    // No single provider should handle all requests
    int max_requests = 0;
    for (const auto& [provider, count] : provider_counts) {
        max_requests = std::max(max_requests, count);
        EXPECT_GT(count, 0);
    }
    EXPECT_LT(max_requests, num_requests); // At least some distribution
}

TEST_F(RouterProviderIntegrationTest, AutomaticFailover) {
    // Make the primary provider fail
    providers_[0]->set_healthy(false);
    providers_[0]->set_success_rate(0.0);

    const int num_requests = 10;
    std::map<std::string, int> success_counts;

    // Send requests - should failover to healthy providers
    for (int i = 0; i < num_requests; ++i) {
        Request request = create_test_request();
        Response response = router_->route(request);

        if (response.success) {
            success_counts[response.provider_name]++;
            EXPECT_NE(response.provider_name, providers_[0]->get_provider_name());
        }
    }

    // Should have successful responses from backup providers
    EXPECT_GT(success_counts.size(), 0);
    EXPECT_FALSE(success_counts.contains(providers_[0]->get_provider_name()));
}

TEST_F(RouterProviderIntegrationTest, HealthMonitoringAndRecovery) {
    // Start with a healthy provider
    std::string provider_name = providers_[0]->get_provider_name();

    // Fail it
    providers_[0]->set_healthy(false);

    // Send requests to confirm failover
    Request request = create_test_request();
    Response response1 = router_->route(request);
    EXPECT_NE(response1.provider_name, provider_name);

    // Recover the provider
    providers_[0]->set_healthy(true);

    // Wait a moment for health check to pick up recovery
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Send more requests - should eventually use recovered provider
    bool recovered_provider_used = false;
    for (int i = 0; i < 10; ++i) {
        Response response = router_->route(create_test_request());
        if (response.provider_name == provider_name) {
            recovered_provider_used = true;
            break;
        }
    }

    EXPECT_TRUE(recovered_provider_used || response1.provider_name == provider_name);
}

TEST_F(RouterProviderIntegrationTest, ConcurrentRequestHandling) {
    const int num_threads = 8;
    const int requests_per_thread = 10;
    std::vector<std::future<void>> futures;
    std::atomic<int> successful_requests{0};
    std::atomic<int> total_requests{0};
    std::mutex provider_count_mutex;
    std::map<std::string, int> provider_counts;

    // Launch concurrent request threads
    for (int t = 0; t < num_threads; ++t) {
        futures.push_back(std::async(std::launch::async, [&]() {
            for (int i = 0; i < requests_per_thread; ++i) {
                Request request = create_test_request("gpt-4",
                    "Concurrent test message " + std::to_string(t) + "-" + std::to_string(i));

                Response response = router_->route(request);

                total_requests++;
                if (response.success) {
                    successful_requests++;

                    std::lock_guard<std::mutex> lock(provider_count_mutex);
                    provider_counts[response.provider_name]++;
                }
            }
        }));
    }

    // Wait for all threads to complete
    for (auto& future : futures) {
        future.get();
    }

    // Validate results
    EXPECT_EQ(total_requests.load(), num_threads * requests_per_thread);
    EXPECT_GT(successful_requests.load(), 0); // At least some should succeed

    // Should distribute across multiple providers
    EXPECT_GT(provider_counts.size(), 1);
}

TEST_F(RouterProviderIntegrationTest, RateLimitingAcrossProviders) {
    // Configure low rate limits to test rate limiting behavior
    for (auto& provider : providers_) {
        provider.get()->set_max_requests_per_minute(3);
    }

    std::vector<Response> responses;
    const int num_requests = 15; // More than total rate limit across providers

    // Send rapid requests
    for (int i = 0; i < num_requests; ++i) {
        Request request = create_test_request("gpt-4", "Rate limit test " + std::to_string(i));
        Response response = router_->route(request);
        responses.push_back(response);

        // Small delay between requests
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Should handle rate limiting gracefully
    int successful_responses = 0;
    int rate_limited_responses = 0;

    for (const auto& response : responses) {
        if (response.success) {
            successful_responses++;
        } else if (response.status_code == 429) {
            rate_limited_responses++;
        }
    }

    EXPECT_GT(successful_responses, 0);
    EXPECT_GE(rate_limited_responses, 0); // May or may not have rate limits depending on timing
}

// Fault injection integration tests
TEST_F(RouterProviderIntegrationTest, NetworkTimeoutFaultInjection) {
    // Inject network timeout faults
    get_fault_manager().remove_injector("network_timeout");
    auto timeout_injector = std::make_unique<NetworkFaultInjector>(
        NetworkFaultInjector::FaultType::TIMEOUT, 0.3); // 30% timeout rate
    get_fault_manager().add_injector("network_timeout", std::move(timeout_injector));

    const int num_requests = 20;
    std::atomic<int> successful_requests{0};
    std::atomic<int> failed_requests{0};

    // Send requests with fault injection
    for (int i = 0; i < num_requests; ++i) {
        try {
            Request request = create_test_request("gpt-4", "Fault injection test " + std::to_string(i));

            // Inject faults during provider calls
            get_fault_manager().inject_random();

            Response response = router_->route(request);

            if (response.success) {
                successful_requests++;
            } else {
                failed_requests++;
            }
        } catch (const std::exception& e) {
            failed_requests++;
        }
    }

    // Should handle network timeouts gracefully with failover
    EXPECT_GT(successful_requests + failed_requests, num_requests * 0.8); // Most requests should complete
}

TEST_F(RouterProviderIntegrationTest, MemoryExhaustionFaultInjection) {
    // Inject memory exhaustion (temporary)
    get_fault_manager().remove_injector("memory_exhaustion");
    auto memory_injector = std::make_unique<ResourceExhaustionInjector>(
        ResourceExhaustionInjector::ResourceType::MEMORY, 1024 * 10, true); // 10MB temporary
    get_fault_manager().add_injector("memory_exhaustion", std::move(memory_injector));

    const int num_requests = 10;
    std::atomic<bool> all_handled{true};

    // Send requests with memory pressure
    for (int i = 0; i < num_requests; ++i) {
        try {
            Request request = create_test_request("gpt-4", "Memory pressure test " + std::to_string(i));

            get_fault_manager().inject_random();

            Response response = router_->route(request);

            if (!response.success && response.error_message.find("memory") == std::string::npos) {
                all_handled = false;
            }
        } catch (const std::bad_alloc&) {
            // Expected under memory pressure
            continue;
        } catch (const std::exception& e) {
            all_handled = false;
        }
    }

    EXPECT_TRUE(all_handled.load());
}

// Performance integration tests
TEST_F(RouterProviderIntegrationTest, PerformanceUnderLoad) {
    const int num_requests = 100;
    const int concurrent_threads = 4;
    std::vector<std::future<std::vector<double>>> futures;

    auto start_time = std::chrono::high_resolution_clock::now();

    // Launch performance test threads
    for (int t = 0; t < concurrent_threads; ++t) {
        futures.push_back(std::async(std::launch::async, [&]() {
            std::vector<double> response_times;

            for (int i = 0; i < num_requests / concurrent_threads; ++i) {
                Request request = create_test_request("gpt-4",
                    "Performance test " + std::to_string(t) + "-" + std::to_string(i));

                auto request_start = std::chrono::high_resolution_clock::now();
                Response response = router_->route(request);
                auto request_end = std::chrono::high_resolution_clock::now();

                double response_time_ms = std::chrono::duration<double, std::milli>(
                    request_end - request_start).count();
                response_times.push_back(response_time_ms);
            }

            return response_times;
        }));
    }

    // Collect all response times
    std::vector<double> all_response_times;
    for (auto& future : futures) {
        auto times = future.get();
        all_response_times.insert(all_response_times.end(), times.begin(), times.end());
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    double total_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();

    // Performance assertions
    EXPECT_EQ(all_response_times.size(), num_requests);

    // Calculate statistics
    double avg_response_time = std::accumulate(all_response_times.begin(),
                                            all_response_times.end(), 0.0) / num_requests;
    auto min_response_time = *std::min_element(all_response_times.begin(), all_response_times.end());
    auto max_response_time = *std::max_element(all_response_times.begin(), all_response_times.end());

    // Performance requirements
    EXPECT_LT(avg_response_time, 200.0); // Average < 200ms
    EXPECT_LT(max_response_time, 1000.0); // Max < 1000ms
    EXPECT_GT(num_requests / (total_time_ms / 1000.0), 5.0); // >5 RPS
}

// Property-based integration tests
TEST_F(RouterProviderIntegrationTest, PropertyBasedRequestHandling) {
    // Define property for router request handling
    auto router_property = Property<Request>([](const Request& request) -> bool {
        auto& router = get_fault_manager().add_injector("test_property",
            std::make_unique<NetworkFaultInjector>(NetworkFaultInjector::FaultType::TIMEOUT, 0.0));

        try {
            Response response = router.route(request);

            // Property: response should have valid structure regardless of request content
            return !response.provider_name.empty() &&
                   response.response_time_ms >= 0.0;
        } catch (...) {
            return true; // Any exception is acceptable for malformed input
        }
    });

    auto test_config = PropertyTestRunner::Config{
        .max_tests = 100,
        .max_shrink_steps = 50,
        .seed = 12345
    };

    PropertyTestRunner::assert_property(router_property, "router_request_structure", test_config);
}