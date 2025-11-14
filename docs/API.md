# Aimux 2.0 - API Documentation

## 📖 Overview

Aimux 2.0 provides a comprehensive REST API for AI service routing, load balancing, and monitoring.

## 🔗 Base URL

```
Development: http://localhost:8080
Production:  https://your-domain.com
```

## 🔐 Authentication

All API endpoints require API key authentication:

```http
Authorization: Bearer YOUR_API_KEY
Content-Type: application/json
```

## 📊 API Endpoints

### Health & Status

#### GET `/health`
Check system health status.

**Response:**
```json
{
  "status": "healthy",
  "timestamp": "2025-11-13T02:55:15.624Z",
  "uptime": 3600,
  "version": "v2.0 - Nov 12, 2025",
  "providers": {
    "cerebras": "healthy",
    "zai": "healthy",
    "synthetic": "healthy"
  }
}
```

#### GET `/status`
Get full system status including performance metrics.

**Response:**
```json
{
  "system": {
    "status": "operational",
    "uptime": 3600,
    "requests_total": 1500,
    "requests_per_second": 15.5
  },
  "providers": [
    {
      "name": "cerebras",
      "status": "healthy",
      "requests": 750,
      "success_rate": 0.98,
      "avg_response_time": 250.5,
      "last_request": "2025-11-13T02:55:15.624Z"
    }
  ],
  "load_balancer": {
    "strategy": "adaptive",
    "round_robin_index": 5,
    "active_providers": 3
  }
}
```

### Metrics & Monitoring

#### GET `/metrics`
Get comprehensive performance metrics.

**Response:**
```json
{
  "timestamp": "2025-11-13T02:55:15.624Z",
  "performance": {
    "total_requests": 1500,
    "success_count": 1470,
    "success_rate": 0.98,
    "avg_response_time_ms": 245.8,
    "p50_response_time_ms": 235.0,
    "p95_response_time_ms": 450.0,
    "p99_response_time_ms": 680.0
  },
  "providers": {
    "cerebras": {
      "requests": 750,
      "success_rate": 0.98,
      "avg_response_time_ms": 240.5,
      "current_connections": 12
    }
  },
  "alerts": [
    {
      "type": "performance",
      "severity": "warning",
      "message": "High average response time",
      "threshold": 200.0,
      "current_value": 245.8,
      "timestamp": "2025-11-13T02:55:15.624Z"
    }
  ]
}
```

#### GET `/providers`
Get detailed provider status and metrics.

**Response:**
```json
{
  "providers": [
    {
      "name": "cerebras",
      "status": "healthy",
      "endpoint": "https://api.cerebras.ai",
      "enabled": true,
      "metrics": {
        "requests_made": 750,
        "max_requests_per_minute": 300,
        "requests_remaining": 250,
        "reset_in_seconds": 45,
        "avg_response_time_ms": 240.5,
        "current_connections": 12,
        "total_requests": 750
      }
    }
  ]
}
```

### Load Balancing

#### GET `/load-balancer`
Get load balancer configuration and statistics.

**Response:**
```json
{
  "strategy": "adaptive",
  "health_check_interval": 30,
  "failover_enabled": true,
  "retry_on_failure": true,
  "statistics": {
    "providers": [
      {
        "name": "cerebras",
        "avg_response_time_ms": 240.5,
        "current_connections": 12,
        "total_requests": 750
      }
    ],
    "round_robin_index": 5
  },
  "circuit_breaker": {
    "enabled": true,
    "failure_threshold": 5,
    "recovery_timeout": 60
  }
}
```

#### PUT `/load-balancer/strategy`
Update load balancing strategy.

**Request:**
```json
{
  "strategy": "weighted_round_robin"
}
```

**Response:**
```json
{
  "message": "Load balancing strategy updated successfully",
  "previous_strategy": "adaptive",
  "new_strategy": "weighted_round_robin",
  "timestamp": "2025-11-13T02:55:15.624Z"
}
```

### Configuration

#### GET `/config`
Get current system configuration.

**Response:**
```json
{
  "system": {
    "environment": "production",
    "log_level": "warn",
    "structured_logging": true,
    "max_concurrent_requests": 1000
  },
  "security": {
    "api_key_encryption": true,
    "rate_limiting": true,
    "ssl_verification": true,
    "input_validation": true
  },
  "monitoring": {
    "metrics_collection": true,
    "health_checks": true,
    "performance_alerts": true,
    "dashboard_enabled": true
  }
}
```

#### PUT `/config`
Update system configuration.

**Request:**
```json
{
  "system": {
    "log_level": "info"
  },
  "monitoring": {
    "performance_alerts": false
  }
}
```

## 🔄 AI Service Routing

#### POST `/chat/completions`
Route chat completion requests to optimal provider.

**Request:**
```json
{
  "model": "llama3.1-70b",
  "messages": [
    {
      "role": "user",
      "content": "Hello, how are you?"
    }
  ],
  "max_tokens": 1000,
  "temperature": 0.7,
  "provider": "cerebras"  // Optional: specify provider
}
```

**Response:**
```json
{
  "id": "chatcmpl-123456789",
  "object": "chat.completion",
  "created": 1699874567,
  "model": "llama3.1-70b",
  "choices": [
    {
      "index": 0,
      "message": {
        "role": "assistant",
        "content": "Hello! I'm doing well, thank you for asking. How can I assist you today?"
      },
      "finish_reason": "stop"
    }
  ],
  "usage": {
    "prompt_tokens": 25,
    "completion_tokens": 18,
    "total_tokens": 43
  },
  "provider": {
    "name": "cerebras",
    "response_time_ms": 245.5,
    "request_id": "req_789"
  }
}
```

## ⚠️ Error Handling

### Error Response Format

```json
{
  "error": {
    "type": "rate_limit_exceeded",
    "message": "Rate limit exceeded. Please try again later.",
    "code": 429,
    "details": {
      "retry_after": 60,
      "requests_made": 300,
      "max_requests_per_minute": 300
    }
  },
  "timestamp": "2025-11-13T02:55:15.624Z"
}
```

### Common Error Codes

| Code | Type | Description |
|-------|-------|-------------|
| 400 | invalid_request | Invalid request parameters |
| 401 | authentication_error | Invalid API key |
| 403 | permission_denied | Insufficient permissions |
| 429 | rate_limit_exceeded | Rate limit exceeded |
| 500 | internal_error | Internal server error |
| 503 | service_unavailable | Provider unavailable |

## 📈 Rate Limiting

### Default Rate Limits

| Endpoint | Requests/Minute | Burst Size |
|-----------|------------------|-------------|
| `/health` | 60 | 5 |
| `/metrics` | 30 | 3 |
| `/chat/completions` | 1000 | 10 |

### Rate Limit Headers

```http
X-RateLimit-Limit: 60
X-RateLimit-Remaining: 45
X-RateLimit-Reset: 1699874620
```

## 🔔 Webhooks

### Configure Alert Webhooks

```json
{
  "webhooks": {
    "alert_webhook": "https://your-domain.com/webhooks/alerts",
    "performance_webhook": "https://your-domain.com/webhooks/performance",
    "events": [
      "provider_failure",
      "high_latency",
      "rate_limit_exceeded"
    ]
  }
}
```

### Webhook Payload Format

```json
{
  "event": "provider_failure",
  "timestamp": "2025-11-13T02:55:15.624Z",
  "data": {
    "provider": "cerebras",
    "error": "Connection timeout",
    "severity": "critical",
    "retry_count": 3
  }
}
```

## 🧪 Testing

### Health Check Test

```bash
curl -X GET http://localhost:8080/health \
  -H "Authorization: Bearer YOUR_API_KEY"
```

### Metrics Test

```bash
curl -X GET http://localhost:8080/metrics \
  -H "Authorization: Bearer YOUR_API_KEY"
```

### Chat Completion Test

```bash
curl -X POST http://localhost:8080/chat/completions \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer YOUR_API_KEY" \
  -d '{
    "model": "llama3.1-70b",
    "messages": [{"role": "user", "content": "Hello!"}],
    "max_tokens": 100
  }'
```

## 📚 Additional Resources

- [Configuration Guide](./configuration.md)
- [Deployment Guide](./deployment.md)
- [Troubleshooting Guide](./troubleshooting.md)
- [Load Balancing Algorithms](./load-balancing.md)

---

**API Version**: 2.0  
**Last Updated**: 2025-11-13  
**Compatibility**: All production builds of Aimux 2.0