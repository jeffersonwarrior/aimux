# Aimux2 QA Procedures

## Table of Contents
1. [Installation Testing](#installation-testing)
2. [Build System Testing](#build-system-testing)
3. [Integration Testing](#integration-testing)
4. [Performance Testing](#performance-testing)
5. [Configuration Testing](#configuration-testing)
6. [Security Testing](#security-testing)
7. [Multi-Platform Testing](#multi-platform-testing)
8. [Release Validation](#release-validation)
9. [QA Checklist](#qa-checklist)

---

## Installation Testing

### GitHub Release Installation
**Script**: `qa/installation_test.sh`
**Purpose**: Validate npm global installation from GitHub releases

```bash
#!/bin/bash
echo "🧪 Testing Aimux2 GitHub Release Installation..."

# Clean up existing installation
npm uninstall -g aimux2 2>/dev/null || true
rm -rf ~/.npm/_cacache/tmp/git-* 2>/dev/null || true

# Test GitHub release installation
echo "📦 Installing from GitHub Release..."
npm install -g https://github.com/aimux/aimux2/releases/latest/download/aimux2.tgz

# Validate installation
if command -v aimux2 &> /dev/null; then
    echo "✅ SUCCESS: aimux2 command available"
    aimux2 --version
    aimux2 --test
else
    echo "❌ FAILED: aimux2 command not found"
    exit 1
fi
```

### Binary Installation Test
**Purpose**: Test standalone binary installation and execution
**Validation**: Binary functionality, version detection, help system

---

## Build System Testing

### CMake Build Validation
**Script**: `qa/build_test.sh`
**Purpose**: Validate CMake build system with all dependencies

```bash
#!/bin/bash
echo "🔨 Testing CMake Build System..."

# Clean build
rm -rf build-debug build-release

# Test Debug Build
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug

# Test Release Build  
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release

# Validate binaries
./build-debug/aimux --test
./build-release/aimux --test

echo "✅ Build system validation complete"
```

### Dependency Verification
**Purpose**: Ensure all vcpkg dependencies are properly installed
**Validation**: nlohmann-json, curl, threads, crow

---

## Integration Testing

### HTTP Client Integration Tests
**File**: `tests/integration/test_http_client_comprehensive.cpp`
**Coverage**: 
- Basic GET/POST requests
- Timeout functionality
- SSL verification
- Custom headers
- Error handling
- Concurrent requests

### Provider Integration Tests
**File**: `tests/integration/test_providers_comprehensive.cpp`
**Coverage**:
- Provider factory creation
- Synthetic provider functionality
- Rate limiting
- Concurrent requests
- Error handling
- Configuration validation

### Running Integration Tests
```bash
# HTTP Client Tests
./build-debug/aimux --test-http-client

# Provider Tests
./build-debug/aimux --test-providers

# All Integration Tests
./build-debug/aimux --test-all
```

---

## Performance Testing

### Benchmark Suite
**Script**: `qa/performance_test.sh`

```bash
#!/bin/bash
echo "🚀 Performance Testing..."

# HTTP Client Performance
./build-debug/aimux --benchmark-http-client --requests 1000

# Provider Performance
./build-debug/aimux --benchmark-providers --providers cerebras,zai,minimax

# Router Performance
./build-debug/aimux --benchmark-router --concurrent-requests 50

# Memory Usage Test
./build-debug/aimux --memory-test --duration 300

echo "✅ Performance testing complete"
```

### Performance Targets
- **Response Time**: <100ms for routing decisions
- **Memory Usage**: <50MB steady state
- **Throughput**: >100 requests/second
- **Concurrent Requests**: Handle 50+ simultaneous connections

---

## Configuration Testing

### Migration Tool Testing
**Script**: `qa/config_migration_test.sh`

```bash
#!/bin/bash
echo "⚙️ Configuration Migration Testing..."

# Test JSON to JSON migration
./build-debug/aimux migrate config_v1.json config_v2.json

# Validate migrated config
./build-debug/aimux validate config_v2.json

# Test provider configuration loading
./build-debug/aimux --config config_v2.json --test-providers

echo "✅ Configuration testing complete"
```

### Configuration Validation
**Test Cases**:
- Invalid API keys
- Missing required fields
- Malformed JSON
- Invalid provider names
- Rate limit validation

---

## Security Testing

### API Key Security Test
**Script**: `qa/security_test.sh`

```bash
#!/bin/bash
echo "🔒 Security Testing..."

# Test API key masking
./build-debug/aimux --test-api-key-masking

# Test SSL certificate validation
./build-debug/aimux --test-ssl-validation

# Test input sanitization
./build-debug/aimux --test-input-sanitization

# Test authentication failure
./build-debug/aimux --test-auth-failure

echo "✅ Security testing complete"
```

### Security Validation
- API key encryption at rest
- SSL/TLS certificate verification
- Input sanitization and validation
- Authentication failure handling
- Rate limiting abuse prevention

---

## Multi-Platform Testing

### Platform Validation Matrix
| Platform | Build | Tests | WebUI | Notes |
|----------|-------|--------|-------|-------|
| macOS (arm64) | ✅ | ✅ | ✅ | Primary development |
| macOS (x64) | 🔄 | 🔄 | 🔄 | CI validation needed |
| Linux (x64) | 🔄 | 🔄 | 🔄 | Docker testing |
| Windows (x64) | 🔄 | 🔄 | 🔄 | MSVC compilation |

### Cross-Platform Test Script
**Script**: `qa/cross_platform_test.sh`

---

## Release Validation

### Release Checklist
**Script**: `qa/release_validation.sh`

```bash
#!/bin/bash
echo "🎉 Release Validation..."

# Version consistency check
./build-debug/aimux --version
cat version.txt

# Full test suite
./qa/run_all_tests.sh

# Documentation validation
./qa/check_documentation.sh

# Performance regression test
./qa/performance_regression_test.sh

# Security scan
./qa/security_scan.sh

echo "✅ Release validation complete"
```

### Release Criteria
- ✅ All tests passing
- ✅ Performance targets met
- ✅ Security scans clean
- ✅ Documentation complete
- ✅ Installation procedures verified

---

## QA Checklist

### Pre-Release Checklist
- [ ] Build system working on all target platforms
- [ ] All unit tests passing (100% coverage)
- [ ] All integration tests passing
- [ ] Performance benchmarks meeting targets
- [ ] Security tests passing
- [ ] Configuration migration working
- [ ] WebUI functional on all browsers
- [ ] Documentation complete and accurate
- [ ] Installation procedures validated
- [ ] Release notes prepared

### Continuous Integration Checklist
- [ ] Code quality checks passing (clang-format, clang-tidy)
- [ ] Static analysis clean (cppcheck, sonarqube)
- [ ] Memory leak checks passing (valgrind)
- [ ] Thread safety validation (helgrind)
- [ ] Dependency vulnerability scan clean
- [ ] License compliance verified

### Post-Release Checklist
- [ ] Release published to all channels
- [ ] Documentation updated and published
- [ ] Migration guides available
- [ ] Support procedures documented
- [ ] Monitoring and alerting configured
- [ ] User feedback collection mechanisms active

---

## Running QA Procedures

### Quick Test Run
```bash
# Run all QA procedures
./qa/run_all_qa.sh

# Run specific test suite
./qa/run_integration_tests.sh
./qa/run_performance_tests.sh
./qa/run_security_tests.sh
```

### Detailed Test Report
```bash
# Generate comprehensive QA report
./qa/generate_qa_report.sh --output qa_report.html
```

### Continuous Monitoring
```bash
# Set up continuous QA monitoring
./qa/setup_monitoring.sh
```

---

## QA Test Results Tracking

### Test History
Track all test results in `qa/test_history.json`:

```json
{
  "test_run": "2025-11-12T16:30:00Z",
  "version": "2.0.0",
  "results": {
    "unit_tests": {"passed": 45, "failed": 0, "coverage": "98%"},
    "integration_tests": {"passed": 12, "failed": 0},
    "performance_tests": {"passed": 8, "failed": 0, "targets_met": true},
    "security_tests": {"passed": 6, "failed": 0},
    "build_tests": {"passed": 3, "failed": 0}
  },
  "overall_status": "PASS"
}
```

### Test Result Visualization
Generate HTML reports with charts and trends:
```bash
./qa/visualize_results.sh --input qa/test_history.json --output qa/trends.html
```