# CLAUDE.md

## Project Overview
Aimux v2.0.0 - Router-bridge-claude code system. Refactored from synclaude into C++ for speed. Claude Code will have aimux for the models. AIMUX will dynamically manage all providers and keys. Automatic high speed retry / failover to other providers with the same model class. Automatic key management (rotation of keys with same provider). For example, if Cerebras token limit reached (per minute) then switch to z.ai or synthetic. Automatic speed measurement (always pick the highest speed provider). All performance data logged for tracking over time. Charts/Provider Management via local WebUI.

## Development Commands
```bash
# Build setup
cmake -S . -B build
cmake --build build

# Run executable
./build/aimux

# Run tests
cmake --build build --target test_all        # All tests
cmake --build build --target test_unit       # Unit tests only
cmake --build build --target test_prettifier # Prettifier tests
cmake --build build --target test_integration # Integration tests
cmake --build build --target test_performance # Performance tests

# Clean build
rm -rf build
```

## Architecture
- **Core**: `src/core/` (Router, Failover, Bridge, Thread Manager)
- **Gateway**: `src/gateway/` (Claude Gateway, Format Detection, API Transformation)
- **Providers**: `src/providers/` (Provider implementations)
- **Network**: `src/network/` (HTTP client, Connection pool, SSL)
- **Prettifier**: `src/prettifier/` (Plugin system, Format-specific formatters)
- **WebUI**: `src/webui/` (Crow-based web server, Metrics streaming)
- **Config**: `src/config/` (TOON parser, Production config, Validation)
- **CLI**: `src/cli/` (Command-line interface)
- **Security**: `src/security/` (Secure config management)
- **Logging**: `src/logging/` (Production logger, Correlation context)
- **Monitoring**: `src/monitoring/` (Performance monitoring)
- **Metrics**: `src/metrics/` (Time-series DB, Metrics collection)

## Key Paths
- Config: `~/.config/aimux/config.json`
- Build: `./build/`
- Main executable: `./build/aimux`
- Gateway service: `./build/claude_gateway`
- Tests: `./build/*_test` and `./build/*_tests`

## Code Standards
- C++23 standard
- CMake build system
- vcpkg for dependency management
- Google Test for unit/integration testing
- Header-only libraries: Asio, Crow
- Production optimizations: -O3, LTO, static linking

## Dependencies
- nlohmann-json (JSON parsing)
- Crow (HTTP server framework)
- Asio (Async I/O)
- libcurl (HTTP client)
- OpenSSL (TLS/SSL)
- Google Test (Testing framework)

## File Management
- Use `.claudeignore` to exclude archives and build artifacts
- Environment files (`.env*`) are accessible for configuration
- Archived files in `./archives/` contain old TypeScript implementation
- vcpkg dependencies in `./vcpkg/`

## Credits
Forked from Synclaude by Paranjay Singh (parnexcodes) to Aimux v2.0.0 by Jefferson Nunn, Claude, and Codex.