#pragma once

#include "aimux/providers/provider_model_query.hpp"
#include <chrono>
#include <mutex>

namespace aimux {
namespace providers {

/**
 * @brief OpenAI-specific implementation of ProviderModelQuery
 *
 * Queries the OpenAI API (https://api.openai.com/v1/models)
 * to retrieve available GPT models.
 *
 * Response format example:
 * {
 *   "data": [
 *     {
 *       "id": "gpt-4-turbo",
 *       "created": 1234567890,
 *       "owned_by": "openai"
 *     },
 *     ...
 *   ]
 * }
 */
class OpenAIModelQuery : public ProviderModelQuery {
public:
    /**
     * @brief Constructor
     * @param api_key OpenAI API key (from .env: OPENAI_API_KEY)
     */
    explicit OpenAIModelQuery(const std::string& api_key);

    /**
     * @brief Query OpenAI API for available models
     * @return Vector of ModelInfo for GPT-4 models only
     * @throws std::runtime_error if API query fails
     */
    std::vector<core::ModelRegistry::ModelInfo> get_available_models() override;

    /**
     * @brief Get provider name
     * @return "openai"
     */
    std::string get_provider_name() const override { return "openai"; }

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
     * @brief Make HTTP GET request to OpenAI API
     * @return JSON response as string
     * @throws std::runtime_error on HTTP error
     */
    std::string query_api();

    /**
     * @brief Parse OpenAI API response JSON
     * @param json_response JSON string from API
     * @return Vector of ModelInfo (filtered to production GPT-4 models)
     */
    std::vector<core::ModelRegistry::ModelInfo> parse_response(const std::string& json_response);

    /**
     * @brief Check if model is a production GPT-4 model
     * Filters out:
     * - GPT-3.5 models
     * - Preview/experimental models
     * - Non-chat models
     * @param model_id Full model identifier
     * @return true if model should be included
     */
    bool is_production_gpt4_model(const std::string& model_id);

    /**
     * @brief Extract version from model ID
     * Examples:
     *   "gpt-4-turbo" -> "4.1"
     *   "gpt-4o" -> "4.2"
     *   "gpt-4" -> "4.0"
     * @param model_id Full model identifier
     * @return Semantic version string
     */
    std::string extract_version(const std::string& model_id);

    /**
     * @brief Convert Unix timestamp to ISO date string
     * @param timestamp Unix timestamp (seconds since epoch)
     * @return Date string in YYYY-MM-DD format
     */
    std::string timestamp_to_date(int64_t timestamp);

    std::string api_key_;
    std::vector<core::ModelRegistry::ModelInfo> cached_models_;
    std::chrono::system_clock::time_point cache_timestamp_;
    mutable std::mutex cache_mutex_;
    static constexpr int CACHE_TTL_HOURS = 24;
};

} // namespace providers
} // namespace aimux
