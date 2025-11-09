# V2 Plan: Multi-Provider AI Bridge Architecture

## Executive Summary

Transform SynClaude from a single-provider launcher into a multi-provider AI bridge system that seamlessly routes Claude Code requests across multiple Anthropic-compatible providers while maintaining a unified user experience.

## Proposed Command Name: **`aimux`**

Alternative names: `ai-bridge`, `ai-router`, `aicli`

**Rationale for `aimux`:**
- **"AI" + "Multiplexer"**: Technical and accurate description of functionality
- **Concise & memorable**: Short 5-character name, easy to type
- **Developer-friendly**: Appeals to technical users who understand multiplexing
- **Future-proof**: Scales well as we add more providers and routing strategies

## Core Requirements

### 1. Provider Management
- Support multiple Anthropic-compatible providers (Minimax M2, Z.AI, Synthetic.New, etc.)
- Persistent configuration per provider
- Automatic provider detection and validation
- Provider-specific model catalogs and capabilities

### 2. Hybrid Model Support
- **Cross-provider routing**: Thinking model from Provider A + Default model from Provider B
- Seamless fallback between providers
- Provider-specific capabilities (e.g., Z.AI no thinking support)
- Load balancing and cost optimization

### 3. Command Structure
```bash
# Launch last used provider
aimux

# Launch specific provider
aimux minimax
aimux zai
aimux synthetic

# Setup specific provider
aimux minimax setup
aimux zai setup

# Dangerous mode for provider
aimux minimax dangerous
aimux zai dangerous

# List providers and status
aimux providers

# Show current config
aimux config

# Cross-provider hybrid mode
aimux hybrid --thinking-provider minimax --default-provider zai
```

### 4. Internal C++ Router Architecture

#### 4.1. Router Responsibilities
- Intercept Claude Code API requests
- parse request type (thinking vs regular)
- Route to appropriate provider based on configuration
- Handle response normalization and error handling
- Maintain request/response correlation
- Implement retry logic and fallback chains

#### 4.2. Router Components

```
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────┐
│ Claude Code     │───▶│ ai-bridge C++    │───▶│ Minimax API     │
│ Process         │    │ Router           │    │                 │
└─────────────────┘    └──────────────────┘    └─────────────────┘
                              │
                              ▼
                       ┌──────────────────┐    ┌─────────────────┐
                       │ Request Parser & │───▶│ Z.AI API        │
                       │ routing engine   │    │                 │
                       └──────────────────┘    └─────────────────┘
                              │
                              ▼
                       ┌──────────────────┐    ┌─────────────────┐
                       │ Response Normal- │◀───│ Synthetic API   │
                       │ izer & Fallback  │    │                 │
                       └──────────────────┘    └─────────────────┘
```

#### 4.3. Router Configuration Logic

```cpp
// Pseudocode for routing logic
class AiBridgeRouter {
  struct ProviderConfig {
    string apiUrl;
    string apiKey;
    string defaultModel;
    string thinkingModel; // May be empty
    bool supportsThinking;
    bool hasApiKey;
  };

  struct HybridConfig {
    string thinkingProvider;
    string defaultProvider;
  };

  // Route request based on type and configuration
  ApiResponse routeRequest(request) {
    if (isThinkingRequest(request)) {
      return routeToThinkingProvider(request);
    } else {
      return routeToDefaultProvider(request);
    }
  }

  // Handle cross-provider scenarios
  ApiResponse handleHybridRequest(request) {
    // For complex scenarios where thinking and default models
    // might need coordination
  }
};
```

## Architecture Design

### 5. Configuration System

#### 5.1. Provider Configuration Structure
```json
{
  "version": "2.0.0",
  "lastUsedProvider": "minimax",
  "providers": {
    "minimax": {
      "name": "Minimax M2",
      "apiUrl": "https://api.minimax.chat/v1",
      "apiKey": "...",
      "models": {
        "default": "claude-3-5-sonnet-20241022",
        "thinking": "claude-3-5-sonnet-20241022",
        "available": ["claude-3-5-sonnet-20241022", "claude-3-haiku-20240307"]
      },
      "capabilities": {
        "thinking": true,
        "vision": true,
        "tools": true
      },
      "status": "active"
    },
    "zai": {
      "name": "Z.AI",
      "apiUrl": "https://api.z.ai/v1",
      "apiKey": "...",
      "models": {
        "default": "claude-3-5-sonnet-20241022",
        "thinking": "",
        "available": ["claude-3-5-sonnet-20241022", "claude-3-haiku-20240307"]
      },
      "capabilities": {
        "thinking": false,
        "vision": true,
        "tools": true
      },
      "status": "active"
    },
    "synthetic": {
      "name": "Synthetic.New",
      "apiUrl": "https://api.synthetic.new/anthropic",
      "apiKey": "...",
      "models": {
        "default": "hf:zai-org/GLM-4.6",
        "thinking": "hf:moonshotai/Kimi-K2-Thinking",
        "available": ["hf:zai-org/GLM-4.6", "hf:moonshotai/Kimi-K2-Thinking"]
      },
      "capabilities": {
        "thinking": true,
        "vision": false,
        "tools": true
      },
      "status": "active"
    }
  },
  "hybridMode": {
    "enabled": true,
    "thinkingProvider": "minimax",
    "defaultProvider": "zai"
  }
}
```

### 6. Component Reorganization

#### 6.1. New Directory Structure
```
src/
├── cli/
│   ├── index.ts           # Main CLI entry point
│   ├── commands.ts        # Command definitions
│   └── parsers.ts         # Argument parsing logic
├── core/
│   ├── app.ts             # Main application orchestrator
│   ├── provider-manager.ts # Multi-provider management
│   └── hybrid-router.ts   # Cross-provider routing logic
├── providers/
│   ├── base-provider.ts   # Abstract provider interface
│   ├── minimax-provider.ts
│   ├── zai-provider.ts
│   ├── synthetic-provider.ts
│   └── provider-registry.ts
├── router/
│   ├── cpp-router/        # C++ router implementation
│   ├── router-interface.ts # TypeScript interface to C++
│   └── request-types.ts
├── config/
│   ├── v2-config.ts       # New configuration system
│   ├── provider-config.ts # Provider-specific config validation
│   └── hybrid-config.ts   # Hybrid mode configuration
├── ui/
│   ├── components/        # Updated UI components
│   ├── provider-selector.tsx
│   └── status-display.tsx
└── launcher/
    ├── claude-launcher.ts # Modified launcher
    └── environment-setup.ts
```

### 7. Provider Interface Abstraction

#### 7.1. Base Provider Class
```typescript
abstract class BaseProvider {
  abstract name: string;
  abstract config: ProviderConfig;

  // Core provider operations
  abstract authenticate(): Promise<boolean>;
  abstract fetchModels(): Promise<ModelInfo[]>;
  abstract validateApiKey(apiKey: string): Promise<boolean>;
  abstract getCapabilities(): ProviderCapabilities;

  // Provider-specific UI setup
  abstract setupInterface(): Promise<SetupResult>;
  abstract modelSelectionInterface(models: ModelInfo[]): Promise<SelectedModels>;
}

class MinimaxProvider extends BaseProvider {
  name = "Minimax M2";
  config: MinimaxConfig;

  async authenticate(): Promise<boolean> {
    // Minimax-specific authentication
  }

  // ... implementation
}
```

### 8. C++ Router Implementation Strategy

#### 8.1. Technology Stack
- **Language**: C++17/20
- **HTTP Library**: libcurl or cpp-httplib
- **JSON Parsing**: nlohmann/json
- **Build System**: CMake
- **Bindings**: Node.js Native Addons or N-API

#### 8.2. Router Node.js Interface
```typescript
// TypeScript interface to C++ router
interface AiBridgeRouter {
  // Initialize router with configuration
  initialize(config: RouterConfig): Promise<void>;

  // Start router server
  start(port: number): Promise<void>;

  // Stop router
  stop(): Promise<void>;

  // Update provider configuration
  updateProvider(provider: string, config: ProviderConfig): Promise<void>;

  // Get router status
  getStatus(): Promise<RouterStatus>;
}
```

#### 8.3. Router Implementation Workflow
1. **Startup**: Load configuration, initialize HTTP server
2. **Request Interception**: Listen on localhost port, intercept Claude Code requests
3. **Provider Selection**: Analyze request type and routing rules
4. **API Forwarding**: Transform and forward to appropriate provider
5. **Response Processing**: Normalize response format, handle errors
6. **Logging**: Maintain request/response logs for debugging

### 9. Migration Strategy

#### 9.1. Phase 1: Foundation (Weeks 1-2)
- [ ] Create new CLI command structure with `aimux` command
- [ ] Implement provider abstraction layer
- [ ] Add multi-provider configuration system
- [ ] Create provider registry and basic Minimax/Z.AI providers

#### 9.2. Phase 2: UI/UX Updates (Weeks 3-4)
- [ ] Update UI for provider selection and management
- [ ] Implement provider status indicators
- [ ] Add hybrid model configuration interface
- [ ] Create provider-specific setup flows

#### 9.3. Phase 3: C++ Router Development (Weeks 5-7)
- [ ] Develop C++ HTTP router core
- [ ] Implement request parsing and routing logic
- [ ] Add Node.js bindings
- [ ] Create cross-provider routing algorithms
- [ ] Implement fallback and retry mechanisms

#### 9.4. Phase 4: Integration & Testing (Week 8)
- [ ] Integrate C++ router with TypeScript components
- [ ] Implement comprehensive error handling
- [ ] Add provider failover logic
- [ ] Create test suites for all provider combinations
- [ ] Performance optimization and load testing

#### 9.5. Phase 5: Polish & Documentation (Week 9)
- [ ] Update documentation for multi-provider setup
- [ ] Add provider comparison matrix
- [ ] Create migration guide from SynClaude v1
- [ ] Final testing and bug fixes

### 10. Security Considerations

#### 10.1. API Key Management
- Separate encryption per provider
- Secure storage in OS keychain when possible
- Provider-specific token refresh mechanisms

#### 10.2. Network Security
- Validate provider SSL certificates
- Implement request signing where required
- Rate limiting and abuse prevention

#### 10.3. Isolation
- Router runs in isolated process
- Provider-specific error boundaries
- Memory safety in C++ components

### 11. Performance & Reliability

#### 11.1. Caching Strategy
- Model catalogs per provider
- Provider capability information
- Response caching for repeated requests

#### 11.2. Fallback Mechanisms
- Provider outage detection
- Automatic failover to backup providers
- Graceful degradation when capabilities differ

#### 11.3. Monitoring
- Provider latency tracking
- Error rate monitoring
- Usage analytics per provider

### 12. Compatibility & Migration

#### 12.1. Backward Compatibility
- SynClaude v1 configuration import
- Gradual migration path for existing users
- Legacy command aliases (`synclaude` → `aimux`)

#### 12.2. Package Distribution
- New npm package: `aimux`
- Maintain `synclaude` as legacy package
- Automated update notifications
- `ai-bridge` alias for users who prefer descriptive names

### 13. Future Enhancements

#### 13.1. Provider Extensibility
- Plugin system for new providers
- Community-contributed provider modules
- Dynamic provider discovery

#### 13.2. Advanced Routing
- Cost-based routing optimization
- Performance-based load balancing
- Intelligent provider selection based on request type

#### 13.3. Enterprise Features
- Team configuration management
- Usage quotas and billing integration
- Provider SLA monitoring

## Technical Challenges & Solutions

### Challenge 1: Cross-Provider Model Compatibility
**Solution**: Develop model capability mapping system that normalizes features across providers, with clear documentation of differences.

### Challenge 2: C++ Router Integration
**Solution**: Use Native Node.js Addons with clean async/await interface, implement proper memory management and error boundaries.

### Challenge 3: Provider API Differences
**Solution**: Provider abstraction layers with translation adapters, comprehensive testing matrix for all provider combinations.

### Challenge 4: Configuration Complexity
**Solution**: Smart configuration wizard with provider detection, sensible defaults, and progressive disclosure of advanced options.

## Success Metrics

- **User Adoption**: Migration rate from SynClaude v1 to aimux
- **Provider Coverage**: Number of supported providers
- **Reliability**: Uptime and fallback success rate
- **Performance**: Latency improvement over single-provider solutions
- **Developer Experience**: Time to add new providers

## Conclusion

This V2 plan transforms SynClaude into a comprehensive multi-provider AI bridge system, maintaining the excellent user experience while adding powerful provider management and routing capabilities. The modular architecture ensures extensibility and maintainability, while the C++ router provides the performance needed for production workloads.

The `aimux` command will become the go-to tool for developers working with Claude Code across multiple AI providers, with intelligent routing and seamless cross-provider capabilities.