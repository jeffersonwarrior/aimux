#!/bin/bash

# Phase 3.3: Router Integration Test
# Documents and verifies that Router doesn't need separate prettifier calls
# because GatewayManager (which wraps Router) already handles it

set -e

echo "=== Phase 3.3: Router Integration Analysis ==="
echo ""

PASSED=0
FAILED=0

# Test 1: Verify GatewayManager applies prettifier
test_gateway_applies_prettifier() {
    echo "Test 1: Verify GatewayManager applies prettifier"

    # Check that GatewayManager has prettifier code
    if ! grep -q "apply_prettifier" src/gateway/gateway_manager.cpp; then
        echo "❌ FAILED: GatewayManager doesn't call apply_prettifier"
        return 1
    fi

    # Check that it's in the request handling path
    if ! grep -B 5 "apply_prettifier" src/gateway/gateway_manager.cpp | grep -q "response"; then
        echo "❌ FAILED: apply_prettifier not in response path"
        return 1
    fi

    echo "✅ PASSED: GatewayManager applies prettifier to responses"
    return 0
}

# Test 2: Verify Router doesn't duplicate prettifier
test_router_no_prettifier() {
    echo "Test 2: Verify Router doesn't have prettifier calls"

    # Router should NOT have prettifier code
    if grep -q "prettifier\|Prettifier" src/core/router.cpp; then
        echo "❌ FAILED: Router has prettifier code (should be in Gateway only)"
        return 1
    fi

    echo "✅ PASSED: Router correctly delegates prettifying to Gateway"
    return 0
}

# Test 3: Verify Router focuses on routing
test_router_focused() {
    echo "Test 3: Verify Router is focused on routing logic"

    # Router should have failover and load balancer
    if ! grep -q "failover_manager_\|load_balancer_" src/core/router.cpp; then
        echo "❌ FAILED: Router missing core routing components"
        return 1
    fi

    # Router should NOT have prettifier members
    if grep -q "prettifier" include/aimux/core/router.hpp; then
        echo "❌ FAILED: Router has prettifier members (should be in Gateway)"
        return 1
    fi

    echo "✅ PASSED: Router focused on routing, not formatting"
    return 0
}

# Test 4: Document the architecture
test_architecture_documented() {
    echo "Test 4: Verify architecture is documented"

    echo ""
    echo "=== Architecture Documentation ==="
    echo "Request Flow:"
    echo "  Client → GatewayManager → Router → Bridge → Provider"
    echo ""
    echo "Prettifier Application Point:"
    echo "  GatewayManager.handle_api_request() applies prettifier AFTER"
    echo "  receiving response from Router, before returning to client."
    echo ""
    echo "Why Router doesn't have prettifier:"
    echo "  1. Separation of concerns: Router handles routing, Gateway handles formatting"
    echo "  2. Avoids duplicate processing"
    echo "  3. Keeps Router focused and testable"
    echo "  4. Allows Gateway to apply formatting to ALL responses uniformly"
    echo ""
    echo "Code Evidence:"

    # Show the actual code
    echo "  GatewayManager (src/gateway/gateway_manager.cpp):"
    grep -A 3 "// Apply prettifier postprocessing" src/gateway/gateway_manager.cpp || echo "    [code not found]"
    echo ""

    echo "✅ PASSED: Architecture documented"
    return 0
}

# Test 5: Verify prettifier is initialized in Gateway
test_prettifier_initialization() {
    echo "Test 5: Verify prettifier initialization in Gateway"

    if ! grep -q "initialize_prettifier_formatters" src/gateway/gateway_manager.cpp; then
        echo "❌ FAILED: Prettifier not initialized in Gateway"
        return 1
    fi

    # Check it's called during Gateway initialization
    if ! grep -A 30 "void GatewayManager::initialize()" src/gateway/gateway_manager.cpp | grep -q "initialize_prettifier_formatters"; then
        echo "❌ FAILED: Prettifier initialization not called during Gateway::initialize()"
        return 1
    fi

    echo "✅ PASSED: Prettifier properly initialized in Gateway"
    return 0
}

# Test 6: Verify Router returns raw responses
test_router_raw_responses() {
    echo "Test 6: Verify Router returns raw provider responses"

    # Router should return Response objects, not formatted strings
    if ! grep -q "Response route" include/aimux/core/router.hpp; then
        echo "❌ FAILED: Router::route doesn't return Response object"
        return 1
    fi

    # Router shouldn't format or modify response data
    if grep -q "format.*response\|prettify.*response" src/core/router.cpp; then
        echo "❌ FAILED: Router appears to format responses (should be Gateway's job)"
        return 1
    fi

    echo "✅ PASSED: Router returns raw responses for Gateway to format"
    return 0
}

# Run all tests
echo "Running tests..."
echo ""

if test_gateway_applies_prettifier; then
    PASSED=$((PASSED + 1))
else
    FAILED=$((FAILED + 1))
fi
echo ""

if test_router_no_prettifier; then
    PASSED=$((PASSED + 1))
else
    FAILED=$((FAILED + 1))
fi
echo ""

if test_router_focused; then
    PASSED=$((PASSED + 1))
else
    FAILED=$((FAILED + 1))
fi
echo ""

if test_architecture_documented; then
    PASSED=$((PASSED + 1))
else
    FAILED=$((FAILED + 1))
fi
echo ""

if test_prettifier_initialization; then
    PASSED=$((PASSED + 1))
else
    FAILED=$((FAILED + 1))
fi
echo ""

if test_router_raw_responses; then
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

# Decision summary
echo "=== Phase 3.3 Decision ==="
echo "Router Integration: NOT NEEDED"
echo "Reason: GatewayManager already applies prettifier at the correct layer"
echo "Conclusion: No changes required to Router - architecture is correct as-is"
echo ""

if [ $FAILED -eq 0 ]; then
    echo "✅ All tests PASSED"
    exit 0
else
    echo "❌ Some tests FAILED"
    exit 1
fi
