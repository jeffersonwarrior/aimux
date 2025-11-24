#pragma once

#include "aimux/providers/provider_model_query.hpp"
#include <chrono>
#include <mutex>

namespace aimux {
namespace providers {

/**
 * @brief Anthropic-specific implementation of ProviderModelQuery
 *
 * Queries the Anthropic API (https://api.anthropic.com/v1/models)
 * to retrieve available Claude models.
 *
 * Response format example:
 * {
 *   "data": [
 *     {
 *       "id": "claude-3-5-sonnet-20241022",
 *       "created_at": "2024-10-22T00:00:00Z",
 *       "type": "model"
 *     },
 *     ...
 *   ]
 * }
 */
class AnthropicModelQuery : public ProviderModelQuery {
public:
    /**
     * @brief Constructor
     * @param api_key Anthropic API key (from .env: ANTHROPIC_API_KEY)
     */
    explicit AnthropicModelQuery(const std::string& api_key);

    /**
     * @brief Query Anthropic API for available models
     * @return Vector of ModelInfo for all Claude models
     * @throws std::runtime_error if API query fails
     */
    std::vector<core::ModelRegistry::ModelInfo> get_available_models() override;

    /**
     * @brief Get provider name
     * @return "anthropic"
     */
    std::string get_provider_name() const override { return "anthropic"; }

    /**
     * @brief Check if cache is valid (24-hour TTL)
     * @return true if cache exists and not expired
     */
    bool has_valid_cache() const override;

    /**
     * @brief Clear the cache
     */
    void clear_cache() override;

private:
    /**
     * @brief Make HTTP GET request to Anthropic API
     * @return JSON response as string
     * @throws std::runtime_error on HTTP error
     */
    std::string query_api();

    /**
     * @brief Parse Anthropic API response JSON
     * @param json_response JSON string from API
     * @return Vector of ModelInfo
     */
    std::vector<core::ModelRegistry::ModelInfo> parse_response(const std::string& json_response);

    /**
     * @brief Extract version from model ID
     * Examples:
     *   "claude-3-5-sonnet-20241022" -> "3.5"
     *   "claude-3-opus-20240229" -> "3.0"
     *   "claude-4-sonnet" -> "4.0"
     * @param model_id Full model identifier
     * @return Semantic version string
     */
    std::string extract_version(const std::string& model_id);

    std::string api_key_;
    std::vector<core::ModelRegistry::ModelInfo> cached_models_;
    std::chrono::system_clock::time_point cache_timestamp_;
    mutable std::mutex cache_mutex_;
    static constexpr int CACHE_TTL_HOURS = 24;
};

} // namespace providers
} // namespace aimux
