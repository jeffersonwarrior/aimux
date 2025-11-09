# Native Router Components

This directory contains the native C++ router implementation for aimux.

## Current Status (MVP)

For the MVP, we're using a simplified TypeScript-based HTTP proxy instead of the full C++ implementation. The C++ router will be implemented in a future release.

## Structure

```
native/
├── router.cpp          # Current C++ router implementation (future)
├── router.hpp          # Router header files (future)
├── binding.cpp         # Node.js bindings (future)
├── CMakeLists.txt      # Build configuration (future)
└── README.md          # This file
```

## MVP Implementation

The MVP uses `src/router/simple-router.ts` which provides:
- HTTP request interception on localhost:8080
- Basic routing to multiple providers
- Request analysis and type detection
- Response normalization
- Simple failover handling

## Future C++ Implementation

When ready, the C++ router will provide:
- Better performance for high-throughput scenarios
- Lower latency for request routing
- Native HTTP handling capabilities
- Provider-specific optimizations
- Advanced load balancing algorithms