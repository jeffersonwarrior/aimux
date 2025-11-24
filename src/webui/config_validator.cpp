/**
 * @file config_validator.cpp
 * @brief Implementation of ConfigValidator for prettifier configuration validation
 */

#include "aimux/webui/config_validator.hpp"
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace aimux {
namespace webui {

// =============================================================================
// Buffer Size Validation
// =============================================================================

ConfigValidator::ValidationResult ConfigValidator::validate_buffer_size(int size_kb) {
    if (size_kb < MIN_BUFFER_SIZE_KB) {
        return ValidationResult(false,
            "Buffer size must be at least " + std::to_string(MIN_BUFFER_SIZE_KB) + "KB",
            "max_buffer_size_kb");
    }

    if (size_kb > MAX_BUFFER_SIZE_KB) {
        return ValidationResult(false,
            "Buffer size must not exceed " + std::to_string(MAX_BUFFER_SIZE_KB) + "KB",
            "max_buffer_size_kb");
    }

    return ValidationResult(true);
}

// =============================================================================
// Timeout Validation
// =============================================================================

ConfigValidator::ValidationResult ConfigValidator::validate_timeout(int timeout_ms) {
    if (timeout_ms < MIN_TIMEOUT_MS) {
        return ValidationResult(false,
            "Timeout must be at least " + std::to_string(MIN_TIMEOUT_MS) + "ms",
            "timeout_ms");
    }

    if (timeout_ms > MAX_TIMEOUT_MS) {
        return ValidationResult(false,
            "Timeout must not exceed " + std::to_string(MAX_TIMEOUT_MS) + "ms",
            "timeout_ms");
    }

    return ValidationResult(true);
}

// =============================================================================
// Provider and Format Validation
// =============================================================================

bool ConfigValidator::is_valid_provider(const std::string& provider) const {
    static const std::unordered_set<std::string> valid_providers = {
        "anthropic", "openai", "cerebras", "synthetic"
    };

    return valid_providers.find(provider) != valid_providers.end();
}

bool ConfigValidator::is_valid_format_for_provider(const std::string& provider, const std::string& format) const {
    static const std::unordered_map<std::string, std::unordered_set<std::string>> provider_formats = {
        {"anthropic", {
            "json-tool-use",
            "xml-tool-calls",
            "thinking-blocks",
            "reasoning-traces"
        }},
        {"openai", {
            "chat-completion",
            "function-calling",
            "structured-output"
        }},
        {"cerebras", {
            "speed-optimized",
            "standard"
        }},
        {"synthetic", {
            "diagnostic",
            "standard"
        }}
    };

    auto provider_it = provider_formats.find(provider);
    if (provider_it == provider_formats.end()) {
        return false;
    }

    const auto& formats = provider_it->second;
    return formats.find(format) != formats.end();
}

ConfigValidator::ValidationResult ConfigValidator::validate_format_preference(
    const std::string& provider,
    const std::string& format) {

    if (!is_valid_provider(provider)) {
        return ValidationResult(false,
            "Unknown provider: " + provider,
            "format_preferences." + provider);
    }

    if (!is_valid_format_for_provider(provider, format)) {
        return ValidationResult(false,
            "Invalid format '" + format + "' for provider '" + provider + "'",
            "format_preferences." + provider);
    }

    return ValidationResult(true);
}

// =============================================================================
// Cross-Field Compatibility Validation
// =============================================================================

ConfigValidator::ValidationResult ConfigValidator::validate_compatibility(const nlohmann::json& config) {
    // Check if config is valid JSON object
    if (!config.is_object()) {
        return ValidationResult(false, "Configuration must be a JSON object", "");
    }

    // Check streaming + timeout compatibility
    if (config.contains("streaming_enabled") && config.contains("timeout_ms")) {
        try {
            bool streaming_enabled = config["streaming_enabled"].get<bool>();
            int timeout_ms = config["timeout_ms"].get<int>();

            if (streaming_enabled && timeout_ms < MIN_STREAMING_TIMEOUT_MS) {
                return ValidationResult(false,
                    "Streaming mode requires timeout of at least " +
                    std::to_string(MIN_STREAMING_TIMEOUT_MS) + "ms (got " +
                    std::to_string(timeout_ms) + "ms)",
                    "timeout_ms");
            }
        } catch (const nlohmann::json::exception& e) {
            return ValidationResult(false,
                "Invalid type for streaming_enabled or timeout_ms",
                "compatibility");
        }
    }

    // All compatibility checks passed
    return ValidationResult(true);
}

// =============================================================================
// Full Configuration Validation
// =============================================================================

ConfigValidator::ValidationResult ConfigValidator::validate_config(const nlohmann::json& config) {
    // Handle null or non-object config
    if (config.is_null()) {
        return ValidationResult(false, "Configuration cannot be null", "");
    }

    if (!config.is_object()) {
        return ValidationResult(false, "Configuration must be a JSON object", "");
    }

    // Empty config is valid (no changes)
    if (config.empty()) {
        return ValidationResult(true);
    }

    // Validate buffer size if present
    if (config.contains("max_buffer_size_kb")) {
        try {
            int buffer_size = config["max_buffer_size_kb"].get<int>();
            auto result = validate_buffer_size(buffer_size);
            if (!result.valid) {
                return result;
            }
        } catch (const nlohmann::json::exception& e) {
            return ValidationResult(false,
                "Invalid type for max_buffer_size_kb (expected integer)",
                "max_buffer_size_kb");
        }
    }

    // Validate timeout if present
    if (config.contains("timeout_ms")) {
        try {
            int timeout = config["timeout_ms"].get<int>();
            auto result = validate_timeout(timeout);
            if (!result.valid) {
                return result;
            }
        } catch (const nlohmann::json::exception& e) {
            return ValidationResult(false,
                "Invalid type for timeout_ms (expected integer)",
                "timeout_ms");
        }
    }

    // Validate enabled flag if present
    if (config.contains("enabled")) {
        if (!config["enabled"].is_boolean()) {
            return ValidationResult(false,
                "Invalid type for enabled (expected boolean)",
                "enabled");
        }
    }

    // Validate streaming_enabled flag if present
    if (config.contains("streaming_enabled")) {
        if (!config["streaming_enabled"].is_boolean()) {
            return ValidationResult(false,
                "Invalid type for streaming_enabled (expected boolean)",
                "streaming_enabled");
        }
    }

    // Validate format preferences if present
    if (config.contains("format_preferences")) {
        const auto& prefs = config["format_preferences"];

        if (!prefs.is_object()) {
            return ValidationResult(false,
                "format_preferences must be a JSON object",
                "format_preferences");
        }

        for (auto it = prefs.begin(); it != prefs.end(); ++it) {
            std::string provider = it.key();
            std::string format;

            try {
                format = it.value().get<std::string>();
            } catch (const nlohmann::json::exception& e) {
                return ValidationResult(false,
                    "Format preference for " + provider + " must be a string",
                    "format_preferences." + provider);
            }

            auto result = validate_format_preference(provider, format);
            if (!result.valid) {
                return result;
            }
        }
    }

    // Validate cross-field compatibility
    auto compat_result = validate_compatibility(config);
    if (!compat_result.valid) {
        return compat_result;
    }

    // All validations passed
    return ValidationResult(true);
}

} // namespace webui
} // namespace aimux
