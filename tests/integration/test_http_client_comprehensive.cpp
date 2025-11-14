/**
 * Comprehensive HTTP Client Test Suite - Integration tests for Aimux v2.0.0 HTTP client
 * Tests actual HTTP requests to external APIs, timeouts, and error handling
 */

#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <future>
#include <atomic>
#include <vector>
#include <string>

// Aimux HTTP client
#include "aimux/network/http_client.hpp"

using namespace aimux;
using namespace std::chrono_literals;

class HttpClientIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        HttpClientConfig config;
        config.default_timeout = 30000ms;
        config.connect_timeout = 10000ms;
        config.user_agent = "Aimux-Integration-Test/2.0.0";
        config.connection_pool_size = 5;
        config.follow_redirects = true;
        config.verify_ssl = true;

        client = std::make_unique<HttpClient>(config);
    }

    void TearDown() override {
        client.reset();
    }

    std::unique_ptr<HttpClient> client;
};

// Test basic GET request to httpbin.org
TEST_F(HttpClientIntegrationTest, BasicGetRequest) {
    HttpResult result = client->get("https://httpbin.org/get");

    ASSERT_TRUE(result.has_value()) << "GET request should succeed. Error: " << client->errorToString(result.error());
    EXPECT_EQ(result->status_code, 200);
    EXPECT_FALSE(result->body.empty());
    EXPECT_GT(result->elapsed_ms.count(), 0);
    EXPECT_GE(result->connect_time_ms.count(), 0);
    EXPECT_GE(result->name_lookup_time_ms.count(), 0);

    // Verify response contains expected content
    EXPECT_TRUE(result->body.find("\"url\":") != std::string::npos);
    EXPECT_TRUE(result->body.find("httpbin.org/get") != std::string::npos);

    std::cout << "GET request completed in " << result->elapsed_ms.count() << "ms\n";
    std::cout << "  Connect time: " << result->connect_time_ms.count() << "ms\n";
    std::cout << "  DNS lookup: " << result->name_lookup_time_ms.count() << "ms\n";
}

// Test POST request with JSON body
TEST_F(HttpClientIntegrationTest, PostRequestWithJson) {
    const std::string json_body = R"({"test": "value", "number": 42, "array": [1,2,3]})";
    const Headers headers = {{"Content-Type", "application/json"}};

    HttpResult result = client->post("https://httpbin.org/post", json_body, headers);

    ASSERT_TRUE(result.has_value()) << "POST request should succeed. Error: " << client->errorToString(result.error());
    EXPECT_EQ(result->status_code, 200);
    EXPECT_FALSE(result->body.empty());

    // Verify the POST data was echoed correctly
    EXPECT_TRUE(result->body.find("\"test\": \"value\"") != std::string::npos);
    EXPECT_TRUE(result->body.find("\"number\": 42") != std::string::npos);
    EXPECT_TRUE(result->body.find("\"array\": [1, 2, 3]") != std::string::npos);

    std::cout << "POST request completed in " << result->elapsed_ms.count() << "ms\n";
}

// Test timeout functionality
TEST_F(HttpClientIntegrationTest, TimeoutFunctionality) {
    HttpClientConfig timeout_config;
    timeout_config.default_timeout = 2000ms; // 2 second timeout
    timeout_config.connect_timeout = 1000ms;

    HttpClient timeout_client(timeout_config);

    // Request to httpbin.org/delay/5 should take 5 seconds, causing timeout
    HttpResult result = timeout_client.get("https://httpbin.org/delay/5");

    EXPECT_FALSE(result.has_value()) << "Request should timeout after 2 seconds";
    EXPECT_EQ(result.error(), HttpError::Timeout);

    std::cout << "Request correctly timed out after 2 seconds\n";
}

// Test connection timeout with invalid host
TEST_F(HttpClientIntegrationTest, ConnectionTimeout) {
    HttpClientConfig timeout_config;
    timeout_config.connect_timeout = 1000ms; // 1 second connection timeout
    timeout_config.default_timeout = 5000ms;

    HttpClient timeout_client(timeout_config);

    // Use a non-existent domain that should timeout quickly
    HttpResult result = timeout_client.get("http://nonexistent-test-domain-12345.com");

    EXPECT_FALSE(result.has_value()) << "Request should fail due to connection timeout/DNS error";
    // Could be DnsError or ConnectionFailure depending on the system
    EXPECT_TRUE(result.error() == HttpError::DnsError || 
                result.error() == HttpError::ConnectionFailure);

    std::cout << "Connection properly failed for non-existent domain\n";
}

// Test error handling for invalid URLs
TEST_F(HttpClientIntegrationTest, InvalidUrlHandling) {
    HttpResult result = client->get("not-a-valid-url");

    EXPECT_FALSE(result.has_value()) << "Invalid URL should fail";
    EXPECT_EQ(result.error(), HttpError::InvalidUrl);

    // Test other invalid URL formats
    std::vector<std::string> invalid_urls = {
        "ftp://example.com/file", // Unsupported protocol
        "http://", // Incomplete URL
        "" // Empty URL
    };

    for (const auto& url : invalid_urls) {
        HttpResult result = client->get(url);
        // Most should fail, though behavior might vary
        if (!result.has_value()) {
            std::cout << "Invalid URL properly rejected: '" << url << "'\n";
        }
    }
}

// Test custom headers
TEST_F(HttpClientIntegrationTest, CustomHeaders) {
    const Headers headers = {
        {"User-Agent", "Aimux-Custom-Agent/1.0"},
        {"X-Test-Header", "test-value"},
        {"Accept", "application/json"}
    };

    HttpResult result = client->get("https://httpbin.org/headers", headers);

    ASSERT_TRUE(result.has_value()) << "Request with custom headers should succeed";
    EXPECT_EQ(result->status_code, 200);

    // Verify our custom headers were sent
    EXPECT_TRUE(result->body.find("\"User-Agent\": \"Aimux-Custom-Agent/1.0\"") != std::string::npos);
    EXPECT_TRUE(result->body.find("\"X-Test-Header\": \"test-value\"") != std::string::npos);
    EXPECT_TRUE(result->body.find("\"Accept\": \"application/json\"") != std::string::npos);

    std::cout << "Custom headers test passed\n";
}

// Test HTTP status code handling (4xx and 5xx errors)
TEST_F(HttpClientIntegrationTest, StatusCodeHandling) {
    // Test 404 Not Found
    HttpResult result_404 = client->get("https://httpbin.org/status/404");
    ASSERT_TRUE(result_404.has_value()) << "404 request should succeed (connection-wise)";
    EXPECT_EQ(result_404->status_code, 404);

    // Test 500 Internal Server Error
    HttpResult result_500 = client->get("https://httpbin.org/status/500");
    ASSERT_TRUE(result_500.has_value()) << "500 request should succeed (connection-wise)";
    EXPECT_EQ(result_500->status_code, 500);

    std::cout << "Status code handling test passed\n";
}

// Test different HTTP methods
TEST_F(HttpClientIntegrationTest, HttpMethods) {
    // Test PUT
    const std::string put_body = R"({"method": "PUT", "data": "test"})";
    HttpRequest put_request("https://httpbin.org/put", HttpMethod::PUT);
    put_request.body = put_body;
    put_request.headers = {{"Content-Type", "application/json"}};

    HttpResult put_result = client->send(put_request);
    ASSERT_TRUE(put_result.has_value()) << "PUT request should succeed";
    EXPECT_EQ(put_result->status_code, 200);
    EXPECT_TRUE(put_result->body.find("\"method\": \"PUT\"") != std::string::npos);

    // Test DELETE
    HttpResult delete_result = client->send("https://httpbin.org/delete", HttpMethod::DELETE);
    ASSERT_TRUE(delete_result.has_value()) << "DELETE request should succeed";
    EXPECT_EQ(delete_result->status_code, 200);

    // Test HEAD
    HttpResult head_result = client->send("https://httpbin.org/get", HttpMethod::HEAD);
    ASSERT_TRUE(head_result.has_value()) << "HEAD request should succeed";
    EXPECT_EQ(head_result->status_code, 200);
    // HEAD requests shouldn't have a body
    EXPECT_TRUE(head_result->body.empty());

    std::cout << "HTTP methods test passed\n";
}

// Test concurrent requests
TEST_F(HttpClientIntegrationTest, ConcurrentRequests) {
    const int num_requests = 5;
    std::vector<std::future<HttpResult>> futures;
    std::atomic<int> successful_requests{0};

    // Launch multiple concurrent requests
    for (int i = 0; i < num_requests; ++i) {
        futures.push_back(std::async(std::launch::async, [this, &successful_requests]() {
            auto result = client->get("https://httpbin.org/delay/1");
            if (result.has_value()) {
                successful_requests.fetch_add(1);
            }
            return result;
        }));
    }

    // Wait for all requests to complete
    for (auto& future : futures) {
        auto result = future.get();
        // Some requests might fail due to timeouts or network issues, but most should succeed
        if (result.has_value()) {
            EXPECT_EQ(result->status_code, 200);
        }
    }

    EXPECT_GE(successful_requests.load(), num_requests * 0.8) << "Most requests should succeed";
    
    const auto& metrics = client->getMetrics();
    EXPECT_EQ(metrics.get_total_requests(), num_requests);
    EXPECT_GE(metrics.get_successful_requests(), num_requests * 0.8);

    std::cout << "Concurrent requests test: " << successful_requests.load() 
              << "/" << num_requests << " successful\n";
}

// Test metrics tracking
TEST_F(HttpClientIntegrationTest, MetricsTracking) {
    client->resetMetrics();

    // Make some successful requests
    client->get("https://httpbin.org/get");
    client->post("https://httpbin.org/post", R"({"test": "data"})", {{"Content-Type", "application/json"}});

    // Make a request that will timeout
    HttpClientConfig timeout_config;
    timeout_config.default_timeout = 1000ms;
    HttpClient timeout_client(timeout_config);
    timeout_client.get("https://httpbin.org/delay/5");

    const auto& metrics = client->getMetrics();
    
    EXPECT_EQ(metrics.get_total_requests(), 2);
    EXPECT_EQ(metrics.get_successful_requests(), 2);
    EXPECT_EQ(metrics.get_failed_requests(), 0);
    EXPECT_GT(metrics.get_bytes_sent(), 0);
    EXPECT_GT(metrics.get_bytes_received(), 0);
    EXPECT_GT(metrics.get_total_response_time().count(), 0);

    std::cout << "Metrics after requests:\n";
    std::cout << "  Total: " << metrics.get_total_requests() << "\n";
    std::cout << "  Successful: " << metrics.get_successful_requests() << "\n";
    std::cout << "  Failed: " << metrics.get_failed_requests() << "\n";
    std::cout << "  Bytes sent: " << metrics.get_bytes_sent() << "\n";
    std::cout << "  Bytes received: " << metrics.get_bytes_received() << "\n";
    std::cout << "  Total response time: " << metrics.get_total_response_time().count() << "ms\n";
}

// Test HTTPS with SSL verification
TEST_F(HttpClientIntegrationTest, SslVerification) {
    // Test with SSL verification enabled (default)
    HttpResult result = client->get("https://httpbin.org/get");
    ASSERT_TRUE(result.has_value()) << "HTTPS request with SSL verification should succeed";
    EXPECT_EQ(result->status_code, 200);

    // Test with SSL verification disabled
    HttpClientConfig no_ssl_config;
    no_ssl_config.verify_ssl = false;
    HttpClient no_ssl_client(no_ssl_config);

    HttpResult no_ssl_result = no_ssl_client.get("https://httpbin.org/get");
    ASSERT_TRUE(no_ssl_result.has_value()) << "HTTPS request without SSL verification should succeed";
    EXPECT_EQ(no_ssl_result->status_code, 200);

    std::cout << "SSL verification test passed\n";
}

// Test redirects
TEST_F(HttpClientIntegrationTest, RedirectHandling) {
    // httpbin.org/redirect/2 will redirect 2 times
    HttpResult result = client->get("https://httpbin.org/redirect/2");

    ASSERT_TRUE(result.has_value()) << "Redirect request should succeed";
    EXPECT_EQ(result->status_code, 200);
    EXPECT_TRUE(result->body.find("\"url\":") != std::string::npos);

    std::cout << "Redirect test passed, final URL in response\n";
}

// Performance test
TEST_F(HttpClientIntegrationTest, PerformanceTest) {
    client->resetMetrics();
    
    const int num_requests = 10;
    const auto start_time = std::chrono::steady_clock::now();

    for (int i = 0; i < num_requests; ++i) {
        HttpResult result = client->get("https://httpbin.org/get");
        ASSERT_TRUE(result.has_value()) << "Request " << i << " should succeed";
        EXPECT_EQ(result->status_code, 200);
    }

    const auto end_time = std::chrono::steady_clock::now();
    const auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    const auto& metrics = client->getMetrics();
    const auto avg_response_time = metrics.get_total_response_time().count() / num_requests;

    std::cout << "Performance test results:\n";
    std::cout << "  Total requests: " << num_requests << "\n";
    std::cout << "  Total time: " << total_time.count() << "ms\n";
    std::cout << "  Average response time: " << avg_response_time << "ms\n";
    std::cout << "  Requests per second: " << (num_requests * 1000.0) / total_time.count() << "\n";

    // Performance assertions (adjust these based on your expectations)
    EXPECT_LT(avg_response_time, 5000) << "Average response time should be under 5 seconds";
    EXPECT_LT(total_time.count(), 30000) << "Total time should be under 30 seconds";
}

// Main function for running tests independently
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}