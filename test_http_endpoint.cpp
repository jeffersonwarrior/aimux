#include <iostream>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

static std::string response_data;

// Callback function for Curl to write response data
size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total_size = size * nmemb;
    response_data.append((char*)contents, total_size);
    return total_size;
}

int main() {
    std::cout << "Testing HTTP endpoint integration..." << std::endl;

    // Initialize curl
    curl_global_init(CURL_GLOBAL_DEFAULT);
    CURL* curl = curl_easy_init();

    if (!curl) {
        std::cout << "❌ Failed to initialize curl" << std::endl;
        return 1;
    }

    // Test HTTP request to ClaudeGateway
    const char* url = "http://localhost:8080/anthropic/v1/messages";

    // Create request JSON
    nlohmann::json request_data = {
        {"model", "claude-3-sonnet-20240229"},
        {"max_tokens", 100},
        {"messages", nlohmann::json::array({
            {{"role", "user"}, {"content", "Hello, this is a test message!"}}
        })}
    };

    std::string json_string = request_data.dump();
    response_data.clear();

    // Set curl options
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_string.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);

    // Set headers
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "anthropic-version: 2023-06-01");
    headers = curl_slist_append(headers, "Authorization: Bearer dummy-key");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    std::cout << "📤 Sending HTTP request to: " << url << std::endl;
    std::cout << "Request body: " << json_string << std::endl;

    // Perform request
    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        std::cout << "❌ HTTP request failed: " << curl_easy_strerror(res) << std::endl;
        std::cout << "💡 Make sure the ClaudeGateway is running on port 8080" << std::endl;
        std::cout << "   Run: ./build-test/claude_gateway --config config.json" << std::endl;
    } else {
        long response_code;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

        std::cout << "📥 HTTP Response Code: " << response_code << std::endl;
        std::cout << "📥 Response Body: " << response_data << std::endl;

        if (response_code == 200) {
            try {
                auto response_json = nlohmann::json::parse(response_data);
                if (response_json.contains("content") || response_json.contains("choices")) {
                    std::cout << "✅ End-to-end integration test PASSED!" << std::endl;
                } else {
                    std::cout << "⚠️  Response format unexpected but request succeeded" << std::endl;
                }
            } catch (const std::exception& e) {
                std::cout << "⚠️  Failed to parse response JSON: " << e.what() << std::endl;
                std::cout << "⚠️  But HTTP request succeeded" << std::endl;
            }
        } else {
            std::cout << "❌ HTTP request failed with status: " << response_code << std::endl;

            // Check if it's PROVIDER_NOT_FOUND error
            if (response_data.find("PROVIDER_NOT_FOUND") != std::string::npos) {
                std::cout << "❌ CONFIRMED: Still getting PROVIDER_NOT_FOUND error" << std::endl;
                std::cout << "💡 The config.json may not be loading properly in ClaudeGateway" << std::endl;
            }
        }
    }

    // Cleanup
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    curl_global_cleanup();

    return 0;
}