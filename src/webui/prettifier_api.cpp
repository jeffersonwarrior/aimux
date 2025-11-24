/**
 * @file prettifier_api.cpp
 * @brief Implementation of PrettifierAPI for WebUI REST endpoints
 */

#include "aimux/webui/prettifier_api.hpp"
#include <chrono>
#include <mutex>
#include <sstream>

namespace aimux {
namespace webui {

// =============================================================================
// Construction and Initialization
// =============================================================================

PrettifierAPI::PrettifierAPI(const std::shared_ptr<prettifier::PrettifierPlugin>& plugin)
    : plugin_(plugin),
      validator_(std::make_shared<ConfigValidator>()),
      start_time_(std::chrono::steady_clock::now()) {

    initialize_default_config();
}

void PrettifierAPI::initialize_default_config() {
    std::lock_guard<std::mutex> lock(config_mutex_);

    current_config_ = {
        {"prettifier_enabled", true},
        {"streaming_enabled", true},
        {"security_hardening", true},
        {"max_buffer_size_kb", 1024},
        {"timeout_ms", 5000},
        {"format_preferences", {
            {"anthropic", "json-tool-use"},
            {"openai", "chat-completion"},
            {"cerebras", "speed-optimized"},
            {"synthetic", "diagnostic"}
        }}
    };
}

// =============================================================================
// Status Endpoint Handler
// =============================================================================

nlohmann::json PrettifierAPI::handle_status_request() {
    total_requests_++;
    successful_requests_++;

    try {
        return get_status_json();
    } catch (const std::exception& e) {
        failed_requests_++;
        successful_requests_--;

        return nlohmann::json{
            {"status", "error"},
            {"error", e.what()}
        };
    }
}

nlohmann::json PrettifierAPI::get_status_json() const {
    nlohmann::json response;

    // Basic status
    response["status"] = "enabled";

    // Version info
    if (plugin_) {
        response["version"] = plugin_->version();
    } else {
        response["version"] = "2.1.1";
    }

    // Supported providers
    if (plugin_) {
        response["supported_providers"] = plugin_->supported_providers();
    } else {
        response["supported_providers"] = nlohmann::json::array({
            "anthropic", "openai", "cerebras", "synthetic"
        });
    }

    // Format preferences
    response["format_preferences"] = {
        {"anthropic", get_provider_formats("anthropic")},
        {"openai", get_provider_formats("openai")},
        {"cerebras", get_provider_formats("cerebras")}
    };

    // Performance metrics
    response["performance_metrics"] = get_performance_metrics();

    // Current configuration
    response["configuration"] = get_configuration();

    return response;
}

// =============================================================================
// Config Endpoint Handler
// =============================================================================

nlohmann::json PrettifierAPI::handle_config_request(const nlohmann::json& config) {
    total_requests_++;

    try {
        // Validate the configuration
        auto validation_result = validator_->validate_config(config);

        if (!validation_result.valid) {
            failed_requests_++;

            return nlohmann::json{
                {"success", false},
                {"error", "Invalid configuration"},
                {"details", {
                    {"invalid_field", validation_result.invalid_field},
                    {"reason", validation_result.error_message}
                }}
            };
        }

        // Apply the configuration
        apply_configuration(config);

        successful_requests_++;

        return nlohmann::json{
            {"success", true},
            {"message", "Configuration updated successfully"},
            {"applied_config", config}
        };

    } catch (const std::exception& e) {
        failed_requests_++;

        return nlohmann::json{
            {"success", false},
            {"error", "Configuration update failed"},
            {"details", e.what()}
        };
    }
}

void PrettifierAPI::apply_configuration(const nlohmann::json& config) {
    std::lock_guard<std::mutex> lock(config_mutex_);

    // Merge new config into current config
    for (auto it = config.begin(); it != config.end(); ++it) {
        current_config_[it.key()] = it.value();
    }
}

// =============================================================================
// Helper Methods - Provider Formats
// =============================================================================

nlohmann::json PrettifierAPI::get_provider_formats(const std::string& provider) const {
    nlohmann::json formats;

    if (provider == "anthropic") {
        formats["default_format"] = "json-tool-use";
        formats["available_formats"] = nlohmann::json::array({
            "json-tool-use",
            "xml-tool-calls",
            "thinking-blocks",
            "reasoning-traces"
        });
    }
    else if (provider == "openai") {
        formats["default_format"] = "chat-completion";
        formats["available_formats"] = nlohmann::json::array({
            "chat-completion",
            "function-calling",
            "structured-output"
        });
    }
    else if (provider == "cerebras") {
        formats["default_format"] = "speed-optimized";
        formats["available_formats"] = nlohmann::json::array({
            "speed-optimized",
            "standard"
        });
    }
    else if (provider == "synthetic") {
        formats["default_format"] = "diagnostic";
        formats["available_formats"] = nlohmann::json::array({
            "diagnostic",
            "standard"
        });
    }
    else {
        formats["default_format"] = "unknown";
        formats["available_formats"] = nlohmann::json::array();
    }

    return formats;
}

// =============================================================================
// Helper Methods - Performance Metrics
// =============================================================================

nlohmann::json PrettifierAPI::get_performance_metrics() const {
    nlohmann::json metrics;

    // Simulated/placeholder metrics - in production, these would come from the plugin
    metrics["avg_formatting_time_ms"] = 2.1;
    metrics["throughput_requests_per_second"] = 450;

    // Calculate success rate
    uint64_t total = total_requests_.load();
    uint64_t successful = successful_requests_.load();

    if (total > 0) {
        double success_rate = (static_cast<double>(successful) / total) * 100.0;
        metrics["success_rate_percent"] = success_rate;
    } else {
        metrics["success_rate_percent"] = 100.0;
    }

    // Uptime
    metrics["uptime_seconds"] = get_uptime_seconds();

    return metrics;
}

uint64_t PrettifierAPI::get_uptime_seconds() const {
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_);
    return duration.count();
}

// =============================================================================
// Helper Methods - Configuration
// =============================================================================

nlohmann::json PrettifierAPI::get_configuration() const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return current_config_;
}

} // namespace webui
} // namespace aimux
