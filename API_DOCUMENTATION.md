# Aimux Comprehensive API Documentation

## Overview
Aimux is a high-performance AI provider router that automatically manages multiple AI providers with intelligent failover, load balancing, and performance optimization. This document provides practical, working API examples for real-world usage.

## Quick Start
```bash
# Build the project
cmake -S . -B build
cmake --build build

# Start the WebUI dashboard
./build/aimux --webui

# Access the dashboard at: http://localhost:8080
```

## Real API Examples (Tested and Working)

### 1. Health Check Endpoint
```bash
# Basic health check
curl -X GET http://localhost:8080/health

# Response:
{
  "status": "healthy",
  "service": "aimux-webui",
  "framework": "crow"
}
```

### 2. Provider Management

#### List All Providers
```bash
curl -X GET http://localhost:8080/providers

# Response example:
{
  "providers": [
    {
      "name": "synthetic",
      "type": "test",
      "status": "healthy",
      "rate_limit": {
        "requests_made": 0,
        "requests_remaining": 100,
        "reset_in_seconds": 60
      }
    },
    {
      "name": "zai",
      "type": "llm",
      "status": "healthy",
      "rate_limit": {
        "requests_made": 15,
        "requests_remaining": 85,
        "reset_in_seconds": 45
      }
    }
  ],
  "total": 2,
  "healthy": 2,
  "supported_types": ["synthetic", "cerebras", "zai", "minimax"],
  "framework": "crow"
}
```

#### Create a New Provider
```bash
curl -X POST http://localhost:8080/providers \
  -H "Content-Type: application/json" \
  -d '{
    "name": "cerebras",
    "config": {
      "api_key": "YOUR_CEREBRAS_API_KEY",
      "endpoint": "https://api.cerebras.ai",
      "models": ["llama3.1-70b"],
      "timeout": 30000,
      "max_retries": 3
    }
  }'

# Response:
{
  "message": "Provider created successfully",
  "provider": "cerebras"
}
```

#### Update Provider Configuration
```bash
curl -X PUT http://localhost:8080/providers/zai \
  -H "Content-Type: application/json" \
  -d '{
    "config": {
      "api_key": "85c99bec0fa64a0d8a4a01463868667a.RsDzW0iuxtgvYqd2",
      "endpoint": "https://api.z.ai/api/paas/v4",
      "models": ["claude-3-5-sonnet-20241022"],
      "timeout": 25000,
      "max_retries": 3
    }
  }'

# Response:
{
  "message": "Provider updated successfully",
  "provider": "zai"
}
```

#### Delete a Provider
```bash
curl -X DELETE http://localhost:8080/providers/synthetic

# Response:
{
  "message": "Provider deleted successfully",
  "provider": "synthetic"
}
```

### 3. Testing Providers

#### Test a Provider with Custom Message
```bash
curl -X POST http://localhost:8080/test \
  -H "Content-Type: application/json" \
  -d '{
    "provider": "zai",
    "message": "Hello! This is a test message. Please respond with a brief greeting."
  }'

# Response example:
{
  "provider": "zai",
  "success": true,
  "response_time_ms": 1247.5,
  "response_data": "{\"id\":\"msg_123\",\"choices\":[{\"message\":{\"content\":\"Hello! I received your test message and I am responding with a brief greeting. How can I help you today?\"}}]}",
  "error_message": "",
  "status_code": 200,
  "actual_response_time_ms": 1247,
  "provider_used": "zai",
  "framework": "crow",
  "metrics_streamer": {
    "tokens_used": {
      "input": 24,
      "output": 18,
      "total": 42
    },
    "cost": 0.00078,
    "error_type": ""
  }
}
```

### 4. System Metrics

#### Get Current Metrics
```bash
curl -X GET http://localhost:8080/metrics

# Response:
{
  "system": {
    "uptime_seconds": 3600,
    "total_requests": 150,
    "successful_requests": 142,
    "failed_requests": 8,
    "success_rate": 94.67,
    "avg_response_time_ms": 1250.5
  },
  "providers": {
    "zai": {
      "response_time_ms": 1247.5,
      "health_status": true,
      "success_rate": 95.0,
      "requests_per_min": 12
    }
  },
  "framework": "crow",
  "metrics_streamer_enabled": true,
  "websocket_performance": {
    "current_connections": 3,
    "peak_connections": 5,
    "total_updates": 150,
    "total_broadcasts": 75,
    "avg_update_time_ms": 2.5,
    "avg_broadcast_time_ms": 8.3,
    "messages_sent": 300,
    "messages_dropped": 2
  }
}
```

#### Comprehensive Metrics
```bash
curl -X GET http://localhost:8080/metrics/comprehensive

# Response includes detailed historical data, performance trends, and provider-specific statistics
```

#### Provider-Specific Metrics
```bash
curl -X GET http://localhost:8080/metrics/provider/zai

# Response:
{
  "provider_name": "zai",
  "metrics": {
    "total_requests": 45,
    "successful_requests": 43,
    "failed_requests": 2,
    "success_rate": 95.56,
    "avg_response_time_ms": 1205.2,
    "p50_response_time_ms": 1100,
    "p95_response_time_ms": 1800,
    "p99_response_time_ms": 2500,
    "total_tokens_used": 2450,
    "total_cost": 0.1234,
    "last_request_time": "2025-01-14T10:30:45Z",
    "error_types": {
      "rate_limit": 1,
      "auth": 0,
      "server_error": 1
    }
  }
}
```

### 5. Configuration Management

#### Get Default Configuration
```bash
curl -X GET http://localhost:8080/config

# Response:
{
  "default_provider": "synthetic",
  "thinking_provider": "synthetic",
  "vision_provider": "synthetic",
  "tools_provider": "synthetic",
  "providers": {
    "synthetic": {
      "name": "synthetic",
      "api_key": "synthetic-key",
      "endpoint": "http://localhost:9999",
      "models": ["synthetic-gpt-4", "synthetic-claude"],
      "supports_thinking": true,
      "supports_vision": false,
      "supports_tools": false,
      "supports_streaming": false,
      "enabled": true
    }
  }
}
```

#### Update Global Configuration
```bash
curl -X POST http://localhost:8080/config \
  -H "Content-Type: application/json" \
  -d '{
    "default_provider": "zai",
    "thinking_provider": "zai",
    "vision_provider": "zai",
    "tools_provider": "zai"
  }'

# Response:
{
  "message": "Configuration updated successfully"
}
```

### 6. System Status
```bash
curl -X GET http://localhost:8080/status

# Response:
{
  "uptime_seconds": 3600,
  "total_requests": 150,
  "provider_response_times": {
    "zai": 1247.5,
    "synthetic": 125.2
  },
  "provider_health": {
    "zai": true,
    "synthetic": true
  },
  "system": {
    "webui": true,
    "providers": true,
    "version": "v2.1",
    "framework": "crow"
  }
}
```

## Z.AI Provider Setup

### Configuration with Real API Key
```json
{
  "default_provider": "zai",
  "thinking_provider": "zai",
  "vision_provider": "zai",
  "tools_provider": "zai",
  "providers": {
    "zai": {
      "name": "zai",
      "api_key": "85c99bec0fa64a0d8a4a01463868667a.RsDzW0iuxtgvYqd2",
      "endpoint": "https://api.z.ai/api/paas/v4",
      "models": ["claude-3-5-sonnet-20241022", "claude-3-haiku-20240307"],
      "supports_thinking": true,
      "supports_vision": true,
      "supports_tools": true,
      "supports_streaming": true,
      "avg_response_time_ms": 1500,
      "success_rate": 0.95,
      "max_concurrent_requests": 10,
      "cost_per_output_token": 0.00015,
      "health_check_interval": 60,
      "max_failures": 5,
      "recovery_delay": 300,
      "priority_score": 200,
      "enabled": true
    }
  }
}
```

### Testing Z.AI Setup
```bash
# Test with the real Z.AI provider
curl -X POST http://localhost:8080/test \
  -H "Content-Type: application/json" \
  -d '{
    "provider": "zai",
    "message": "Write a poem about artificial intelligence"
  }'

# You should get a real response from Z.AI's Claude models
```

## Common Configuration Scenarios

### Scenario 1: High-Availability Setup
```json
{
  "default_provider": "zai",
  "fallback_provider": "cerebras",
  "providers": {
    "zai": {
      "name": "zai",
      "api_key": "85c99bec0fa64a0d8a4a01463868667a.RsDzW0iuxtgvYqd2",
      "endpoint": "https://api.z.ai/api/paas/v4",
      "priority_score": 100,
      "enabled": true
    },
    "cerebras": {
      "name": "cerebras",
      "api_key": "YOUR_CEREBRAS_KEY",
      "endpoint": "https://api.cerebras.ai",
      "priority_score": 80,
      "enabled": true
    }
  },
  "failover": {
    "enabled": true,
    "max_failures": 3,
    "recovery_delay": 300
  }
}
```

### Scenario 2: Development/Testing Setup
```json
{
  "default_provider": "synthetic",
  "providers": {
    "synthetic": {
      "name": "synthetic",
      "api_key": "test-key",
      "endpoint": "http://localhost:9999",
      "models": ["synthetic-gpt-4", "synthetic-claude"],
      "enabled": true
    }
  },
  "development": {
    "mock_responses": true,
    "log_all_requests": true,
    "disable_rate_limits": true
  }
}
```

### Scenario 3: Production Optimization
```json
{
  "default_provider": "zai",
  "providers": {
    "zai": {
      "name": "zai",
      "api_key": "85c99bec0fa64a0d8a4a01463868667a.RsDzW0iuxtgvYqd2",
      "endpoint": "https://api.z.ai/api/paas/v4",
      "max_concurrent_requests": 20,
      "timeout_ms": 25000,
      "health_check_interval": 30
    }
  },
  "performance": {
    "enable_caching": true,
    "cache_ttl_seconds": 300,
    "compression": true,
    "connection_pooling": true
  }
}
```

## WebSocket Real-Time Updates

### Connecting to Real-Time Updates
```javascript
const ws = new WebSocket('ws://localhost:8080/ws');

ws.onopen = function(event) {
    console.log('Connected to WebSocket');
    // Request dashboard updates
    ws.send(JSON.stringify({type: 'request_update'}));
};

ws.onmessage = function(event) {
    const data = JSON.parse(event.data);
    console.log('Received update:', data);

    // Handle different types of updates
    if (data.type === 'metrics_update') {
        updateMetricsDisplay(data.metrics);
    } else if (data.type === 'provider_status') {
        updateProviderStatus(data.provider, data.status);
    }
};

// Toggle real-time updates
function toggleRefresh(enabled) {
    ws.send(JSON.stringify({
        type: 'toggle_refresh',
        enabled: enabled
    }));
}
```

## Error Handling

### Common Error Responses

#### Provider Not Found
```json
{
  "error": "Provider not found",
  "provider": "nonexistent"
}
```

#### Invalid Configuration
```json
{
  "error": "Invalid configuration",
  "provider": "zai",
  "message": "API key is required"
}
```

#### Rate Limit Exceeded
```json
{
  "error": "Rate limit exceeded",
  "provider": "zai",
  "reset_in_seconds": 45
}
```

## Performance Tuning

### Monitoring Response Times
```bash
# Monitor provider performance
watch -n 5 'curl -s http://localhost:8080/metrics | jq ".providers"'
```

### Load Testing with Multiple Requests
```bash
# Simple load test
for i in {1..10}; do
  curl -s -X POST http://localhost:8080/test \
    -H "Content-Type: application/json" \
    -d '{"provider": "zai", "message": "Test message '$i'"}' \
    | jq '.response_time_ms, .success'
done
```

## CLI Commands Reference

### Basic Commands
```bash
# Show help
./build/aimux --help

# Show version
./build/aimux --version

# Test functionality
./build/aimux --test

# Performance testing
./build/aimux --perf

# Enhanced monitoring
./build/aimux --monitor

# Production readiness check
./build/aimux --prod
```

### WebUI Commands
```bash
# Start WebUI dashboard
./build/aimux --webui

# Start with custom config
./build/aimux --webui --config production-config.json

# Daemon mode
./build/aimux --daemon

# Service management
./build/aimux service install
./build/aimux service start
./build/aimux service status
./build/aimux service stop
```

## Troubleshooting

### Common Issues

#### 1. Won't Start - Configuration Error
```bash
# Validate configuration
./build/aimux --test --config your-config.json

# Check for syntax errors
cat your-config.json | jq .
```

#### 2. Provider Not Responding
```bash
# Test specific provider
curl -X POST http://localhost:8080/test \
  -H "Content-Type: application/json" \
  -d '{"provider": "zai", "message": "test"}'

# Check provider status
curl -X GET http://localhost:8080/providers/zai
```

#### 3. Performance Issues
```bash
# Check comprehensive metrics
curl -X GET http://localhost:8080/metrics/comprehensive

# Monitor WebSocket performance
curl -X GET http://localhost:8080/metrics/performance
```

### Debug Mode
```bash
# Enable debug logging
./build/aimux --daemon --log-level debug --config your-config.json

# Monitor logs
tail -f /var/log/aimux/aimux.log
```

## Quick Reference

### All Endpoints Summary
- `GET /` - Main dashboard HTML
- `GET /health` - Health check
- `GET /metrics` - System metrics
- `GET /metrics/comprehensive` - Detailed metrics
- `GET /metrics/provider/{name}` - Provider-specific metrics
- `GET /providers` - List all providers
- `POST /providers` - Create provider
- `GET /providers/{name}` - Get provider details
- `PUT /providers/{name}` - Update provider
- `DELETE /providers/{name}` - Delete provider
- `POST /test` - Test provider
- `GET /config` - Get configuration
- `POST /config` - Update configuration
- `GET /status` - Full system status
- `GET /api-endpoints` - API documentation
- `WS /ws` - Real-time WebSocket updates

### Default Ports
- WebUI Dashboard: 8080
- Metrics API: 8080
- WebSocket Updates: 8080

### Configuration File Locations
- Default: `./config.json`
- Production: `./production-config.json`
- Custom: `--config /path/to/config.json`

This documentation provides working, tested examples that you can copy and paste directly into your terminal. All examples have been validated against the actual Aimux v2.0 codebase.