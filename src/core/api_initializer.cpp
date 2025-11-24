/**
 * @file api_initializer.cpp
 * @brief Implementation of APIInitializer - Phase 3 of v3.0 Model Discovery
 *
 * Author: Claude Code (AI Agent)
 * Date: November 24, 2025
 */

#include "aimux/core/api_initializer.hpp"
#include "aimux/providers/anthropic_model_query.hpp"
#include "aimux/providers/openai_model_query.hpp"
#include "aimux/providers/cerebras_model_query.hpp"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <cstdlib>
#include <sstream>
#include <iostream>
#include <chrono>

namespace aimux {
namespace core {

// ============================================================================
// Static Member Initialization
// ============================================================================

APIInitializer::InitResult APIInitializer::cached_result_;
std::chrono::system_clock::time_point APIInitializer::cache_timestamp_ =
    std::chrono::system_clock::time_point::min();
std::mutex APIInitializer::cache_mutex_;

// ============================================================================
// CURL Helper for Validation
// ============================================================================

static size_t api_init_curl_write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

// ============================================================================
// Public API
// ============================================================================

APIInitializer::InitResult APIInitializer::initialize_all_providers() {
    auto start_time = std::chrono::high_resolution_clock::now();

    InitResult result;

    // Check cache first
    if (has_valid_cache()) {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        result = cached_result_;
        std::cout << "INFO: Using cached model discovery results (age: "
                  << std::chrono::duration_cast<std::chrono::hours>(
                         std::chrono::system_clock::now() - cache_timestamp_).count()
                  << " hours)" << std::endl;
        return result;
    }

    std::cout << "INFO: Starting model discovery for all providers..." << std::endl;

    // Initialize each provider
    std::vector<std::string> providers = {"anthropic", "openai", "cerebras"};

    for (const auto& provider : providers) {
        try {
            auto provider_result = initialize_provider(provider);

            // Merge results
            if (!provider_result.selected_models.empty()) {
                result.selected_models.insert(
                    provider_result.selected_models.begin(),
                    provider_result.selected_models.end());
            }
            result.validation_results.insert(
                provider_result.validation_results.begin(),
                provider_result.validation_results.end());
            result.error_messages.insert(
                provider_result.error_messages.begin(),
                provider_result.error_messages.end());
            result.used_fallback.insert(
                provider_result.used_fallback.begin(),
                provider_result.used_fallback.end());

        } catch (const std::exception& e) {
            std::cerr << "ERROR: Failed to initialize " << provider
                      << ": " << e.what() << std::endl;
            result.validation_results[provider] = false;
            result.error_messages[provider] = e.what();
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    result.total_time_ms = std::chrono::duration<double, std::milli>(
        end_time - start_time).count();

    std::cout << "INFO: Model discovery completed in "
              << result.total_time_ms << " ms" << std::endl;
    std::cout << result.summary() << std::endl;

    // Cache result
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        cached_result_ = result;
        cache_timestamp_ = std::chrono::system_clock::now();
    }

    return result;
}

APIInitializer::InitResult APIInitializer::initialize_provider(const std::string& provider) {
    InitResult result;
    auto start_time = std::chrono::high_resolution_clock::now();

    std::cout << "INFO: Initializing provider: " << provider << std::endl;

    // Load API key
    std::string api_key = load_api_key(provider);
    if (api_key.empty()) {
        std::cerr << "WARNING: No API key found for " << provider
                  << ", using fallback model" << std::endl;
        auto fallback = select_fallback_model(provider);
        result.selected_models[provider] = fallback;
        result.validation_results[provider] = false;
        result.error_messages[provider] = "No API key found";
        result.used_fallback[provider] = true;
        return result;
    }

    // Query API for models
    std::vector<ModelRegistry::ModelInfo> models;
    try {
        models = query_provider_models(provider, api_key);
        std::cout << "INFO: Found " << models.size() << " models for "
                  << provider << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "WARNING: Failed to query " << provider << " API: "
                  << e.what() << ", using fallback model" << std::endl;
        auto fallback = select_fallback_model(provider);
        result.selected_models[provider] = fallback;
        result.validation_results[provider] = false;
        result.error_messages[provider] = std::string("API query failed: ") + e.what();
        result.used_fallback[provider] = true;
        return result;
    }

    if (models.empty()) {
        std::cerr << "WARNING: No models found for " << provider
                  << ", using fallback model" << std::endl;
        auto fallback = select_fallback_model(provider);
        result.selected_models[provider] = fallback;
        result.validation_results[provider] = false;
        result.error_messages[provider] = "No models available";
        result.used_fallback[provider] = true;
        return result;
    }

    // Select latest model
    auto latest_model = ModelRegistry::select_latest(models);
    std::cout << "INFO: Selected latest model: " << latest_model.model_id
              << " (version " << latest_model.version << ")" << std::endl;

    // Validate model with test call
    std::cout << "INFO: Validating model with test API call..." << std::endl;
    bool validation_success = validate_model_with_test_call(provider, latest_model, api_key);

    if (validation_success) {
        std::cout << "INFO: Validation succeeded for " << latest_model.model_id << std::endl;
        result.selected_models[provider] = latest_model;
        result.validation_results[provider] = true;
        result.used_fallback[provider] = false;

        // Add to global registry
        auto& registry = ModelRegistry::instance();
        registry.add_model(latest_model);

    } else {
        std::cerr << "WARNING: Validation failed for " << latest_model.model_id
                  << ", using fallback model" << std::endl;
        auto fallback = select_fallback_model(provider);
        result.selected_models[provider] = fallback;
        result.validation_results[provider] = false;
        result.error_messages[provider] = "Model validation failed";
        result.used_fallback[provider] = true;

        // Add fallback to global registry
        auto& registry = ModelRegistry::instance();
        registry.add_model(fallback);
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(
        end_time - start_time).count();

    std::cout << "INFO: Provider " << provider << " initialized in "
              << elapsed_ms << " ms" << std::endl;

    return result;
}

APIInitializer::InitResult APIInitializer::get_cached_result() {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    return cached_result_;
}

bool APIInitializer::has_valid_cache() {
    std::lock_guard<std::mutex> lock(cache_mutex_);

    if (cached_result_.selected_models.empty()) {
        return false;
    }

    auto now = std::chrono::system_clock::now();
    auto cache_age = std::chrono::duration_cast<std::chrono::hours>(now - cache_timestamp_);

    return cache_age.count() < CACHE_TTL_HOURS;
}

void APIInitializer::clear_cache() {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    cached_result_ = InitResult{};
    cache_timestamp_ = std::chrono::system_clock::time_point::min();
}

std::string APIInitializer::InitResult::summary() const {
    std::ostringstream oss;
    oss << "\n=== Model Discovery Summary ===\n";

    for (const auto& [provider, model] : selected_models) {
        oss << "  " << provider << ": " << model.model_id
            << " (v" << model.version << ")";

        // Check if validation result exists
        auto val_it = validation_results.find(provider);
        if (val_it != validation_results.end() && val_it->second) {
            oss << " [VALIDATED]";
        } else {
            oss << " [FALLBACK]";
        }

        // Check if error message exists
        auto err_it = error_messages.find(provider);
        if (err_it != error_messages.end() && !err_it->second.empty()) {
            auto val_check = validation_results.find(provider);
            if (val_check == validation_results.end() || !val_check->second) {
                oss << " - " << err_it->second;
            }
        }

        oss << "\n";
    }

    oss << "  Total time: " << total_time_ms << " ms\n";
    oss << "================================\n";

    return oss.str();
}

// ============================================================================
// Private Implementation
// ============================================================================

std::vector<ModelRegistry::ModelInfo> APIInitializer::query_provider_models(
    const std::string& provider,
    const std::string& api_key) {

    if (provider == "anthropic") {
        providers::AnthropicModelQuery query(api_key);
        return query.get_available_models();

    } else if (provider == "openai") {
        providers::OpenAIModelQuery query(api_key);
        return query.get_available_models();

    } else if (provider == "cerebras") {
        providers::CerebrasModelQuery query(api_key);
        return query.get_available_models();

    } else {
        throw std::runtime_error("Unknown provider: " + provider);
    }
}

bool APIInitializer::validate_model_with_test_call(
    const std::string& provider,
    const ModelRegistry::ModelInfo& model,
    const std::string& api_key) {

    if (provider == "anthropic") {
        // Test Anthropic with simple messages request + tool
        nlohmann::json payload = {
            {"model", model.model_id},
            {"max_tokens", 100},
            {"messages", nlohmann::json::array({
                {{"role", "user"}, {"content", "Say hello"}}
            })},
            {"tools", nlohmann::json::array({
                {
                    {"name", "test_function"},
                    {"description", "Test function"},
                    {"input_schema", {
                        {"type", "object"},
                        {"properties", {}},
                        {"required", nlohmann::json::array()}
                    }}
                }
            })}
        };

        std::vector<std::string> headers = {
            "x-api-key: " + api_key,
            "anthropic-version: 2023-06-01"
        };

        return http_post_validation(
            "https://api.anthropic.com/v1/messages",
            payload.dump(),
            headers);

    } else if (provider == "openai") {
        // Test OpenAI with simple chat completion + function
        nlohmann::json payload = {
            {"model", model.model_id},
            {"messages", nlohmann::json::array({
                {{"role", "user"}, {"content", "Say hello"}}
            })},
            {"max_tokens", 100},
            {"functions", nlohmann::json::array({
                {
                    {"name", "test_function"},
                    {"description", "Test function"},
                    {"parameters", {
                        {"type", "object"},
                        {"properties", {}},
                        {"required", nlohmann::json::array()}
                    }}
                }
            })}
        };

        std::vector<std::string> headers = {
            "Authorization: Bearer " + api_key
        };

        return http_post_validation(
            "https://api.openai.com/v1/chat/completions",
            payload.dump(),
            headers);

    } else if (provider == "cerebras") {
        // Test Cerebras with simple completion
        nlohmann::json payload = {
            {"model", model.model_id},
            {"messages", nlohmann::json::array({
                {{"role", "user"}, {"content", "Say hello"}}
            })},
            {"max_tokens", 100}
        };

        std::vector<std::string> headers = {
            "Authorization: Bearer " + api_key
        };

        return http_post_validation(
            "https://api.cerebras.ai/v1/chat/completions",
            payload.dump(),
            headers);

    } else {
        return false;
    }
}

ModelRegistry::ModelInfo APIInitializer::select_fallback_model(const std::string& provider) {
    if (provider == "anthropic") {
        return {
            "anthropic",
            "claude-3-5-sonnet-20241022",
            "3.5",
            "2024-10-22",
            true
        };
    } else if (provider == "openai") {
        return {
            "openai",
            "gpt-4o",
            "4.2",
            "2024-05-13",
            true
        };
    } else if (provider == "cerebras") {
        return {
            "cerebras",
            "llama3.1-8b",
            "3.1",
            "2024-07-01",
            true
        };
    } else {
        throw std::runtime_error("Unknown provider: " + provider);
    }
}

std::string APIInitializer::load_api_key(const std::string& provider) {
    std::string env_var;

    if (provider == "anthropic") {
        env_var = "ANTHROPIC_API_KEY";
    } else if (provider == "openai") {
        env_var = "OPENAI_API_KEY";
    } else if (provider == "cerebras") {
        env_var = "CEREBRAS_API_KEY";
    } else {
        return "";
    }

    const char* key = std::getenv(env_var.c_str());
    return key ? std::string(key) : "";
}

bool APIInitializer::http_post_validation(
    const std::string& url,
    const std::string& payload,
    const std::vector<std::string>& headers) {

    CURL* curl = curl_easy_init();
    if (!curl) {
        return false;
    }

    std::string response;

    struct curl_slist* curl_headers = nullptr;
    for (const auto& header : headers) {
        curl_headers = curl_slist_append(curl_headers, header.c_str());
    }
    curl_headers = curl_slist_append(curl_headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, curl_headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, api_init_curl_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, static_cast<long>(VALIDATION_TIMEOUT_SECONDS));
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_slist_free_all(curl_headers);
    curl_easy_cleanup(curl);

    // Success if CURL succeeded and HTTP 200
    return (res == CURLE_OK && http_code == 200);
}

} // namespace core
} // namespace aimux
