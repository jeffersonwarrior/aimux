# CLAUDE.md - Development Guide for AIMUX

This file provides guidance to Claude Code when working with code in this repository.

## Git Remote Push Strategy

### Problem
Claude Code cannot use interactive git authentication (like `git push` with HTTPS prompts for username/password) because:
- No TTY (terminal) available for interactive prompts
- GitHub token/credentials cannot be passed interactively
- Standard credential helpers don't work in non-interactive environments
- SSH keys may not be configured for GitHub

### Solution: GIT_ASKPASS Method
This is the working strategy for pushing commits to GitHub remote:

#### Step 1: Create a git password helper script
```bash
cat > /tmp/git-askpass.sh << 'EOF'
#!/bin/bash
echo "YOUR_GITHUB_PAT_TOKEN_HERE"
EOF
chmod +x /tmp/git-askpass.sh
```

#### Step 2: Push using GIT_ASKPASS environment variable
```bash
GIT_ASKPASS=/tmp/git-askpass.sh GIT_ASKPASS_INHERIT=never git push origin master
```

**Why this works:**
- `GIT_ASKPASS=/tmp/git-askpass.sh` - Tells git to use the script instead of prompting
- `GIT_ASKPASS_INHERIT=never` - Prevents git from using inherited SSH_ASKPASS
- The script returns the token, which git uses for authentication
- No interactive prompts needed

#### Full Working Example
```bash
# 1. Create helper script with token from .env
cat > /tmp/git-askpass.sh << 'EOF'
#!/bin/bash
echo "github_pat_XXXXXXXXXXXXX"
EOF
chmod +x /tmp/git-askpass.sh

# 2. Push to remote
GIT_ASKPASS=/tmp/git-askpass.sh GIT_ASKPASS_INHERIT=never git push origin master
```

### Alternative Methods (Not Recommended)

**Method 1: Credential Store (Doesn't work in non-interactive environments)**
```bash
git config --global credential.helper store
# Won't work - git still tries to prompt interactively
```

**Method 2: SSH (Requires configured SSH keys)**
```bash
git remote set-url origin git@github.com:username/repo.git
git push origin master
# Only works if SSH keys are set up on GitHub
```

**Method 3: HTTPS with token in URL (Security Risk)**
```bash
# DON'T DO THIS - leaves token in shell history
git push https://username:TOKEN@github.com/repo.git
```

### Important Notes

1. **Never commit tokens to git** - The token should come from `.env` file (gitignored)
2. **GIT_ASKPASS is the recommended approach** - It's secure and works in non-interactive environments
3. **Keep scripts temporary** - Delete `/tmp/git-askpass.sh` after pushing to avoid leaving tokens in temp
4. **Token should have push access** - Personal Access Token (PAT) needs `repo` scope

### Debugging
If push fails:
```bash
# Test GitHub token works
GIT_ASKPASS=/tmp/git-askpass.sh GIT_ASKPASS_INHERIT=never git ls-remote origin

# Check git config
git config --list | grep credential

# Verify token expiry
# Check token permissions on github.com/settings/tokens
```

## Project Overview

**Aimux** (AI Multiplexer) is a C++ based AI response prettifier and routing system. The current focus is on v2.1 production release with support for multiple AI provider formats.

## C++ Build Commands

```bash
# Configure build
cd /home/aimux
mkdir -p build
cd build
cmake ..

# Build specific targets
make prettifier_plugin_tests     # Build prettifier tests
make -j4                          # Parallel build with 4 threads
cmake --build . -j4              # Alternative parallel build

# Run tests
./prettifier_plugin_tests        # Run all prettifier tests
./prettifier_plugin_tests --gtest_filter="Anthropic*"  # Filter tests
```

## Key Development Areas

### Anthropic Formatter (v2.1.1 Hotfix)
- **File**: `src/prettifier/anthropic_formatter.cpp`
- **Status**: Supports both JSON (v3.5+) and XML (legacy) tool formats
- **Method**: `extract_claude_json_tool_uses()` for modern format
- **Fallback**: `extract_claude_xml_tool_calls()` for older Claude versions

### Performance Targets
- Response formatting: <50ms
- Tool call extraction: <20ms per call
- All formatters: 20-60x faster than targets

### Testing
- 98.8% test pass rate (83/84 tests)
- Security tests: 16+ injection attack patterns blocked
- Memory safety: Zero memory leaks verified
- Thread safety: Concurrent access tested

## Common Commands Reference

```bash
# Git workflow
git status                                    # Check status
git log --oneline -10                        # View recent commits
git add file.cpp                             # Stage changes
git commit -m "message"                      # Commit locally
GIT_ASKPASS=/tmp/git-askpass.sh \
  GIT_ASKPASS_INHERIT=never \
  git push origin master                     # Push to remote

# Build workflow
cd /home/aimux/build
cmake ..
make prettifier_plugin_tests -j4
./prettifier_plugin_tests

# Testing
./prettifier_plugin_tests --gtest_filter="*" --gtest_brief=1
./prettifier_plugin_tests --gtest_filter="Anthropic*"
```

## Important Files

- **CLAUDE.md** - This file (development guide)
- **Version2.1.TODO.md** - Current TODO items
- **ANTHROPIC_JSON_TOOLUSE_FIX_REPORT.md** - Latest hotfix documentation
- **.env** - Environment variables (gitignored, secure)
- **.gitignore** - Git ignore rules

## Notes for Claude Code

When working in this repo:
1. Always use the GIT_ASKPASS method for pushing
2. Don't commit `.env` file - it contains secrets
3. Check test results before pushing
4. Run `make prettifier_plugin_tests` to verify builds
5. Use `git log --oneline` to understand commit history
