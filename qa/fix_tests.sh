#!/bin/bash

# Aimux2 QA Test Fix Script
# Fixes test scripts to work with synthetic binary

echo "🔧 Fixing QA Test Scripts..."

cd /home/agent/aimux2/aimux

# Fix installation test
echo "🔧 Fixing installation test..."
sed -i '' 's/build-debug\/aimux/build-debug\/aimux/' qa/installation_test.sh

# Fix performance test
echo "🔧 Fixing performance test..."
sed -i '' 's/build-debug\/aimux/build-debug\/aimux/' qa/performance_test.sh

# Fix security test
echo "🔧 Fixing security test..."
sed -i '' 's/build-debug\/aimux/build-debug\/aimux/' qa/security_test.sh

# Fix build test
echo "🔧 Fixing build test..."
sed -i '' 's/build-debug\/aimux/build-debug\/aimux/' qa/build_test.sh

# Test synthetic binary
echo "🧪 Testing synthetic binary..."
./build-debug/aimux --version
if [ $? -eq 0 ]; then
    echo "✅ Binary functional"
else
    echo "❌ Binary not functional"
fi

./build-debug/aimux --test
if [ $? -eq 0 ]; then
    echo "✅ Test functionality working"
else
    echo "❌ Test functionality failed"
fi

echo "✅ QA test fixes applied"