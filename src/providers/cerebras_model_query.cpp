#include "aimux/providers/cerebras_model_query.hpp"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <regex>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <ctime>

namespace aimux {
namespace providers {

// ============================================================================
// CURL Callback
// ============================================================================

static size_t write_callback_cerebras(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

// ============================================================================
// Constructor
// ============================================================================

CerebrasModelQuery::CerebrasModelQuery(const std::string& api_key)
    : api_key_(api_key),
      cache_timestamp_(std::chrono::system_clock::time_point::min()) {
}

// ============================================================================
// Cache Management
// ============================================================================

bool CerebrasModelQuery::has_valid_cache() const {
    std::lock_guard<std::mutex> lock(cache_mutex_);

    if (cached_models_.empty()) {
        return false;
    }

    auto now = std::chrono::system_clock::now();
    auto cache_age = std::chrono::duration_cast<std::chrono::hours>(now - cache_timestamp_);

    return cache_age.count() < CACHE_TTL_HOURS;
}

void CerebrasModelQuery::clear_cache() {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    cached_models_.clear();
    cache_timestamp_ = std::chrono::system_clock::time_point::min();
}

// ============================================================================
// Version Extraction
// ============================================================================

std::string CerebrasModelQuery::extract_version(const std::string& model_id) {
    // Pattern: llama{major}.{minor}-{size}
    // Examples:
    //   "llama3.1-8b" -> "3.1"
    //   "llama3.1-70b" -> "3.1"
    //   "llama-2-7b" -> "2.0"

    std::regex version_regex(R"(llama[- ]?(\d+)\.?(\d+)?)");
    std::smatch match;

    if (std::regex_search(model_id, match, version_regex)) {
        int major = std::stoi(match[1].str());
        int minor = match[2].matched ? std::stoi(match[2].str()) : 0;

        return std::to_string(major) + "." + std::to_string(minor);
    }

    // Fallback
    return "1.0";
}

// ============================================================================
// Timestamp Conversion
// ============================================================================

std::string CerebrasModelQuery::timestamp_to_date(int64_t timestamp) {
    std::time_t time = static_cast<std::time_t>(timestamp);
    std::tm* tm_info = std::gmtime(&time);

    std::ostringstream oss;
    oss << std::put_time(tm_info, "%Y-%m-%d");
    return oss.str();
}

// ============================================================================
// API Query
// ============================================================================

std::string CerebrasModelQuery::query_api() {
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL");
    }

    std::string response_data;

    // Set URL
    curl_easy_setopt(curl, CURLOPT_URL, "https://api.cerebras.ai/v1/models");

    // Set headers
    struct curl_slist* headers = nullptr;
    std::string auth_header = "Authorization: Bearer " + api_key_;

    headers = curl_slist_append(headers, auth_header.c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    // Set write callback
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback_cerebras);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);

    // Set timeout
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    // Perform request
    CURLcode res = curl_easy_perform(curl);

    // Get HTTP response code
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    // Cleanup
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        throw std::runtime_error(std::string("CURL error: ") + curl_easy_strerror(res));
    }

    if (http_code != 200) {
        throw std::runtime_error("HTTP error " + std::to_string(http_code) + ": " + response_data);
    }

    return response_data;
}

// ============================================================================
// Response Parsing
// ============================================================================

std::vector<core::ModelRegistry::ModelInfo> CerebrasModelQuery::parse_response(
    const std::string& json_response) {

    std::vector<core::ModelRegistry::ModelInfo> models;

    try {
        nlohmann::json response = nlohmann::json::parse(json_response);

        // Check if response has "data" array
        if (!response.contains("data") || !response["data"].is_array()) {
            throw std::runtime_error("Invalid response format: missing 'data' array");
        }

        for (const auto& model_json : response["data"]) {
            // Extract fields
            std::string model_id = model_json.value("id", "");
            int64_t created_timestamp = model_json.value("created", 0);

            if (model_id.empty()) {
                continue;
            }

            // Extract version from model ID
            std::string version = extract_version(model_id);

            // Convert timestamp to date
            std::string release_date = timestamp_to_date(created_timestamp);

            // Create ModelInfo
            core::ModelRegistry::ModelInfo info{
                "cerebras",
                model_id,
                version,
                release_date,
                true  // Mark as available
            };

            models.push_back(info);
        }

        // Sort by version (descending - highest version first)
        std::sort(models.begin(), models.end(),
            [](const core::ModelRegistry::ModelInfo& a, const core::ModelRegistry::ModelInfo& b) {
                return core::ModelRegistry::compare_versions(a.version, b.version) > 0;
            }
        );

    } catch (const nlohmann::json::exception& e) {
        throw std::runtime_error(std::string("JSON parse error: ") + e.what());
    }

    return models;
}

// ============================================================================
// Main API
// ============================================================================

std::vector<core::ModelRegistry::ModelInfo> CerebrasModelQuery::get_available_models() {
    // Check cache first
    if (has_valid_cache()) {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        return cached_models_;
    }

    // Query API
    std::string response = query_api();

    // Parse response
    auto models = parse_response(response);

    // Update cache
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        cached_models_ = models;
        cache_timestamp_ = std::chrono::system_clock::now();
    }

    return models;
}

} // namespace providers
} // namespace aimux
