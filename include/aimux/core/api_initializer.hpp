/**
 * @file api_initializer.hpp
 * @brief APIInitializer - Phase 3 of v3.0 Model Discovery System
 *
 * The APIInitializer orchestrates the model discovery and validation pipeline:
 * 1. Query all provider APIs for available models
 * 2. Select latest stable version for each provider
 * 3. Validate models with test API calls
 * 4. Fall back to known stable versions on validation failure
 * 5. Cache results for 24-hour TTL
 *
 * Architecture:
 * - Uses ProviderModelQuery implementations for API queries
 * - Uses ModelRegistry for version comparison and selection
 * - Makes test API calls to validate model availability
 * - Returns InitResult with selected models and validation status
 *
 * Author: Claude Code (AI Agent)
 * Date: November 24, 2025
 * Status: Phase 3 - APIInitializer Implementation
 */

#pragma once

#include "aimux/core/model_registry.hpp"
#include "aimux/providers/provider_model_query.hpp"
#include <memory>
#include <map>
#include <string>
#include <vector>
#include <mutex>
#include <chrono>

namespace aimux {
namespace core {

/**
 * @brief APIInitializer orchestrates model discovery and validation
 *
 * Thread-safe singleton that queries provider APIs, selects latest models,
 * validates them with test calls, and caches results.
 */
class APIInitializer {
public:
    /**
     * @brief Result of initialization for one or more providers
     */
    struct InitResult {
        /// Selected model for each provider (provider_name -> ModelInfo)
        std::map<std::string, ModelRegistry::ModelInfo> selected_models;

        /// Validation result for each provider (provider_name -> success/failure)
        std::map<std::string, bool> validation_results;

        /// Error messages for failed initializations (provider_name -> error)
        std::map<std::string, std::string> error_messages;

        /// Fallback status for each provider (true if fallback was used)
        std::map<std::string, bool> used_fallback;

        /// Total time taken for initialization (ms)
        double total_time_ms = 0.0;

        /**
         * @brief Check if initialization was successful for all providers
         * @return true if at least one provider succeeded
         */
        bool has_success() const {
            for (const auto& [provider, success] : validation_results) {
                if (success) return true;
            }
            return false;
        }

        /**
         * @brief Get summary string for logging
         */
        std::string summary() const;
    };

    /**
     * @brief Initialize all configured providers (Anthropic, OpenAI, Cerebras)
     *
     * This method:
     * 1. Loads API keys from environment variables
     * 2. Queries each provider API for available models
     * 3. Selects latest stable version using ModelRegistry
     * 4. Validates each model with a test API call
     * 5. Falls back to known stable version on failure
     * 6. Caches results
     *
     * @return InitResult with selected models and validation status
     */
    static InitResult initialize_all_providers();

    /**
     * @brief Initialize a specific provider
     *
     * @param provider Provider name ("anthropic", "openai", or "cerebras")
     * @return InitResult for single provider
     * @throws std::runtime_error if provider name is invalid
     */
    static InitResult initialize_provider(const std::string& provider);

    /**
     * @brief Get cached initialization result
     *
     * Returns cached result if available and not expired (24-hour TTL).
     * Returns empty InitResult if no cache or cache expired.
     *
     * @return Cached InitResult or empty if not available
     */
    static InitResult get_cached_result();

    /**
     * @brief Check if cache is valid (not expired)
     * @return true if cache exists and not expired
     */
    static bool has_valid_cache();

    /**
     * @brief Clear the cache (force re-initialization on next call)
     */
    static void clear_cache();

private:
    /**
     * @brief Query a provider API for available models
     *
     * Creates appropriate ProviderModelQuery instance and retrieves models.
     *
     * @param provider Provider name
     * @param api_key API key for the provider
     * @return Vector of ModelInfo from provider API
     * @throws std::runtime_error on API error
     */
    static std::vector<ModelRegistry::ModelInfo> query_provider_models(
        const std::string& provider,
        const std::string& api_key);

    /**
     * @brief Validate a model with a test API call
     *
     * Makes a simple API call to verify the model is available and responding.
     *
     * Test API Calls:
     * - Anthropic: POST /v1/messages with text + tool use request
     * - OpenAI: POST /v1/chat/completions with function call
     * - Cerebras: POST inference endpoint with test prompt
     *
     * @param provider Provider name
     * @param model Model to validate
     * @param api_key API key for the provider
     * @return true if validation succeeded, false otherwise
     */
    static bool validate_model_with_test_call(
        const std::string& provider,
        const ModelRegistry::ModelInfo& model,
        const std::string& api_key);

    /**
     * @brief Select fallback model for a provider
     *
     * Returns a known stable model as fallback when API query fails
     * or validation fails.
     *
     * Fallback models:
     * - Anthropic: claude-3-5-sonnet-20241022 (3.5)
     * - OpenAI: gpt-4o (4.2)
     * - Cerebras: llama3.1-8b (3.1)
     *
     * @param provider Provider name
     * @return ModelInfo for fallback model
     */
    static ModelRegistry::ModelInfo select_fallback_model(
        const std::string& provider);

    /**
     * @brief Load API key from environment variable
     *
     * @param provider Provider name
     * @return API key or empty string if not found
     */
    static std::string load_api_key(const std::string& provider);

    /**
     * @brief Perform HTTP POST request for validation
     *
     * @param url API endpoint URL
     * @param payload JSON payload
     * @param headers HTTP headers
     * @return true if HTTP 200 response, false otherwise
     */
    static bool http_post_validation(
        const std::string& url,
        const std::string& payload,
        const std::vector<std::string>& headers);

    // Cache management
    static InitResult cached_result_;
    static std::chrono::system_clock::time_point cache_timestamp_;
    static std::mutex cache_mutex_;
    static constexpr int CACHE_TTL_HOURS = 24;

    // Validation timeout (seconds)
    static constexpr int VALIDATION_TIMEOUT_SECONDS = 10;
};

} // namespace core
} // namespace aimux
