# Provider API Analysis & Interface Specification

## Overview

This document analyzes the three AI providers that need to be implemented in Aimux2, detailing their API endpoints, authentication methods, capabilities, and integration requirements.

## Provider API Specifications

### 1. Cerebras AI Provider

#### API Details
- **Base URL**: `https://api.cerebras.ai/v1`
- **Authentication**: Bearer token in Authorization header
- **Rate Limits**: TBD (documentation required)
- **Special Capabilities**: Thinking/reasoning models

#### Authentication
```http
Authorization: Bearer <cerebras-api-key>
Content-Type: application/json
```

#### Key Endpoints
1. **Models**: `GET /v1/models` - List available models
2. **Chat Completions**: `POST /v1/chat/completions` - Generate completions

#### Request Format
```json
{
  "model": "llama3.1-70b",
  "messages": [
    {"role": "system", "content": "You are a helpful assistant."},
    {"role": "user", "content": "Explain quantum computing."}
  ],
  "max_tokens": 1000,
  "temperature": 0.7,
  "stream": false
}
```

#### Response Format
```json
{
  "id": "chatcmpl-abc123",
  "object": "chat.completion",
  "created": 1699012345,
  "model": "llama3.1-70b",
  "choices": [{
    "index": 0,
    "message": {
      "role": "assistant",
      "content": "Quantum computing harnesses quantum mechanical phenomena..."
    },
    "finish_reason": "stop"
  }],
  "usage": {
    "prompt_tokens": 20,
    "completion_tokens": 45,
    "total_tokens": 65
  }
}
```

#### Capabilities
- ✅ **Thinking Models**: Advanced reasoning, step-by-step analysis
- ❌ **Vision**: No image input support
- ✅ **Tools**: Function calling support (verify)
- ✅ **Large Context**: Extended context windows available

#### Integration Notes
- Must handle token counting for cost tracking
- Implement proper error handling for rate limits
- Support streaming responses for better UX
- Model selection based on thinking vs standard requirements

---

### 2. Z.AI Provider

#### API Details
- **Base URL**: `https://api.z.ai/api/anthropic/v1`
- **Authentication**: Bearer token in Authorization header
- **Rate Limits**: 100 requests/minute (estimated)
- **Special Capabilities**: Vision + Tools

#### Authentication
```http
Authorization: Bearer <zai-api-key>
Content-Type: application/json
```

#### Key Endpoints
1. **Models**: `GET /api/anthropic/v1/models` - List available models
2. **Messages**: `POST /api/anthropic/v1/messages` - Generate responses

#### Request Format
```json
{
  "model": "claude-3-5-sonnet-20241022",
  "max_tokens": 1000,
  "messages": [
    {"role": "user", "content": "Explain this image"}
  ],
  "stream": false
}
```

#### Vision Request Format
```json
{
  "model": "claude-3-5-sonnet-20241022",
  "max_tokens": 1000,
  "messages": [
    {
      "role": "user",
      "content": [
        {
          "type": "text",
          "text": "What's in this image?"
        },
        {
          "type": "image",
          "source": {
            "type": "base64",
            "media_type": "image/jpeg",
            "data": "/9j/4AAQSkZJRgABA..."
          }
        }
      ]
    }
  ]
}
```

#### Response Format
```json
{
  "id": "msg_abc123",
  "type": "message",
  "role": "assistant",
  "content": [
    {
      "type": "text",
      "text": "This image appears to show..."
    }
  ],
  "model": "claude-3-5-sonnet-20241022",
  "stop_reason": "end_turn",
  "stop_sequence": null,
  "usage": {
    "input_tokens": 25,
    "output_tokens": 42
  }
}
```

#### Capabilities
- ❌ **Thinking**: Models focused on standard responses
- ✅ **Vision**: Full image analysis with multiple formats
- ✅ **Tools**: Function calling and tool use
- ✅ **Large Context**: 100K+ token context windows

#### Integration Notes
- Implement Base64 image encoding for vision requests
- Handle different media types (jpeg, png, webp, gif)
- Support multiple images in single request
- Tool calling requires proper function serialization

---

### 3. MiniMax M2 Provider

#### API Details
- **Base URL**: `https://api.minimax.io/anthropic`
- **Authentication**: Custom headers (Authorization + X-GroupId)
- **Rate Limits**: 60 requests/minute
- **Special Capabilities**: M2 thinking model

#### Authentication
```http
Authorization: Bearer <minimax-api-key>
X-GroupId: <group-id>
Content-Type: application/json
```

#### Key Endpoints
1. **Models**: `GET /anthropic/v1/models` - List available models
2. **Messages**: `POST /anthropic/v1/messages` - Generate responses

#### Request Format
```json
{
  "model": "minimax-m2-100k",
  "max_tokens": 8192,
  "messages": [
    {"role": "user", "content": "Solve this step by step: 23 * 47"}
  ],
  "stream": false
}
```

#### Response Format
```json
{
  "id": "msg_abc123",
  "type": "message",
  "role": "assistant",
  "content": [
    {
      "type": "text",
      "text": "Let me solve this step by step:\n\n1. Break down 23 * 47\n2. ... = 1081"
    }
  ],
  "model": "minimax-m2-100k",
  "stop_reason": "max_tokens",
  "usage": {
    "input_tokens": 15,
    "output_tokens": 67
  }
}
```

#### Capabilities
- ✅ **Thinking**: M2 model specialized in reasoning
- ❌ **Vision**: No image input support
- ✅ **Tools**: Basic function calling support
- ✅ **Large Context**: Up to 100K tokens

#### Integration Notes
- Must include X-GroupId header in all requests
- Implement proper rate limiting (60 req/min)
- M2 model requires specific reasoning prompt patterns
- Higher token limits than standard providers

---

## Provider Implementation Interface

### BaseProvider Class (Already Defined)

```cpp
class BaseProvider : public core::Bridge {
protected:
    std::string provider_name_;
    nlohmann::json config_;
    bool is_healthy_ = true;
    std::string encrypted_api_key_;
    std::string api_key_hash_;
    std::string endpoint_;

    // Rate limiting
    int requests_made_ = 0;
    int max_requests_per_minute_ = 60;
    std::chrono::steady_clock::time_point rate_limit_reset_;
};
```

### Implementation Requirements

#### For Each Provider (Cerebras, Z.AI, MiniMax):

1. **Constructor**: Parse configuration, set up API key encryption
2. **send_request()**: Format request, call API, parse response
3. **is_healthy()**: Implement health check logic
4. **get_provider_name()**: Return provider identifier
5. **get_rate_limit_status()**: Return rate limiting information

#### Core Functionality

**Request Formatting**:
- Transform universal Request format to provider-specific
- Handle authentication headers
- Apply rate limiting checks
- Format special capabilities (vision, tools)

**Response Parsing**:
- Normalize provider responses to standard format
- Extract usage statistics (tokens, costs)
- Handle error responses appropriately
- Extract thinking/reasoning steps where applicable

**Error Handling**:
- HTTP status codes (400, 401, 429, 500+)
- Network timeouts and connection failures
- API-specific error formats
- Rate limit responses with retry logic

---

## Configuration Requirements

### Provider Configuration Schema

```json
{
  "providers": {
    "cerebras": {
      "type": "cerebras",
      "name": "Cerebras AI",
      "api_key": "encrypted:abcdef123456...",
      "endpoint": "https://api.cerebras.ai/v1",
      "enabled": true,
      "rate_limit_rpm": 100,
      "models": ["llama3.1-70b", "llama3.1-8b"],
      "capabilities": {
        "thinking": true,
        "vision": false,
        "tools": true
      }
    },
    "zai": {
      "type": "zai",
      "name": "Z.AI",
      "api_key": "encrypted:xyz789...",
      "endpoint": "https://api.z.ai/api/anthropic/v1",
      "enabled": true,
      "rate_limit_rpm": 100,
      "models": ["claude-3-5-sonnet-20241022"],
      "capabilities": {
        "thinking": false,
        "vision": true,
        "tools": true
      }
    },
    "minimax": {
      "type": "minimax",
      "name": "MiniMax M2",
      "api_key": "encrypted:uvw456...",
      "endpoint": "https://api.minimax.io/anthropic",
      "enabled": true,
      "rate_limit_rpm": 60,
      "group_id": "group-123",
      "models": ["minimax-m2-100k"],
      "capabilities": {
        "thinking": true,
        "vision": false,
        "tools": true
      }
    }
  }
}
```

---

## Implementation Priority

### Critical Path Dependencies

1. **CerebrasProvider** - Thinking model support, blocks thinking requests
2. **ZAIProvider** - Vision + Tools, blocks image and function calling
3. **MiniMaxProvider** - Alternative thinking model, provides redundancy

### Testing Strategy

#### Unit Tests (per provider)
- API request formatting
- Response parsing accuracy
- Error handling coverage
- Rate limiting behavior

#### Integration Tests
- Real API calls with test credentials
- End-to-end request flow
- Failover scenarios
- Performance benchmarking

#### Mock Testing
- Offline development capabilities
- Error simulation
- Performance testing without API costs

---

## Security Considerations

### API Key Protection
- Encryption at rest (AES-256-GCM)
- Secure key derivation (PBKDF2)
- Hash-based key validation
- Environment variable support

### Request Security
- TLS 1.3 for all external communications
- Certificate pinning for生产环境
- Request validation and sanitization
- Rate limiting per API key

### Data Protection
- No sensitive data in logs
- Configurable data retention
- Secure cleanup on shutdown
- Audit logging for compliance

---

## Performance Requirements

### Targets
- **Response Time**: <50ms p95 (not including provider latency)
- **Throughput**: 34+ requests/second
- **Memory**: <20MB total for providers
- **CPU**: <10% overhead at target throughput

### Optimization Strategies
- Connection pooling and reuse
- Async request processing
- Response caching where appropriate
- Efficient JSON parsing and serialization

---

This analysis provides the foundation for implementing all three providers in Aimux2 with proper API integration, security, performance optimization, and error handling.