/**
 * @file startup_validator.cpp
 * @brief Critical configuration validation implementation
 *
 * Implementation of comprehensive startup validation that ensures production
 * deployment safety through strict configuration requirements and fail-fast
 * behavior on critical issues.
 */

#include "aimux/config/startup_validator.hpp"
#include "aimux/core/error_handler.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <regex>
#include <algorithm>
#include <cstdlib>
#include <unistd.h>

namespace aimux {
namespace config {

// ConfigurationValidationError implementation
ConfigurationValidationError::ConfigurationValidationError(
    const std::vector<std::string>& errors,
    const std::string& config_path,
    const std::string& environment)
    : AimuxException(aimux::core::ErrorCode::CONFIG_VALIDATION_FAILED,
                     "Configuration validation failed with " + std::to_string(errors.size()) + " errors",
                     "StartupValidator")
    , errors_(errors)
    , config_path_(config_path)
    , environment_(environment) {
}

const std::vector<std::string>& ConfigurationValidationError::get_errors() const noexcept {
    return errors_;
}

const std::string& ConfigurationValidationError::get_config_path() const noexcept {
    return config_path_;
}

const std::string& ConfigurationValidationError::get_environment() const noexcept {
    return environment_;
}

// StartupValidator implementation
ValidationResult StartupValidator::validate_startup_config(
    const ProductionConfig& config,
    const std::string& config_path,
    const std::string& environment) {

    ValidationResult overall_result;
    overall_result.is_valid = true;
    overall_result.can_proceed = true;

    AIMUX_ERROR_CONTEXT("StartupValidator", "validate_startup_config");

    try {
        AIMUX_INFO("Starting comprehensive configuration validation");

        // 1. Validate configuration file first
        auto file_result = validate_config_file(config_path);
        overall_result.errors.insert(overall_result.errors.end(),
                                   file_result.errors.begin(),
                                   file_result.errors.end());
        overall_result.warnings.insert(overall_result.warnings.end(),
                                     file_result.warnings.begin(),
                                     file_result.warnings.end());

        // 2. Validate core configuration sections
        auto system_result = validate_system(config.system);
        overall_result.errors.insert(overall_result.errors.end(),
                                   system_result.errors.begin(),
                                   system_result.errors.end());
        overall_result.warnings.insert(overall_result.warnings.end(),
                                     system_result.warnings.begin(),
                                     system_result.warnings.end());

        auto security_result = validate_security(config.security, environment);
        overall_result.errors.insert(overall_result.errors.end(),
                                   security_result.errors.begin(),
                                   security_result.errors.end());
        overall_result.warnings.insert(overall_result.warnings.end(),
                                     security_result.warnings.begin(),
                                     security_result.warnings.end());

        auto providers_result = validate_providers(config.providers, environment);
        overall_result.errors.insert(overall_result.errors.end(),
                                   providers_result.errors.begin(),
                                   providers_result.errors.end());
        overall_result.warnings.insert(overall_result.warnings.end(),
                                     providers_result.warnings.begin(),
                                     providers_result.warnings.end());

        auto webui_result = validate_webui(config.webui);
        overall_result.errors.insert(overall_result.errors.end(),
                                   webui_result.errors.begin(),
                                   webui_result.errors.end());
        overall_result.warnings.insert(overall_result.warnings.end(),
                                     webui_result.warnings.begin(),
                                     webui_result.warnings.end());

        auto daemon_result = validate_daemon(config.daemon);
        overall_result.errors.insert(overall_result.errors.end(),
                                   daemon_result.errors.begin(),
                                   daemon_result.errors.end());
        overall_result.warnings.insert(overall_result.warnings.end(),
                                     daemon_result.warnings.begin(),
                                     daemon_result.warnings.end());

        // 3. Validate environment consistency
        auto env_result = validate_environment_consistency(config.system, environment);
        overall_result.errors.insert(overall_result.errors.end(),
                                   env_result.errors.begin(),
                                   env_result.errors.end());
        overall_result.warnings.insert(overall_result.warnings.end(),
                                     env_result.warnings.begin(),
                                     env_result.warnings.end());

        // Calculate overall validity
        overall_result.is_valid = overall_result.errors.empty();
        overall_result.severity_score = calculate_severity_score(
            overall_result.errors, overall_result.warnings);
        overall_result.can_proceed = can_proceed_with_startup(overall_result, environment);

        // Validation summary
        if (overall_result.is_valid) {
            AIMUX_INFO("Configuration validation completed successfully");
        } else {
            AIMUX_ERROR("Configuration validation failed with " +
                       std::to_string(overall_result.errors.size()) + " errors");
        }

        if (!overall_result.warnings.empty()) {
            AIMUX_WARNING("Configuration validation generated " +
                         std::to_string(overall_result.warnings.size()) + " warnings");
        }

    } catch (const std::exception& e) {
        overall_result.errors.push_back("Validation threw exception: " + std::string(e.what()));
        overall_result.is_valid = false;
        overall_result.can_proceed = false;
        AIMUX_ERROR("Configuration validation exception: " + std::string(e.what()));
    }

    return overall_result;
}

void StartupValidator::validate_or_abort(
    const ProductionConfig& config,
    const std::string& config_path,
    const std::string& environment) {

    AIMUX_ERROR_CONTEXT("StartupValidator", "validate_or_abort");
    AIMUX_INFO("Validating configuration with abort-on-fail policy");

    auto result = validate_startup_config(config, config_path, environment);

    // Always log validation results
    log_validation_results(result);

    if (!result.can_proceed) {
        abort_startup(result, environment);
    }
}

ValidationResult StartupValidator::validate_config_file(const std::string& config_path) {
    ValidationResult result;
    result.is_valid = true;

    AIMUX_DEBUG("Validating configuration file: " + config_path);

    // Check if file exists
    if (!std::filesystem::exists(config_path)) {
        result.errors.push_back("Configuration file does not exist: " + config_path);
        result.is_valid = false;
        return result;
    }

    // Check if file is accessible
    std::ifstream file(config_path);
    if (!file.is_open()) {
        result.errors.push_back("Cannot access configuration file: " + config_path);
        result.is_valid = false;
        return result;
    }
    file.close();

    // Check file permissions (security)
    if (!validate_file_permissions_security(config_path)) {
        result.warnings.push_back("Configuration file has insecure permissions: " + config_path);
    }

    // Check file size (avoid extremely large config files)
    auto file_size = std::filesystem::file_size(config_path);
    if (file_size > 10 * 1024 * 1024) { // 10MB limit
        result.errors.push_back("Configuration file too large (" +
                               std::to_string(file_size) + " bytes): " + config_path);
        result.is_valid = false;
    } else if (file_size == 0) {
        result.errors.push_back("Configuration file is empty: " + config_path);
        result.is_valid = false;
    }

    // Try to parse JSON
    try {
        std::ifstream json_file(config_path);
        nlohmann::json config_json;
        json_file >> config_json;
        json_file.close();

        auto schema_result = validate_json_schema(config_json);
        result.errors.insert(result.errors.end(),
                            schema_result.errors.begin(),
                            schema_result.errors.end());
        result.warnings.insert(result.warnings.end(),
                              schema_result.warnings.begin(),
                              schema_result.warnings.end());

        if (!schema_result.is_valid) {
            result.is_valid = false;
        }

    } catch (const nlohmann::json::parse_error& e) {
        result.errors.push_back("Invalid JSON in configuration file: " +
                               std::string(e.what()) + " (file: " + config_path + ")");
        result.is_valid = false;
    } catch (const std::exception& e) {
        result.errors.push_back("Error reading configuration file: " +
                               std::string(e.what()) + " (file: " + config_path + ")");
        result.is_valid = false;
    }

    return result;
}

ValidationResult StartupValidator::validate_json_schema(const nlohmann::json& config_json) {
    ValidationResult result;
    result.is_valid = true;

    AIMUX_DEBUG("Validating JSON schema compliance");

    // Check for required top-level sections
    auto mandatory_fields = get_production_mandatory_fields();
    for (const auto& field : mandatory_fields) {
        if (!validate_required_field(config_json, field)) {
            result.errors.push_back("Missing required configuration section: " + field);
            result.is_valid = false;
        }
    }

    // Validate system section if present
    if (config_json.contains("system")) {
        const auto& system = config_json["system"];

        if (!system.contains("environment")) {
            result.errors.push_back("Missing system.environment field");
            result.is_valid = false;
        }

        if (system.contains("log_level")) {
            std::string log_level = system["log_level"];
            if (!validation::isValidLogLevel(log_level)) {
                result.errors.push_back("Invalid log_level: " + log_level);
                result.is_valid = false;
            }
        }
    }

    // Validate security section if present
    if (config_json.contains("security")) {
        const auto& security = config_json["security"];
        auto security_fields = get_security_critical_fields();

        for (const auto& field : security_fields) {
            if (!validate_required_field(security, field)) {
                result.errors.push_back("Missing critical security field: security." + field);
                result.is_valid = false;
            }
        }
    }

    // Validate providers section if present
    if (config_json.contains("providers")) {
        if (!config_json["providers"].is_array()) {
            result.errors.push_back("providers section must be an array");
            result.is_valid = false;
        } else if (config_json["providers"].empty()) {
            result.errors.push_back("At least one provider must be configured");
            result.is_valid = false;
        } else {
            for (const auto& provider : config_json["providers"]) {
                auto provider_fields = get_provider_critical_fields();
                for (const auto& field : provider_fields) {
                    if (!validate_required_field(provider, field)) {
                        result.errors.push_back("Provider missing required field: " + field);
                        result.is_valid = false;
                    }
                }
            }
        }
    }

    return result;
}

ValidationResult StartupValidator::validate_providers(
    const std::vector<ProviderConfig>& providers,
    const std::string& /* environment */) {

    ValidationResult result;
    result.is_valid = true;

    AIMUX_DEBUG("Validating provider configurations");

    if (providers.empty()) {
        result.errors.push_back("No providers configured - at least one provider is required");
        result.is_valid = false;
        return result;
    }

    int enabled_providers = 0;
    for (const auto& provider : providers) {
        auto provider_errors = validation::validateProviderConfig(provider);
        result.errors.insert(result.errors.end(),
                            provider_errors.begin(),
                            provider_errors.end());

        // Additional security validations
        if (!validate_api_key_security(provider.api_key)) {
            result.errors.push_back("Invalid or missing API key for provider: " + provider.name);
            result.is_valid = false;
        }

        if (!validate_endpoint_security(provider.endpoint)) {
            result.errors.push_back("Insecure endpoint for provider " + provider.name +
                                   ": " + provider.endpoint);
            result.is_valid = false;
        }

        // Check if provider is enabled
        if (provider.enabled) {
            enabled_providers++;
        }
    }

    if (enabled_providers == 0) {
        result.errors.push_back("No providers are enabled - at least one provider must be enabled");
        result.is_valid = false;
    }

    return result;
}

ValidationResult StartupValidator::validate_security(
    const SecurityConfig& security,
    const std::string& environment) {

    ValidationResult result;
    result.is_valid = true;

    AIMUX_DEBUG("Validating security configuration");

    // In production, security features should be enabled
    if (environment == "production") {
        if (!security.api_key_encryption) {
            result.errors.push_back("API key encryption must be enabled in production");
            result.is_valid = false;
        }

        if (!security.audit_logging) {
            result.warnings.push_back("Audit logging should be enabled in production");
        }

        if (!security.ssl_verification) {
            result.errors.push_back("SSL verification must be enabled in production");
            result.is_valid = false;
        }

        if (!security.require_https) {
            result.errors.push_back("HTTPS must be required in production");
            result.is_valid = false;
        }
    }

    // Validate allowed origins
    if (security.allowed_origins.empty()) {
        result.warnings.push_back("No allowed origins specified - using default values");
    }

    // Check encryption settings
    if (!validate_encryption_settings(security, environment)) {
        result.errors.push_back("Security encryption configuration is invalid");
        result.is_valid = false;
    }

    return result;
}

ValidationResult StartupValidator::validate_system(const SystemConfig& system) {
    ValidationResult result;
    result.is_valid = true;

    AIMUX_DEBUG("Validating system configuration");

    auto system_errors = validation::validateSystemConfig(system);
    result.errors.insert(result.errors.end(),
                        system_errors.begin(),
                        system_errors.end());

    // Additional validations
    if (!validate_log_level_security(system.log_level, "production")) {
        result.errors.push_back("Inappropriate log level for production: " + system.log_level);
        result.is_valid = false;
    }

    // Validate directory paths
    if (!validate_path_security(system.log_dir)) {
        result.errors.push_back("Invalid log directory path: " + system.log_dir);
        result.is_valid = false;
    }

    if (!validate_path_security(system.backup_dir)) {
        result.errors.push_back("Invalid backup directory path: " + system.backup_dir);
        result.is_valid = false;
    }

    // Check concurrent requests limit
    if (system.max_concurrent_requests > 10000) {
        result.warnings.push_back("Very high concurrent request limit may overwhelm the system: " +
                                 std::to_string(system.max_concurrent_requests));
    }

    if (system.backup_retention_days < 7 || system.backup_retention_days > 365) {
        result.warnings.push_back("Backup retention days should be between 7 and 365");
    }

    if (!system_errors.empty()) {
        result.is_valid = false;
    }

    return result;
}

ValidationResult StartupValidator::validate_webui(const WebUIConfig& webui) {
    ValidationResult result;
    result.is_valid = true;

    AIMUX_DEBUG("Validating WebUI configuration");

    // Port validation
    if (!validate_port_security(webui.port, false)) {
        result.errors.push_back("Invalid WebUI port: " + std::to_string(webui.port));
        result.is_valid = false;
    }

    if (webui.ssl_enabled && !validate_port_security(webui.ssl_port, true)) {
        result.errors.push_back("Invalid WebUI SSL port: " + std::to_string(webui.ssl_port));
        result.is_valid = false;
    }

    if (webui.ssl_enabled && webui.port == webui.ssl_port) {
        result.errors.push_back("WebUI HTTP and SSL ports cannot be the same");
        result.is_valid = false;
    }

    // SSL configuration validation
    if (webui.ssl_enabled) {
        if (webui.cert_file.empty() || webui.key_file.empty()) {
            result.errors.push_back("SSL enabled but cert_file or key_file not specified");
            result.is_valid = false;
        }
    }

    // CORS origins validation
    if (webui.cors_enabled && webui.cors_origins.empty()) {
        result.warnings.push_back("CORS enabled but no origins specified");
    }

    return result;
}

ValidationResult StartupValidator::validate_daemon(const DaemonConfig& daemon) {
    ValidationResult result;
    result.is_valid = true;

    AIMUX_DEBUG("Validating daemon configuration");

    // User and group validation
    if (daemon.enabled) {
        if (daemon.user.empty() || daemon.group.empty()) {
            result.errors.push_back("Daemon user and group must be specified when daemon is enabled");
            result.is_valid = false;
        }

        // Path validations
        if (!validate_path_security(daemon.working_directory)) {
            result.errors.push_back("Invalid daemon working directory: " + daemon.working_directory);
            result.is_valid = false;
        }

        if (!validate_path_security(daemon.log_file)) {
            result.errors.push_back("Invalid daemon log file: " + daemon.log_file);
            result.is_valid = false;
        }

        if (!validate_path_security(daemon.pid_file)) {
            result.errors.push_back("Invalid daemon PID file: " + daemon.pid_file);
            result.is_valid = false;
        }
    }

    return result;
}

ValidationResult StartupValidator::validate_environment_consistency(
    const SystemConfig& system,
    const std::string& target_environment) {

    ValidationResult result;
    result.is_valid = true;

    AIMUX_DEBUG("Validating environment consistency");

    // Check if declared environment matches target
    if (system.environment != target_environment) {
        if (target_environment == "production") {
            result.errors.push_back("System environment is '" + system.environment +
                                   "' but target is '" + target_environment + "'");
            result.is_valid = false;
        } else {
            result.warnings.push_back("System environment '" + system.environment +
                                    "' differs from target '" + target_environment + "'");
        }
    }

    // Production-specific checks
    if (target_environment == "production") {
        if (system.log_level == "debug" || system.log_level == "trace") {
            result.warnings.push_back("Debug/trace logging should not be used in production");
        }

        if (!system.structured_logging) {
            result.warnings.push_back("Structured logging should be enabled in production");
        }
    }

    return result;
}

int StartupValidator::calculate_severity_score(
    const std::vector<std::string>& errors,
    const std::vector<std::string>& warnings) {

    int score = 0;

    // Each error contributes 10 points
    score += static_cast<int>(errors.size()) * 10;

    // Each warning contributes 3 points
    score += static_cast<int>(warnings.size()) * 3;

    // Cap at 100
    return std::min(score, 100);
}

bool StartupValidator::can_proceed_with_startup(
    const ValidationResult& result,
    const std::string& environment) {

    // In production, be more strict
    if (environment == "production") {
        // No errors allowed in production
        if (!result.errors.empty()) {
            return false;
        }

        // Severity score must be low
        if (result.severity_score > 20) { // More than 2 warnings
            return false;
        }
    } else {
        // In non-production, allow some warnings
        // But still abort on critical errors
        if (!result.errors.empty()) {
            // Check if any errors are fatal
            for (const auto& error : result.errors) {
                auto severity = classify_issue(error, environment);
                if (severity >= ValidationSeverity::CRITICAL) {
                    return false;
                }
            }
        }
    }

    return true;
}

std::string StartupValidator::generate_validation_report(const ValidationResult& result) {
    std::ostringstream report;

    report << "=== Configuration Validation Report ===\n\n";

    if (result.is_valid) {
        report << "✅ VALIDATION PASSED\n";
    } else {
        report << "❌ VALIDATION FAILED\n";
    }

    report << "Severity Score: " << result.severity_score << "/100\n";
    report << "Can Proceed: " << (result.can_proceed ? "YES" : "NO") << "\n\n";

    if (!result.errors.empty()) {
        report << "Errors (" << result.errors.size() << "):\n";
        for (size_t i = 0; i < result.errors.size(); ++i) {
            report << "  " << (i + 1) << ". " << result.errors[i] << "\n";
        }
        report << "\n";
    }

    if (!result.warnings.empty()) {
        report << "Warnings (" << result.warnings.size() << "):\n";
        for (size_t i = 0; i < result.warnings.size(); ++i) {
            report << "  " << (i + 1) << ". " << result.warnings[i] << "\n";
        }
        report << "\n";
    }

    if (result.is_valid) {
        report << "🚀 Configuration is valid for startup.\n";
    } else {
        report << "🛑 Configuration issues must be resolved before startup.\n";
    }

    return report.str();
}

void StartupValidator::log_validation_results(
    const ValidationResult& result,
    const std::string& /* component */) {

    // Log summary
    if (result.is_valid) {
        AIMUX_INFO("Configuration validation passed - Severity: " +
                  std::to_string(result.severity_score) + "/100");
    } else {
        AIMUX_ERROR("Configuration validation failed - " +
                   std::to_string(result.errors.size()) + " errors, " +
                   std::to_string(result.warnings.size()) + " warnings");
    }

    // Log errors
    for (const auto& error : result.errors) {
        AIMUX_ERROR("Config validation error: " + error);
    }

    // Log warnings
    for (const auto& warning : result.warnings) {
        AIMUX_WARNING("Config validation warning: " + warning);
    }
}

// Private helper methods implementation

bool StartupValidator::validate_file_permissions(const std::string& path) {
    // Check if file is readable
    std::ifstream file(path);
    bool readable = file.is_open();
    file.close();
    return readable;
}

bool StartupValidator::validate_file_permissions_security(const std::string& path) {
    // Check if file permissions are secure (not world-readable for configs)
    auto perms = std::filesystem::status(path).permissions();

    // In production, config files should not be world-readable
    if ((perms & std::filesystem::perms::others_read) != std::filesystem::perms::none) {
        return false;
    }

    return true;
}

bool StartupValidator::validate_required_field(const nlohmann::json& json,
                                              const std::string& field_path) {
    std::istringstream iss(field_path);
    std::string segment;
    const nlohmann::json* current = &json;

    while (std::getline(iss, segment, '.')) {
        if (!current->contains(segment)) {
            return false;
        }
        current = &(*current)[segment];
    }

    return true;
}

bool StartupValidator::validate_api_key_security(const std::string& api_key) {
    if (api_key.empty()) {
        return false; // API key is required
    }

    // Check for common placeholder values
    if (api_key == "YOUR_API_KEY_HERE" ||
        api_key == "your-api-key" ||
        api_key == "test-key" ||
        api_key.length() < 16) {
        return false;
    }

    return true;
}

bool StartupValidator::validate_endpoint_security(const std::string& endpoint) {
    if (endpoint.empty()) {
        return false;
    }

    // Must use HTTPS in production
    if (endpoint.substr(0, 8) != "https://") {
        return false;
    }

    // Basic URL format validation
    std::regex url_regex(R"(^https://[a-zA-Z0-9.-]+(\:[0-9]+)?(/.*)?$)");
    return std::regex_match(endpoint, url_regex);
}

bool StartupValidator::validate_port_security(int port, bool /* ssl_enabled */) {
    return validation::isValidPort(port) && port != 0;
}

bool StartupValidator::validate_path_security(const std::string& path) {
    return validation::isValidPath(path);
}

bool StartupValidator::validate_log_level_security(const std::string& log_level,
                                                  const std::string& environment) {
    if (!validation::isValidLogLevel(log_level)) {
        return false;
    }

    // In production, debug/trace should not be used
    if (environment == "production") {
        if (log_level == "debug" || log_level == "trace") {
            return false;
        }
    }

    return true;
}

bool StartupValidator::validate_encryption_settings(const SecurityConfig& security,
                                                   const std::string& environment) {
    // In production, encryption should be enabled
    if (environment == "production") {
        if (!security.api_key_encryption) {
            return false;
        }
    }

    return true;
}

std::vector<std::string> StartupValidator::get_production_mandatory_fields() {
    return {
        "system",
        "security",
        "providers",
        "webui"
    };
}

std::vector<std::string> StartupValidator::get_security_critical_fields() {
    return {
        "api_key_encryption",
        "ssl_verification",
        "require_https"
    };
}

std::vector<std::string> StartupValidator::get_provider_critical_fields() {
    return {
        "name",
        "api_key",
        "endpoint",
        "models",
        "enabled"
    };
}

StartupValidator::ValidationSeverity StartupValidator::classify_issue(
    const std::string& issue,
    const std::string& /* environment */) {

    // Check for critical security issues
    if (issue.find("SSL must be enabled") != std::string::npos ||
        issue.find("API key encryption must be enabled") != std::string::npos ||
        issue.find("HTTPS must be required") != std::string::npos ||
        issue.find("Invalid or missing API key") != std::string::npos) {
        return ValidationSeverity::CRITICAL;
    }

    // Check for configuration failures
    if (issue.find("does not exist") != std::string::npos ||
        issue.find("Missing required") != std::string::npos ||
        issue.find("Invalid JSON") != std::string::npos ||
        issue.find("cannot be empty") != std::string::npos) {
        return ValidationSeverity::ERROR;
    }

    // Check for warnings
    if (issue.find("Inappropriate log level") != std::string::npos ||
        issue.find("should be enabled") != std::string::npos ||
        issue.find("insecure permissions") != std::string::npos) {
        return ValidationSeverity::WARNING;
    }

    return ValidationSeverity::INFO;
}

void StartupValidator::abort_startup(const ValidationResult& result,
                                     const std::string& /* environment */) {

    std::cerr << "\n🚨 CRITICAL CONFIGURATION ERRORS DETECTED 🚨\n";
    std::cerr << "Environment: production\n";
    std::cerr << "Severity Score: " << result.severity_score << "/100\n\n";

    std::cerr << "ERRORS (" << result.errors.size() << "):\n";
    for (size_t i = 0; i < result.errors.size(); ++i) {
        std::cerr << "  ❌ " << (i + 1) << ". " << result.errors[i] << "\n";
    }

    if (!result.warnings.empty()) {
        std::cerr << "\nWARNINGS (" << result.warnings.size() << "):\n";
        for (size_t i = 0; i < result.warnings.size(); ++i) {
            std::cerr << "  ⚠️  " << (i + 1) << ". " << result.warnings[i] << "\n";
        }
    }

    std::cerr << "\n" << generate_validation_report(result);

    // Also log through the error handler if available
    try {
        for (const auto& error : result.errors) {
            AIMUX_FATAL("Configuration validation error: " + error);
        }
    } catch (...) {
        // Error handler might not be initialized
    }

    std::cerr << "\n🛑 STARTUP ABORTED: Fix configuration issues and try again\n";

    // Exit with appropriate error code
    exit(1);
}

} // namespace config
} // namespace aimux