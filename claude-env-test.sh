#!/bin/bash

echo "=== Testing Claude Code Environment Variables ==="

# Set up the exact same environment that aimux would set for Z.AI
export ANTHROPIC_API_KEY="85c99bec0fa64a0d8a4a01463868667a.RsDzW0iuxtgvYqd2"
export ANTHROPIC_BASE_URL="https://api.z.ai/api/anthropic/v1"

echo "Environment variables set:"
echo "ANTHROPIC_API_KEY=${ANTHROPIC_API_KEY:0:10}..."
echo "ANTHROPIC_BASE_URL=$ANTHROPIC_BASE_URL"

# Test what Claude Code sees
echo
echo "=== Testing Claude Code directly ==="
echo "claude" --version 2>/dev/null || echo "Claude Code not available for testing"

# Test if we can make a direct API call with these exact env vars
echo
echo "=== Testing API call with same env ==="
response=$(curl -s -w "\nHTTP_CODE:%{http_code}" -X POST "$ANTHROPIC_BASE_URL/messages" \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $ANTHROPIC_API_KEY" \
  -d '{
    "model": "glm-4.6",
    "max_tokens": 5,
    "messages": [{"role": "user", "content": "test"}]
  }')

echo "$response" | tail -1

echo
echo "=== Test Complete ==="