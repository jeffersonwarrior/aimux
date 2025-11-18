# Security Regression Test Suite

## Overview
This document defines comprehensive security regression tests to prevent common vulnerabilities and ensure robust security practices within the Aimux codebase.

## Test Categories

### 1. XSS Injection Attack Prevention Tests

#### 1.1 HTTP Response XSS Prevention
**Objective**: Verify that all HTTP responses properly sanitize user input to prevent XSS attacks.

**Test Cases**:
- **Script Injection**: Input containing `<script>alert('xss')</script>` should be escaped
- **Event Handler Injection**: Input with `onload="alert('xss')"` should be sanitized
- **JavaScript Protocol**: `javascript:alert('xss')` URLs should be blocked/escaped
- **HTML Entity Encoding**: Verify proper encoding of `<`, `>`, `&`, `"`, `'`

**Implementation Test**:
```cpp
// test/xss_prevention_test.cpp
TEST(SecurityRegressionTest, PreventScriptInjection) {
    std::string malicious_input = "<script>alert('xss')</script>";
    std::string sanitized = input_validator::sanitize_html(malicious_input);
    EXPECT_THAT(sanitized, Not(HasSubstr("<script>")));
    EXPECT_THAT(sanitized, Not(HasSubstr("alert")));
}
```

**Pass Criteria**:
- All malicious scripts are properly escaped or removed
- No executable JavaScript remains in output
- HTML entities are correctly encoded

#### 1.2 JSON Response XSS Prevention
**Objective**: Ensure JSON responses don't contain executable content when rendered as HTML.

**Test Cases**:
- JSON content with HTML tags should be properly escaped
- Content-Type headers should be set to `application/json`
- Response should begin with `)]}',` prefix for JSONP protection

### 2. SQL Injection Validation Tests

#### 2.1 Parameterized Query Enforcement
**Objective**: Verify all database operations use parameterized queries.

**Test Cases**:
- All database queries use prepared statements
- No string concatenation for SQL queries
- Input parameters are properly bound

**Implementation Test**:
```cpp
// test/sql_injection_test.cpp
TEST(SecurityRegressionTest, EnforceParameterizedQueries) {
    std::string user_input = "'; DROP TABLE users; --";
    auto query = db.prepare("SELECT * FROM users WHERE name = ?");
    query.bind(0, user_input);

    // Should execute safely without dropping table
    EXPECT_NO_THROW(query.execute());
}
```

#### 2.2 Input Validation for SQL Contexts
**Objective**: Validate inputs before they reach database layer.

**Test Cases**:
- SQL keywords are blocked in user inputs
- Special characters are properly escaped
- Input length limits are enforced

### 3. Path Traversal Attack Tests

#### 3.1 File System Access Validation
**Objective**: Prevent directory traversal attacks in file operations.

**Test Cases**:
- `../../../etc/passwd` should be blocked
- Absolute paths should be rejected in user input
- Current directory references `./` should be controlled
- Windows path traversal `..\..\..\` should be blocked

**Implementation Test**:
```cpp
// test/path_traversal_test.cpp
TEST(SecurityRegressionTest, PreventDirectoryTraversal) {
    std::vector<std::string> malicious_paths = {
        "../../../etc/passwd",
        "..\\..\\..\\windows\\system32\\config\\sam",
        "/etc/shadow",
        "file:///etc/passwd"
    };

    for (const auto& path : malicious_paths) {
        EXPECT_FALSE(file_system::is_safe_path(path))
            << "Path should be blocked: " << path;
    }
}
```

**Pass Criteria**:
- All path traversal attempts are blocked
- Only allowed directory structures are accessible
- File access is limited to intended directories

#### 3.2 Resource Loading Security
**Objective**: Secure WebUI resource loading from traversal attacks.

**Test Cases**:
- Resource loader validates file paths
- Only whitelisted file extensions are served
- MIME types are validated against file content

### 4. Buffer Overflow Protection Tests

#### 4.1 String Input Validation
**Objective**: Ensure all string inputs are properly validated for length.

**Test Cases**:
- Input strings longer than defined limits should be rejected
- Buffer boundaries are checked before copy operations
- String functions use size limits

**Implementation Test**:
```cpp
// test/buffer_overflow_test.cpp
TEST(SecurityRegressionTest, PreventStringBufferOverflow) {
    std::string oversized_input(10000, 'A');  // Larger than expected limit
    EXPECT_THROW(config_validator::validate_api_key(oversized_input),
                 ValidationError);
}
```

#### 4.2 Binary Data Handling
**Objective**: Safe handling of binary data that could cause buffer overflows.

**Test Cases**:
- Binary uploads have size limits
- Memory allocation checks before processing
- Safe copy operations with bounds checking

### 5. Input Sanitization Validation Tests

#### 5.1 API Input Sanitization
**Objective**: Validate all API inputs are properly sanitized.

**Test Cases**:
- Control characters are stripped/escaped
- Unicode normalization for security issues
- Null byte injection prevention
- HTTP header injection prevention

**Implementation Test**:
```cpp
// test/input_sanitization_test.cpp
TEST(SecurityRegressionTest, SanitizeAPIInput) {
    std::vector<std::string> malicious_inputs = {
        "test\x00malicious",
        "test\r\nSet-Cookie: evil=true",
        "test\uFEFFunicode_attack",
        "<script>alert('xss')</script>"
    };

    for (const auto& input : malicious_inputs) {
        auto sanitized = input_validator::sanitize_api_input(input);
        EXPECT_FALSE(input_validator::contains_control_characters(sanitized));
        EXPECT_FALSE(input_validator::contains_null_bytes(sanitized));
    }
}
```

#### 5.2 Configuration File Security
**Objective**: Secure parsing of configuration files.

**Test Cases**:
- Config parser rejects malicious JSON structures
- Environment variable injection is prevented
- Include file attacks are blocked

### 6. Memory Corruption Detection Tests

#### 6.1 Dynamic Memory Safety
**Objective**: Detect memory corruption in dynamic allocations.

**Test Cases**:
- Double free detection
- Use-after-free detection
- Memory leak detection
- Heap corruption detection

**Implementation with Sanitizers**:
```cpp
// test/memory_corruption_test.cpp
TEST(SecurityRegressionTest, DetectMemoryCorruption) {
    // This test should run under AddressSanitizer
    char* ptr = new char[100];
    delete[] ptr;

    // This should trigger AddressSanitizer if not caught properly
    EXPECT_DEATH(ptr[0] = 'x', "heap-use-after-free");
}
```

#### 6.2 Stack Corruption Prevention
**Objective**: Prevent stack-based vulnerabilities.

**Test Cases**:
- Stack buffer overflow prevention
- Return address protection
- Stack canary validation

## Test Execution Requirements

### Environment Setup
- Compile with `-fsanitize=address` for memory safety
- Use Valgrind for additional memory checks
- Enable all compiler security flags:
  ```bash
  -D_FORTIFY_SOURCE=2 -fstack-protector-strong -Wformat -Wformat-security
  ```

### Test Runner Integration
```bash
# Run all security regression tests
make test_security

# Run with sanitizers
make test_security_sanitized

# Continuous integration check
make security_regression_check
```

### Performance Requirements
- All security tests must complete within 30 seconds
- No false positives in legitimate use cases
- Performance impact < 5% for all security validations

## Automated Checking

### Static Analysis Integration
- clang-tidy security checks enabled
- cppcheck security analysis
- Flawfinder vulnerability scanning

### Dynamic Analysis
- AddressSanitizer integration
- Valgrind memory checking
- Fuzzing integration for input parsing

## Success Criteria

### Must Pass
- All XSS prevention tests
- SQL injection validation tests
- Path traversal prevention tests
- Buffer overflow protection tests

### Should Pass
- All input sanitization tests
- Memory corruption detection tests

### Continuous Monitoring
- Security test coverage > 90%
- No new security vulnerabilities introduced
- All findings from security scans address

## Documentation Requirements

### Test Documentation
- Each test must have clear security rationale
- Test cases must cover real attack scenarios
- False positive handling documented

### Reporting
- Security test results included in CI/CD pipeline
- Vulnerability findings tracked to resolution
- Security metrics reported regularly