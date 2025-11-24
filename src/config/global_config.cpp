/**
 * @file global_config.cpp
 * @brief Global configuration variables for AIMUX
 *
 * This file defines global configuration variables used across the application.
 */

#include "aimux/core/model_registry.hpp"
#include <map>
#include <string>

namespace aimux {
namespace config {

// Global selected models (populated at startup by model discovery)
std::map<std::string, aimux::core::ModelRegistry::ModelInfo> g_selected_models;

} // namespace config
} // namespace aimux
