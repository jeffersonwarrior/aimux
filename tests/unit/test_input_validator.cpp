#include <gtest/gtest.h>
#include "aimux/validation/input_validator.hpp"
#include <nlohmann/json.hpp>

using namespace aimux::validation;

class InputValidatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        validator = &InputValidator::getInstance();
    }

    InputValidator* validator;
};

// Test Validation Error
TEST_F(InputValidatorTest, ValidationErrorCreation) {
    ValidationError error(ValidationStatus::ERROR, "user.email", "invalid_format",
                         "Invalid email format", "user@domain.com", "invalid-email",
                         "Use valid email format");

    EXPECT_EQ(error.status, ValidationStatus::ERROR);
    EXPECT_EQ(error.field_path, "user.email");
    EXPECT_EQ(error.error_type, "invalid_format");
    EXPECT_EQ(error.message, "Invalid email format");
    EXPECT_EQ(error.expected_value, "user@domain.com");
    EXPECT_EQ(error.actual_value, "invalid-email");
    EXPECT_EQ(error.suggestion, "Use valid email format");

    nlohmann::json json_error = error.toJson();
    EXPECT_TRUE(json_error.contains("status"));
    EXPECT_TRUE(json_error.contains("field_path"));
    EXPECT_TRUE(json_error.contains("error_type"));
    EXPECT_TRUE(json_error.contains("message"));
}

// Test Validation Result
TEST_F(InputValidatorTest, ValidationResultManagement) {
    ValidationResult result;

    EXPECT_TRUE(result.isValid());
    EXPECT_FALSE(result.hasWarnings());
    EXPECT_EQ(result.totalIssues(), 0);

    // Add errors
    ValidationError error1(ValidationStatus::ERROR, "field1", "required",
                          "Field is required", "value", "", "Add value");
    result.addError(error1);

    EXPECT_FALSE(result.isValid());
    EXPECT_EQ(result.totalIssues(), 1);
    EXPECT_EQ(result.errors.size(), 1);

    // Add warning
    ValidationError warning(ValidationStatus::WARNING, "field2", "deprecated",
                           "Field is deprecated", "new_field", "old_field", "Use new field");
    result.addError(warning);

    EXPECT_FALSE(result.isValid());
    EXPECT_TRUE(result.hasWarnings());
    EXPECT_EQ(result.totalIssues(), 2);
    EXPECT_EQ(result.errors.size(), 1);
    EXPECT_EQ(result.warnings.size(), 1);
}

// Test String Validation
TEST_F(InputValidatorTest, StringValidationBasic) {
    StringValidation::Config config;
    config.min_length = 5;
    config.max_length = 20;

    // Valid string
    auto result = StringValidation::validate("valid_string", config);
    EXPECT_TRUE(result.isValid());

    // Too short
    result = StringValidation::validate("short", config);
    EXPECT_FALSE(result.isValid());
    EXPECT_EQ(result.errors[0].error_type, "min_length");

    // Too long
    std::string long_string(30, 'a');
    result = StringValidation::validate(long_string, config);
    EXPECT_FALSE(result.isValid());
    EXPECT_EQ(result.errors[0].error_type, "max_length");
}

TEST_F(InputValidatorTest, StringValidationPattern) {
    StringValidation::Config config;
    config.pattern = R"(^[A-Z][a-z]+$)";

    auto result = StringValidation::validate("Valid", config);
    EXPECT_TRUE(result.isValid());

    result = StringValidation::validate("invalid", config);
    EXPECT_FALSE(result.isValid());
    EXPECT_EQ(result.errors[0].error_type, "pattern_mismatch");

    result = StringValidation::validate("Invalid123", config);
    EXPECT_FALSE(result.isValid());
}

TEST_F(InputValidatorTest, StringValidationEnum) {
    StringValidation::Config config;
    config.allowed_values = {"GET", "POST", "PUT", "DELETE"};

    auto result = StringValidation::validate("GET", config);
    EXPECT_TRUE(result.isValid());

    result = StringValidation::validate("PATCH", config);
    EXPECT_FALSE(result.isValid());
    EXPECT_EQ(result.errors[0].error_type, "enum_mismatch");
}

TEST_F(InputValidatorTest, StringValidationSanitization) {
    StringValidation::Config config;
    config.trim_whitespace = true;
    config.lowercase = true;
    config.sanitize_html = true;

    ValidationContext context;
    context.sanitize_input = true;

    auto result = StringValidation::validate("  HELLO <script>alert('xss')</script> WORLD  ", config, context);
    EXPECT_TRUE(result.isValid());
    EXPECT_EQ(result.sanitized_data.get<std::string>(), "hello alert('xss') world");
}

// Test Numeric Validation
TEST_F(InputValidatorTest, NumericValidationBasic) {
    NumericValidation::Config config;
    config.min_value = 0.0;
    config.max_value = 100.0;

    // Valid number
    auto result = NumericValidation::validate(50.0, config);
    EXPECT_TRUE(result.isValid());

    // Too small
    result = NumericValidation::validate(-10.0, config);
    EXPECT_FALSE(result.isValid());
    EXPECT_EQ(result.errors[0].error_type, "min_value");

    // Too large
    result = NumericValidation::validate(150.0, config);
    EXPECT_FALSE(result.isValid());
    EXPECT_EQ(result.errors[0].error_type, "max_value");
}

TEST_F(InputValidatorTest, NumericValidationInteger) {
    NumericValidation::Config config;
    config.integer_only = true;

    auto result = NumericValidation::validate(42.0, config);
    EXPECT_TRUE(result.isValid());

    result = NumericValidation::validate(42.5, config);
    EXPECT_FALSE(result.isValid());
    EXPECT_EQ(result.errors[0].error_type, "integer_required");
}

TEST_F(InputValidatorTest, NumericValidationZeroNegative) {
    NumericValidation::Config config;
    config.allow_zero = false;
    config.allow_negative = false;

    auto result = NumericValidation::validate(5.0, config);
    EXPECT_TRUE(result.isValid());

    result = NumericValidation::validate(0.0, config);
    EXPECT_FALSE(result.isValid());
    EXPECT_EQ(result.errors[0].error_type, "zero_not_allowed");

    result = NumericValidation::validate(-5.0, config);
    EXPECT_FALSE(result.isValid());
    EXPECT_EQ(result.errors[0].error_type, "negative_not_allowed");
}

// Test Email Validation
TEST_F(InputValidatorTest, EmailValidationBasic) {
    EmailValidation::Config config;

    auto result = EmailValidation::validate("user@example.com", config);
    EXPECT_TRUE(result.isValid());

    result = EmailValidation::validate("invalid-email", config);
    EXPECT_FALSE(result.isValid());
    EXPECT_EQ(result.errors[0].error_type, "invalid_format");
}

TEST_F(InputValidatorTest, EmailValidationTLD) {
    EmailValidation::Config config;
    config.require_tld = true;

    auto result = EmailValidation::validate("user@domain.com", config);
    EXPECT_TRUE(result.isValid());

    result = EmailValidation::validate("user@localhost", config);
    EXPECT_FALSE(result.isValid());
    EXPECT_EQ(result.errors[0].error_type, "missing_tld");
}

TEST_F(InputValidatorTest, EmailValidationDomain) {
    EmailValidation::Config config;
    config.allowed_domains = {"example.com", "test.com"};

    auto result = EmailValidation::validate("user@example.com", config);
    EXPECT_TRUE(result.isValid());

    result = EmailValidation::validate("user@other.com", config);
    EXPECT_FALSE(result.isValid());
    EXPECT_EQ(result.errors[0].error_type, "domain_not_allowed");
}

TEST_F(InputValidatorTest, EmailValidationBlockedDomain) {
    EmailValidation::Config config;
    config.blocked_domains = {"spam.com", "malicious.com"};

    auto result = EmailValidation::validate("user@legit.com", config);
    EXPECT_TRUE(result.isValid());

    result = EmailValidation::validate("user@spam.com", config);
    EXPECT_FALSE(result.isValid());
    EXPECT_EQ(result.errors[0].error_type, "domain_blocked");
}

// Test URL Validation
TEST_F(InputValidatorTest, UrlValidationBasic) {
    UrlValidation::Config config;

    auto result = UrlValidation::validate("https://example.com/path", config);
    EXPECT_TRUE(result.isValid());

    result = UrlValidation::validate("not-a-url", config);
    EXPECT_FALSE(result.isValid());
    EXPECT_EQ(result.errors[0].error_type, "invalid_format");
}

TEST_F(InputValidatorTest, UrlValidationScheme) {
    UrlValidation::Config config;
    config.allowed_schemes = {"https"};

    auto result = UrlValidation::validate("https://example.com", config);
    EXPECT_TRUE(result.isValid());

    result = UrlValidation::validate("http://example.com", config);
    EXPECT_FALSE(result.isValid());
    EXPECT_EQ(result.errors[0].error_type, "invalid_scheme");
}

TEST_F(InputValidatorTest, UrlValidationSSL) {
    UrlValidation::Config config;
    config.require_ssl = true;

    auto result = UrlValidation::validate("https://example.com", config);
    EXPECT_TRUE(result.isValid());

    result = UrlValidation::validate("http://example.com", config);
    EXPECT_FALSE(result.isValid());
    EXPECT_EQ(result.errors[0].error_type, "ssl_required");
}

// Test API Key Validation
TEST_F(InputValidatorTest, ApiKeyValidationBasic) {
    ApiKeyValidation::Config config;

    auto result = ApiKeyValidation::validate("sk-valid1234567890123456", config);
    EXPECT_TRUE(result.isValid());

    result = ApiKeyValidation::validate("short", config);
    EXPECT_FALSE(result.isValid());
    EXPECT_EQ(result.errors[0].error_type, "min_length");
}

TEST_F(InputValidatorTest, ApiKeyValidationPattern) {
    ApiKeyValidation::Config config;
    config.pattern = R"(^sk_[a-zA-Z0-9]{24}$)";

    auto result = ApiKeyValidation::validate("sk_abcdefghijklmnop123456", config);
    EXPECT_TRUE(result.isValid());

    result = ApiKeyValidation::validate("invalid-key", config);
    EXPECT_FALSE(result.isValid());
    EXPECT_EQ(result.errors[0].error_type, "pattern_mismatch");
}

TEST_F(InputValidatorTest, ApiKeyValidationForbiddenPatterns) {
    ApiKeyValidation::Config config;
    config.forbidden_patterns = {"password", "secret"};

    auto result = ApiKeyValidation::validate("sk-valid123456", config);
    EXPECT_TRUE(result.isValid());

    result = ApiKeyValidation::validate("sk_password123", config);
    EXPECT_FALSE(result.isValid());
    EXPECT_EQ(result.errors[0].error_type, "forbidden_pattern");
}

// Test JSON Schema Validation
TEST_F(InputValidatorTest, JsonSchemaValidationType) {
    JsonSchemaValidation::Config config;
    config.schema = {{"type", "object"}};

    auto result = JsonSchemaValidation::validate(nlohmann::json::object(), config);
    EXPECT_TRUE(result.isValid());

    result = JsonSchemaValidation::validate(nlohmann::json::array(), config);
    EXPECT_FALSE(result.isValid());
    EXPECT_EQ(result.errors[0].error_type, "type_mismatch");
}

TEST_F(InputValidatorTest, JsonSchemaValidationRequired) {
    JsonSchemaValidation::Config config;
    config.schema = {
        {"type", "object"},
        {"required", nlohmann::json::array({"name", "email"})},
        {"properties", {
            {"name", {{"type", "string"}}},
            {"email", {{"type", "string"}}}
        }}
    };

    nlohmann::json valid_data = {{"name", "John"}, {"email", "john@example.com"}};
    auto result = JsonSchemaValidation::validate(valid_data, config);
    EXPECT_TRUE(result.isValid());

    nlohmann::json invalid_data = {{"name", "John"}};
    result = JsonSchemaValidation::validate(invalid_data, config);
    EXPECT_FALSE(result.isValid());
    EXPECT_EQ(result.errors[0].error_type, "required_field_missing");
}

// Test InputValidator Main Class
TEST_F(InputValidatorTest, InputValidatorString) {
    StringValidation::Config config;
    config.min_length = 3;

    auto result = validator->validateString("test", config);
    EXPECT_TRUE(result.isValid());

    result = validator->validateString("", config);
    EXPECT_FALSE(result.isValid());
}

TEST_F(InputValidatorTest, InputValidatorNumber) {
    NumericValidation::Config config;
    config.min_value = 0;

    auto result = validator->validateNumber(10.0, config);
    EXPECT_TRUE(result.isValid());

    result = validator->validateNumber(-5.0, config);
    EXPECT_FALSE(result.isValid());
}

TEST_F(InputValidatorTest, InputValidatorEmail) {
    EmailValidation::Config config;

    auto result = validator->validateEmail("test@example.com", config);
    EXPECT_TRUE(result.isValid());

    result = validator->validateEmail("invalid", config);
    EXPECT_FALSE(result.isValid());
}

TEST_F(InputValidatorTest, InputValidatorUrl) {
    UrlValidation::Config config;

    auto result = validator->validateUrl("https://example.com", config);
    EXPECT_TRUE(result.isValid());

    result = validator->validateUrl("invalid-url", config);
    EXPECT_FALSE(result.isValid());
}

TEST_F(InputValidatorTest, InputValidatorApiKey) {
    ApiKeyValidation::Config config;

    auto result = validator->validateApiKey("sk_valid1234567890123456", config);
    EXPECT_TRUE(result.isValid());

    result = validator->validateApiKey("short", config);
    EXPECT_FALSE(result.isValid());
}

TEST_F(InputValidatorTest, InputValidatorJson) {
    nlohmann::json schema = {
        {"type", "object"},
        {"properties", {
            {"name", {{"type", "string"}, {"minLength", 1}}},
            {"age", {{"type", "number"}, {"minimum", 0}}}
        }},
        {"required", nlohmann::json::array({"name"})}
    };

    nlohmann::json data = {{"name", "John"}, {"age", 30}};
    auto result = validator->validateJson(data, schema);
    EXPECT_TRUE(result.isValid());

    nlohmann::json invalid_data = {{"age", -5}};
    result = validator->validateJson(invalid_data, schema);
    EXPECT_FALSE(result.isValid());
}

// Test Input Sanitization
TEST_F(InputValidatorTest, StringSanitization) {
    ValidationContext context;
    context.sanitize_input = true;

    auto sanitized = validator->sanitizeString("  <script>alert('xss')</script> Hello World  ", context);
    EXPECT_EQ(sanitized, "alert('xss') Hello World");
}

// Test Presets
TEST_F(InputValidatorTest, ApiRequestPreset) {
    auto preset = InputValidator::Presets::createApiRequestSchema();
    EXPECT_EQ(preset.name, "api_request_schema");
    EXPECT_TRUE(preset.schema.contains("type"));
    EXPECT_TRUE(preset.schema.contains("required"));
    EXPECT_TRUE(preset.schema.contains("properties"));
}

TEST_F(InputValidatorTest, ConfigurationPreset) {
    auto preset = InputValidator::Presets::createConfigurationSchema();
    EXPECT_EQ(preset.name, "config_schema");
    EXPECT_TRUE(preset.schema.contains("properties"));
}

TEST_F(InputValidatorTest, UsernamePreset) {
    auto preset = InputValidator::Presets::createUsernameConfig();
    EXPECT_EQ(preset.min_length, 3);
    EXPECT_EQ(preset.max_length, 50);
    EXPECT_FALSE(preset.pattern.empty());
}

TEST_F(InputValidatorTest, ApiKeyPreset) {
    auto preset = InputValidator::Presets::createApiKeyConfig();
    EXPECT_EQ(preset.min_length, 16);
    EXPECT_EQ(preset.max_length, 256);
    EXPECT_TRUE(preset.sanitize_html);
}

TEST_F(InputValidatorTest, StandardEmailPreset) {
    auto preset = InputValidator::Presets::createStandardEmailConfig();
    EXPECT_TRUE(preset.allow_domain_validation);
    EXPECT_TRUE(preset.require_tld);
}

// Test Context Creation
TEST_F(InputValidatorTest, ProductionContext) {
    auto context = InputValidator::createProductionContext();
    EXPECT_TRUE(context.strict_mode);
    EXPECT_TRUE(context.sanitize_input);
    EXPECT_FALSE(context.detailed_errors);
}

TEST_F(InputValidatorTest, DevelopmentContext) {
    auto context = InputValidator::createDevelopmentContext();
    EXPECT_FALSE(context.strict_mode);
    EXPECT_TRUE(context.sanitize_input);
    EXPECT_TRUE(context.detailed_errors);
}

// Test Complex Validation Scenarios
TEST_F(InputValidatorTest, ComplexObjectValidation) {
    ValidationContext context = InputValidator::createProductionContext();

    nlohmann::json schema = {
        {"type", "object"},
        {"required", nlohmann::json::array({"user", "request"})},
        {"properties", {
            {"user", {
                {"type", "object"},
                {"required", nlohmann::json::array({"email", "username"})},
                {"properties", {
                    {"email", {{"type", "string"}}},
                    {"username", {
                        {"type", "string"},
                        {"minLength", 3},
                        {"maxLength", 20}
                    }}
                }}
            }},
            {"request", {
                {"type", "object"},
                {"required", nlohmann::json::array({"method", "endpoint"})},
                {"properties", {
                    {"method", {
                        {"type", "string"},
                        {"enum", nlohmann::json::array({"GET", "POST"})}
                    }},
                    {"endpoint", {
                        {"type", "string"},
                        {"pattern", R"(^/api/v[\d]+/[\w\-]+)"}
                    }}
                }}
            }}
        }}
    };

    nlohmann::json valid_request = {
        {"user", {
            {"email", "user@example.com"},
            {"username", "validuser"}
        }},
        {"request", {
            {"method", "POST"},
            {"endpoint", "/api/v1/users"}
        }}
    };

    auto result = validator->validateJson(valid_request, schema, context);
    EXPECT_TRUE(result.isValid());

    nlohmann::json invalid_request = {
        {"user", {
            {"email", "invalid-email"},
            {"username", "ab"} // too short
        }},
        {"request", {
            {"method", "INVALID"},
            {"endpoint", "not-a-url"}
        }}
    };

    result = validator->validateJson(invalid_request, schema, context);
    EXPECT_FALSE(result.isValid());
    EXPECT_GT(result.totalIssues(), 2); // Multiple validation errors
}

// Test Performance and Thread Safety
TEST_F(InputValidatorTest, ThreadSafetyValidation) {
    const int num_threads = 10;
    const int operations_per_thread = 100;
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    StringValidation::Config config;
    config.min_length = 5;
    config.max_length = 10;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            for (int j = 0; j < operations_per_thread; ++j) {
                std::string test_string = "test_" + std::to_string(i) + "_" + std::to_string(j);
                auto result = validator->validateString(test_string, config);
                if (result.isValid()) {
                    success_count++;
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(success_count, num_threads * operations_per_thread);
}

// Test Custom Validation Rule
class CustomLengthRule : public ValidationRule {
public:
    ValidationResult validate(const nlohmann::json& value,
                             const ValidationContext& context) const override {
        if (!value.is_string()) {
            return ValidationError(ValidationStatus::ERROR, "field", "type_required",
                                 "String value required", "string", value.type_name());
        }

        std::string str = value.get<std::string>();
        if (str.length() % 2 != 0) {
            return ValidationError(ValidationStatus::WARNING, "field", "odd_length",
                                 "String has odd length", "even length", std::to_string(str.length()),
                                 "Consider using even length strings");
        }

        return ValidationResult();
    }

    std::string getRuleName() const override { return "custom_length_rule"; }
    std::string getDescription() const override { return "Validates string length parity"; }
};

TEST_F(InputValidatorTest, CustomValidationRule) {
    validator->registerRule("custom_length", std::make_unique<CustomLengthRule>());

    // This test shows how custom rules could be used
    // In practice, you'd access the rule through the validator's rule registry
    CustomLengthRule rule;
    ValidationContext context;

    auto result = rule.validate("even", context);
    EXPECT_TRUE(result.isValid()) << "Even length string should pass";

    result = rule.validate("odd", context);
    EXPECT_TRUE(result.isValid()) << "Valid string should pass (with warning)";
}

// Integration Test
TEST_F(InputValidatorTest, FullApiValidationFlow) {
    // Simulate a complete API request validation
    ValidationContext context = InputValidator::createProductionContext();

    // Create comprehensive validation schema
    nlohmann::json api_schema = {
        {"type", "object"},
        {"required", nlohmann::json::array({"request_id", "endpoint", "method"})},
        {"properties", {
            {"request_id", {
                {"type", "string"},
                {"pattern", R"(^[a-f0-9]{8}-[a-f0-9]{4}-4[a-f0-9]{3}-[89ab][a-f0-9]{3}-[a-f0-9]{12}$)"}
            }},
            {"endpoint", {
                {"type", "string"},
                {"minLength", 1},
                {"maxLength", 255}
            }},
            {"method", {
                {"type", "string"},
                {"enum", nlohmann::json::array({"GET", "POST", "PUT", "DELETE"})}
            }},
            {"headers", {
                {"type", "object"},
                {"properties", {
                    {"content-type", {{"type", "string"}}},
                    {"authorization", {{"type", "string"}, {"minLength", 16}}}
                }}
            }},
            {"body", {
                {"type", "object"}
            }}
        }}
    }};

    // Valid API request
    nlohmann::json valid_api_request = {
        {"request_id", "550e8400-e29b-41d4-a716-446655440000"},
        {"endpoint", "/api/v1/models"},
        {"method", "POST"},
        {"headers", {
            {"content-type", "application/json"},
            {"authorization", "Bearer sk_valid1234567890123456"}
        }},
        {"body", {
            {"model", "llama-70b"},
            {"provider", "cerebras"}
        }}
    };

    auto result = validator->validateJson(valid_api_request, api_schema, context);
    EXPECT_TRUE(result.isValid()) << "Valid API request should pass all validations";

    // Validate specific fields with additional validation
    if (valid_api_request.contains("headers") && valid_api_request["headers"].contains("authorization")) {
        ApiKeyValidation::Config api_key_config;
        api_key_config.pattern = R"(^Bearer [a-zA-Z0-9_-]{16,}$)";

        auto auth_header = valid_api_request["headers"]["authorization"].get<std::string>();
        auto api_key_result = validator->validateApiKey(auth_header.substr(7), api_key_config, context);
        EXPECT_TRUE(api_key_result.isValid()) << "API key format should be valid";
    }
}