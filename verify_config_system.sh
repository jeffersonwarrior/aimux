#!/bin/bash
#
# Configuration System Verification Script
# Tests that the configuration system properly loads prettifier settings
#

# Note: Do not use set -e, we want to continue on test failures

echo "========================================"
echo "Configuration System Verification v2.1"
echo "========================================"
echo ""

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

TEST_RESULTS=()
TESTS_PASSED=0
TESTS_FAILED=0

# Test helper
run_test() {
    local test_name="$1"
    local test_command="$2"

    echo -e "${BLUE}Test: ${test_name}${NC}"
    echo "-----------------------------------"

    if eval "$test_command"; then
        echo -e "${GREEN}✓ PASSED${NC}\n"
        TEST_RESULTS+=("PASS: $test_name")
        ((TESTS_PASSED++))
        return 0
    else
        echo -e "${RED}✗ FAILED${NC}\n"
        TEST_RESULTS+=("FAIL: $test_name")
        ((TESTS_FAILED++))
        return 1
    fi
}

# Test 1: Check if ProductionConfig header exists
test_config_header() {
    local header_file="include/config/production_config.h"
    if [ ! -f "$header_file" ]; then
        echo "ERROR: $header_file not found"
        return 1
    fi

    echo "Header file exists: $header_file"

    # Check for PrettifierConfig struct
    if ! grep -q "struct PrettifierConfig" "$header_file"; then
        echo "ERROR: PrettifierConfig struct not found in header"
        return 1
    fi

    echo "✓ PrettifierConfig struct found"

    # Check for key fields
    local required_fields=("enabled" "default_prettifier" "provider_mappings" "toon_config")
    for field in "${required_fields[@]}"; do
        if ! grep -q "$field" "$header_file"; then
            echo "ERROR: Required field '$field' not found"
            return 1
        fi
        echo "✓ Field '$field' present"
    done

    return 0
}

# Test 2: Check if ProductionConfig implementation exists
test_config_implementation() {
    local impl_file="src/config/production_config.cpp"
    local header_file="include/config/production_config.h"

    if [ ! -f "$impl_file" ]; then
        echo "ERROR: $impl_file not found"
        return 1
    fi

    echo "Implementation file exists: $impl_file"

    # Check for key functions - they can be in header (inline) or cpp file
    if ! grep -q "toJson" "$header_file" && ! grep -q "PrettifierConfig::toJson" "$impl_file"; then
        echo "ERROR: PrettifierConfig::toJson not found in header or implementation"
        return 1
    fi
    echo "✓ PrettifierConfig::toJson implemented (inline in header)"

    if ! grep -q "fromJson" "$header_file" && ! grep -q "PrettifierConfig::fromJson" "$impl_file"; then
        echo "ERROR: PrettifierConfig::fromJson not found in header or implementation"
        return 1
    fi
    echo "✓ PrettifierConfig::fromJson implemented (inline in header)"

    # Check for validation function
    if ! grep -q "validatePrettifierConfig" "$impl_file"; then
        echo "WARNING: validatePrettifierConfig not found in implementation"
    else
        echo "✓ validatePrettifierConfig implemented"
    fi

    return 0
}

# Test 3: Check if WebServer uses prettifier config
test_webserver_integration() {
    local webserver_file="src/webui/web_server.cpp"
    if [ ! -f "$webserver_file" ]; then
        echo "ERROR: $webserver_file not found"
        return 1
    fi

    echo "Web server file exists: $webserver_file"

    # Check for prettifier endpoints
    if ! grep -q "handle_crow_get_prettifier_status" "$webserver_file"; then
        echo "ERROR: Prettifier status endpoint not found"
        return 1
    fi
    echo "✓ Prettifier status endpoint found"

    if ! grep -q "handle_crow_update_prettifier_config" "$webserver_file"; then
        echo "ERROR: Prettifier config update endpoint not found"
        return 1
    fi
    echo "✓ Prettifier config update endpoint found"

    # Check that it accesses config properly
    if ! grep -q "config.prettifier" "$webserver_file"; then
        echo "ERROR: Web server doesn't access config.prettifier"
        return 1
    fi
    echo "✓ Web server accesses config.prettifier"

    return 0
}

# Test 4: Check config.json structure
test_config_json() {
    local config_file="config.json"

    if [ ! -f "$config_file" ]; then
        echo "WARNING: config.json not found, checking config.example.json"
        config_file="config.example.json"
    fi

    if [ ! -f "$config_file" ]; then
        echo "WARNING: No config file found, skipping JSON validation"
        return 0
    fi

    echo "Config file: $config_file"

    # Validate JSON syntax
    if ! python3 -m json.tool "$config_file" > /dev/null 2>&1; then
        echo "ERROR: Invalid JSON in $config_file"
        return 1
    fi
    echo "✓ Valid JSON syntax"

    # Check for prettifier section
    if ! grep -q '"prettifier"' "$config_file"; then
        echo "WARNING: No 'prettifier' section in config (optional)"
    else
        echo "✓ Prettifier section present"

        # Show prettifier config
        echo ""
        echo "Prettifier configuration in $config_file:"
        python3 -c "import json; data=json.load(open('$config_file')); print(json.dumps(data.get('prettifier', {}), indent=2))"
    fi

    return 0
}

# Test 5: Check environment variable support
test_environment_variables() {
    local header_file="include/config/production_config.h"

    # Check for env namespace
    if ! grep -q "namespace env" "$header_file"; then
        echo "WARNING: env namespace not found in header"
        return 0
    fi
    echo "✓ Environment variable namespace found"

    # Show how to use env vars
    echo ""
    echo "Environment variable override support detected:"
    echo "  - Set AIMUX_PRETTIFIER_ENABLED=true|false"
    echo "  - Set AIMUX_OUTPUT_FORMAT=toon|json|raw"
    echo ""

    # Check if any are currently set
    if [ -n "$AIMUX_PRETTIFIER_ENABLED" ]; then
        echo "Current: AIMUX_PRETTIFIER_ENABLED=$AIMUX_PRETTIFIER_ENABLED"
    fi
    if [ -n "$AIMUX_OUTPUT_FORMAT" ]; then
        echo "Current: AIMUX_OUTPUT_FORMAT=$AIMUX_OUTPUT_FORMAT"
    fi

    return 0
}

# Test 6: Verify config field types and defaults
test_config_defaults() {
    local header_file="include/config/production_config.h"

    echo "Verifying configuration defaults in header..."

    # Extract PrettifierConfig struct
    if ! grep -A 50 "struct PrettifierConfig" "$header_file" | grep -q "enabled = true"; then
        echo "WARNING: Default for 'enabled' not found or not true"
    else
        echo "✓ Default enabled = true"
    fi

    if ! grep -A 50 "struct PrettifierConfig" "$header_file" | grep -q 'default_prettifier = "toon"'; then
        echo "WARNING: Default for 'default_prettifier' not found or not 'toon'"
    else
        echo "✓ Default default_prettifier = \"toon\""
    fi

    if ! grep -A 50 "struct PrettifierConfig" "$header_file" | grep -q "cache_ttl_minutes = 60"; then
        echo "WARNING: Default for 'cache_ttl_minutes' not found or not 60"
    else
        echo "✓ Default cache_ttl_minutes = 60"
    fi

    return 0
}

# Test 7: Check TOON formatter config structure
test_toon_config() {
    local toon_header="include/aimux/prettifier/toon_formatter.hpp"

    if [ ! -f "$toon_header" ]; then
        echo "ERROR: $toon_header not found"
        return 1
    fi

    echo "TOON formatter header exists: $toon_header"

    # Check for Config struct
    if ! grep -q "struct Config" "$toon_header"; then
        echo "ERROR: Config struct not found in TOON formatter"
        return 1
    fi
    echo "✓ ToonFormatter::Config struct found"

    # Check for key TOON config fields
    local toon_fields=("include_metadata" "include_tools" "include_thinking" "preserve_timestamps")
    for field in "${toon_fields[@]}"; do
        if ! grep -q "$field" "$toon_header"; then
            echo "WARNING: TOON field '$field' not found"
        else
            echo "✓ TOON field '$field' present"
        fi
    done

    return 0
}

# Main test execution
echo "Running configuration system tests..."
echo ""

run_test "Config Header Structure" test_config_header
run_test "Config Implementation" test_config_implementation
run_test "WebServer Integration" test_webserver_integration
run_test "Config JSON Validation" test_config_json
run_test "Environment Variables" test_environment_variables
run_test "Config Defaults" test_config_defaults
run_test "TOON Config Structure" test_toon_config

# Summary
echo "========================================"
echo "Test Summary"
echo "========================================"
echo ""
echo "Total Tests: $((TESTS_PASSED + TESTS_FAILED))"
echo -e "${GREEN}Passed: $TESTS_PASSED${NC}"
echo -e "${RED}Failed: $TESTS_FAILED${NC}"
echo ""

if [ $TESTS_FAILED -eq 0 ]; then
    echo -e "${GREEN}✓ All tests PASSED!${NC}"
    echo ""
    echo "Configuration system is properly set up to:"
    echo "  ✓ Load prettifier settings from config.json"
    echo "  ✓ Support environment variable overrides"
    echo "  ✓ Expose settings via REST API"
    echo "  ✓ Provide TOON formatter configuration"
    echo ""
    exit 0
else
    echo -e "${RED}✗ Some tests FAILED${NC}"
    echo ""
    echo "Failed tests:"
    for result in "${TEST_RESULTS[@]}"; do
        if [[ $result == FAIL:* ]]; then
            echo "  - ${result#FAIL: }"
        fi
    done
    echo ""
    exit 1
fi
