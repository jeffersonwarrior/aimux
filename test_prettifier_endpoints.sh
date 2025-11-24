#!/bin/bash
# Test script for Prettifier REST API Endpoints
# Tests both GET /api/prettifier/status and POST /api/prettifier/config

PORT=8080
HOST="localhost"
BASE_URL="http://${HOST}:${PORT}"

echo "========================================="
echo "Prettifier REST API Endpoint Tests"
echo "========================================="
echo ""

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test 1: GET /api/prettifier/status
echo "Test 1: GET /api/prettifier/status"
echo "-----------------------------------"
echo "Expected: HTTP 200 with JSON containing enabled, default_prettifier, provider_mappings, toon_config"
echo ""

RESPONSE=$(curl -s -w "\nHTTP_STATUS:%{http_code}" "${BASE_URL}/api/prettifier/status")
HTTP_BODY=$(echo "$RESPONSE" | sed -e 's/HTTP_STATUS\:.*//g')
HTTP_STATUS=$(echo "$RESPONSE" | tr -d '\n' | sed -e 's/.*HTTP_STATUS://')

echo "HTTP Status: $HTTP_STATUS"
echo "Response Body:"
echo "$HTTP_BODY" | python3 -m json.tool 2>/dev/null || echo "$HTTP_BODY"
echo ""

if [ "$HTTP_STATUS" -eq 200 ]; then
    echo -e "${GREEN}✓ Test 1 PASSED${NC}"
else
    echo -e "${RED}✗ Test 1 FAILED - Expected HTTP 200, got $HTTP_STATUS${NC}"
fi
echo ""

# Test 2: POST /api/prettifier/config - Enable prettifier
echo "Test 2: POST /api/prettifier/config (enable prettifier)"
echo "-------------------------------------------------------"
echo "Expected: HTTP 200 with success message"
echo ""

POST_DATA='{
  "enabled": true,
  "default_prettifier": "toon"
}'

RESPONSE=$(curl -s -w "\nHTTP_STATUS:%{http_code}" -X POST \
  -H "Content-Type: application/json" \
  -d "$POST_DATA" \
  "${BASE_URL}/api/prettifier/config")
HTTP_BODY=$(echo "$RESPONSE" | sed -e 's/HTTP_STATUS\:.*//g')
HTTP_STATUS=$(echo "$RESPONSE" | tr -d '\n' | sed -e 's/.*HTTP_STATUS://')

echo "HTTP Status: $HTTP_STATUS"
echo "Response Body:"
echo "$HTTP_BODY" | python3 -m json.tool 2>/dev/null || echo "$HTTP_BODY"
echo ""

if [ "$HTTP_STATUS" -eq 200 ]; then
    echo -e "${GREEN}✓ Test 2 PASSED${NC}"
else
    echo -e "${RED}✗ Test 2 FAILED - Expected HTTP 200, got $HTTP_STATUS${NC}"
fi
echo ""

# Test 3: POST /api/prettifier/config - Update TOON config
echo "Test 3: POST /api/prettifier/config (update TOON config)"
echo "---------------------------------------------------------"
echo "Expected: HTTP 200 with success message"
echo ""

POST_DATA='{
  "enabled": true,
  "default_prettifier": "toon",
  "toon_config": {
    "include_metadata": true,
    "include_tools": true,
    "include_thinking": false,
    "preserve_timestamps": true
  }
}'

RESPONSE=$(curl -s -w "\nHTTP_STATUS:%{http_code}" -X POST \
  -H "Content-Type: application/json" \
  -d "$POST_DATA" \
  "${BASE_URL}/api/prettifier/config")
HTTP_BODY=$(echo "$RESPONSE" | sed -e 's/HTTP_STATUS\:.*//g')
HTTP_STATUS=$(echo "$RESPONSE" | tr -d '\n' | sed -e 's/.*HTTP_STATUS://')

echo "HTTP Status: $HTTP_STATUS"
echo "Response Body:"
echo "$HTTP_BODY" | python3 -m json.tool 2>/dev/null || echo "$HTTP_BODY"
echo ""

if [ "$HTTP_STATUS" -eq 200 ]; then
    echo -e "${GREEN}✓ Test 3 PASSED${NC}"
else
    echo -e "${RED}✗ Test 3 FAILED - Expected HTTP 200, got $HTTP_STATUS${NC}"
fi
echo ""

# Test 4: POST /api/prettifier/config - Invalid format
echo "Test 4: POST /api/prettifier/config (invalid format)"
echo "-----------------------------------------------------"
echo "Expected: HTTP 400 with error message"
echo ""

POST_DATA='{
  "enabled": true,
  "default_prettifier": "invalid_format"
}'

RESPONSE=$(curl -s -w "\nHTTP_STATUS:%{http_code}" -X POST \
  -H "Content-Type: application/json" \
  -d "$POST_DATA" \
  "${BASE_URL}/api/prettifier/config")
HTTP_BODY=$(echo "$RESPONSE" | sed -e 's/HTTP_STATUS\:.*//g')
HTTP_STATUS=$(echo "$RESPONSE" | tr -d '\n' | sed -e 's/.*HTTP_STATUS://')

echo "HTTP Status: $HTTP_STATUS"
echo "Response Body:"
echo "$HTTP_BODY" | python3 -m json.tool 2>/dev/null || echo "$HTTP_BODY"
echo ""

if [ "$HTTP_STATUS" -eq 400 ]; then
    echo -e "${GREEN}✓ Test 4 PASSED - Correctly rejected invalid format${NC}"
else
    echo -e "${RED}✗ Test 4 FAILED - Expected HTTP 400, got $HTTP_STATUS${NC}"
fi
echo ""

# Test 5: POST /api/prettifier/config - Update provider mappings
echo "Test 5: POST /api/prettifier/config (update provider mappings)"
echo "---------------------------------------------------------------"
echo "Expected: HTTP 200 with success message"
echo ""

POST_DATA='{
  "provider_mappings": {
    "cerebras": "toon",
    "openai": "json",
    "anthropic": "toon"
  }
}'

RESPONSE=$(curl -s -w "\nHTTP_STATUS:%{http_code}" -X POST \
  -H "Content-Type: application/json" \
  -d "$POST_DATA" \
  "${BASE_URL}/api/prettifier/config")
HTTP_BODY=$(echo "$RESPONSE" | sed -e 's/HTTP_STATUS\:.*//g')
HTTP_STATUS=$(echo "$RESPONSE" | tr -d '\n' | sed -e 's/.*HTTP_STATUS://')

echo "HTTP Status: $HTTP_STATUS"
echo "Response Body:"
echo "$HTTP_BODY" | python3 -m json.tool 2>/dev/null || echo "$HTTP_BODY"
echo ""

if [ "$HTTP_STATUS" -eq 200 ]; then
    echo -e "${GREEN}✓ Test 5 PASSED${NC}"
else
    echo -e "${RED}✗ Test 5 FAILED - Expected HTTP 200, got $HTTP_STATUS${NC}"
fi
echo ""

# Test 6: Verify changes with GET
echo "Test 6: Verify changes with GET /api/prettifier/status"
echo "-------------------------------------------------------"
echo "Expected: HTTP 200 with updated configuration"
echo ""

RESPONSE=$(curl -s -w "\nHTTP_STATUS:%{http_code}" "${BASE_URL}/api/prettifier/status")
HTTP_BODY=$(echo "$RESPONSE" | sed -e 's/HTTP_STATUS\:.*//g')
HTTP_STATUS=$(echo "$RESPONSE" | tr -d '\n' | sed -e 's/.*HTTP_STATUS://')

echo "HTTP Status: $HTTP_STATUS"
echo "Response Body:"
echo "$HTTP_BODY" | python3 -m json.tool 2>/dev/null || echo "$HTTP_BODY"
echo ""

if [ "$HTTP_STATUS" -eq 200 ]; then
    echo -e "${GREEN}✓ Test 6 PASSED${NC}"
else
    echo -e "${RED}✗ Test 6 FAILED - Expected HTTP 200, got $HTTP_STATUS${NC}"
fi
echo ""

echo "========================================="
echo "Test Summary"
echo "========================================="
echo "All tests completed. Check results above."
echo ""
echo "Note: Tests require web server to be running on ${BASE_URL}"
echo "Start server with: ./build/aimux --webui"
