# Z.AI Provider Setup Guide

## Overview
Z.AI is a premium AI provider offering Claude models with excellent performance and reliability. This guide walks you through setting up Z.AI with Aimux for the best possible AI routing experience.

## Prerequisites
- Aimux v2.0 installed and running
- Z.AI API key (provided below)
- Basic understanding of JSON configuration

## Z.AI API Credentials

**API Key:** `85c99bec0fa64a0d8a4a01463868667a.RsDzW0iuxtgvYqd2`
**Endpoint:** `https://api.z.ai/api/paas/v4`
**Models Available:**
- `claude-3-5-sonnet-20241022` (Primary model)
- `claude-3-haiku-20240307` (Fast/Small model)

## Quick Setup

### 1. Update Your Configuration

Edit your `config.json` file to include Z.AI:

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

### 2. Test the Setup

Start Aimux and test Z.AI:

```bash
# Start the service with your configuration
./build/aimux --webui --config config.json

# In another terminal, test Z.AI
curl -X POST http://localhost:8080/test \
  -H "Content-Type: application/json" \
  -d '{
    "provider": "zai",
    "message": "Hello! Can you introduce yourself?"
  }'

# Expected response:
{
  "provider": "zai",
  "success": true,
  "response_time_ms": 1247.5,
  "response_data": "Hello! I\\'m Claude, an AI assistant...",
  "status_code": 200,
  "provider_used": "zai"
}
```

## Advanced Configuration

### High-Performance Setup

```json
{
  "default_provider": "zai",
  "providers": {
    "zai": {
      "name": "zai",
      "api_key": "85c99bec0fa64a0d8a4a01463868667a.RsDzW0iuxtgvYqd2",
      "endpoint": "https://api.z.ai/api/paas/v4",
      "models": ["claude-3-5-sonnet-20241022"],
      "max_concurrent_requests": 20,
      "timeout_ms": 25000,
      "max_retries": 3,
      "retry_delay_ms": 1000,
      "health_check_interval": 30,
      "priority_score": 100,
      "enabled": true
    }
  },
  "load_balancing": {
    "strategy": "adaptive",
    "health_check_interval": 30,
    "failover_enabled": true
  }
}
```

### Failover Configuration (Z.AI + Cerebras)

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
    "recovery_delay": 300,
    "auto_recovery": true
  }
}
```

## Testing Z.AI Capabilities

### 1. Basic Chat Test
```bash
curl -X POST http://localhost:8080/test \
  -H "Content-Type: application/json" \
  -d '{
    "provider": "zai",
    "message": "Explain quantum computing in simple terms"
  }'
```

### 2. Vision Capabilities Test
```bash
curl -X POST http://localhost:8080/test \
  -H "Content-Type: application/json" \
  -d '{
    "provider": "zai",
    "message": "What can you see in this image?",
    "image_base64": "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z/C/HgAGgwJ/lK3Q6wAAAABJRU5ErkJggg=="
  }'
```

### 3. Tool/Capabilities Test
```bash
curl -X POST http://localhost:8080/test \
  -H "Content-Type: application/json" \
  -d '{
    "provider": "zai",
    "message": "Can you help me with web searches and calculations?"
  }'
```

### 4. Streaming Response Test (if supported)
```bash
curl -X POST http://localhost:8080/test \
  -H "Content-Type: application/json" \
  -d '{
    "provider": "zai",
    "message": "Write a story about AI",
    "stream": true
  }'
```

## Monitoring Z.AI Performance

### Real-Time Monitoring via WebUI

1. Access the dashboard: http://localhost:8080
2. Go to the "Providers" section
3. Look for Z.AI status and metrics

### API Monitoring

```bash
# Check Z.AI provider status
curl -X GET http://localhost:8080/providers/zai

# Get Z.AI specific metrics
curl -X GET http://localhost:8080/metrics/provider/zai

# Monitor rate limits
curl -X GET http://localhost:8080/providers | jq '.providers[] | select(.name=="zai") | .rate_limit'
```

### Expected Z.AI Metrics

```json
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
    "p99_response_time_ms": 2500
  }
}
```

## Troubleshooting Z.AI

### Common Issues and Solutions

#### 1. Authentication Errors
```bash
# Error: "API key invalid"
# Solution: Verify the API key exactly as provided
echo "85c99bec0fa64a0d8a4a01463868667a.RsDzW0iuxtgvYqd2" | wc -c
# Should output: 55 characters
```

#### 2. Rate Limit Issues
```bash
# Check current rate limit status
curl -X GET http://localhost:8080/providers | jq '.providers[] | select(.name=="zai") | .rate_limit'

# If approaching limits, reduce concurrent requests
curl -X PUT http://localhost:8080/providers/zai \
  -H "Content-Type: application/json" \
  -d '{
    "config": {
      "api_key": "85c99bec0fa64a0d8a4a01463868667a.RsDzW0iuxtgvYqd2",
      "max_concurrent_requests": 5
    }
  }'
```

#### 3. Slow Response Times
```bash
# Check current metrics
curl -X GET http://localhost:8080/metrics/provider/zai

# If response time is high (>2000ms), consider:
# 1. Reducing max concurrent requests
# 2. Increasing timeout settings
# 3. Adding fallback provider
```

#### 4. Connection Errors
```bash
# Test connectivity to Z.AI endpoint
curl -I https://api.z.ai/api/paas/v4

# Should return HTTP 200 or 404 (endpoint exists)
# If no response, check network connectivity
```

### Debug Mode

```bash
# Start with debug logging for detailed troubleshooting
./build/aimux --daemon --log-level debug --config config.json

# Monitor logs for Z.AI specific messages
tail -f /var/log/aimux/aimux.log | grep -i zai
```

## Performance Optimization

### 1. Optimize Concurrent Requests
```json
{
  "zai": {
    "max_concurrent_requests": 15,
    "timeout_ms": 20000,
    "retry_attempts": 2
  }
}
```

### 2. Enable Caching
```json
{
  "performance": {
    "enable_caching": true,
    "cache_ttl_seconds": 300,
    "cache_responses_for_same_prompts": true
  }
}
```

### 3. Load Balancing
```json
{
  "load_balancing": {
    "strategy": "weighted_round_robin",
    "weights": {
      "zai": 0.8,
      "cerebras": 0.2
    }
  }
}
```

## Security Considerations

### 1. API Key Protection
```bash
# Set proper file permissions
chmod 600 config.json

# Consider using environment variables for production
export ZAI_API_KEY="85c99bec0fa64a0d8a4a01463868667a.RsDzW0iuxtgvYqd2"
```

### 2. Network Security
```json
{
  "security": {
    "ssl_verification": true,
    "allowed_origins": ["https://your-domain.com"],
    "rate_limiting": {
      "enabled": true,
      "requests_per_minute": 100
    }
  }
}
```

## Production Deployment

### Production Configuration
```json
{
  "system": {
    "environment": "production",
    "log_level": "warn"
  },
  "providers": {
    "zai": {
      "name": "zai",
      "api_key": "85c99bec0fa64a0d8a4a01463868667a.RsDzW0iuxtgvYqd2",
      "max_concurrent_requests": 20,
      "health_check_interval": 30,
      "priority_score": 100
    }
  },
  "monitoring": {
    "enable_alerts": true,
    "alert_thresholds": {
      "max_response_time_ms": 5000,
      "min_success_rate": 0.95
    }
  }
}
```

### Health Checks
```bash
# Set up automated health monitoring
*/5 * * * * curl -f http://localhost:8080/health || alert-admin.sh
*/10 * * * * curl -f http://localhost:8080/providers/zai || alert-admin.sh
```

## Usage Examples

### 1. General Purpose Chat
```bash
curl -X POST http://localhost:8080/test \
  -H "Content-Type: application/json" \
  -d '{
    "provider": "zai",
    "message": "Help me write a professional email to my team about the project deadline"
  }'
```

### 2. Code Generation
```bash
curl -X POST http://localhost:8080/test \
  -H "Content-Type: application/json" \
  -d '{
    "provider": "zai",
    "message": "Write a Python function that connects to a PostgreSQL database and fetches user data"
  }'
```

### 3. Data Analysis
```bash
curl -X POST http://localhost:8080/test \
  -H "Content-Type: application/json" \
  -d '{
    "provider": "zai",
    "message": "I have sales data for the past 12 months. Can you help me identify trends and suggest improvements?",
    "context": "You are an experienced data analyst"
  }'
```

## Best Practices

1. **Always include the full API key** exactly as provided
2. **Monitor rate limits** to avoid throttling
3. **Set appropriate timeouts** for your use case
4. **Enable failover** for production systems
5. **Regularly update configuration** based on performance metrics
6. **Use structured logging** for easier troubleshooting
7. **Test all models** to find the best fit for your needs

## Support and Monitoring

### Dashboard Access
- URL: http://localhost:8080
- Real-time metrics and provider status
- WebSocket updates for live monitoring

### API Documentation
- Full API reference: `/api-endpoints`
- Interactive testing via WebUI
- Comprehensive metrics: `/metrics/comprehensive`

### Log Locations
- Application logs: `/var/log/aimux/aimux.log`
- Provider metrics: `/var/log/aimux/metrics.log`
- Error logs: `/var/log/aimux/errors.log`

This setup guide provides everything you need to successfully integrate Z.AI with Aimux for optimal AI routing performance.