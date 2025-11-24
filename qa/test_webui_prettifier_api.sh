#!/bin/bash

# Phase 3.4: WebUI Prettifier API Test
# Tests the REST API endpoints for prettifier status and configuration

set -e

echo "=== Phase 3.4: WebUI Prettifier API Tests ==="
echo ""

PASSED=0
FAILED=0

# Test 1: Verify GET /api/prettifier/status endpoint exists in code
test_get_status_endpoint() {
    echo "Test 1: Verify GET /api/prettifier/status endpoint exists"

    # Check route is defined
    if ! grep -q "/api/prettifier/status" src/webui/web_server.cpp; then
        echo "❌ FAILED: /api/prettifier/status route not found"
        return 1
    fi

    # Check handler method exists
    if ! grep -q "handle_crow_get_prettifier_status" src/webui/web_server.cpp; then
        echo "❌ FAILED: handle_crow_get_prettifier_status handler not found"
        return 1
    fi

    # Check handler declaration in header
    if ! grep -q "handle_crow_get_prettifier_status" include/aimux/webui/web_server.hpp; then
        echo "❌ FAILED: handle_crow_get_prettifier_status not declared in header"
        return 1
    fi

    echo "✅ PASSED: GET /api/prettifier/status endpoint defined"
    return 0
}

# Test 2: Verify POST /api/prettifier/config endpoint exists
test_post_config_endpoint() {
    echo "Test 2: Verify POST /api/prettifier/config endpoint exists"

    # Check route is defined
    if ! grep -q "/api/prettifier/config" src/webui/web_server.cpp; then
        echo "❌ FAILED: /api/prettifier/config route not found"
        return 1
    fi

    # Check handler method exists
    if ! grep -q "handle_crow_update_prettifier_config" src/webui/web_server.cpp; then
        echo "❌ FAILED: handle_crow_update_prettifier_config handler not found"
        return 1
    fi

    # Check handler declaration in header
    if ! grep -q "handle_crow_update_prettifier_config" include/aimux/webui/web_server.hpp; then
        echo "❌ FAILED: handle_crow_update_prettifier_config not declared in header"
        return 1
    fi

    echo "✅ PASSED: POST /api/prettifier/config endpoint defined"
    return 0
}

# Test 3: Verify status endpoint returns correct fields
test_status_response_fields() {
    echo "Test 3: Verify status endpoint returns correct fields"

    # Check that handler returns enabled field
    if ! grep -A 30 "handle_crow_get_prettifier_status" src/webui/web_server.cpp | grep -q 'status\["enabled"\]'; then
        echo "❌ FAILED: Status handler doesn't return 'enabled' field"
        return 1
    fi

    # Check that handler returns default_prettifier field
    if ! grep -A 30 "handle_crow_get_prettifier_status" src/webui/web_server.cpp | grep -q 'status\["default_prettifier"\]'; then
        echo "❌ FAILED: Status handler doesn't return 'default_prettifier' field"
        return 1
    fi

    # Check that handler returns provider_mappings
    if ! grep -A 30 "handle_crow_get_prettifier_status" src/webui/web_server.cpp | grep -q 'status\["provider_mappings"\]'; then
        echo "❌ FAILED: Status handler doesn't return 'provider_mappings' field"
        return 1
    fi

    # Check that handler returns toon_config
    if ! grep -A 40 "handle_crow_get_prettifier_status" src/webui/web_server.cpp | grep -q 'status\["toon_config"\]'; then
        echo "❌ FAILED: Status handler doesn't return 'toon_config' field"
        return 1
    fi

    echo "✅ PASSED: Status endpoint returns all required fields"
    return 0
}

# Test 4: Verify config endpoint accepts updates
test_config_accepts_updates() {
    echo "Test 4: Verify config endpoint accepts updates"

    # Check that handler accepts 'enabled' parameter
    if ! grep -A 100 "handle_crow_update_prettifier_config" src/webui/web_server.cpp | grep -q 'request.contains("enabled")'; then
        echo "❌ FAILED: Config handler doesn't accept 'enabled' parameter"
        return 1
    fi

    # Check that handler accepts 'default_prettifier' parameter
    if ! grep -A 100 "handle_crow_update_prettifier_config" src/webui/web_server.cpp | grep -q 'request.contains("default_prettifier")'; then
        echo "❌ FAILED: Config handler doesn't accept 'default_prettifier' parameter"
        return 1
    fi

    # Check that handler validates format
    if ! grep -A 100 "handle_crow_update_prettifier_config" src/webui/web_server.cpp | grep -q '"toon" || format == "json" || format == "raw"'; then
        echo "❌ FAILED: Config handler doesn't validate prettifier format"
        return 1
    fi

    echo "✅ PASSED: Config endpoint accepts and validates updates"
    return 0
}

# Test 5: Verify CORS headers are set
test_cors_headers() {
    echo "Test 5: Verify CORS headers are set"

    # Check that status handler sets CORS headers
    if ! grep -A 50 "handle_crow_get_prettifier_status" src/webui/web_server.cpp | grep -q 'Access-Control-Allow-Origin'; then
        echo "❌ FAILED: Status handler doesn't set CORS headers"
        return 1
    fi

    # Check that config handler sets CORS headers
    if ! grep -A 180 "handle_crow_update_prettifier_config" src/webui/web_server.cpp | grep -q 'Access-Control-Allow-Origin'; then
        echo "❌ FAILED: Config handler doesn't set CORS headers"
        return 1
    fi

    echo "✅ PASSED: CORS headers properly set"
    return 0
}

# Test 6: Verify error handling
test_error_handling() {
    echo "Test 6: Verify error handling"

    # Check that status handler has try-catch
    if ! grep -A 50 "handle_crow_get_prettifier_status" src/webui/web_server.cpp | grep -q "catch (const std::exception& e)"; then
        echo "❌ FAILED: Status handler missing exception handling"
        return 1
    fi

    # Check that config handler has JSON parse error handling
    if ! grep -A 180 "handle_crow_update_prettifier_config" src/webui/web_server.cpp | grep -q "catch (const nlohmann::json::parse_error& e)"; then
        echo "❌ FAILED: Config handler missing JSON parse error handling"
        return 1
    fi

    # Check that config handler has general exception handling
    if ! grep -A 180 "handle_crow_update_prettifier_config" src/webui/web_server.cpp | grep -q "catch (const std::exception& e)"; then
        echo "❌ FAILED: Config handler missing general exception handling"
        return 1
    fi

    echo "✅ PASSED: Error handling properly implemented"
    return 0
}

# Test 7: Verify routes use correct HTTP methods
test_http_methods() {
    echo "Test 7: Verify routes use correct HTTP methods"

    # Check GET for status
    if ! grep -B 2 "handle_crow_get_prettifier_status" src/webui/web_server.cpp | grep -q 'methods("GET"_method)'; then
        echo "❌ FAILED: Status endpoint doesn't use GET method"
        return 1
    fi

    # Check POST for config
    if ! grep -B 2 "handle_crow_update_prettifier_config" src/webui/web_server.cpp | grep -q 'methods("POST"_method)'; then
        echo "❌ FAILED: Config endpoint doesn't use POST method"
        return 1
    fi

    echo "✅ PASSED: HTTP methods correctly assigned"
    return 0
}

# Test 8: Verify JSON response format
test_json_responses() {
    echo "Test 8: Verify JSON response format"

    # Check that status returns JSON
    if ! grep -A 50 "handle_crow_get_prettifier_status" src/webui/web_server.cpp | grep -q 'Content-Type.*application/json'; then
        echo "❌ FAILED: Status endpoint doesn't set JSON content-type"
        return 1
    fi

    # Check that config returns JSON
    if ! grep -A 180 "handle_crow_update_prettifier_config" src/webui/web_server.cpp | grep -q 'Content-Type.*application/json'; then
        echo "❌ FAILED: Config endpoint doesn't set JSON content-type"
        return 1
    fi

    # Check that status uses dump(2) for pretty printing
    if ! grep -A 50 "handle_crow_get_prettifier_status" src/webui/web_server.cpp | grep -q 'status.dump(2)'; then
        echo "❌ FAILED: Status endpoint doesn't pretty-print JSON"
        return 1
    fi

    echo "✅ PASSED: JSON responses properly formatted"
    return 0
}

# Run all tests
echo "Running tests..."
echo ""

if test_get_status_endpoint; then
    PASSED=$((PASSED + 1))
else
    FAILED=$((FAILED + 1))
fi
echo ""

if test_post_config_endpoint; then
    PASSED=$((PASSED + 1))
else
    FAILED=$((FAILED + 1))
fi
echo ""

if test_status_response_fields; then
    PASSED=$((PASSED + 1))
else
    FAILED=$((FAILED + 1))
fi
echo ""

if test_config_accepts_updates; then
    PASSED=$((PASSED + 1))
else
    FAILED=$((FAILED + 1))
fi
echo ""

if test_cors_headers; then
    PASSED=$((PASSED + 1))
else
    FAILED=$((FAILED + 1))
fi
echo ""

if test_error_handling; then
    PASSED=$((PASSED + 1))
else
    FAILED=$((FAILED + 1))
fi
echo ""

if test_http_methods; then
    PASSED=$((PASSED + 1))
else
    FAILED=$((FAILED + 1))
fi
echo ""

if test_json_responses; then
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

# API Documentation
echo "=== API Documentation ==="
echo ""
echo "GET /api/prettifier/status"
echo "  Description: Get current prettifier configuration and status"
echo "  Returns: JSON object with enabled, default_prettifier, provider_mappings, toon_config, etc."
echo "  Example: curl http://localhost:8080/api/prettifier/status"
echo ""
echo "POST /api/prettifier/config"
echo "  Description: Update prettifier configuration"
echo "  Body: JSON object with fields to update (enabled, default_prettifier, etc.)"
echo "  Example: curl -X POST http://localhost:8080/api/prettifier/config \\"
echo "           -H 'Content-Type: application/json' \\"
echo "           -d '{\"enabled\": false}'"
echo ""

if [ $FAILED -eq 0 ]; then
    echo "✅ All tests PASSED"
    exit 0
else
    echo "❌ Some tests FAILED"
    exit 1
fi
