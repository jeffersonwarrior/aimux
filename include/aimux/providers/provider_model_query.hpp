#pragma once

#include "aimux/core/model_registry.hpp"
#include <string>
#include <vector>
#include <memory>

namespace aimux {
namespace providers {

/**
 * @brief Abstract interface for querying AI provider APIs for available models
 *
 * Each AI provider (Anthropic, OpenAI, Cerebras) implements this interface
 * to query their specific API endpoint and return model information.
 */
class ProviderModelQuery {
public:
    virtual ~ProviderModelQuery() = default;

    /**
     * @brief Query the provider's API for available models
     * @return Vector of ModelInfo for all available models from this provider
     * @throws std::runtime_error if API query fails
     */
    virtual std::vector<core::ModelRegistry::ModelInfo> get_available_models() = 0;

    /**
     * @brief Get the provider name
     * @return Provider identifier (e.g., "anthropic", "openai", "cerebras")
     */
    virtual std::string get_provider_name() const = 0;

    /**
     * @brief Check if the query has cached results
     * @return true if cache is valid and not expired
     */
    virtual bool has_valid_cache() const = 0;

    /**
     * @brief Clear the cache
     */
    virtual void clear_cache() = 0;
};

/**
 * @brief Factory function to create provider query instances
 * @param provider_name Provider identifier (e.g., "anthropic", "openai", "cerebras")
 * @param api_key API key for the provider
 * @return Unique pointer to ProviderModelQuery implementation
 */
std::unique_ptr<ProviderModelQuery> create_provider_query(
    const std::string& provider_name,
    const std::string& api_key);

} // namespace providers
} // namespace aimux
