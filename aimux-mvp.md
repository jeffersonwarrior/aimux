# Aimux MVP Development Plan

## Executive Summary

**Current State**: Aimux is Synclaude v1.4.1 with updated documentation describing features that don't exist yet. We have a solid single-provider foundation but need to build the actual multi-provider architecture.

**MVP Goal**: Transform aimux into a working multi-provider CLI tool with intelligent routing, failover, and provider management capabilities.

## Current Technical Foundation

### What We Have ✅
- Complete Synclaude v1.4.1 codebase (21 TypeScript files)
- Mature single-provider (Synthetic.New) implementation
- React-based terminal UI with Ink
- Robust configuration management with Zod validation
- Claude Code launcher integration
- Professional CLI interface with Commander.js
- TypeScript build system with strict mode

### What We Need to Build 🚧
- Multi-provider architecture (providers don't exist yet)
- C++ HTTP router for request interception and routing
- Provider abstraction layer
- Intelligent failover and routing logic
- Cross-provider model capabilities mapping

## MVP Architecture Overview

```
aimux/
├── src/
│   ├── providers/           # NEW - Provider management system
│   ├── router/             # NEW - C++ HTTP router integration
│   ├── core/               # MODIFIED - Multi-provider orchestration
│   ├── config/             # MODIFIED - Multi-provider configs
│   ├── cli/                # MODIFIED - Provider commands
│   ├── models/             # MODIFIED - Cross-provider models
│   └── ui/                 # MODIFIED - Provider selection UI
├── native/                  # NEW - C++ router source
├── tests/                   # NEW - Test infrastructure
└── scripts/                 # NEW - Installation tools
```

## Phase 1: Core Infrastructure (Week 1)

### 1.1 Fix Identity Crisis
```bash
# Package.json updates
- name: "synclaude" → "aimux"
- bin: {"synclaude": "..."} → {"aimux": "..."}
- version: bump to 2.0.0-rc.1
- description: update for multi-provider focus
- repository: update URLs
- keywords: add "multi-provider", "router", "failover"
```

### 1.2 Provider Architecture Foundation
```typescript
// NEW: src/providers/base-provider.ts
abstract class BaseProvider {
  abstract name: string;
  abstract capabilities: ProviderCapabilities;
  abstract authenticate(): Promise<boolean>;
  abstract fetchModels(): Promise<ModelInfo[]>;
  abstract validateApiKey(apiKey: string): Promise<boolean>;
  abstract makeRequest(request: AIRequest): Promise<AIResponse>;
  abstract healthCheck(): Promise<ProviderHealth>;
}

// NEW: src/providers/provider-registry.ts
class ProviderRegistry {
  private providers: Map<string, BaseProvider>;
  register(provider: BaseProvider): void;
  getProvider(name: string): BaseProvider | null;
  getProvidersByCapability(capability: string): BaseProvider[];
}
```

### 1.3 C++ Router Integration (Simplified MVP)
```cpp
// NEW: native/router.cpp - Simple HTTP proxy
- HTTP request interception on localhost:8080
- Provider selection based on request analysis
- Basic load balancing between available providers
- Request forwarding and response normalization
```

### 1.4 Test Infrastructure Migration
```bash
# Port from synclaude
cp ../synclaude/tests/ ./tests/
# Update test references from synclaude → aimux
# Add multi-provider test scenarios
```

## Phase 2: Provider Implementation (Week 2)

### 2.1 Initial Provider Support
```typescript
// NEW: src/providers/minimax-provider.ts
class MinimaxM2Provider extends BaseProvider {
  name = "minimax-m2";
  capabilities = {
    thinking: true,
    vision: true,
    tools: true,
    maxTokens: 200000
  };
}

// NEW: src/providers/zai-provider.ts
class ZAIProvider extends BaseProvider {
  name = "z-ai";
  capabilities = {
    thinking: false,
    vision: true,
    tools: true,
    maxTokens: 100000
  };
}

// MODIFIED: src/providers/synthetic-provider.ts
class SyntheticProvider extends BaseProvider {
  name = "synthetic-new";
  capabilities = {
    thinking: true,
    vision: true,
    tools: true,
    maxTokens: 200000
  };
}
```

### 2.2 Enhanced Configuration System
```typescript
// MODIFIED: src/config/types.ts
const MultiProviderConfigSchema = z.object({
  providers: z.object({
    "minimax-m2": ProviderConfigSchema.optional(),
    "z-ai": ProviderConfigSchema.optional(),
    "synthetic-new": ProviderConfigSchema.optional(),
  }),
  routing: RoutingConfigSchema,
  fallback: FallbackConfigSchema,
});

const RoutingConfigSchema = z.object({
  defaultProvider: z.string(),
  thinkingProvider: z.string().optional(),
  visionProvider: z.string().optional(),
  toolsProvider: z.string().optional(),
});
```

### 2.3 Basic Routing Logic
```typescript
// NEW: src/core/routing-engine.ts
class RoutingEngine {
  selectProvider(request: AIRequest): BaseProvider {
    // Simple capability-based routing
    if (request.requiresThinking) {
      return this.getThinkingProvider();
    }
    if (request.requiresVision) {
      return this.getVisionProvider();
    }
    return this.getDefaultProvider();
  }

  async handleFailover(request: AIRequest): Promise<AIResponse> {
    // Try backup providers if primary fails
  }
}
```

## Phase 3: User Interface & CLI (Week 3)

### 3.1 Enhanced CLI Commands
```bash
# NEW CLI Structure
aimux                           # Launch with default provider
aimux <provider>                 # Launch specific provider
aimux providers setup           # Setup multiple providers
aimux providers list            # Show provider status
aimux providers test           # Test provider connectivity
aimux hybrid --thinking <p> --default <p>  # Hybrid mode
aimux router                     # Router management
aimux config                     # Multi-provider config
aimux doctor                    # System health check
```

### 3.2 Provider Setup UI
```typescript
// MODIFIED: src/ui/provider-setup.ts
async function providerSetupFlow() {
  // Welcome and provider selection
  // API key collection for each provider
  // Provider testing and validation
  // Routing preferences setup
  // Save configuration
}
```

### 3.3 Interactive Provider Status
```typescript
// NEW: src/ui/provider-status.ts
function ProviderStatus() {
  // Real-time provider health
  // Connection status indicators
  // API key validation status
  // Performance metrics
}
```

## Phase 4: Advanced Features (Week 4)

### 4.1 Intelligent Failover
```typescript
// ENHANCED: src/core/failover-manager.ts
class FailoverManager {
  private healthMonitor: ProviderHealthMonitor;

  async handleRequest(request: AIRequest): Promise<AIResponse> {
    const primary = this.routingEngine.selectProvider(request);

    try {
      const response = await primary.makeRequest(request);
      this.healthMonitor.recordSuccess(primary.name);
      return response;
    } catch (error) {
      this.healthMonitor.recordFailure(primary.name, error);
      return this.tryFallbackProviders(request, primary.name);
    }
  }
}
```

### 4.2 Performance Monitoring
```typescript
// NEW: src/core/performance-tracker.ts
class PerformanceTracker {
  trackLatency(provider: string, latency: number): void;
  trackErrorRate(provider: string, success: boolean): void;
  getProviderStats(): ProviderStats[];
  recommendOptimalProvider(): string;
}
```

### 4.3 Cost Optimization (Simple MVP)
```typescript
// NEW: src/core/cost-optimizer.ts
class CostOptimizer {
  selectCheapestProvider(request: AIRequest): BaseProvider {
    // Simple price comparison between available providers
    // Consider token costs for each provider
    // Factor in provider capabilities
  }
}
```

## Technical Implementation Details

### C++ Router MVP Implementation
```cpp
// Simplified router.cpp for MVP
class SimpleRouter {
public:
    void startListening(int port = 8080);
    void handleRequest(http::request<http::string_body>& request);
private:
    ProviderSelector providerSelector;
    HttpClient httpClient;
    ResponseNormalizer responseNormalizer;
};
```

### TypeScript Bindings
```typescript
// NEW: src/router/router-binding.ts
import { createRouter, startRouter, stopRouter } from '../native/router.node';

export class RouterManager {
  async start(port: number): Promise<void>;
  async stop(): Promise<void>;
  addProvider(name: string, config: ProviderConfig): void;
  setRoutingRules(rules: RoutingRule[]): void;
}
```

## Testing Strategy

### Unit Tests
```bash
# Provider tests
npm test -- provider-manager.test.ts
npm test -- minimax-provider.test.ts
npm test -- routing-engine.test.ts

# C++ router tests
npm run test:native

# Integration tests
npm test -- multi-provider-integration.test.ts
npm test -- failover-behavior.test.ts
```

### Manual Testing Checklist
- [ ] Provider setup flow for all 3 providers
- [ ] Hybrid mode routing (thinking to minimax, regular to synthetic)
- [ ] Failover during provider outage
- [ ] Cost-based provider selection
- [ ] CLI command compatibility
- [ ] Configuration migration from synclaude

## Migration Path from Synclaude

### Data Migration
```typescript
// Migration utility
async function migrateFromSynclaude() {
  const synclaudeConfig = readSynclaudeConfig();
  const aimuxConfig = {
    providers: {
      "synthetic-new": synclaudeConfig
    },
    routing: {
      defaultProvider: "synthetic-new"
    }
  };
  saveAimuxConfig(aimuxConfig);
}
```

### Backward Compatibility
- Support `synclaude` command as alias for `aimux`
- Import existing configuration automatically
- Preserve user settings and API keys

## Deliverables

### MVP Release Checklist
- [ ] Package.json updated to "aimux" v2.0.0-rc.1
- [ ] Provider abstraction layer implemented
- [ ] C++ router with basic HTTP proxy working
- [ ] 3 providers implemented (Minimax M2, Z.AI, Synthetic)
- [ ] CLI commands for provider management
- [ ] Basic failover mechanisms
- [ ] Test suite with >80% coverage
- [ ] Installation scripts updated for aimux
- [ ] Documentation updated with actual features
- [ ] Migration wizard from synclaude

### Release Artifacts
- `aimux-2.0.0-rc.1.tgz` - npm package
- Install scripts for all platforms
- Updated README with real feature list
- Migration guide for synclaude users

## Development Environment

### Prerequisites
```bash
# Native dependencies
sudo apt-get install build-essential cmake
# or
brew install cmake

# Node.js and TypeScript
node >= 18.0.0
npm >= 8.0.0
typescript >= 5.2.0
```

### Development Commands
```bash
# Build C++ router
npm run build:router

# Build TypeScript
npm run build

# Development with hot reload for C++
npm run dev:router  # Compiles router with file watching
npm run dev         # Runs TypeScript in dev mode

# Testing
npm test                    # All tests
npm run test:watch         # Watch mode
npm run test:coverage      # With coverage
npm run test:integration   # Integration tests only
```

## Success Metrics for MVP

### Technical Metrics
- **Provider Success Rate**: >95% for all 3 providers
- **Failover Time**: <2 seconds to switch providers
- **Router Latency**: <100ms overhead per request
- **Test Coverage**: >80% across all modules

### User Experience Metrics
- **Setup Time**: <5 minutes to configure first provider
- **CLI Compatibility**: 100% backward compatibility with synclaude commands
- **Migration Success**: >90% successful migrations from synclaude
- **Error Recovery**: Graceful handling of provider outages

## Next Steps After MVP

### V2 Features (Future Releases)
- Team management and shared configurations
- Advanced AI-based routing optimization
- Real-time performance analytics dashboard
- Plugin system for custom providers
- Global provider endpoint optimization
- Usage quotas and cost tracking
- Advanced security features (API key rotation, audit logs)

## Risk Assessment & Mitigation

### Technical Risks
- **C++ Router Complexity**: Mitigate with simplified HTTP proxy for MVP
- **Provider API Changes**: Implement flexible request/response mapping
- **Performance Bottlenecks**: Add caching and connection pooling

### Timeline Risks
- **Native Compilation Issues**: Focus on Node.js proxy fallback if C++ proves difficult
- **Provider Integration Delays**: Prioritize 2 providers for MVP, add third later
- **Testing Complexity**: Start with core functionality testing first

## Conclusion

The aimux MVP is achievable in 4 weeks by building incrementally on the solid synclaude foundation. The key is starting with infrastructure fixes, then implementing core multi-provider capabilities, followed by user interface polish and advanced features.

The existing synclaude codebase provides excellent architecture patterns, testing strategies, and user experience principles that we can leverage to accelerate development.

**Next Action**: Begin Phase 1 by fixing the identity crisis in package.json and implementing the base provider architecture.