# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**Aimux** (AI Multiplexer) is a multi-provider CLI tool that intelligently routes Claude Code requests across multiple Anthropic-compatible AI providers. It transforms single-provider interactions into a flexible, provider-agnostic experience with intelligent routing, failover, and cross-provider model combinations.

## Key Features

- **Multi-Provider Support**: Minimax M2, Z.AI, Synthetic.New, and more
- **Hybrid Model Routing**: Thinking requests to one provider, regular requests to another
- **C++ Performance Router**: Native HTTP request interception and routing
- **Intelligent Failover**: Automatic provider switching during outages
- **Cost Optimization**: Route requests to most cost-effective provider per request type

## Development Commands

### Environment Setup
```bash
# Install dependencies
npm install

# Build the project
npm run build

# Run in development mode
npm run dev

# Link for global testing
npm link
```

### Testing
```bash
# Run all tests with coverage
npm test

# Run tests in watch mode
npm run test:watch

# Run tests with coverage report
npm run test:coverage

# Run specific test file
npm test -- provider-manager.test.ts
```

### Code Quality
```bash
# Format code
npm run format

# Lint with ESLint
npm run lint

# Fix linting issues
npm run lint:fix

# Run all quality checks together
npm run lint && npm test && npm run build
```

### Local Installation and Testing
```bash
# Install locally for development
npm install && npm run build && npm link

# Test the CLI
aimux --help
aimux providers
aimux minimax setup

# Uninstall when done
npm unlink -g aimux
```

## Architecture Overview

The application follows a modular TypeScript architecture with a multi-provider focus:

### Core Components

1. **CLI Layer (`src/cli/`)** - Commander.js-based command-line interface with provider-specific commands
2. **Application Layer (`src/core/`)** - Main orchestrator coordinating providers and the C++ router
3. **Provider Management (`src/providers/`)** - Abstract provider interface with concrete implementations
4. **Router (`src/router/`)** - C++ router with TypeScript bindings for request multiplexing
5. **Configuration (`src/config/`)** - Multi-provider configuration management with validation
6. **User Interface (`src/ui/`)** - React-based terminal UI for provider selection and management
7. **Launcher (`src/launcher/`)** - Modified Claude Code launcher with environment setup

### Key Data Flows

1. **Provider Selection Flow**: CLI → App → ProviderManager → UI → Provider-specific setup
2. **Request Routing Flow**: Claude Code → C++ Router → Provider → Response normalization
3. **Hybrid Mode Flow**: Request parsing → Provider routing logic → Cross-provider response aggregation
4. **Configuration Flow**: CLI → App → Multi-provider config → UI prompts → Config persistence

### Configuration Architecture

- **Config Storage**: `~/.config/aimux/config.json`
- **Provider Configs**: Individual provider settings and capabilities
- **Model Caches**: Per-provider model catalogs with capability mapping
- **Router Config**: C++ router configuration and provider routing rules

### C++ Router Integration

The router acts as an intelligent HTTP proxy that:

1. **Intercepts Requests**: Listens on localhost, intercepting Claude Code API calls
2. **Analyzes Type**: Determines thinking vs regular request patterns
3. **Routes Intelligently**: Forwards to appropriate provider based on configuration
4. **Normalizes Responses**: Ensures consistent response format across providers
5. **Handles Failover**: Automatically switches providers on errors or timeouts

## Development Guidelines

### Command Structure

The aimux CLI follows this pattern:
```bash
# Main commands
aimux                              # Launch last-used provider
aimux <provider>                   # Launch specific provider
aimux <provider> setup             # Setup provider
aimux <provider> dangerous         # Launch with dangerous flags
aimux providers                    # List all providers and status
aimux config                       # Show current configuration
aimux hybrid --thinking-provider <x> --default-provider <y>  # Hybrid mode
```

### Provider Development

When implementing a new provider:

1. **Implement BaseProvider Interface**:
   ```typescript
   class NewProvider extends BaseProvider {
     abstract name = "Provider Name";
     abstract authenticate(): Promise<boolean>;
     abstract fetchModels(): Promise<ModelInfo[]>;
     abstract validateApiKey(apiKey: string): Promise<boolean>;
     abstract getCapabilities(): ProviderCapabilities;
   }
   ```

2. **Add Provider Configuration**:
   - Update provider registry
   - Add configuration schema validation
   - Implement provider-specific UI components

3. **Test Integration**:
   - Add provider-specific test suites
   - Test routing logic with new provider
   - Verify failover behavior

### C++ Router Development

The router uses:
- **C++20** with modern language features
- **libcurl** for HTTP client functionality
- **nlohmann/json** for JSON parsing
- **Node.js N-API** for TypeScript bindings

When modifying the router:
- Maintain thread safety for concurrent requests
- Implement proper error handling and logging
- Test with all provider implementations
- Monitor memory usage and prevent leaks

### Error Handling

- **Provider Errors**: Graceful fallback with user notification
- **Network Errors**: Automatic retry with exponential backoff
- **Configuration Errors**: Clear error messages with setup guidance
- **Router Errors**: Fallback to direct provider communication

### Testing Strategy

- **Unit Tests**: Individual provider implementations
- **Integration Tests**: End-to-end request routing
- **Performance Tests**: Router latency and throughput
- **Failover Tests**: Provider outage scenarios
- **Configuration Tests**: Edge cases and validation

## Provider Capabilities Matrix

| Provider | Thinking | Vision | Tools | Cost Priority | Speed Priority |
|----------|----------|--------|-------|---------------|----------------|
| Minimax M2 | ✅ | ✅ | ✅ | Medium | High |
| Z.AI | ❌ | ✅ | ✅ | Low | High |
| Synthetic.New | ✅ | ❌ | ✅ | High | Medium |

## Environment Variables for Development

- `AIMUX_DEBUG`: Enable debug logging for router and provider requests
- `AIMUX_ROUTER_PORT`: Port for C++ router (default: 8080)
- `AIMUX_PROVIDER_OVERRIDE`: Force using specific provider for testing
- `AIMUX_LOG_LEVEL`: Set logging verbosity (debug, info, warn, error)

## Build Process

```bash
# Development
npm run dev          # Run with ts-node

# Production
npm run build        # Compile TypeScript to JavaScript
npm run build:router # Build C++ router component
npm run start        # Run compiled version

# Testing
npm test             # Run all tests
npm run test:coverage # Run with coverage report
```

The build process compiles TypeScript to `dist/` directory and builds the C++ router as a native addon.

## Type Safety

This project uses TypeScript's strict mode with:
- No implicit any
- Strict null checks
- Exact optional properties
- Zod runtime validation for all external data
- Provider interface contracts enforced at compile and runtime

## Multi-Provider Routing Logic

The router implements sophisticated routing algorithms:

1. **Capability-Based Routing**: Route based on request requirements (thinking, vision, tools)
2. **Cost Optimization**: Select cheapest provider for supported capabilities
3. **Performance Routing**: Choose fastest provider for latency-sensitive requests
4. **Reliability Routing**: Prefer providers with best uptime for critical requests

## Security Considerations

- **API Key Isolation**: Separate encrypted storage per provider
- **Request Sanitization**: Validate and sanitize all outbound requests
- **Response Filtering**: Remove sensitive information from provider responses
- **Network Security**: TLS verification and certificate pinning where supported

## Future Enhancement Roadmap

Based on the V2 plan (v2plan.md), future features include:

- **Provider Plugins**: Dynamic loading of new provider implementations
- **Team Management**: Shared configurations and usage quotas
- **Advanced Analytics**: Detailed usage metrics and cost tracking
- **Performance Monitoring**: Real-time provider performance comparison
- **AI-Optimized Routing**: ML-based provider selection based on request patterns

## Migration from SynClaude

For users migrating from SynClaude v1:
- Automatic configuration import from `~/.config/synclaude/`
- Legacy command aliases (`synclaude` → `aimux`)
- Migration wizard for provider setup
- Backward compatibility during transition period

## Troubleshooting

### Common Issues

1. **Router Not Starting**: Check if port 8080 is available
2. **Provider Authentication**: Verify API keys and network connectivity
3. **Model Selection**: Ensure models support required capabilities
4. **Performance Issues**: Monitor router logs for bottlenecks

### Debug Mode
```bash
AIMUX_DEBUG=1 aimux --verbose
```

This enables detailed logging for both the TypeScript components and the C++ router, helping diagnose routing issues, provider communication problems, and performance bottlenecks.