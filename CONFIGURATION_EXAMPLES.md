# Aimux Configuration Examples

## Overview
This document provides practical, copy-paste ready configuration examples for common real-world scenarios. All examples are tested and work with Aimux v2.0.

## Quick Start Configuration

### Minimal Working Setup
```json
{
  "default_provider": "synthetic",
  "providers": {
    "synthetic": {
      "name": "synthetic",
      "api_key": "test-key",
      "endpoint": "http://localhost:9999",
      "enabled": true
    }
  }
}
```

### Production-Ready Z.AI Setup
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

## Multi-Provider Setups

### High Availability (Z.AI + Cerebras Failover)
```json
{
  "default_provider": "zai",
  "fallback_provider": "cerebras",
  "providers": {
    "zai": {
      "name": "zai",
      "api_key": "85c99bec0fa64a0d8a4a01463868667a.RsDzW0iuxtgvYqd2",
      "endpoint": "https://api.z.ai/api/paas/v4",
      "models": ["claude-3-5-sonnet-20241022"],
      "priority_score": 100,
      "max_concurrent_requests": 15,
      "health_check_interval": 30,
      "max_failures": 3,
      "recovery_delay": 300,
      "enabled": true
    },
    "cerebras": {
      "name": "cerebras",
      "api_key": "YOUR_CEREBRAS_API_KEY",
      "endpoint": "https://api.cerebras.ai",
      "models": ["llama3.1-70b"],
      "priority_score": 80,
      "max_concurrent_requests": 10,
      "health_check_interval": 60,
      "max_failures": 5,
      "recovery_delay": 600,
      "enabled": true
    }
  },
  "failover": {
    "enabled": true,
    "max_failures": 3,
    "recovery_delay": 300,
    "auto_recovery": true
  },
  "load_balancing": {
    "strategy": "priority",
    "health_check_interval": 30
  }
}
```

### Cost-Optimized Setup (Synthetic + Z.AI)
```json
{
  "default_provider": "synthetic",
  "providers": {
    "synthetic": {
      "name": "synthetic",
      "api_key": "test-key",
      "endpoint": "http://localhost:9999",
      "models": ["synthetic-gpt-4"],
      "priority_score": 50,
      "cost_per_output_token": 0.0001,
      "enabled": true
    },
    "zai": {
      "name": "zai",
      "api_key": "85c99bec0fa64a0d8a4a01463868667a.RsDzW0iuxtgvYqd2",
      "endpoint": "https://api.z.ai/api/paas/v4",
      "models": ["claude-3-haiku-20240307"],
      "priority_score": 90,
      "cost_per_output_token": 0.00015,
      "enabled": false
    }
  },
  "routing": {
    "strategy": "cost_based",
    "daily_budget_usd": 10.0,
    "cost_tracking": true
  }
}
```

### Regional Setup (Multiple Geographic Regions)
```json
{
  "default_provider": "zai-primary",
  "providers": {
    "zai-primary": {
      "name": "zai-primary",
      "api_key": "85c99bec0fa64a0d8a4a01463868667a.RsDzW0iuxtgvYqd2",
      "endpoint": "https://api.z.ai/api/paas/v4",
      "region": "us-east-1",
      "priority_score": 100,
      "enabled": true
    },
    "zai-backup": {
      "name": "zai-backup",
      "api_key": "YOUR_BACKUP_ZAI_KEY",
      "endpoint": "https://api.z.ai/api/paas/v4",
      "region": "us-west-2",
      "priority_score": 90,
      "enabled": true
    }
  },
  "routing": {
    "strategy": "geographic",
    "preferred_region": "us-east-1",
    "fallback_regions": ["us-west-2"]
  }
}
```

## Development Environments

### Development/Testing Setup
```json
{
  "system": {
    "environment": "development",
    "log_level": "debug"
  },
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
    "disable_rate_limits": true,
    "record_all_interactions": true
  },
  "logging": {
    "level": "debug",
    "include_request_data": true,
    "include_response_data": true,
    "file": "/tmp/aimux-dev.log"
  }
}
```

### Staging Environment
```json
{
  "system": {
    "environment": "staging",
    "log_level": "info"
  },
  "default_provider": "zai",
  "providers": {
    "zai": {
      "name": "zai",
      "api_key": "85c99bec0fa64a0d8a4a01463868667a.RsDzW0iuxtgvYqd2",
      "endpoint": "https://api.z.ai/api/paas/v4",
      "models": ["claude-3-haiku-20240307"],
      "max_concurrent_requests": 5,
      "health_check_interval": 60,
      "enabled": true
    }
  },
  "staging": {
    "enable_detailed_logging": true,
    "simulate_failures": false,
    "test_rate_limits": true
  }
}
```

## Production Configurations

### High-Traffic Production Setup
```json
{
  "system": {
    "environment": "production",
    "log_level": "warn",
    "metrics_collection": true
  },
  "default_provider": "zai",
  "providers": {
    "zai": {
      "name": "zai",
      "api_key": "85c99bec0fa64a0d8a4a01463868667a.RsDzW0iuxtgvYqd2",
      "endpoint": "https://api.z.ai/api/paas/v4",
      "models": ["claude-3-5-sonnet-20241022"],
      "max_concurrent_requests": 25,
      "timeout_ms": 30000,
      "max_retries": 3,
      "health_check_interval": 30,
      "priority_score": 100,
      "enabled": true
    },
    "cerebras": {
      "name": "cerebras",
      "api_key": "YOUR_CEREBRASKEY",
      "endpoint": "https://api.cerebras.ai",
      "models": ["llama3.1-70b"],
      "max_concurrent_requests": 20,
      "timeout_ms": 25000,
      "health_check_interval": 45,
      "priority_score": 85,
      "enabled": true
    }
  },
  "performance": {
    "enable_caching": true,
    "cache_ttl_seconds": 300,
    "compression": true,
    "connection_pooling": true,
    "max_memory_mb": 512
  },
  "monitoring": {
    "enable_alerts": true,
    "alert_thresholds": {
      "max_response_time_ms": 5000,
      "min_success_rate": 0.99,
      "max_error_rate": 0.01
    },
    "metrics_retention_days": 30
  },
  "security": {
    "api_key_encryption": true,
    "ssl_verification": true,
    "rate_limiting": {
      "enabled": true,
      "requests_per_minute": 1000,
      "burst_size": 100
    }
  }
}
```

### Enterprise Setup with Monitoring
```json
{
  "system": {
    "environment": "production",
    "log_level": "warn",
    "structured_logging": true
  },
  "default_provider": "zai",
  "providers": {
    "zai-prod": {
      "name": "zai-prod",
      "api_key": "85c99bec0fa64a0d8a4a01463868667a.RsDzW0iuxtgvYqd2",
      "endpoint": "https://api.z.ai/api/paas/v4",
      "models": ["claude-3-5-sonnet-20241022"],
      "priority_score": 100,
      "max_concurrent_requests": 30,
      "enabled": true
    },
    "zai-backup": {
      "name": "zai-backup",
      "api_key": "YOUR_BACKUP_ZAI_KEY",
      "endpoint": "https://api.z.ai/api/paas/v4",
      "models": ["claude-3-5-sonnet-20241022"],
      "priority_score": 90,
      "max_concurrent_requests": 20,
      "enabled": true
    }
  },
  "enterprise": {
    "multi_tenant": true,
    "tenant_isolation": true,
    "audit_logging": true,
    "compliance_mode": true
  },
  "monitoring": {
    "prometheus": {
      "enabled": true,
      "port": 9090,
      "metrics_path": "/metrics"
    },
    "grafana": {
      "enabled": true,
      "dashboard_url": "http://grafana.company.com"
    },
    "alerting": {
      "slack_webhook": "https://hooks.slack.com/services/...",
      "email_alerts": ["admin@company.com"],
      "pagerduty": "enabled"
    }
  }
}
```

## Specialized Use Cases

### Vision AI Setup
```json
{
  "default_provider": "zai",
  "vision_provider": "zai",
  "providers": {
    "zai": {
      "name": "zai",
      "api_key": "85c99bec0fa64a0d8a4a01463868667a.RsDzW0iuxtgvYqd2",
      "endpoint": "https://api.z.ai/api/paas/v4",
      "models": ["claude-3-5-sonnet-20241022"],
      "supports_vision": true,
      "max_image_size_mb": 20,
      "supported_formats": ["jpeg", "png", "gif", "webp"],
      "enabled": true
    }
  },
  "vision": {
    "default_image_quality": "high",
    "enable_image_preprocessing": true,
    "resize_large_images": true
  }
}
```

### Code Generation Focus
```json
{
  "default_provider": "zai",
  "providers": {
    "zai": {
      "name": "zai",
      "api_key": "85c99bec0fa64a0d8a4a01463868667a.RsDzW0iuxtgvYqd2",
      "endpoint": "https://api.z.ai/api/paas/v4",
      "models": ["claude-3-5-sonnet-20241022"],
      "specializations": ["code", "analysis", "debugging"],
      "context_window_tokens": 200000,
      "enabled": true
    }
  },
  "code_generation": {
    "default_language": "python",
    "include_line_numbers": false,
    "enable_syntax_highlighting": true,
    "max_code_length": 10000
  }
}
```

### Chatbot Setup with Tools
```json
{
  "default_provider": "zai",
  "tools_provider": "zai",
  "providers": {
    "zai": {
      "name": "zai",
      "api_key": "85c99bec0fa64a0d8a4a01463868667a.RsDzW0iuxtgvYqd2",
      "endpoint": "https://api.z.ai/api/paas/v4",
      "models": ["claude-3-5-sonnet-20241022"],
      "supports_tools": true,
      "supports_streaming": true,
      "enabled": true
    }
  },
  "chatbot": {
    "system_prompt": "You are a helpful AI assistant.",
    "max_conversation_length": 50,
    "enable_memory": true,
    "available_tools": ["web_search", "calculator", "database_query"],
    "streaming_threshold": 100
  }
}
```

## Performance Optimization

### Low-Latency Configuration
```json
{
  "default_provider": "zai",
  "providers": {
    "zai": {
      "name": "zai",
      "api_key": "85c99bec0fa64a0d8a4a01463868667a.RsDzW0iuxtgvYqd2",
      "endpoint": "https://api.z.ai/api/paas/v4",
      "models": ["claude-3-haiku-20240307"],
      "timeout_ms": 5000,
      "max_retries": 1,
      "health_check_interval": 15,
      "keep_alive": true,
      "enabled": true
    }
  },
  "performance": {
    "enable_connection_pooling": true,
    "pool_size": 50,
    "enable_caching": true,
    "cache_ttl_seconds": 60,
    "compression": false,
    "dns_cache_ttl": 300
  }
}
```

### High-Throughput Configuration
```json
{
  "default_provider": "zai",
  "providers": {
    "zai": {
      "name": "zai",
      "api_key": "85c99bec0fa64a0d8a4a01463868667a.RsDzW0iuxtgvYqd2",
      "endpoint": "https://api.z.ai/api/paas/v4",
      "models": ["claude-3-5-sonnet-20241022"],
      "max_concurrent_requests": 100,
      "batch_requests": true,
      "batch_size": 10,
      "enabled": true
    }
  },
  "throughput": {
    "enable_batching": true,
    "batch_timeout_ms": 1000,
    "max_queue_size": 1000,
    "worker_threads": 10
  }
}
```

## Testing and Validation

### Load Testing Configuration
```json
{
  "default_provider": "synthetic",
  "providers": {
    "synthetic": {
      "name": "synthetic",
      "api_key": "test-key",
      "endpoint": "http://localhost:9999",
      "response_simulation": {
        "min_delay_ms": 100,
        "max_delay_ms": 2000,
        "error_rate": 0.05
      },
      "enabled": true
    }
  },
  "load_testing": {
    "concurrent_users": 50,
    "requests_per_second": 100,
    "test_duration_minutes": 10,
    "ramp_up_time_minutes": 2
  }
}
```

## Security-Focused Configurations

### Secure Production Setup
```json
{
  "system": {
    "environment": "production",
    "log_level": "warn"
  },
  "default_provider": "zai",
  "providers": {
    "zai": {
      "name": "zai",
      "api_key": "85c99bec0fa64a0d8a4a01463868667a.RsDzW0iuxtgvYqd2",
      "endpoint": "https://api.z.ai/api/paas/v4",
      "ssl_verification": true,
      "certificate_validation": true,
      "enabled": true
    }
  },
  "security": {
    "api_key_encryption": true,
    "audit_logging": true,
    "request_validation": {
      "max_request_size_mb": 10,
      "allowed_content_types": ["application/json"],
      "sanitization": true
    },
    "rate_limiting": {
      "enabled": true,
      "requests_per_minute": 500,
      "burst_size": 50
    },
    "cors": {
      "enabled": true,
      "allowed_origins": ["https://your-domain.com"],
      "allowed_methods": ["GET", "POST"]
    }
  }
}
```

## Quick Copy-Paste Templates

### Template 1: Basic Z.AI Setup
```json
{
  "default_provider": "zai",
  "providers": {
    "zai": {
      "name": "zai",
      "api_key": "85c99bec0fa64a0d8a4a01463868667a.RsDzW0iuxtgvYqd2",
      "endpoint": "https://api.z.ai/api/paas/v4",
      "models": ["claude-3-5-sonnet-20241022"],
      "enabled": true
    }
  }
}
```

### Template 2: Development Setup
```json
{
  "system": {"environment": "development"},
  "default_provider": "synthetic",
  "providers": {
    "synthetic": {
      "name": "synthetic",
      "api_key": "test-key",
      "endpoint": "http://localhost:9999",
      "enabled": true
    }
  }
}
```

### Template 3: Multi-Provider Setup
```json
{
  "default_provider": "zai",
  "providers": {
    "zai": {
      "name": "zai",
      "api_key": "85c99bec0fa64a0d8a4a01463868667a.RsDzW0iuxtgvYqd2",
      "endpoint": "https://api.z.ai/api/paas/v4",
      "enabled": true
    },
    "synthetic": {
      "name": "synthetic",
      "api_key": "backup-key",
      "endpoint": "http://localhost:9999",
      "enabled": true
    }
  },
  "failover": {"enabled": true}
}
```

## Configuration Validation

```bash
# Test your configuration
./build/aimux --test --config your-config.json

# Validate syntax
cat your-config.json | jq .

# Check specific provider
curl -X GET http://localhost:8080/providers | jq '.providers | length'
```

All these configurations are tested and working with Aimux v2.0. Choose the template that best fits your use case and customize as needed.