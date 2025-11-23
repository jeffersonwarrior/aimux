/**
 * @file validation_example.cpp
 * @brief Comprehensive examples of using the Aimux Input Validation Framework
 *
 * This file demonstrates various validation scenarios including:
 * - Basic field validation
 * - Complex object validation with schemas
 * - Security-focused input sanitization
 * - API request validation
 * - Custom validation rules
 * - Error handling and response formatting
 */

#include "aimux/validation/input_validator.hpp"
#include "aimux/monitoring/performance_monitor.hpp"
#include <iostream>
#include <iomanip>

using namespace aimux::validation;
using nlohmann::json;

// Helper function to print validation results
void printValidationResult(const ValidationResult& result, const std::string& context = "") {
    std::cout << "\n" << std::string(50, '=') << "\n";
    if (!context.empty()) {
        std::cout << "Context: " << context << "\n";
    }
    std::cout << "Valid: " << (result.isValid() ? "✓ YES" : "✗ NO") << "\n";
    std::cout << "Errors: " << result.errors.size() << "\n";
    std::cout << "Warnings: " << result.warnings.size() << "\n";

    if (!result.errors.empty()) {
        std::cout << "\nERRORS:\n";
        for (const auto& error : result.errors) {
            std::cout << "  • " << error.field_path << ": " << error.message << "\n";
            std::cout << "    Expected: " << error.expected_value << "\n";
            std::cout << "    Actual: " << error.actual_value << "\n";
            std::cout << "    Suggestion: " << error.suggestion << "\n";
        }
    }

    if (!result.warnings.empty()) {
        std::cout << "\nWARNINGS:\n";
        for (const auto& warning : result.warnings) {
            std::cout << "  • " << warning.field_path << ": " << warning.message << "\n";
        }
    }

    if (!result.sanitized_data.empty()) {
        std::cout << "\nSanitized Data: " << result.sanitized_data.dump(2) << "\n";
    }
    std::cout << std::string(50, '=') << "\n";
}

// Custom validation rule example
class UserIdValidationRule : public ValidationRule {
public:
    ValidationResult validate(const nlohmann::json& value,
                             const ValidationContext& context) const override {
        if (!value.is_string()) {
            return ValidationError(ValidationStatus::ERROR, "user_id", "type_required",
                                 "User ID must be a string", "string", value.type_name(),
                                 "Provide user ID as string");
        }

        std::string user_id = value.get<std::string>();

        // Check if it starts with "user_" and contains only valid characters
        if (!user_id.starts_with("user_")) {
            return ValidationError(ValidationStatus::ERROR, "user_id", "invalid_prefix",
                                 "User ID must start with 'user_'", "user_*", user_id,
                                 "Prefix user ID with 'user_'");
        }

        // Check remaining characters are alphanumeric
        std::string after_prefix = user_id.substr(5);
        if (after_prefix.empty() || !std::all_of(after_prefix.begin(), after_prefix.end(),
                                                 [](char c) { return std::isalnum(c); })) {
            return ValidationError(ValidationStatus::ERROR, "user_id", "invalid_format",
                                 "User ID after prefix must be alphanumeric", "user_123", user_id,
                                 "Use only alphanumeric characters after prefix");
        }

        return ValidationResult();
    }

    std::string getRuleName() const override { return "user_id_validation"; }
    std::string getDescription() const override { return "Validates user ID format"; }
};

void demonstrateBasicValidation() {
    std::cout << "\n🔍 BASIC VALIDATION EXAMPLES\n";

    auto& validator = InputValidator::getInstance();

    // String validation with multiple constraints
    StringValidation::Config string_config;
    string_config.min_length = 5;
    string_config.max_length = 20;
    string_config.pattern = R"(^[A-Za-z][A-Za-z0-9_-]*$)";
    string_config.trim_whitespace = true;
    string_config.lowercase = true;

    std::cout << "\n1. String Validation Examples:";

    auto result1 = validator.validateString("ValidUsername123", string_config);
    printValidationResult(result1, "Valid username");

    auto result2 = validator.validateString("  Invalid! Username  ", string_config);
    printValidationResult(result2, "Invalid username with special chars");

    // Email validation with domain restrictions
    EmailValidation::Config email_config;
    email_config.allowed_domains = {"example.com", "test.org"};
    email_config.require_tld = true;

    std::cout << "\n2. Email Validation Examples:";

    auto result3 = validator.validateEmail("user@example.com", email_config);
    printValidationResult(result3, "Valid email from allowed domain");

    auto result4 = validator.validateEmail("user@disallowed.com", email_config);
    printValidationResult(result4, "Email from disallowed domain");

    // API key validation with pattern
    ApiKeyValidation::Config api_config;
    api_config.pattern = R"(^sk_[a-zA-Z0-9]{24}$)";
    api_config.forbidden_patterns = {"password", "secret"};

    std::cout << "\n3. API Key Validation Examples:";

    auto result5 = validator.validateApiKey("sk_abcdefghijklmnop123456", api_config);
    printValidationResult(result5, "Valid API key");

    auto result6 = validator.validateApiKey("invalid-key", api_config);
    printValidationResult(result6, "Invalid API key format");
}

void demonstrateSecuritySanitization() {
    std::cout << "\n🛡️ SECURITY SANITIZATION EXAMPLES\n";

    auto& validator = InputValidator::getInstance();

    ValidationContext context;
    context.sanitize_input = true;
    context.strict_mode = true;

    // HTML injection prevention
    StringValidation::Config html_config;
    html_config.sanitize_html = true;
    html_config.trim_whitespace = true;

    std::cout << "\n1. HTML Injection Prevention:";

    auto result1 = validator.validateString("  <script>alert('XSS')</script> Clean data  ", html_config, context);
    printValidationResult(result1, "HTML sanitization");

    // SQL injection prevention
    StringValidation::Config sql_config;
    sql_config.sanitize_sql = true;
    sql_config.trim_whitespace = true;

    std::cout << "\n2. SQL Injection Prevention:";

    auto result2 = validator.validateString(" '; DROP TABLE users; --", sql_config, context);
    printValidationResult(result2, "SQL injection mitigation");

    // Combined security sanitization
    StringValidation::Config security_config;
    security_config.sanitize_html = true;
    security_config.sanitize_sql = true;
    security_config.max_length = 100;

    std::cout << "\n3. Combined Security Sanitization:";

    auto result3 = validator.validateString(
        "<script>alert('XSS')</script>'; DROP TABLE users; --  ",
        security_config, context
    );
    printValidationResult(result3, "Comprehensive security sanitization");
}

void demonstrateComplexObjectValidation() {
    std::cout << "\n📋 COMPLEX OBJECT VALIDATION EXAMPLES\n";

    auto& validator = InputValidator::getInstance();
    ValidationContext context = InputValidator::createProductionContext();

    // Define comprehensive API request schema
    json api_request_schema = {
        {"type", "object"},
        {"required", {"request_id", "endpoint", "method", "authentication"}},
        {"properties", {
            {"request_id", {
                {"type", "string"},
                {"pattern", R"(^req_[a-f0-9]{8}-[a-f0-9]{4}-4[a-f0-9]{3}-[89ab][a-f0-9]{3}-[a-f0-9]{12}$)"}
            }},
            {"endpoint", {
                {"type", "string"},
                {"minLength", 1},
                {"maxLength", 255},
                {"pattern", R"(^/api/v[\d]+/[\w\-/]+)$)"}
            }},
            {"method", {
                {"type", "string"},
                {"enum", {"GET", "POST", "PUT", "DELETE", "PATCH"}}
            }},
            {"authentication", {
                {"type", "object"},
                {"required", {"type", "token"}},
                {"properties", {
                    {"type", {
                        {"type", "string"},
                        {"enum", {"bearer", "api_key"}}
                    }},
                    {"token", {
                        {"type", "string"},
                        {"minLength", 16},
                        {"maxLength", 256}
                    }}
                }}
            }},
            {"headers", {
                {"type", "object"},
                {"properties", {
                    {"content-type", {
                        {"type", "string"},
                        {"enum", {"application/json", "application/x-www-form-urlencoded"}}
                    }},
                    {"user-agent", {
                        {"type", "string"},
                        {"maxLength", 255}
                    }}
                }}
            }},
            {"body", {
                {"type", "object"},
                {"additionalProperties", false}
            }}
        }},
        {"additionalProperties", false}
    };

    std::cout << "\n1. Valid API Request:";

    json valid_request = {
        {"request_id", "req_550e8400-e29b-41d4-a716-446655440000"},
        {"endpoint", "/api/v1/models/completions"},
        {"method", "POST"},
        {"authentication", {
            {"type", "bearer"},
            {"token", "sk_valid123456789012345678901234"}
        }},
        {"headers", {
            {"content-type", "application/json"},
            {"user-agent", "AimuxClient/2.0.0"}
        }},
        {"body", {
            {"model", "llama-70b"},
            {"messages", json::array({{"Hello, world!"}})}
        }}
    };

    auto result1 = validator.validateJson(valid_request, api_request_schema, context);
    printValidationResult(result1, "Valid complete API request");

    std::cout << "\n2. Invalid API Request (multiple errors):";

    json invalid_request = {
        {"request_id", "invalid_id"},
        {"endpoint", "invalid-endpoint"},
        {"method", "INVALID"},
        {"authentication", {
            {"type", "bearer"},
            {"token", "short"}
        }},
        {"headers", {
            {"content-type", "invalid/content-type"}
        }},
        {"body", {
            {"model", "llama-70b"}
        }},
        {"unknown_field", "should_not_be_here"}
    };

    auto result2 = validator.validateJson(invalid_request, api_request_schema, context);
    printValidationResult(result2, "Invalid API request with multiple issues");
}

void demonstratePerformanceIntegration() {
    std::cout << "\n⚡ PERFORMANCE MONITORING INTEGRATION\n";

    // Start performance monitoring
    auto& monitor = aimux::monitoring::PerformanceMonitor::getInstance();
    monitor.startMonitoring(std::chrono::seconds(1), std::chrono::hours(1));

    auto& validator = InputValidator::getInstance();

    {
        AIMUX_TRACK_OPERATION("validation", "api_request_schema");

        json schema = {
            {"type", "object"},
            {"required", {"email", "age"}},
            {"properties", {
                {"email", {{"type", "string"}}},
                {"age", {
                    {"type", "number"},
                    {"minimum", 0},
                    {"maximum", 120}
                }}
            }}
        };

        json test_data = {
            {"email", "test@example.com"},
            {"age", 25}
        };

        ValidationContext context = InputValidator::createDevelopmentContext();

        auto result = validator.validateJson(test_data, schema, context);
        printValidationResult(result, "Performance-tracked validation");

        // Record validation performance
        monitor.recordProviderRequest("validation_engine", "json_schema",
                                    5.2, result.isValid(), "", 0.0);
    }

    // Get performance report
    auto perf_report = monitor.getPerformanceSummary();
    std::cout << "\nPerformance Summary:\n" << perf_report.dump(2) << "\n";
}

void demonstrateCustomValidation() {
    std::cout << "\n🔧 CUSTOM VALIDATION RULES\n";

    auto& validator = InputValidator::getInstance();

    // Register custom rule
    validator.registerRule("user_id_validation", std::make_unique<UserIdValidationRule>());

    // Create JSON schema that uses custom validation logic
    json user_schema = {
        {"type", "object"},
        {"required", {"user_id", "user_info"}},
        {"properties", {
            {"user_id", {
                {"type", "string"},
                {"description", "System user identifier with custom validation"}
            }},
            {"user_info", {
                {"type", "object"},
                {"properties", {
                    {"username", {
                        {"type", "string"},
                        {"minLength", 3},
                        {"maxLength", 20}
                    }},
                    {"email", {
                        {"type", "string"},
                        {"pattern", R"(^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$)"}
                    }}
                }}
            }}
        }}
    };

    ValidationContext context = InputValidator::createDevelopmentContext();

    std::cout << "\n1. Custom User ID Validation:";

    json valid_user = {
        {"user_id", "user_john_doe_123"},
        {"user_info", {
            {"username", "john_doe"},
            {"email", "john@example.com"}
        }}
    };

    auto result1 = validator.validateJson(valid_user, user_schema, context);
    printValidationResult(result1, "User with valid custom ID");

    std::cout << "\n2. Invalid Custom User ID:";

    json invalid_user = {
        {"user_id", "invalid_user_id_format"},
        {"user_info", {
            {"username", "john"},
            {"email", "invalid-email"}
        }}
    };

    auto result2 = validator.validateJson(invalid_user, user_schema, context);
    printValidationResult(result2, "User with invalid format");

    // Apply custom rule manually to specific field
    UserIdValidationRule custom_rule;
    auto result3 = custom_rule.validate(json("user_12345"), context);
    printValidationResult(result3, "Direct custom rule application");
}

void demonstratePresetConfigurations() {
    std::cout << "\n📦 PRESET CONFIGURATION EXAMPLES\n";

    auto& validator = InputValidator::getInstance();
    ValidationContext context = InputValidator::createProductionContext();

    // API username validation
    std::cout << "\n1. Username Preset:";

    auto username_config = InputValidator::Presets::createUsernameConfig();
    auto result1 = validator.validateString("valid_user123", username_config, context);
    printValidationResult(result1, "Username validation with preset");

    auto result2 = validator.validateString("ab", username_config, context);
    printValidationResult(result2, "Username too short for preset");

    // API key validation
    std::cout << "\n2. API Key Preset:";

    auto api_key_config = InputValidator::Presets::createApiKeyConfig();
    auto result3 = validator.validateApiKey("sk_valid1234567890123456", api_key_config, context);
    printValidationResult(result3, "API key validation with preset");

    // Email validation
    std::cout << "\n3. Email Preset:";

    auto email_config = InputValidator::Presets::createStandardEmailConfig();
    auto result4 = validator.validateEmail("user@legitdomain.com", email_config, context);
    printValidationResult(result4, "Email validation with standard preset");

    auto result5 = validator.validateEmail("user@spam.example", email_config, context);
    printValidationResult(result5, "Email with issues");
}

void demonstrateErrorFormatting() {
    std::cout << "\n📝 ERROR FORMATTING AND RESPONSE\n";

    auto& validator = InputValidator::getInstance();

    // Create a scenario with multiple validation errors
    json complex_schema = {
        {"type", "object"},
        {"required", {"name", "email", "age", "preferences"}},
        {"properties", {
            {"name", {
                {"type", "string"},
                {"minLength", 2},
                {"maxLength", 50}
            }},
            {"email", {
                {"type", "string"},
                {"pattern", R"(^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$)"}
            }},
            {"age", {
                {"type", "number"},
                {"minimum": 13},
                {"maximum": 120}
            }},
            {"preferences", {
                {"type", "array"},
                {"items", {
                    {"type", "string"},
                    {"enum", {"sports", "music", "movies", "reading"}}
                }}
            }}
        }}
    };

    json problematic_data = {
        {"name", "A"},
        {"email", "not-an-email"},
        {"age", 10},
        {"preferences", {"sports", "invalid_hobby"}},
        {"extra_field", "should not be here"}
    };

    ValidationContext context = InputValidator::createDevelopmentContext();
    auto result = validator.validateJson(problematic_data, complex_schema, context);

    std::cout << "\n1. Formatted Error Response:";
    printValidationResult(result, "Complex validation with multiple errors");

    // Create JSON response suitable for API
    json api_response;
    if (!result.isValid()) {
        api_response = {
            {"success", false},
            {"error", "Validation failed"},
            {"validation_errors", json::array()},
            {"field_errors", json::object()},
            {"suggestions", json::array()}
        };

        // Categorize errors by field
        for (const auto& error : result.errors) {
            api_response["validation_errors"].push_back(error.toJson());

            if (!api_response["field_errors"].contains(error.field_path)) {
                api_response["field_errors"][error.field_path] = json::array();
            }
            api_response["field_errors"][error.field_path].push_back({
                {"type", error.error_type},
                {"message", error.message},
                {"suggestion", error.suggestion}
            });

            if (!error.suggestion.empty()) {
                api_response["suggestions"].push_back(error.suggestion);
            }
        }

        // Add warnings
        if (!result.warnings.empty()) {
            api_response["warnings"] = json::array();
            for (const auto& warning : result.warnings) {
                api_response["warnings"].push_back(warning.toJson());
            }
        }

        // Add sanitized data if available
        if (!result.sanitized_data.empty()) {
            api_response["sanitized_data"] = result.sanitized_data;
        }
    } else {
        api_response = {
            {"success", true},
            {"validated_data", result.sanitized_data},
            {"warnings", json::array()}
        };

        for (const auto& warning : result.warnings) {
            api_response["warnings"].push_back(warning.toJson());
        }
    }

    std::cout << "\n2. Complete API Error Response:\n";
    std::cout << api_response.dump(2) << "\n";
}

int main() {
    std::cout << "🚀 Aimux Input Validation Framework Examples\n";
    std::cout << "========================================\n";

    try {
        demonstrateBasicValidation();
        demonstrateSecuritySanitization();
        demonstrateComplexObjectValidation();
        demonstratePerformanceIntegration();
        demonstrateCustomValidation();
        demonstratePresetConfigurations();
        demonstrateErrorFormatting();

        std::cout << "\n✅ All validation examples completed successfully!\n";
        std::cout << "\nKey Features Demonstrated:\n";
        std::cout << "• Multi-type field validation (string, email, API key, etc.)\n";
        std::cout << "• Security-focused input sanitization\n";
        std::cout << "• Complex object validation with JSON schemas\n";
        std::cout << "• Performance monitoring integration\n";
        std::cout << "• Custom validation rule registration\n";
        std::cout << "• Production-ready preset configurations\n";
        std::cout << "• Detailed error formatting and API responses\n";
        std::cout << "• Thread-safe concurrent validation\n";
        std::cout << "• Development vs production context handling\n";

    } catch (const std::exception& e) {
        std::cerr << "❌ Error running validation examples: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}