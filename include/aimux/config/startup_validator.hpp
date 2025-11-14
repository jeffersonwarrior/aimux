/**
 * @file startup_validator.hpp
 * @brief Critical configuration validation for production deployment
 *
 * This module provides comprehensive configuration validation that aborts
 * application startup if mandatory configuration is missing or invalid.
 *
 * Features:
 * - Mandatory configuration validation
 * - JSON schema validation
 * - Security policy compliance checking
 * - Environment consistency validation
 * - Provider configuration validation
 * - Abort-on-fail behavior for production safety
 */

#pragma once

#include <string>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>
#include "aimux/core/error_handler.hpp"
#include "config/production_config.h"

namespace aimux {
namespace config {

/**
 * @brief Configuration validation results
 */
struct ValidationResult {
    bool is_valid = false;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    bool can_proceed = false;  // If false, application should abort
    int severity_score = 0;    // 0-100, higher is more severe
};

/**
 * @brief Critical startup validator for production deployment
 *
 * This class implements strict validation rules that must pass for
 * safe production operation. It aborts startup on critical failures.
 */
class StartupValidator {
public:
    /**
     * @brief Validate configuration for startup
     *
     * @param config Production configuration to validate
     * @param config_path Path to configuration file (for error reporting)
     * @param environment Target environment (production, staging, development)
     * @return ValidationResult with detailed validation outcome
     */
    static ValidationResult validate_startup_config(
        const ProductionConfig& config,
        const std::string& config_path,
        const std::string& environment = "production");

    /**
     * @brief Validate and abort startup if critical issues found
     *
     * This method implements the "fail-fast" principle for production safety.
     * It will terminate the application with appropriate error codes if critical
     * validation fails.
     *
     * @param config Production configuration to validate
     * @param config_path Path to configuration file
     * @param environment Target environment
     */
    static void validate_or_abort(
        const ProductionConfig& config,
        const std::string& config_path,
        const std::string& environment = "production");

    /**
     * @brief Validate configuration file existence and accessibility
     *
     * @param config_path Path to configuration file
     * @return ValidationResult
     */
    static ValidationResult validate_config_file(const std::string& config_path);

    /**
     * @brief Validate JSON schema compliance
     *
     * @param config_json Configuration JSON to validate
     * @return ValidationResult
     */
    static ValidationResult validate_json_schema(const nlohmann::json& config_json);

    /**
     * @brief Validate mandatory provider configuration
     *
     * @param providers Vector of provider configurations
     * @param environment Target environment
     * @return ValidationResult
     */
    static ValidationResult validate_providers(
        const std::vector<ProviderConfig>& providers,
        const std::string& environment = "production");

    /**
     * @brief Validate security configuration
     *
     * @param security Security configuration to validate
     * @param environment Target environment
     * @return ValidationResult
     */
    static ValidationResult validate_security(
        const SecurityConfig& security,
        const std::string& environment = "production");

    /**
     * @brief Validate system configuration
     *
     * @param system System configuration to validate
     * @return ValidationResult
     */
    static ValidationResult validate_system(const SystemConfig& system);

    /**
     * @brief Validate WebUI configuration
     *
     * @param webui WebUI configuration to validate
     * @return ValidationResult
     */
    static ValidationResult validate_webui(const WebUIConfig& webui);

    /**
     * @brief Validate daemon configuration
     *
     * @param daemon Daemon configuration to validate
     * @return ValidationResult
     */
    static ValidationResult validate_daemon(const DaemonConfig& daemon);

    /**
     * @brief Validate environment consistency
     *
     * Checks that environment settings are consistent and appropriate
     * for the target deployment environment.
     *
     * @param system System configuration
     * @param target_environment Target environment name
     * @return ValidationResult
     */
    static ValidationResult validate_environment_consistency(
        const SystemConfig& system,
        const std::string& target_environment);

    /**
     * @brief Get severity score for validation result
     *
     * Calculates a 0-100 score where higher values indicate more severe issues.
     * Used to determine if startup should proceed.
     *
     * @param errors Vector of error messages
     * @param warnings Vector of warning messages
     * @return Severity score (0-100)
     */
    static int calculate_severity_score(
        const std::vector<std::string>& errors,
        const std::vector<std::string>& warnings);

    /**
     * @brief Determine if validation result allows startup to proceed
     *
     * @param result ValidationResult to evaluate
     * @param environment Target environment (production has stricter rules)
     * @return true if startup can proceed, false otherwise
     */
    static bool can_proceed_with_startup(
        const ValidationResult& result,
        const std::string& environment);

    /**
     * @brief Generate validation report
     *
     * @param result ValidationResult to format
     * @return Formatted validation report string
     */
    static std::string generate_validation_report(const ValidationResult& result);

    /**
     * @brief Log validation results with appropriate severity levels
     *
     * @param result ValidationResult to log
     * @param component Component name for logging context
     */
    static void log_validation_results(
        const ValidationResult& result,
        const std::string& component = "StartupValidator");

private:
    // Validation helper methods
    static bool validate_file_permissions(const std::string& path);
    static bool validate_file_permissions_security(const std::string& path);
    static bool validate_required_field(const nlohmann::json& json,
                                      const std::string& field_path);
    static bool validate_api_key_security(const std::string& api_key);
    static bool validate_endpoint_security(const std::string& endpoint);
    static bool validate_port_security(int port, bool ssl_enabled);
    static bool validate_path_security(const std::string& path);
    static bool validate_log_level_security(const std::string& log_level,
                                           const std::string& environment);
    static bool validate_encryption_settings(const SecurityConfig& security,
                                           const std::string& environment);

    // Critical validation rules for production
    static std::vector<std::string> get_production_mandatory_fields();
    static std::vector<std::string> get_security_critical_fields();
    static std::vector<std::string> get_provider_critical_fields();

    // Error classification
    enum class ValidationSeverity {
        INFO = 0,
        WARNING = 1,
        ERROR = 2,
        CRITICAL = 3,
        FATAL = 4
    };

    static ValidationSeverity classify_issue(const std::string& issue,
                                           const std::string& environment);

    // Abort behavior
    [[noreturn]] static void abort_startup(const ValidationResult& result,
                                         const std::string& environment);
};

/**
 * @brief Configuration validation exception
 */
class ConfigurationValidationError : public core::AimuxException {
public:
    ConfigurationValidationError(const std::vector<std::string>& errors,
                                const std::string& config_path = "",
                                const std::string& environment = "");

    const std::vector<std::string>& get_errors() const noexcept;
    const std::string& get_config_path() const noexcept;
    const std::string& get_environment() const noexcept;

private:
    std::vector<std::string> errors_;
    std::string config_path_;
    std::string environment_;
};

/**
 * @brief Macro for critical configuration validation
 *
 * This macro provides a convenient way to validate configuration and abort
 * on critical failures. It should be called early in application startup.
 */
#define AIMUX_VALIDATE_CONFIG_OR_ABORT(config, config_path, env) \
    aimux::config::StartupValidator::validate_or_abort(config, config_path, env)

} // namespace config
} // namespace aimux