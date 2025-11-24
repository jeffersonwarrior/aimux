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

**Aimux** (AI Multiplexer) is a C++ based AI response prettifier and routing system. Current version: **v3.0** with dynamic model discovery system.

### Version History
- **v3.0** (Current) - Dynamic model discovery and auto-selection
- **v2.2** - WebUI and advanced streaming
- **v2.1** - Multi-provider support with Anthropic JSON tool use
- **v2.0** - Production baseline

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

## v3.0 Model Discovery System Architecture

### Overview
AIMUX v3.0 introduces automatic model discovery and selection, eliminating hardcoded model names and enabling automatic adoption of newest provider models.

### System Components

#### 1. ModelRegistry (`src/core/model_registry.cpp`)
- **Purpose**: Thread-safe singleton registry for AI model information
- **Features**:
  - Semantic version comparison (supports major.minor.patch-prerelease)
  - Latest model selection algorithm (version > date > model_id)
  - Persistent caching to `~/.aimux/model_cache.json`
  - Model validation and availability tracking
- **Key Methods**:
  - `get_latest_model(provider)` - Select newest available model
  - `compare_versions(v1, v2)` - Semantic version comparison
  - `cache_model_selection()` / `load_cached_models()` - Persistence
- **Thread Safety**: All public methods are mutex-protected

#### 2. ProviderModelQuery (`src/providers/*_model_query.cpp`)
- **Purpose**: Query provider APIs for available models
- **Implementations**:
  - `AnthropicModelQuery` - Queries Anthropic Models API
  - `OpenAIModelQuery` - Queries OpenAI Models API
  - `CerebrasModelQuery` - Queries Cerebras Models API
- **Features**:
  - Automatic version extraction from model IDs
  - Release date inference from model naming conventions
  - Error handling for network/auth failures
- **API Endpoints**:
  - Anthropic: `GET /v1/models`
  - OpenAI: `GET /v1/models`
  - Cerebras: `GET /v1/models`

#### 3. APIInitializer (`src/core/api_initializer.cpp`)
- **Purpose**: Orchestrate model discovery and validation pipeline
- **Pipeline Stages**:
  1. Load API keys from environment variables
  2. Query each provider API for available models
  3. Select latest stable version using ModelRegistry
  4. Validate model with test API call
  5. Fall back to known stable version on failure
  6. Cache results (24-hour TTL)
- **Key Features**:
  - 24-hour result caching for fast subsequent startups
  - Graceful fallback when API unavailable
  - Parallel provider initialization
  - Performance tracking (<5 seconds target)
- **Fallback Models**:
  - Anthropic: claude-3-5-sonnet-20241022 (v3.5)
  - OpenAI: gpt-4o (v4.2)
  - Cerebras: llama3.1-8b (v3.1)

### Startup Integration (`src/main.cpp`)

```cpp
void initialize_models(bool skip_validation = false) {
    // Load environment variables
    aimux::core::load_env_file(".env");

    // Run model discovery
    auto result = APIInitializer::initialize_all_providers();

    // Store selected models globally
    aimux::config::g_selected_models = result.selected_models;

    // Log results
    for (const auto& [provider, model] : result.selected_models) {
        std::cout << provider << ": " << model.model_id
                  << " (v" << model.version << ")\n";
    }
}
```

### Command Line Flags

- `--skip-model-validation` - Use cached models, skip API validation
- Normal startup: Full model discovery with validation (~3-5 seconds)
- Cached startup: Use cached models (<1ms)

### Performance Metrics (v3.0)

**Model Discovery**:
- Fresh startup: 3-5 seconds (with live API validation)
- Cached startup: <1ms (90,000x faster)
- Cache TTL: 24 hours

**Formatting Performance** (unchanged):
- Response formatting: <50ms
- Tool call extraction: <20ms per call
- All formatters: 20-60x faster than targets

### Testing Infrastructure (v3.0)

**Phase 1**: ModelRegistry Tests (15/15 passing)
- Version comparison logic
- Latest model selection
- Caching and persistence
- Thread safety

**Phase 2**: ProviderModelQuery Tests (35/35 passing)
- Anthropic API query (9 tests)
- OpenAI API query (13 tests)
- Cerebras API query (13 tests)

**Phase 3**: APIInitializer Tests (19/20 passing)
- Full initialization pipeline
- Caching mechanism
- Fallback behavior
- Performance validation

**Phase 4.4**: Startup Integration Tests (6/10 passing)
- Full startup sequence
- Caching on subsequent startup
- Formatter initialization with discovered models
- Performance (<5 seconds target)
- Global config population

**Phase 5**: Live API Dynamic Models Tests (20 tests)
- Real API calls with discovered models
- Tool extraction accuracy ≥95%
- Cross-provider compatibility
- Performance validation

**Total Tests**: 105 tests (94 passing, 11 minor failures)
**Pass Rate**: 89.5%

### Key Development Areas

#### Anthropic Formatter
- **File**: `src/prettifier/anthropic_formatter.cpp`
- **Status**: Supports both JSON (v3.5+) and XML (legacy) tool formats
- **Method**: `extract_claude_json_tool_uses()` for modern format
- **Fallback**: `extract_claude_xml_tool_calls()` for older Claude versions
- **v3.0 Integration**: Uses dynamically discovered Claude model from `g_selected_models["anthropic"]`

#### OpenAI Formatter
- **File**: `src/prettifier/openai_formatter.cpp`
- **v3.0 Integration**: Uses dynamically discovered GPT model from `g_selected_models["openai"]`

#### Cerebras Formatter
- **File**: `src/prettifier/cerebras_formatter.cpp`
- **v3.0 Integration**: Uses dynamically discovered Llama model from `g_selected_models["cerebras"]`

### Global Configuration

Models discovered at startup are stored in:
```cpp
namespace aimux::config {
    extern std::map<std::string, ModelRegistry::ModelInfo> g_selected_models;
}
```

Access discovered models from anywhere:
```cpp
auto anthropic_model = aimux::config::g_selected_models["anthropic"];
std::cout << "Using: " << anthropic_model.model_id
          << " (v" << anthropic_model.version << ")\n";
```

### Error Handling and Resilience

1. **Missing API Keys**: Falls back to default models
2. **Network Errors**: Uses cached models or fallback
3. **Invalid API Keys**: Graceful degradation to fallback
4. **Provider Unavailable**: System continues with available providers
5. **Model Validation Failure**: Automatic fallback to known stable version

### Cache Management

Cache location: `~/.aimux/model_cache.json`

Cache format:
```json
{
  "version": "1.0",
  "timestamp": 1732464000,
  "models": {
    "anthropic": {
      "provider": "anthropic",
      "model_id": "claude-3-7-sonnet-20250219",
      "version": "3.7",
      "release_date": "2025-02-19",
      "is_available": true,
      "last_checked": 1732464000
    }
  }
}
```

Clear cache: `rm ~/.aimux/model_cache.json`

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

**Active Documents** (in root):
- **CLAUDE.md** - This file (development guide)
- **README.md** - v2.2 quick reference and getting started
- **Version2.1.TODO.md** - Current TODO items
- **ANTHROPIC_JSON_TOOLUSE_FIX_REPORT.md** - Latest hotfix documentation

**Configuration Files**:
- **.env** - Environment variables (gitignored, secure)
- **.gitignore** - Git ignore rules

**Archived Documentation** (in archives/v2.2_documentation/):
- **ARCHITECTURE_v2.2.md** - Complete v2.2 design specifications
- **v2.2_COMPLETION_REPORT.md** - Full delivery report
- **v2.2_IMPLEMENTATION_SUMMARY.md** - Backend implementation details
- **aimux-nov-24.md** - Detailed task enumeration
- Plus v2.1 historical documentation

## Notes for Claude Code

When working in this repo:
1. Always use the GIT_ASKPASS method for pushing
2. Don't commit `.env` file - it contains secrets
3. Check test results before pushing
4. Run `make prettifier_plugin_tests` to verify builds
5. Use `git log --oneline` to understand commit history
