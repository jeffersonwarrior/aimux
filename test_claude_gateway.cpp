#include <iostream>
#include <thread>
#include <chrono>
#include <memory>
#include <nlohmann/json.hpp>
#include <curl/curl.h>

#include "aimux/gateway/claude_gateway.hpp"
#include "aimux/core/bridge.hpp"

using namespace aimux::gateway;
using namespace aimux::core;

// Test configuration
const std::string GATEWAY_URL = "http://127.0.0.1:8080";
const int TEST_TIMEOUT_MS = 10000;

// HTTP request helper
struct HttpResponse {
    int status_code = 0;
    std::string body;
    std::map<std::string, std::string> headers;
};

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, std::string *userp) {
    userp->append((char*)contents, size * nmemb);
    return size * nmemb;
}

static size_t HeaderCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    std::string header((char*)contents, size * nmemb);
    auto headers = static_cast<std::map<std::string, std::string>*>(userp);

    size_t colon_pos = header.find(':');
    if (colon_pos != std::string::npos) {
        std::string key = header.substr(0, colon_pos);
        std::string value = header.substr(colon_pos + 1);

        // Trim whitespace
        key.erase(0, key.find_first_not_of(" \t\r\n"));
        key.erase(key.find_last_not_of(" \t\r\n") + 1);
        value.erase(0, value.find_first_not_of(" \t\r\n"));
        value.erase(value.find_last_not_of(" \t\r\n") + 1);

        (*headers)[key] = value;
    }

    return size * nmemb;
}

HttpResponse make_http_request(const std::string& url, const std::string& method = "GET",
                              const std::string& body = "", const std::map<std::string, std::string>& headers = {}) {
    HttpResponse response;

    CURL *curl = curl_easy_init();
    if (!curl) {
        return response;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, HeaderCallback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response.headers);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, TEST_TIMEOUT_MS);

    // Set method
    if (method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    } else if (method == "GET") {
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    }

    // Set headers
    struct curl_slist *header_list = nullptr;
    for (const auto& [key, value] : headers) {
        std::string header_str = key + ": " + value;
        header_list = curl_slist_append(header_list, header_str.c_str());
    }

    if (header_list) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
    }

    CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status_code);
    }

    curl_slist_free_all(header_list);
    curl_easy_cleanup(curl);

    return response;
}

// Test cases
bool test_health_endpoint() {
    std::cout << "Testing health endpoint..." << std::endl;

    HttpResponse response = make_http_request(GATEWAY_URL + "/health");

    if (response.status_code != 200) {
        std::cerr << "❌ Health endpoint failed: status " << response.status_code << std::endl;
        return false;
    }

    try {
        nlohmann::json health = nlohmann::json::parse(response.body);
        if (health["status"] != "healthy") {
            std::cerr << "❌ Health status not healthy: " << health["status"] << std::endl;
            return false;
        }
    } catch (const std::exception& e) {
        std::cerr << "❌ Failed to parse health response: " << e.what() << std::endl;
        return false;
    }

    std::cout << "✅ Health endpoint working" << std::endl;
    return true;
}

bool test_metrics_endpoint() {
    std::cout << "Testing metrics endpoint..." << std::endl;

    HttpResponse response = make_http_request(GATEWAY_URL + "/metrics");

    if (response.status_code != 200) {
        std::cerr << "❌ Metrics endpoint failed: status " << response.status_code << std::endl;
        return false;
    }

    try {
        nlohmann::json metrics = nlohmann::json::parse(response.body);
        if (!metrics.contains("total_requests") || !metrics.contains("service_status")) {
            std::cerr << "❌ Metrics response missing required fields" << std::endl;
            return false;
        }
    } catch (const std::exception& e) {
        std::cerr << "❌ Failed to parse metrics response: " << e.what() << std::endl;
        return false;
    }

    std::cout << "✅ Metrics endpoint working" << std::endl;
    return true;
}

bool test_models_endpoint() {
    std::cout << "Testing models endpoint..." << std::endl;

    HttpResponse response = make_http_request(GATEWAY_URL + "/anthropic/v1/models");

    if (response.status_code != 200) {
        std::cerr << "❌ Models endpoint failed: status " << response.status_code << std::endl;
        return false;
    }

    try {
        nlohmann::json models = nlohmann::json::parse(response.body);
        if (models["object"] != "list" || !models.contains("data")) {
            std::cerr << "❌ Models response invalid format" << std::endl;
            return false;
        }
    } catch (const std::exception& e) {
        std::cerr << "❌ Failed to parse models response: " << e.what() << std::endl;
        return false;
    }

    std::cout << "✅ Models endpoint working" << std::endl;
    return true;
}

bool test_messages_endpoint() {
    std::cout << "Testing messages endpoint..." << std::endl;

    nlohmann::json request_body = {
        {"model", "claude-3-sonnet-20240229"},
        {"max_tokens", 100},
        {"messages", {
            {
                {"role", "user"},
                {"content", "Hello! Please respond with a short greeting."}
            }
        }}
    };

    std::map<std::string, std::string> headers = {
        {"Content-Type", "application/json"},
        {"anthropic-version", "2023-06-01"}
    };

    HttpResponse response = make_http_request(
        GATEWAY_URL + "/anthropic/v1/messages",
        "POST",
        request_body.dump(),
        headers
    );

    if (response.status_code != 200) {
        std::cerr << "❌ Messages endpoint failed: status " << response.status_code << std::endl;
        std::cerr << "Response body: " << response.body << std::endl;
        return false;
    }

    try {
        nlohmann::json resp = nlohmann::json::parse(response.body);
        if (!resp.contains("content") || !resp.contains("role")) {
            std::cerr << "❌ Messages response invalid format" << std::endl;
            std::cerr << "Response: " << response.body << std::endl;
            return false;
        }
    } catch (const std::exception& e) {
        std::cerr << "❌ Failed to parse messages response: " << e.what() << std::endl;
        return false;
    }

    std::cout << "✅ Messages endpoint working" << std::endl;
    return true;
}

bool test_providers_endpoint() {
    std::cout << "Testing providers endpoint..." << std::endl;

    HttpResponse response = make_http_request(GATEWAY_URL + "/providers");

    if (response.status_code != 200) {
        std::cerr << "❌ Providers endpoint failed: status " << response.status_code << std::endl;
        return false;
    }

    try {
        nlohmann::json providers = nlohmann::json::parse(response.body);
        if (!providers.contains("providers") || !providers.contains("healthy")) {
            std::cerr << "❌ Providers response missing required fields" << std::endl;
            return false;
        }
    } catch (const std::exception& e) {
        std::cerr << "❌ Failed to parse providers response: " << e.what() << std::endl;
        return false;
    }

    std::cout << "✅ Providers endpoint working" << std::endl;
    return true;
}

bool test_error_handling() {
    std::cout << "Testing error handling..." << std::endl;

    // Test invalid JSON
    HttpResponse response = make_http_request(
        GATEWAY_URL + "/anthropic/v1/messages",
        "POST",
        "invalid json {",
        {{"Content-Type", "application/json"}}
    );

    if (response.status_code != 400) {
        std::cerr << "❌ Invalid JSON should return 400, got: " << response.status_code << std::endl;
        return false;
    }

    // Test missing required fields
    nlohmann::json incomplete_request = {
        {"model", "claude-3-sonnet-20240229"}
        // Missing messages
    };

    response = make_http_request(
        GATEWAY_URL + "/anthropic/v1/messages",
        "POST",
        incomplete_request.dump(),
        {{"Content-Type", "application/json"}}
    );

    if (response.status_code != 400) {
        std::cerr << "❌ Missing messages should return 400, got: " << response.status_code << std::endl;
        return false;
    }

    std::cout << "✅ Error handling working" << std::endl;
    return true;
}

bool test_cors_headers() {
    std::cout << "Testing CORS headers..." << std::endl;

    HttpResponse response = make_http_request(GATEWAY_URL + "/health");

 auto it = response.headers.find("Access-Control-Allow-Origin");
    if (it == response.headers.end()) {
        std::cerr << "❌ Missing CORS header: Access-Control-Allow-Origin" << std::endl;
        return false;
    }

    std::cout << "✅ CORS headers present" << std::endl;
    return true;
}

void print_test_summary(const std::vector<bool>& results) {
    int passed = 0;
    int total = results.size();

    for (bool result : results) {
        if (result) passed++;
    }

    std::cout << "" << std::endl;
    std::cout << "╔══════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║                        Test Summary                        ║" << std::endl;
    std::cout << "╚══════════════════════════════════════════════════════════════╝" << std::endl;
    std::cout << "" << std::endl;
    std::cout << "Tests Passed: " << passed << "/" << total << std::endl;
    std::cout << "Success Rate: " << (passed * 100 / total) << "%" << std::endl;

    if (passed == total) {
        std::cout << "" << std::endl;
        std::cout << "🎉 All tests passed! ClaudeGateway V3.2 is working correctly!" << std::endl;
    } else {
        std::cout << "" << std::endl;
        std::cout << "⚠️  Some tests failed. Please check the output above for details." << std::endl;
    }
}

int main(int argc, char* argv[]) {
    std::string gateway_url = GATEWAY_URL;

    // Allow custom gateway URL
    if (argc > 1) {
        gateway_url = argv[1];
    }

    std::cout << "" << std::endl;
    std::cout << "╔══════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║              ClaudeGateway V3.2 Test Suite                 ║" << std::endl;
    std::cout << "║                  Single Unified Endpoint                   ║" << std::endl;
    std::cout << "╚══════════════════════════════════════════════════════════════╝" << std::endl;
    std::cout << "" << std::endl;
    std::cout << "Testing Gateway URL: " << gateway_url << std::endl;
    std::cout << "" << std::endl;

    // Initialize curl
    curl_global_init(CURL_GLOBAL_DEFAULT);

    std::vector<bool> test_results;

    // Run tests
    test_results.push_back(test_health_endpoint());
    test_results.push_back(test_metrics_endpoint());
    test_results.push_back(test_models_endpoint());
    test_results.push_back(test_messages_endpoint());
    test_results.push_back(test_providers_endpoint());
    test_results.push_back(test_error_handling());
    test_results.push_back(test_cors_headers());

    // Print summary
    print_test_summary(test_results);

    // Cleanup
    curl_global_cleanup();

    return std::all_of(test_results.begin(), test_results.end(), [](bool result) { return result; }) ? 0 : 1;
}