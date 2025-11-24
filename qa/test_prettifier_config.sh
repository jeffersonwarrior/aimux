#!/bin/bash

# Phase 3.2: Configuration Loading Test
# Tests prettifier configuration loading and environment overrides

set -e

echo "=== Phase 3.2: Configuration Loading Tests ==="
echo ""

PASSED=0
FAILED=0

# Test 1: Verify config.json contains prettifier section
test_config_json_prettifier() {
    echo "Test 1: Verify config.json contains prettifier section"

    if [ ! -f "config.json" ]; then
        echo "❌ FAILED: config.json not found"
        return 1
    fi

    # Check if prettifier section exists
    if ! grep -q '"prettifier"' config.json; then
        echo "❌ FAILED: No prettifier section in config.json"
        return 1
    fi

    # Check if enabled field exists
    if ! grep -A 5 '"prettifier"' config.json | grep -q '"enabled"'; then
        echo "❌ FAILED: No 'enabled' field in prettifier config"
        return 1
    fi

    echo "✅ PASSED: Prettifier section exists in config.json"
    return 0
}

# Test 2: Verify ProductionConfig struct includes prettifier
test_production_config_struct() {
    echo "Test 2: Verify ProductionConfig struct includes prettifier"

    if ! grep -q "PrettifierConfig prettifier" include/config/production_config.h; then
        echo "❌ FAILED: PrettifierConfig not found in ProductionConfig struct"
        return 1
    fi

    echo "✅ PASSED: ProductionConfig includes prettifier member"
    return 0
}

# Test 3: Verify environment override code exists
test_env_override_code() {
    echo "Test 3: Verify environment override code for prettifier"

    if ! grep -q "AIMUX_PRETTIFIER_ENABLED" src/config/production_config.cpp; then
        echo "❌ FAILED: AIMUX_PRETTIFIER_ENABLED override not found"
        return 1
    fi

    if ! grep -q "AIMUX_OUTPUT_FORMAT" src/config/production_config.cpp; then
        echo "❌ FAILED: AIMUX_OUTPUT_FORMAT override not found"
        return 1
    fi

    echo "✅ PASSED: Environment override code exists"
    return 0
}

# Test 4: Verify PrettifierConfig fromJson implementation
test_prettifier_from_json() {
    echo "Test 4: Verify PrettifierConfig::fromJson exists"

    if ! grep -q "PrettifierConfig::fromJson" include/config/production_config.h; then
        echo "❌ FAILED: PrettifierConfig::fromJson not found"
        return 1
    fi

    echo "✅ PASSED: PrettifierConfig::fromJson exists"
    return 0
}

# Test 5: Verify provider_mappings support
test_provider_mappings() {
    echo "Test 5: Verify provider_mappings in PrettifierConfig"

    if ! grep -q "provider_mappings" include/config/production_config.h; then
        echo "❌ FAILED: provider_mappings not found in PrettifierConfig"
        return 1
    fi

    # Check if it's a map<string, string>
    if ! grep -q "std::map<std::string, std::string> provider_mappings" include/config/production_config.h; then
        echo "❌ FAILED: provider_mappings has wrong type"
        return 1
    fi

    echo "✅ PASSED: Provider mappings supported"
    return 0
}

# Test 6: Verify TOON config structure
test_toon_config() {
    echo "Test 6: Verify TOON config structure"

    if ! grep -q "toon_config" include/config/production_config.h; then
        echo "❌ FAILED: toon_config not found in PrettifierConfig"
        return 1
    fi

    echo "✅ PASSED: TOON config structure exists"
    return 0
}

# Test 7: Verify prettifier validation
test_prettifier_validation() {
    echo "Test 7: Verify prettifier validation function"

    if ! grep -q "validatePrettifierConfig" src/config/production_config.cpp; then
        echo "❌ FAILED: validatePrettifierConfig not found"
        return 1
    fi

    echo "✅ PASSED: Prettifier validation exists"
    return 0
}

# Test 8: Verify config.json has proper prettifier defaults
test_config_defaults() {
    echo "Test 8: Verify config.json prettifier defaults"

    if [ ! -f "config.json" ]; then
        echo "❌ FAILED: config.json not found"
        return 1
    fi

    # Extract prettifier section
    PRETTIFIER_SECTION=$(python3 -c "
import json
import sys
try:
    with open('config.json', 'r') as f:
        config = json.load(f)
        if 'prettifier' in config:
            print(json.dumps(config['prettifier'], indent=2))
            sys.exit(0)
        else:
            sys.exit(1)
except Exception as e:
    print(f'Error: {e}', file=sys.stderr)
    sys.exit(1)
" 2>&1)

    if [ $? -ne 0 ]; then
        echo "❌ FAILED: Could not extract prettifier section"
        return 1
    fi

    echo "Prettifier config:"
    echo "$PRETTIFIER_SECTION"
    echo "✅ PASSED: Prettifier config loaded from JSON"
    return 0
}

# Test 9: Simulate environment override
test_env_simulation() {
    echo "Test 9: Simulate environment variable override (code check)"

    # Check that the override code is in the right place (loadEnvironmentOverrides)
    if ! grep -A 80 "void ProductionConfigManager::loadEnvironmentOverrides" src/config/production_config.cpp | grep -q "AIMUX_PRETTIFIER_ENABLED"; then
        echo "❌ FAILED: Environment override not in loadEnvironmentOverrides function"
        return 1
    fi

    # Check that it uses env::getBool
    if ! grep -A 80 "void ProductionConfigManager::loadEnvironmentOverrides" src/config/production_config.cpp | grep -q "env::getBool.*AIMUX_PRETTIFIER_ENABLED"; then
        echo "❌ FAILED: Environment override not using env::getBool"
        return 1
    fi

    echo "✅ PASSED: Environment override code properly placed"
    return 0
}

# Run all tests
echo "Running tests..."
echo ""

if test_config_json_prettifier; then
    PASSED=$((PASSED + 1))
else
    FAILED=$((FAILED + 1))
fi
echo ""

if test_production_config_struct; then
    PASSED=$((PASSED + 1))
else
    FAILED=$((FAILED + 1))
fi
echo ""

if test_env_override_code; then
    PASSED=$((PASSED + 1))
else
    FAILED=$((FAILED + 1))
fi
echo ""

if test_prettifier_from_json; then
    PASSED=$((PASSED + 1))
else
    FAILED=$((FAILED + 1))
fi
echo ""

if test_provider_mappings; then
    PASSED=$((PASSED + 1))
else
    FAILED=$((FAILED + 1))
fi
echo ""

if test_toon_config; then
    PASSED=$((PASSED + 1))
else
    FAILED=$((FAILED + 1))
fi
echo ""

if test_prettifier_validation; then
    PASSED=$((PASSED + 1))
else
    FAILED=$((FAILED + 1))
fi
echo ""

if test_config_defaults; then
    PASSED=$((PASSED + 1))
else
    FAILED=$((FAILED + 1))
fi
echo ""

if test_env_simulation; then
    PASSED=$((PASSED + 1))
else
    FAILED=$((FAILED + 1))
fi
echo ""

# Summary
echo "=== Test Summary ==="
echo "Passed: $PASSED"
echo "Failed: $FAILED"
echo "Total:  $((PASSED + FAILED))"
echo ""

if [ $FAILED -eq 0 ]; then
    echo "✅ All tests PASSED"
    exit 0
else
    echo "❌ Some tests FAILED"
    exit 1
fi
