#include "aimux/providers/provider_model_query.hpp"
#include "aimux/providers/anthropic_model_query.hpp"
#include "aimux/providers/openai_model_query.hpp"
#include "aimux/providers/cerebras_model_query.hpp"
#include <stdexcept>

namespace aimux {
namespace providers {

std::unique_ptr<ProviderModelQuery> create_provider_query(
    const std::string& provider_name,
    const std::string& api_key) {

    if (provider_name == "anthropic") {
        return std::make_unique<AnthropicModelQuery>(api_key);
    } else if (provider_name == "openai") {
        return std::make_unique<OpenAIModelQuery>(api_key);
    } else if (provider_name == "cerebras") {
        return std::make_unique<CerebrasModelQuery>(api_key);
    } else {
        throw std::runtime_error("Unknown provider: " + provider_name);
    }
}

} // namespace providers
} // namespace aimux
