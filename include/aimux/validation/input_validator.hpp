#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>
#include <regex>
#include <limits>
#include <nlohmann/json.hpp>

namespace aimux {
namespace validation {

/**
 * @brief Comprehensive Input Validation Framework for Aimux v2.0.0
 *
 * This framework provides consistent and extensible input validation for all API endpoints
 * configuration parameters, and user inputs. It includes predefined validation rules,
 * custom validation support, detailed error reporting, and security-focused validation.
 *
 * Key Features:
 * - Predefined validation rules for common data types
 * - Custom validation rule registration
 * - JSON schema validation support
 * - Security-focused input sanitization
 * - Detailed error messages with field-level precision
 * - Validation chaining and composition
 * - Performance-optimized validation
 */

// Validation result status
enum class ValidationStatus {
    VALID,
    WARNING,
    ERROR
};

// Validation error with detailed information
struct ValidationError {
    ValidationStatus status;
    std::string field_path;      // Dotted path to the field (e.g., "user.email")
    std::string error_type;       // Type of validation error
    std::string message;          // Human-readable error message
    std::string expected_value;   // Expected format or value
    std::string actual_value;      // Actual invalid value
    std::string suggestion;        // Suggested fix or alternative
    std::unordered_map<std::string, std::string> details; // Additional context

    /**
     *Constructor for validation error
     */
    ValidationError(ValidationStatus stat, const std::string& field, const std::string& type,
                   const std::string& msg, const std::string& expected = "",
                   const std::string& actual = "", const std::string& suggest = "")
        : status(stat), field_path(field), error_type(type), message(msg),
          expected_value(expected), actual_value(actual), suggestion(suggest) {}

    /**
     * Convert error to JSON format
     */
    nlohmann::json toJson() const {
        nlohmann::json j;
        j["status"] = static_cast<int>(status);
        j["field_path"] = field_path;
        j["error_type"] = error_type;
        j["message"] = message;
        if (!expected_value.empty()) j["expected_value"] = expected_value;
        if (!actual_value.empty()) j["actual_value"] = actual_value;
        if (!suggestion.empty()) j["suggestion"] = suggestion;
        if (!details.empty()) j["details"] = details;
        return j;
    }
};

// Validation result with errors and warnings
struct ValidationResult {
    bool is_valid;
    std::vector<ValidationError> errors;
    std::vector<ValidationError> warnings;
    nlohmann::json sanitized_data; // Cleaned/sanitized input data

    /**
     * Default constructor - assumes validation passed
     */
    ValidationResult() : is_valid(true) {}

    /**
     * Merge validation results
     */
    void merge(const ValidationResult& other) {
        is_valid = is_valid && other.is_valid;
        errors.insert(errors.end(), other.errors.begin(), other.errors.end());
        warnings.insert(warnings.end(), other.warnings.begin(), other.warnings.end());

        // Merge sanitized data (other takes precedence for overlapping fields)
        if (!other.sanitized_data.empty()) {
            for (auto& [key, value] : other.sanitized_data.items()) {
                sanitized_data[key] = value;
            }
        }
    }

    /**
     * Add error to result
     */
    void addError(const ValidationError& error) {
        if (error.status == ValidationStatus::ERROR) {
            is_valid = false;
            errors.push_back(error);
        } else {
            warnings.push_back(error);
        }
    }

    /**
     * Check if validation passed
     */
    bool isValid() const { return is_valid; }

    /**
     * Check if there are any warnings
     */
    bool hasWarnings() const { return !warnings.empty(); }

    /**
     * Get total issues count (errors + warnings)
     */
    size_t totalIssues() const { return errors.size() + warnings.size(); }

    /**
     * Convert result to JSON format
     */
    nlohmann::json toJson() const {
        nlohmann::json j;
        j["is_valid"] = is_valid;
        j["errors"] = nlohmann::json::array();
        j["warnings"] = nlohmann::json::array();

        for (const auto& error : errors) {
            j["errors"].push_back(error.toJson());
        }

        for (const auto& warning : warnings) {
            j["warnings"].push_back(warning.toJson());
        }

        if (!sanitized_data.empty()) {
            j["sanitized_data"] = sanitized_data;
        }

        j["validation_summary"] = {
            {"error_count", errors.size()},
            {"warning_count", warnings.size()},
            {"total_issues", totalIssues()}
        };

        return j;
    }
};

// Forward declaration
class ValidationRule;

// Validation context containing shared state
struct ValidationContext {
    std::unordered_map<std::string, std::string> variables;
    nlohmann::json shared_data;
    std::unordered_map<std::string, std::function<ValidationResult(const nlohmann::json&)>> custom_rules;
    bool strict_mode = true;
    bool sanitize_input = true;
    bool detailed_errors = true;

    /**
     * Add custom validation function
     */
    void addCustomRule(const std::string& name,
                       std::function<ValidationResult(const nlohmann::json&)> rule) {
        custom_rules[name] = rule;
    }
};

// Base validation rule interface
class ValidationRule {
public:
    virtual ~ValidationRule() = default;
    virtual ValidationResult validate(const nlohmann::json& value,
                                   const ValidationContext& context = {}) const = 0;
    virtual std::string getRuleName() const = 0;
    virtual std::string getDescription() const = 0;
};

// Type-specific validation rules

// String validation rule
class StringValidation {
public:
    struct Config {
        std::string name = "string_validation";
        std::string description = "Validates string input";
        size_t min_length = 0;
        size_t max_length = std::numeric_limits<size_t>::max();
        std::string pattern;          // Regex pattern
        std::vector<std::string> allowed_values; // Enum validation
        bool trim_whitespace = true;
        bool lowercase = false;
        bool uppercase = false;
        std::unordered_map<std::string, std::string> custom_validators;
        bool sanitize_html = false;
        bool sanitize_sql = false;
    };

    static ValidationResult validate(const std::string& input, const Config& config,
                                   const ValidationContext& context = {});
};

// Numeric validation rule
class NumericValidation {
public:
    struct Config {
        std::string name = "numeric_validation";
        std::string description = "Validates numeric input";
        double min_value = std::numeric_limits<double>::lowest();
        double max_value = std::numeric_limits<double>::max();
        bool integer_only = false;
        bool allow_zero = true;
        bool allow_negative = true;
        int decimal_places = -1; // -1 for unlimited
        std::string format; // "percentage", "currency", etc.
    };

    static ValidationResult validate(double input, const Config& config,
                                   const ValidationContext& context = {});
};

// Email validation rule
class EmailValidation {
public:
    struct Config {
        std::string name = "email_validation";
        std::string description = "Validates email addresses";
        bool allow_domain_validation = false;
        std::vector<std::string> allowed_domains;
        std::vector<std::string> blocked_domains;
        bool check_mx_record = false;
        bool require_tld = true;
    };

    static ValidationResult validate(const std::string& email, const Config& config,
                                   const ValidationContext& context = {});
};

// URL validation rule
class UrlValidation {
public:
    struct Config {
        std::string name = "url_validation";
        std::string description = "Validates URLs";
        std::vector<std::string> allowed_schemes = {"http", "https"};
        std::vector<std::string> blocked_domains;
        bool require_ssl = false;
        bool check_domain_exists = false;
        int max_length = 2048;
    };

    static ValidationResult validate(const std::string& url, const Config& config,
                                   const ValidationContext& context = {});
};

// JSON schema validation
class JsonSchemaValidation {
public:
    struct Config {
        std::string name = "json_schema_validation";
        std::string description = "Validates JSON against schema";
        nlohmann::json schema;
        bool strict_type_checking = true;
        bool allow_unknown_fields = false;
        std::unordered_map<std::string, std::string> field_descriptions;
    };

    static ValidationResult validate(const nlohmann::json& data, const Config& config,
                                   const ValidationContext& context = {});
};

// API key validation
class ApiKeyValidation {
public:
    struct Config {
        std::string name = "api_key_validation";
        std::string description = "Validates API keys";
        std::string pattern; // Regex pattern for validation
        size_t min_length = 16;
        size_t max_length = 256;
        std::vector<std::string> forbidden_patterns;
        bool simulate_check = false; // Check against known patterns
    };

    static ValidationResult validate(const std::string& api_key, const Config& config,
                                   const ValidationContext& context = {});
};

// Main InputValidator class
class InputValidator {
public:
    /**
     * Get singleton instance
     */
    static InputValidator& getInstance();

    /**
     * Validate JSON object against schema
     */
    ValidationResult validateJson(const nlohmann::json& data,
                               const nlohmann::json& schema,
                               const ValidationContext& context = {}) const;

    /**
     * Validate string field
     */
    ValidationResult validateString(const std::string& input,
                                 const StringValidation::Config& config = {},
                                 const ValidationContext& context = {}) const;

    /**
     * Validate numeric field
     */
    ValidationResult validateNumber(const double input,
                                 const NumericValidation::Config& config = {},
                                 const ValidationContext& context = {}) const;

    /**
     * Validate email address
     */
    ValidationResult validateEmail(const std::string& email,
                                 const EmailValidation::Config& config = {},
                                 const ValidationContext& context = {}) const;

    /**
     * Validate URL
     */
    ValidationResult validateUrl(const std::string& url,
                              const UrlValidation::Config& config = {},
                              const ValidationContext& context = {}) const;

    /**
     * Validate API key
     */
    ValidationResult validateApiKey(const std::string& api_key,
                                 const ApiKeyValidation::Config& config = {},
                                 const ValidationContext& context = {}) const;

    /**
     * Sanitize input string according to security policies
     */
    std::string sanitizeString(const std::string& input,
                              const ValidationContext& context = {}) const;

    /**
     * Register custom validation rule
     */
    void registerRule(const std::string& name,
                     std::unique_ptr<ValidationRule> rule);

    /**
     * Get preset validation configurations
     */
    struct Presets {
        static JsonSchemaValidation::Config createApiRequestSchema();
        static JsonSchemaValidation::Config createConfigurationSchema();
        static StringValidation::Config createUsernameConfig();
        static StringValidation::Config createApiKeyConfig();
        static EmailValidation::Config createStandardEmailConfig();
    };

    /**
     * Create validation context with常用 settings
     */
    static ValidationContext createProductionContext();
    static ValidationContext createDevelopmentContext();

private:
    InputValidator() = default;
    ~InputValidator() = default;
    InputValidator(const InputValidator&) = delete;
    InputValidator& operator=(const InputValidator&) = delete;

    std::unordered_map<std::string, std::unique_ptr<ValidationRule>> rules_;

    // Helper methods
    bool isValidRegex(const std::string& pattern) const;
    bool validateDomainExists(const std::string& domain) const;
    std::string extractDomainFromEmail(const std::string& email) const;
    std::string extractDomainFromUrl(const std::string& url) const;
    nlohmann::json sanitizeJson(const nlohmann::json& input,
                              const ValidationContext& context) const;
};

// Convenience validation macros
#define AIMUX_VALIDATE_JSON(data, schema) \
    aimux::validation::InputValidator::getInstance().validateJson(data, schema)

#define AIMUX_VALIDATE_STRING(input, config) \
    aimux::validation::InputValidator::getInstance().validateString(input, config)

#define AIMUX_VALIDATE_EMAIL(email) \
    aimux::validation::InputValidator::getInstance().validateEmail(email)

#define AIMUX_VALIDATE_URL(url) \
    aimux::validation::InputValidator::getInstance().validateUrl(url)

#define AIMUX_VALIDATE_API_KEY(key) \
    aimux::validation::InputValidator::getInstance().validateApiKey(key)

// Validation result checking macros
#define AIMUX_IF_VALID(validation_result, block) \
    do { \
        const auto& _result = validation_result; \
        if (_result.isValid()) { block } \
    } while(0)

#define AIMUX_IF_INVALID(validation_result, block) \
    do { \
        const auto& _result = validation_result; \
        if (!_result.isValid()) { block } \
    } while(0)

// Error handling pattern
#define AIMUX_HANDLE_VALIDATION_ERROR(result, error_handler) \
    do { \
        if (!(result).isValid()) { \
            for (const auto& error : (result).errors) { \
                error_handler(error); \
            } \
        } \
    } while(0)

} // namespace validation
} // namespace aimux