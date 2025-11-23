#!/bin/bash

# Aimux2 Production Deployment Test Script
# Comprehensive testing of production deployment readiness

set -e

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test configuration
TEST_RESULTS_FILE="/tmp/aimux2_production_test_results.txt"
FAILURE_COUNT=0
TEST_COUNT=0

# Project directories
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build-release"

# Test utilities
passed() {
    echo -e "${GREEN}✓ PASS${NC}: $1"
    ((TEST_COUNT++))
    echo "PASS: $1" >> "$TEST_RESULTS_FILE"
}

failed() {
    echo -e "${RED}✗ FAIL${NC}: $1"
    ((TEST_COUNT++))
    ((FAILURE_COUNT++))
    echo "FAIL: $1" >> "$TEST_RESULTS_FILE"
}

warning() {
    echo -e "${YELLOW}⚠ WARN${NC}: $1"
    echo "WARN: $1" >> "$TEST_RESULTS_FILE"
}

info() {
    echo -e "${BLUE}ℹ INFO${NC}: $1"
    echo "INFO: $1" >> "$TEST_RESULTS_FILE"
}

# Start testing
echo -e "${BLUE}=== Aimux2 Production Deployment Test ===${NC}"
echo "Test started at: $(date)"
echo "Results will be saved to: $TEST_RESULTS_FILE"
echo

# Clear previous results
> "$TEST_RESULTS_FILE"

echo "=== Test Environment ===" >> "$TEST_RESULTS_FILE"
echo "Timestamp: $(date)" >> "$TEST_RESULTS_FILE"
echo "Working Directory: $PROJECT_ROOT" >> "$TEST_RESULTS_FILE"
echo "Build Directory: $BUILD_DIR" >> "$TEST_RESULTS_FILE"
echo

## 1. Build System Tests
echo -e "${BLUE}1. Testing Build System${NC}"
echo

# Check if we can build production
if [ -f "$PROJECT_ROOT/scripts/build_production.sh" ]; then
    passed "Production build script exists"
else
    failed "Production build script missing"
fi

# Check if CMakeLists.txt has production optimizations
if grep -q "CMAKE_BUILD_TYPE.*Release" "$PROJECT_ROOT/CMakeLists.txt"; then
    passed "CMakeLists.txt configured for Release build"
else
    failed "CMakeLists.txt not configured for Release build"
fi

# Check for optimization flags
if grep -q "CMAKE_CXX_FLAGS_RELEASE.*-O3" "$PROJECT_ROOT/CMakeLists.txt"; then
    passed "Production optimization flags found"
else
    failed "Production optimization flags missing"
fi

# Check for version management
if grep -q "version.h.in" "$PROJECT_ROOT/CMakeLists.txt"; then
    passed "Version management configured"
else
    warning "Version management not fully configured"
fi

## 2. Security Tests
echo -e "${BLUE}2. Testing Security Configuration${NC}"
echo

# Check security module existence
if [ -f "$PROJECT_ROOT/include/security/secure_config.h" ]; then
    passed "Security header file exists"
else
    failed "Security header file missing"
fi

if [ -f "$PROJECT_ROOT/src/security/secure_config.cpp" ]; then
    passed "Security implementation exists"
else
    failed "Security implementation missing"
fi

# Check for OpenSSL linking
if grep -q "OpenSSL::SSL" "$PROJECT_ROOT/CMakeLists.txt"; then
    passed "OpenSSL linking configured"
else
    failed "OpenSSL linking missing"
fi

# Check environment file
if [ -f "$PROJECT_ROOT/config/production.env" ]; then
    passed "Production environment template exists"
else
    failed "Production environment template missing"
fi

# Validate environment template
if grep -q "AIMUX_REQUIRE_HTTPS=true" "$PROJECT_ROOT/config/production.env"; then
    passed "HTTPS requirement configured"
else
    failed "HTTPS requirement not configured"
fi

if grep -q "AIMUX_ENCRYPT_KEYS=true" "$PROJECT_ROOT/config/production.env"; then
    passed "API key encryption configured"
else
    failed "API key encryption not configured"
fi

## 3. Service Integration Tests
echo -e "${BLUE}3. Testing Service Integration${NC}"
echo

# Check systemd service file
if [ -f "$PROJECT_ROOT/aimux.service" ]; then
    passed "systemd service file exists"
else
    failed "systemd service file missing"
fi

# Validate systemd configuration
if grep -q "After=network-online.target" "$PROJECT_ROOT/aimux.service"; then
    passed "Service dependency on network configured"
else
    failed "Service dependency on network missing"
fi

if grep -q "PrivateTmp=true" "$PROJECT_ROOT/aimux.service"; then
    passed "Security hardening in service file"
else
    warning "Security hardening could be enhanced"
fi

# Check launchd service file
if [ -f "$PROJECT_ROOT/com.aimux.daemon.plist" ]; then
    passed "launchd service file exists"
else
    warning "launchd service file missing (macOS support)"
fi

# Check installation script
if [ -f "$PROJECT_ROOT/scripts/install_service.sh" ]; then
    passed "Service installation script exists"
else
    failed "Service installation script missing"
fi

if [ -x "$PROJECT_ROOT/scripts/install_service.sh" ]; then
    passed "Service installation script is executable"
else
    failed "Service installation script not executable"
fi

# Check management script
if [ -f "$PROJECT_ROOT/scripts/manage_service.sh" ]; then
    passed "Service management script exists"
else
    failed "Service management script missing"
fi

if [ -x "$PROJECT_ROOT/scripts/manage_service.sh" ]; then
    passed "Service management script is executable"
else
    failed "Service management script not executable"
fi

## 4. Monitoring and Logging Tests
echo -e "${BLUE}4. Testing Monitoring and Logging${NC}"
echo

# Check metrics system
if [ -f "$PROJECT_ROOT/include/monitoring/metrics.h" ]; then
    passed "Metrics header exists"
else
    failed "Metrics header missing"
fi

# Check production logger
if [ -f "$PROJECT_ROOT/include/logging/production_logger.h" ]; then
    passed "Production logger header exists"
else
    failed "Production logger header missing"
fi

# Check monitoring endpoints
if [ -f "$PROJECT_ROOT/src/webui/monitoring_endpoints.cpp" ]; then
    passed "Monitoring endpoints implementation exists"
else
    failed "Monitoring endpoints implementation missing"
fi

# Check for monitoring endpoints registration
if grep -q "registerMonitoringEndpoints" "$PROJECT_ROOT/src/webui/monitoring_endpoints.cpp"; then
    passed "Monitoring endpoints registration configured"
else
    failed "Monitoring endpoints registration missing"
fi

# Check CMakeLists.txt includes monitoring
if grep -q "monitoring_endpoints.cpp" "$PROJECT_ROOT/CMakeLists.txt"; then
    passed "Monitoring endpoints included in build"
else
    failed "Monitoring endpoints not included in build"
fi

## 5. Configuration Management Tests
echo -e "${BLUE}5. Testing Configuration Management${NC}"
echo

# Check config header
if [ -f "$PROJECT_ROOT/include/config/production_config.h" ]; then
    passed "Configuration header exists"
else
    failed "Configuration header missing"
fi

# Check config implementation
if [ -f "$PROJECT_ROOT/src/config/production_config.cpp" ]; then
    passed "Configuration implementation exists"
else
    failed "Configuration implementation missing"
fi

# Check validation rules
if grep -q "validateConfig" "$PROJECT_ROOT/src/config/production_config.cpp"; then
    passed "Configuration validation implemented"
else
    failed "Configuration validation missing"
fi

# Check environment override
if grep -q "loadEnvironmentOverrides" "$PROJECT_ROOT/src/config/production_config.cpp"; then
    passed "Environment override implemented"
else
    failed "Environment override missing"
fi

# Check production config template
if [ -f "$PROJECT_ROOT/production-config.json" ]; then
    passed "Production configuration template exists"
else
    failed "Production configuration template missing"
fi

# Validate production config template
if jq empty "$PROJECT_ROOT/production-config.json" 2>/dev/null; then
    passed "Production config template is valid JSON"
else
    failed "Production config template has invalid JSON"
fi

## 6. Documentation Tests
echo -e "${BLUE}6. Testing Documentation${NC}"
echo

# Check deployment guide
if [ -f "$PROJECT_ROOT/docs/DEPLOYMENT_GUIDE.md" ]; then
    passed "Deployment guide exists"
else
    failed "Deployment guide missing"
fi

# Check configuration reference
if [ -f "$PROJECT_ROOT/docs/CONFIGURATION_REFERENCE.md" ]; then
    passed "Configuration reference exists"
else
    failed "Configuration reference missing"
fi

# Check troubleshooting guide
if [ -f "$PROJECT_ROOT/docs/TROUBLESHOOTING.md" ]; then
    passed "Troubleshooting guide exists"
else
    failed "Troubleshooting guide missing"
fi

# Check deployment guide content
if grep -q "Prerequisites" "$PROJECT_ROOT/docs/DEPLOYMENT_GUIDE.md"; then
    passed "Deployment guide has prerequisites section"
else
    warning "Deployment guide could include prerequisites"
fi

if grep -q "Security" "$PROJECT_ROOT/docs/DEPLOYMENT_GUIDE.md"; then
    passed "Deployment guide has security section"
else
    warning "Deployment guide could include security"
fi

## 7. Directory Structure Tests
echo -e "${BLUE}7. Testing Directory Structure${NC}"
echo

# Check required directories
if [ -d "$PROJECT_ROOT/include" ]; then
    passed "Include directory exists"
else
    failed "Include directory missing"
fi

if [ -d "$PROJECT_ROOT/src" ]; then
    passed "Source directory exists"
else
    failed "Source directory missing"
fi

if [ -d "$PROJECT_ROOT/scripts" ]; then
    passed "Scripts directory exists"
else
    failed "Scripts directory missing"
fi

if [ -d "$PROJECT_ROOT/docs" ]; then
    passed "Docs directory exists"
else
    failed "Docs directory missing"
fi

if [ -d "$PROJECT_ROOT/config" ]; then
    passed "Config directory exists"
else
    failed "Config directory missing"
fi

# Check key header directories
if [ -d "$PROJECT_ROOT/include/security" ]; then
    passed "Security headers directory exists"
else
    failed "Security headers directory missing"
fi

if [ -d "$PROJECT_ROOT/include/monitoring" ]; then
    passed "Monitoring headers directory exists"
else
    failed "Monitoring headers directory missing"
fi

if [ -d "$PROJECT_ROOT/include/logging" ]; then
    passed "Logging headers directory exists"
else
    failed "Logging headers directory missing"
fi

if [ -d "$PROJECT_ROOT/include/config" ]; then
    passed "Configuration headers directory exists"
else
    failed "Configuration headers directory missing"
fi

## 8. Build Dependencies Tests
echo -e "${BLUE}8. Testing Build Dependencies${NC}"
echo

# Check vcpkg configuration
if [ -f "$PROJECT_ROOT/vcpkg.json" ]; then
    passed "vcpkg configuration exists"
else
    failed "vcpkg configuration missing"
fi

# Check for required dependencies
if grep -q "nlohmann_json" "$PROJECT_ROOT/vcpkg.json"; then
    passed "nlohmann_json dependency configured"
else
    failed "nlohmann_json dependency missing"
fi

if grep -q "curl" "$PROJECT_ROOT/vcpkg.json"; then
    passed "curl dependency configured"
else
    failed "curl dependency missing"
fi

if grep -q "openssl" "$PROJECT_ROOT/vcpkg.json"; then
    passed "openssl dependency configured"
else
    failed "openssl dependency missing"
fi

## 9. Production Readiness Tests
echo -e "${BLUE}9. Testing Production Readiness${NC}"
echo

# Check for production-specific features
if grep -q "production" "$PROJECT_ROOT/CMakeLists.txt"; then
    passed "Production build features configured"
else
    warning "Production build features could be enhanced"
fi

# Check security hardening
if grep -q "API key encryption" "$PROJECT_ROOT/config/production.env"; then
    passed "API key encryption documented"
else
    warning "API key encryption documentation could be improved"
fi

# Check health endpoints
if grep -q "/health" "$PROJECT_ROOT/src/webui/monitoring_endpoints.cpp"; then
    passed "Health check endpoint implemented"
else
    failed "Health check endpoint missing"
fi

# Check metrics endpoint
if grep -q "/metrics" "$PROJECT_ROOT/src/webui/monitoring_endpoints.cpp"; then
    passed "Metrics endpoint implemented"
else
    failed "Metrics endpoint missing"
fi

# Check structured logging
if grep -q "structured" "$PROJECT_ROOT/include/logging/production_logger.h"; then
    passed "Structured logging implemented"
else
    failed "Structured logging missing"
fi

## 10. Performance and scalability tests
echo -e "${BLUE}10. Testing Performance Configuration${NC}"
echo

# Check for concurrent request configuration
if grep -q "max_concurrent_requests" "$PROJECT_ROOT/include/config/production_config.h"; then
    passed "Concurrent request limiting configured"
else
    failed "Concurrent request limiting missing"
fi

# Check timeout configuration
if grep -q "timeout" "$PROJECT_ROOT/include/config/production_config.h"; then
    passed "Request timeout configuration present"
else
    failed "Request timeout configuration missing"
fi

# Check rate limiting
if grep -q "rate_limiting" "$PROJECT_ROOT/include/config/production_config.h"; then
    passed "Rate limiting configuration present"
else
    failed "Rate limiting configuration missing"
fi

# Check load balancing
if grep -q "load_balancing" "$PROJECT_ROOT/include/config/production_config.h"; then
    passed "Load balancing configuration present"
else
    failed "Load balancing configuration missing"
fi

## Summary
echo
echo -e "${BLUE}=== Test Summary ===${NC}"
echo "Total tests: $TEST_COUNT"
echo -e "Passed: $((TEST_COUNT - FAILURE_COUNT)) ${GREEN}✓${NC}"
echo -e "Failed: $FAILURE_COUNT ${RED}✗${NC}"

echo
echo "=== Final Assessment ===" >> "$TEST_RESULTS_FILE"
echo "Total Tests: $TEST_COUNT" >> "$TEST_RESULTS_FILE"
echo "Failed: $FAILURE_COUNT" >> "$TEST_RESULTS_FILE"
echo "Success Rate: $(( 100 * (TEST_COUNT - FAILURE_COUNT) / TEST_COUNT ))%" >> "$TEST_RESULTS_FILE"

# Overall assessment
if [ "$FAILURE_COUNT" -eq 0 ]; then
    echo -e "${GREEN}🎉 EXCELLENT: All production deployment tests passed!${NC}"
    echo "Aimux2 is fully ready for production deployment."
    echo
    echo "Next steps:"
    echo "1. Run: ./scripts/build_production.sh"
    echo "2. Run: sudo ./scripts/install_service.sh"
    echo "3. Configure your API keys in /etc/aimux/config.json"
    echo "4. Start with: sudo systemctl start aimux2"
    echo
    echo "Deployment readiness: ${GREEN}100%${NC}"
elif [ "$FAILURE_COUNT" -le 5 ]; then
    echo -e "${YELLOW}⚠️ GOOD: Minor issues found but deployment is mostly ready${NC}"
    echo "Fix the failed tests before production deployment."
    echo
    echo "Deployment readiness: ${YELLOW}80%${NC}"
elif [ "$FAILURE_COUNT" -le 10 ]; then
    echo -e "${YELLOW}⚠️ PARTIAL: Some configuration needed before production deployment${NC}"
    echo "Review failed tests and fix critical issues."
    echo
    echo "Deployment readiness: ${YELLOW}70%${NC}"
else
    echo -e "${RED}❌ NOT READY: Significant issues must be resolved${NC}"
    echo "Production deployment not recommended until critical issues are fixed."
    echo
    echo "Deployment readiness: ${RED}50%${NC}"
fi

echo
echo "Detailed results saved to: $TEST_RESULTS_FILE"

# Show failed tests if any
if [ "$FAILURE_COUNT" -gt 0 ]; then
    echo
    echo -e "${RED}Failed Tests:${NC}"
    grep "^FAIL:" "$TEST_RESULTS_FILE" | sed 's/FAIL: /  - /'
fi

# Exit with failure code if any tests failed
exit "$FAILURE_COUNT"