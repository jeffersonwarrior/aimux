# Aimux v2.0.0 Router Bridge: Simple + Fast

## Goal
C++ router bridge with simple WebUI. Auto-failover between providers, key rotation, always pick fastest.

## Core Architecture
```
aimux-core (C++) → Simple HTTP API → Clean WebUI (HTML/JS)
```

### Key Components
- **Router**: Route requests to fastest provider
- **Provider Manager**: Handle multiple providers and keys
- **Metrics**: Track speed and performance
- **WebUI**: Simple dashboard for management
- **CLI**: Same interface as current

### Technology Stack (2025)
- **Core**: C++23 (not C++20) - latest features, std::print, std::generator
- **HTTP**: libcurl v8.0+ with HTTP/2 support
- **JSON**: nlohmann/json v3.11+ with string_view support
- **Config**: TOON format (AI-optimized, 30-60% token savings)
- **Storage**: Plain files + in-memory cache (no SQLite bloat)
- **Build**: CMake 3.20+ + vcpkg package manager
- **WebUI**: Tailwind CSS + Chart.js + vanilla JS
- **Server**: Simple embedded HTTP server
- **Logging**: JSON structured logging (network performance patterns)
- **Concurrency**: std::jthread (C++20) for thread-safe operations

### Key C++23 Features Used
- `std::print` instead of iostream for faster output
- `std::generator` for lazy metric streaming
- `std::string_view` throughout to avoid allocations
- `std::flat_map` for provider registry (faster lookup)
- `std::expected` instead of exceptions for error handling
- `uz` literals for size_t safety

## Performance Target
- **Speed**: 10x faster than current
- **Memory**: <50MB total
- **Startup**: <50ms
- **Failover**: <100ms

## Provider System (Simple)

### Provider Structure
```cpp
struct Provider {
    string name;
    string endpoint;
    vector<string> api_keys;
    double avg_response_time;
    bool is_healthy;
};

class ProviderManager {
    vector<Provider> providers;
    string get_next_provider(string model);
    void rotate_key(string provider_name);
    void update_speed(string provider, double ms);
};
```

### Failover Logic
- Always start with fastest provider
- Rate limit → rotate key
- Key exhausted → try next provider
- Provider down → next provider
- Track success/failure rates

### Key Management
- Multiple keys per provider
- Auto-rotate on rate limits
- Track key performance
- Simple round-robin within provider

### Configuration Format (TOON)
```
providers[3]{name,endpoint,api_keys,max_requests_per_minute}:
  cerebras,https://api.cerebras.ai/v1,["key1","key2"],1000
  z.ai,https://api.z.ai/v1,["key1"],5000
  synthetic,https://api.synthetic.new/v1,["key1","key2"],2000

settings:
  webui_port: 8080
  log_level: INFO
  failover_timeout_ms: 100
```

### Health Check Details
- **ping_ms**: Actual round-trip latency to provider
- **payload_tokens_in**: Request token count
- **payload_tokens_out**: Response token count
- **payload_bytes_in**: Raw request bytes
- **payload_bytes_out**: Raw response bytes
- **error_codes**: Structured error with extended descriptions
- **success_rate**: Rolling window success percentage
- **uptime_ms**: Provider availability duration

## Simple WebUI

### The Dashboard
**Single page** - clean, minimal, elegant

**Main View:**
- Provider status cards (online/offline, speed, health)
- Live response time chart (last hour)
- Add/remove providers button
- API key management

**API Endpoints:**
```
GET  /api/providers          # Get all providers
POST /api/providers          # Add provider
PUT  /api/providers/:id      # Update provider
GET  /api/metrics            # Get performance data
```

**Frontend (Awards-Inspired Design):**
- Following 2025 design trends: zero interface, bold typography, glassmorphism
- Tailwind CSS + Chart.js for performance graphs
- Subtle micro-interactions (0.3s transitions)
- Auto-refresh every 5 seconds
- Mobile-first responsive design
- Dark theme with high contrast accents

**UI Layout (Minimalist):**
```
AIMUX v2.0    12-25-24 / 14:30:45

┌───────────┐ ┌───────────┐ ┌─────────┐
│ Cerebras  │ │ z.ai      │ │ Synthetic│
│ 125ms     │ │ 89ms      │ │ 203ms   │
│ ●         │ │ ●         │ │ ●      │
● = Healthy │ ● = Slow   │ ○ = Down │
└───────────┘ └───────────┘ └─────────┘

Performance (1h)
[📊 live chart]

[+ Add Provider] [⚙ Settings]
```

**Provider Editor (Clean Modal):**
```
Provider: Cerebras
Endpoint: https://api.cerebras.ai

API Keys
key1_live_abc123     [⚙] [−]
key2_live_def456     [⚙] [−]
key3_test_ghi789     [⚙] [−]

[+ Add Key]    [Cancel] [Save]
```

**Design Principles (2025 Best Practices):**
- Content-first, zero interface approach
- Bold typography for hierarchy
- Strategic whitespace (45% negative space)
- Single-purpose elements per award-winning apps
- Auto-save all changes
- Subtle hover states (transform scale 1.02)
- High WCAG contrast ratios
- No jargon, no marketing text

## 4-Week Timeline

### Week 1: Core Router
- C++ project with CMake + vcpkg
- Basic HTTP client (libcurl)
- Simple provider struct
- TOON config files + in-memory cache
- Same CLI interface
- JSON structured logging

### Week 2: Provider System
- Multi-provider support
- Key rotation logic
- Basic failover
- Speed measurement
- Health check implementation

### Week 3: Simple WebUI
- Embedded HTTP server
- Single HTML page
- Provider cards + chart.js
- REST API endpoints
- Basic auth (admin/admin)

### Week 4: Polish & Migration
- Migration tool from TS configs + JSON scanning
- Performance testing + load testing
- systemd service file
- Backup/restore CLI commands
- Documentation

## File Structure (Bridge Pattern)
```
src/
├── main.cpp              # Entry point
├── core/
│   ├── router.cpp         # Bridge pattern - router abstraction
│   ├── bridge.cpp         # Protocol translation
│   └── failover.cpp       # Circuit breaker logic
├── providers/
│   ├── provider_base.cpp  # Provider interface
│   ├── cerebras.cpp       # Cerebras provider
│   ├── zai.cpp           # z.ai provider
│   └── synthetic.cpp      # Synthetic provider
├── network/
│   ├── http_client.cpp    # libcurl wrapper
│   └── connection_pool.cpp # Keep-alive connections
├── storage/
│   ├── config_store.cpp   # TOON file management
│   ├── metrics_store.cpp  # Plain text log storage
│   └── cache.cpp          # In-memory provider cache
├── logging/
│   ├── json_logger.cpp    # Structured logging
│   └── health_logger.cpp  # Health check logging
├── webui/
│   ├── server.cpp         # Embedded HTTP server
│   └── api_handler.cpp    # REST API + basic auth
├── cli/
│   ├── commands.cpp       # CLI interface
│   └── migrate.cpp        # Migration tool
└── utils/
    ├── toon_parser.cpp    # TOON format parser
    └── backup.cpp         # Backup/restore utilities

webui/
├── index.html            # Dashboard
├── assets/
│   ├── style.css         # Tailwind styles
│   ├── script.js         # Chart.js + API
│   └── icons.svg         # Status icons
└── components/
    ├── provider-card.js
    └── performance-chart.js
```

## High-Performance Build (C++23 + vcpkg)
```cmake
cmake_minimum_required(VERSION 3.20)
project(aimux VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# vcpkg integration
set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake"
  CACHE STRING "Vcpkg toolchain file")

# Optimizations
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native")
set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wpedantic")

find_package(CURL 8.0 REQUIRED)
find_package(nlohmann_json 3.11 REQUIRED)

add_executable(aimux
    src/main.cpp
    src/core/*.cpp
    src/providers/*.cpp
    src/network/*.cpp
    src/storage/*.cpp
    src/logging/*.cpp
    src/webui/*.cpp
    src/cli/*.cpp
    src/utils/*.cpp
)

target_link_libraries(aimux
    CURL::libcurl
    nlohmann_json::nlohmann_json
    pthread
)

target_compile_features(aimux PRIVATE cxx_std_23)

# vcpkg.json dependencies:
# {
#   "name": "aimux",
#   "version": "2.0.0",
#   "dependencies": [
#     "curl",
#     "nlohmann-json"
#   ]
# }

# Install targets
install(TARGETS aimux DESTINATION bin)
install(FILES aimux.service DESTINATION lib/systemd/user)
install(DIRECTORY webui/ DESTINATION share/aimux/webui)
```

## systemd Service
```ini
# ~/.config/systemd/user/aimux.service
[Unit]
Description=Aimux v2.0.0 Router Bridge
After=network.target

[Service]
Type=simple
ExecStart=/usr/local/bin/aimux --daemon
Restart=always
RestartSec=5
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=default.target
```

## CLI Commands
```bash
# Basic usage
aimux --help
aimux --list-providers
aimux --test-provider cerebras

# Configuration management
aimux --backup-config /path/to/backup.toon
aimux --restore-config /path/to/backup.toon

# Migration
aimux --migrate-from-ts ~/.config/aimux/
aimux --scan-json-keys /path/to/configs/

# Testing
aimux --load-test 1000             # 1000 requests
aimux --benchmark-providers       # Speed comparison
```

**Architecture Benefits:**
- Bridge pattern: Independent evolution of router logic and providers
- Hot/cold path separation for low latency (HFT patterns)
- Lock-free queue between network and processing threads
- Zero-copy where possible with string_view
- TOON format: 30-60% token savings for AI configs
- Plain file storage: No database bloat, instant startup
- JSON logging: Network ops patterns, easy debugging
- systemd integration: Native Linux service management

**Deployment Options:**
- **Native**: Single binary, systemdService (preferred for speed)
- **Docker**: Optional multi-stage build for compatibility
- **Backup**: TOON format configs - version control friendly

**Testing Strategy:**
- **Unit tests**: Google Test for all core components
- **Load testing**: Simulate 1000+ concurrent requests
- **Failover testing**: Provider outage simulation
- **Migration testing**: TS → TOON conversion validation

That's it. Native performance, AI-optimized configs, enterprise reliability.