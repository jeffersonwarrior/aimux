#pragma once

#include <map>
#include <string>
#include <aimux/core/model_registry.hpp>

namespace aimux {
namespace config {

// Global map of discovered models by provider
// Updated during APIInitializer::initialize_all_providers()
extern std::map<std::string, aimux::core::ModelRegistry::ModelInfo> g_selected_models;

} // namespace config
} // namespace aimux
