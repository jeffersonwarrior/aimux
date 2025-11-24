#pragma once

#include "aimux/providers/provider_model_query.hpp"
#include <chrono>
#include <mutex>

namespace aimux {
namespace providers {

/**
 * @brief Cerebras-specific implementation of ProviderModelQuery
 *
 * Queries the Cerebras API (https://api.cerebras.ai/v1/models)
 * to retrieve available Llama models.
 *
 * Response format example:
 * {
 *   "data": [
 *     {
 *       "id": "llama3.1-8b",
 *       "created": 1234567890,
 *       "owned_by": "cerebras"
 *     },
 *     ...
 *   ]
 * }
 */
class CerebrasModelQuery : public ProviderModelQuery {
public:
    /**
     * @brief Constructor
     * @param api_key Cerebras API key (from .env: CEREBRAS_API_KEY)
     */
    explicit CerebrasModelQuery(const std::string& api_key);

    /**
     * @brief Query Cerebras API for available models
     * @return Vector of ModelInfo for Llama models
     * @throws std::runtime_error if API query fails
     */
    std::vector<core::ModelRegistry::ModelInfo> get_available_models() override;

    /**
     * @brief Get provider name
     * @return "cerebras"
     */
    std::string get_provider_name() const override { return "cerebras"; }

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
     * @brief Make HTTP GET request to Cerebras API
     * @return JSON response as string
     * @throws std::runtime_error on HTTP error
     */
    std::string query_api();

    /**
     * @brief Parse Cerebras API response JSON
     * @param json_response JSON string from API
     * @return Vector of ModelInfo
     */
    std::vector<core::ModelRegistry::ModelInfo> parse_response(const std::string& json_response);

    /**
     * @brief Extract version from model ID
     * Examples:
     *   "llama3.1-8b" -> "3.1"
     *   "llama3.1-70b" -> "3.1"
     *   "llama-2-7b" -> "2.0"
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
