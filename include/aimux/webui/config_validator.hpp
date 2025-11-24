/**
 * @file config_validator.hpp
 * @brief Configuration validation for prettifier settings
 *
 * Provides validation logic for prettifier configuration updates via WebUI API.
 * Ensures all configuration values are within acceptable ranges and compatible
 * with each other before applying changes.
 *
 * @version 2.2.0
 * @date 2025-11-24
 */

#pragma once

#include <string>
#include <nlohmann/json.hpp>

namespace aimux {
namespace webui {

/**
 * @brief Validates prettifier configuration before applying changes
 *
 * This class provides comprehensive validation for all prettifier
 * configuration parameters, including:
 * - Buffer size constraints
 * - Timeout ranges
 * - Format preference validation
 * - Cross-field compatibility checks
 */
class ConfigValidator {
public:
    /**
     * @brief Result of a validation operation
     */
    struct ValidationResult {
        bool valid;                    ///< Whether validation passed
        std::string error_message;     ///< Human-readable error description
        std::string invalid_field;     ///< Name of the field that failed validation

        ValidationResult() : valid(true) {}
        ValidationResult(bool v, const std::string& msg = "", const std::string& field = "")
            : valid(v), error_message(msg), invalid_field(field) {}
    };

    ConfigValidator() = default;
    ~ConfigValidator() = default;

    /**
     * @brief Validate complete configuration object
     *
     * Performs comprehensive validation of all configuration fields,
     * including individual field validation and cross-field compatibility checks.
     *
     * @param config JSON configuration object to validate
     * @param allow_static_mode If true, relaxes validation for static mode configs
     * @return ValidationResult indicating success or failure with details
     */
    ValidationResult validate_config(const nlohmann::json& config, bool allow_static_mode = true);

    /**
     * @brief Validate buffer size parameter
     *
     * Ensures buffer size is within acceptable range (256KB - 8192KB).
     *
     * @param size_kb Buffer size in kilobytes
     * @return ValidationResult indicating whether size is valid
     */
    ValidationResult validate_buffer_size(int size_kb);

    /**
     * @brief Validate timeout parameter
     *
     * Ensures timeout is within acceptable range (1000ms - 60000ms).
     *
     * @param timeout_ms Timeout value in milliseconds
     * @return ValidationResult indicating whether timeout is valid
     */
    ValidationResult validate_timeout(int timeout_ms);

    /**
     * @brief Validate format preference for a specific provider
     *
     * Checks that the requested format is supported by the given provider.
     *
     * @param provider Provider name (e.g., "anthropic", "openai", "cerebras")
     * @param format Format identifier (e.g., "json-tool-use", "chat-completion")
     * @return ValidationResult indicating whether combination is valid
     */
    ValidationResult validate_format_preference(
        const std::string& provider,
        const std::string& format);

    /**
     * @brief Validate cross-field compatibility
     *
     * Checks for incompatible configuration combinations, such as:
     * - Streaming enabled with very low timeout
     * - Large buffer with insufficient timeout
     *
     * @param config JSON configuration object
     * @return ValidationResult indicating whether config fields are compatible
     */
    ValidationResult validate_compatibility(const nlohmann::json& config);

private:
    // Configuration constraints
    static constexpr int MIN_BUFFER_SIZE_KB = 256;
    static constexpr int MAX_BUFFER_SIZE_KB = 8192;
    static constexpr int MIN_TIMEOUT_MS = 1000;
    static constexpr int MAX_TIMEOUT_MS = 60000;
    static constexpr int MIN_STREAMING_TIMEOUT_MS = 1000;

    /**
     * @brief Check if a provider is known/supported
     */
    bool is_valid_provider(const std::string& provider) const;

    /**
     * @brief Check if a format is valid for a given provider
     */
    bool is_valid_format_for_provider(const std::string& provider, const std::string& format) const;
};

} // namespace webui
} // namespace aimux
