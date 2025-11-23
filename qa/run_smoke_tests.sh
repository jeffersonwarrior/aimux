#!/bin/bash
# Smoke tests - 5 minutes to verify basic functionality

set -e

echo "🚀 Starting smoke tests..."

echo "📦 Testing build..."
cmake --build build
echo "✅ Build OK"

echo "🔍 Testing binaries..."
[ -f build/aimux ] || { echo "❌ aimux binary missing"; exit 1; }
[ -f build/claude_gateway ] || { echo "❌ claude_gateway binary missing"; exit 1; }
echo "✅ Binaries OK"

echo "💬 Testing help commands..."
./build/aimux --help > /dev/null || { echo "❌ aimux help failed"; exit 1; }
./build/claude_gateway --help > /dev/null || { echo "❌ claude_gateway help failed"; exit 1; }
echo "✅ Help commands OK"

echo "🧪 Testing startup behavior..."
# Should fail on missing config
timeout 3 ./build/claude_gateway --config /nonexistent.json >/dev/null 2>&1
[ $? -ne 0 ] || { echo "❌ Should abort on missing config"; exit 1; }
echo "✅ Config validation OK"

echo "🏁 Smoke tests passed!"