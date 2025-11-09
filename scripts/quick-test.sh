#!/bin/bash

# Quick test script for aimux Docker environment
set -e

CONTAINER_NAME="aimux-test"
ROUTER_PORT="8080"

echo "=== Aimux Quick Test ==="

# Check if container is running
if ! docker ps --format '{{.Names}}' | grep -q "^${CONTAINER_NAME}$"; then
    echo "❌ Container is not running. Run './scripts/test-docker.sh test' first."
    exit 1
fi

echo "✅ Container is running"

# Test router health
if curl -s -f http://localhost:${ROUTER_PORT}/health >/dev/null; then
    echo "✅ Router is healthy"
else
    echo "❌ Router health check failed"
fi

# Test aimux CLI
if docker exec "${CONTAINER_NAME}" aimux --help >/dev/null 2>&1; then
    echo "✅ aimux CLI is working"
else
    echo "❌ aimux CLI test failed"
fi

echo "=== Quick Test Complete ==="
