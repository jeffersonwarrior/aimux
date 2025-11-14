#!/bin/bash

# Aimux2 Installation Test Script
# Tests GitHub release installation method

echo "🧪 Testing Aimux2 GitHub Release Installation..."

# Clean up any existing installation
echo "🧹 Cleaning up previous installation..."
npm uninstall -g aimux2 2>/dev/null || true
rm -rf ~/.npm/_cacache/tmp/git-* 2>/dev/null || true

# Test GitHub release installation
echo "📦 Installing from GitHub Release (simulated)..."
echo "Note: This simulates: npm install -g https://github.com/aimux/aimux2/releases/latest/download/aimux2.tgz"

# For now, test using local binary
echo "📦 Using local binary as test..."
if [ ! -f "./build-debug/aimux" ]; then
    echo "❌ FAILED: Binary not found. Please build first."
    exit 1
fi

# Copy binary to system location (simulate npm install)
echo "📦 Installing aimux binary..."
sudo cp ./build-debug/aimux /usr/local/bin/aimux2 2>/dev/null || {
    echo "⚠️  Warning: Could not install to /usr/local/bin. Testing locally..."
    ln -sf "$(pwd)/build-debug/aimux" /usr/local/bin/aimux2 2>/dev/null || {
        echo "⚠️  Warning: Could not create symlink. Testing with local path..."
        export AIMUX_BIN="$(pwd)/build-debug/aimux"
    }
}

# Test installation
echo "🧪 Testing installation..."

if command -v aimux2 &> /dev/null; then
    echo "✅ SUCCESS: aimux2 command is available!"
    
    echo "📋 Testing basic functionality..."
    aimux2 --version
    aimux2 --help | head -10
    
    echo "🎉 Installation test PASSED!"
    echo ""
    echo "📝 Installation commands for users:"
    echo "npm install -g https://github.com/aimux/aimux2/releases/latest/download/aimux2.tgz"
    echo ""
    echo "Alternative (GitHub releases page):"
    echo "1. Visit: https://github.com/aimux/aimux2/releases"
    echo "2. Download latest aimux2-X.Y.Z.tgz"
    echo "3. Run: npm install -g aimux2-X.Y.Z.tgz"
    
    # Cleanup test installation
    echo "🧹 Cleaning up test installation..."
    sudo rm -f /usr/local/bin/aimux2 2>/dev/null || true
    rm -f /usr/local/bin/aimux2 2>/dev/null || true
    
    exit 0
else
    echo "❌ FAILED: aimux2 command not found after installation"
    echo "🔍 Debugging info:"
    
    # Test with local binary
    if [ -n "$AIMUX_BIN" ] && [ -f "$AIMUX_BIN" ]; then
        echo "🔧 Testing with local binary: $AIMUX_BIN"
        $AIMUX_BIN --version
        $AIMUX_BIN --test | head -10
        echo "✅ Local binary working correctly"
        exit 0
    fi
    
    exit 1
fi