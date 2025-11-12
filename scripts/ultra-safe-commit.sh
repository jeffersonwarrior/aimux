#!/bin/bash

# Ultra-Safe Commit Script for Aimux v2.0
# Explicitly adds only the files we want

set -e

echo "🔒 Ultra-safe commit - Only adding approved files..."

# Clear any existing staged changes
git reset --quiet

# Explicitly add ONLY essential files (no wildcards that could catch unwanted stuff)
echo "📋 Adding explicit approved files..."

# Core version and build files
git add version.txt
git add CMakeLists.txt
git add CMakePresets.json

# Source code directories (but only if they're clean)
[ -d "src" ] && git add src/
[ -d "include" ] && git add include/

# Configuration and webui
[ -d "config" ] && git add config/
[ -d "webui" ] && git add webui/
[ -d "docs" ] && git add docs/
[ -d "qa" ] && git add qa/
[ -d "tests" ] && git add tests/
[ -d "tmp" ] && git add tmp/.gitkeep

# Build system
[ -d "cmake" ] && git add cmake/
[ -f "vcpkg.json" ] && git add vcpkg.json

# Service files
[ -f "aimux.service" ] && git add aimux.service
[ -f ".clang-format" ] && git add .clang-format
[ -f ".gitignore" ] && git add .gitignore

# Scripts (only specific ones)
[ -f "scripts/git-setup.sh" ] && git add scripts/git-setup.sh
[ -f "scripts/clean-commit.sh" ] && git add scripts/clean-commit.sh
[ -f "scripts/emergency-cleanup.sh" ] && git add scripts/emergency-cleanup.sh

# Documentation
[ -f "cerebras.md" ] && git add cerebras.md
[ -f "V20.1-Todo.md" ] && git add V20.1-Todo.md
[ -f "V20-Todo.md" ] && git add V20-Todo.md

# Check what we staged
echo ""
echo "🔍 Files staged for commit:"
git diff --staged --name-status
echo ""
echo "📊 Total staged files: $(git diff --staged --name-only | wc -l | tr -d ' ')"

# Double-check no archives got in
ARCHIVES_STAGED=$(git diff --staged --name-only | grep '^archives/' || true)
if [ -n "$ARCHIVES_STAGED" ]; then
    echo ""
    echo "❌ ERROR: archives files found in staging!"
    echo "📁 Archives files:"
    echo "$ARCHIVES_STAGED"
    echo ""
    echo "🗑️  Resetting and aborting..."
    git reset --quiet
    exit 1
fi

# Confirm commit
echo ""
read -p "✅ Staging looks clean. Commit these files? [y/N]: " response
if [[ ! $response =~ ^[Yy]$ ]]; then
    echo "❌ Commit cancelled"
    exit 1
fi

# Get version
VERSION=$(cat version.txt 2>/dev/null || echo "2.0")

# Create commit
echo ""
echo "💾 Creating safe commit..."
git commit -m "Release v$VERSION

Aimux v2.0 - Jefferson Nunn, Claude Code, Crush Code, GLM 4.6, Sonnet 4.5, GPT-5
(c) 2025 Zackor, LLC. All Rights Reserved

Core Release:
- Production-ready CLI with version management
- C++23 build system with vcpkg integration  
- JSON logging with clean console output
- Router/Bridge/Failover core architecture
- HTTP client and connection pool
- Provider management system
- WebUI server and API handler
- Configuration management (TOON format)
- Comprehensive testing framework

Technical Stack:
- C++23, libcurl, nlohmann/json, Crow (optional)
- clang-tidy, clang-format code quality
- GoogleTest testing framework
- CMake build system"

# Create tag
echo ""
echo "🏷️  Creating release tag..."
git tag -a "v$VERSION" -m "Release v$VERSION

Version $VERSION of Aimux AI Provider Router Bridge
Jefferson Nunn, Claude Code, Crush Code, GLM 4.6, Sonnet 4.5, GPT-5
(c) 2025 Zackor, LLC. All Rights Reserved"

echo ""
echo "✅ Safe commit and tag created!"
echo ""
echo "📊 Final status:"
git status --short

echo ""
echo "🚀 Ready for remote push!"
echo ""
echo "To push to repository:"
echo "  git remote add origin <your-github-url>  # If not already set"
echo "  git push -u origin main               # First time push"
echo "  git push origin v$VERSION              # Push tag"