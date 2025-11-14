# Aimux 2.0 - Configuration Reference

## 📖 Overview

Aimux 2.0 uses JSON configuration files for all system settings. This document provides a comprehensive reference for all configuration options.

## 🔧 Configuration File Structure

The main configuration file follows this structure:

```json
{
  "system": { ... },
  "security": { ... },
  "providers": { ... },
  "load_balancing": { ... },
  "webui": { ... },
  "monitoring": { ... },
  "daemon": { ... }
}
```

## ⚙️ System Configuration

```json
{
  "system": {
    "environment": "production",
    "log_level": "warn",
    "structured_logging": true,
    "max_concurrent_requests": 1000,
    "working_directory": "/var/lib/aimux",
    "pid_file": "/var/run/aimux.pid",
    "user": "aimux",
    "group": "aimux"
  }
}
```

### System Options

| Option | Type | Default | Description |
|---------|-------|---------|-------------|
| `environment` | string | `"development"` | Runtime environment (`development`, `staging`, `production`) |
| `log_level` | string | `"info"` | Logging level (`trace`, `debug`, `info`, `warn`, `error`, `fatal`) |
| `structured_logging` | boolean | `true` | Enable structured JSON logging |
| `max_concurrent_requests` | integer | `1000` | Maximum concurrent API requests |
| `working_directory` | string | `"./data"` | Directory for runtime data |
| `pid_file` | string | `"./aimux.pid"` | Process ID file location |
| `user` | string | `""` | System user for daemon mode |
| `group` | string | `""` | System group for daemon mode |

## 🔐 Security Configuration

```json
{
  "security": {
    "api_key_encryption": true,
    "rate_limiting": true,
    "ssl_verification": true,
    "input_validation": true,
    "audit_logging": true,
    "cors_origins": ["https://your-domain.com"],
    "allowed_ips": ["0.0.0.0/0"],
    "max_request_size": "10MB",
    "session_timeout": 3600
  }
}
```

### Security Options

| Option | Type | Default | Description |
|---------|-------|---------|-------------|
| `api_key_encryption` | boolean | `true` | Encrypt stored API keys |
| `rate_limiting` | boolean | `true` | Enable rate limiting |
| `ssl_verification` | boolean | `true` | Verify SSL certificates |
| `input_validation` | boolean | `true` | Validate input parameters |
| `audit_logging` | boolean | `true` | Log all actions for audit |
| `cors_origins` | array | `["*"]` | Allowed CORS origins |
| `allowed_ips` | array | `["0.0.0.0/0"]` | Allowed client IP ranges |
| `max_request_size` | string | `"10MB"` | Maximum request body size |
| `session_timeout` | integer | `3600` | Session timeout in seconds |

## 🌐 Provider Configuration

### Cerebras Provider

```json
{
  "cerebras": {
    "endpoint": "https://api.cerebras.ai",
    "enabled": true,
    "api_key": "${CEREBRAS_API_KEY}",
    "max_requests_per_minute": 300,
    "timeout_ms": 30000,
    "retry_attempts": 3,
    "retry_delay_ms": 1000,
    "priority": 1,
    "models": ["llama3.1-70b", "llama3.1-8b"],
    "health_check_url": "/v1/models",
    "custom_headers": {
      "User-Agent": "Aimux/2.0"
    }
  }
}
```

### Z.ai Provider

```json
{
  "zai": {
    "endpoint": "https://api.z.ai",
    "enabled": true,
    "api_key": "${ZAI_API_KEY}",
    "max_requests_per_minute": 200,
    "timeout_ms": 25000,
    "retry_attempts": 3,
    "retry_delay_ms": 1500,
    "priority": 2,
    "models": ["zai-1-large", "zai-1-medium"],
    "health_check_url": "/v1/health"
  }
}
```

### MiniMax Provider

```json
{
  "minimax": {
    "endpoint": "https://api.minimax.chat",
    "enabled": true,
    "api_key": "${MINIMAX_API_KEY}",
    "max_requests_per_minute": 150,
    "timeout_ms": 20000,
    "retry_attempts": 3,
    "retry_delay_ms": 2000,
    "priority": 3,
    "models": ["abab6.5s-chat", "abab5.5-chat"],
    "auth_headers": {
      "Authorization": "Bearer ${MINIMAX_API_KEY}",
      "X-Provider": "minimax-m2"
    }
  }
}
```

### Synthetic Provider (Testing)

```json
{
  "synthetic": {
    "endpoint": "https://synthetic.test",
    "enabled": false,
    "max_requests_per_minute": 1000,
    "timeout_ms": 5000,
    "priority": 99,
    "response_delay_ms": {
      "min": 50,
      "max": 500
    },
    "error_rate": 0.0,
    "success_rate": 1.0
  }
}
```

### Common Provider Options

| Option | Type | Default | Description |
|---------|-------|---------|-------------|
| `endpoint` | string | required | Provider API endpoint URL |
| `enabled` | boolean | `true` | Enable/disable provider |
| `api_key` | string | required | API key (supports `${VAR}` env vars) |
| `max_requests_per_minute` | integer | `60` | Rate limit per minute |
| `timeout_ms` | integer | `30000` | Request timeout in milliseconds |
| `retry_attempts` | integer | `3` | Number of retry attempts |
| `retry_delay_ms` | integer | `1000` | Delay between retries |
| `priority` | integer | `1` | Load balancing priority (1=highest) |
| `models` | array | `["*"]` | Supported model list |

## ⚖️ Load Balancing Configuration

```json
{
  "load_balancing": {
    "strategy": "adaptive",
    "health_check_interval": 30,
    "failover_enabled": true,
    "retry_on_failure": true,
    "circuit_breaker": {
      "enabled": true,
      "failure_threshold": 5,
      "success_threshold": 3,
      "recovery_timeout": 60,
      "half_open_max_calls": 10
    },
    "connection_pool": {
      "enabled": true,
      "max_connections": 100,
      "idle_timeout": 300
    }
  }
}
```

### Load Balancing Options

#### Strategy Types

| Strategy | Description | Use Case |
|----------|-------------|-----------|
| `round_robin` | Sequential distribution | Simple load distribution |
| `least_connections` | Fewest active connections | Connection-intensive workloads |
| `fastest_response` | Lowest average response time | Performance-sensitive apps |
| `weighted_round_robin` | Response time weighted | Intelligent distribution |
| `adaptive` | Multi-factor scoring | Complex workloads |
| `random` | Random selection | Testing or special cases |

#### Circuit Breaker Options

| Option | Type | Default | Description |
|---------|-------|---------|-------------|
| `enabled` | boolean | `true` | Enable circuit breaker |
| `failure_threshold` | integer | `5` | Failures before opening circuit |
| `success_threshold` | integer | `3` | Successes before closing circuit |
| `recovery_timeout` | integer | `60` | Seconds to wait before testing |
| `half_open_max_calls` | integer | `10` | Max calls in half-open state |

## 🖥️ WebUI Configuration

```json
{
  "webui": {
    "enabled": true,
    "port": 8080,
    "host": "0.0.0.0",
    "ssl_enabled": true,
    "ssl_cert": "/etc/ssl/certs/aimux.crt",
    "ssl_key": "/etc/ssl/private/aimux.key",
    "cors_enabled": true,
    "cors_origins": ["*"],
    "api_docs": true,
    "real_time_metrics": true,
    "auto_refresh": 5,
    "theme": "dark",
    "custom_css": "/opt/aimux/webui/custom.css",
    "branding": {
      "title": "Aimux Dashboard",
      "logo": "/images/logo.svg",
      "footer": "© 2025 Your Company"
    }
  }
}
```

### WebUI Options

| Option | Type | Default | Description |
|---------|-------|---------|-------------|
| `enabled` | boolean | `true` | Enable WebUI interface |
| `port` | integer | `8080` | WebUI port |
| `host` | string | `"0.0.0.0"` | Bind address |
| `ssl_enabled` | boolean | `true` | Enable HTTPS for WebUI |
| `ssl_cert` | string | required for SSL | SSL certificate path |
| `ssl_key` | string | required for SSL | SSL private key path |
| `cors_enabled` | boolean | `true` | Enable CORS headers |
| `api_docs` | boolean | `true` | Show API documentation |
| `real_time_metrics` | boolean | `true` | Enable real-time updates |
| `auto_refresh` | integer | `5` | Dashboard refresh interval (seconds) |
| `theme` | string | `"light"` | UI theme (`light`, `dark`) |

## 📊 Monitoring Configuration

```json
{
  "monitoring": {
    "metrics_collection": true,
    "health_checks": true,
    "performance_alerts": true,
    "dashboard_enabled": true,
    "alert_thresholds": {
      "max_response_time_ms": 1000,
      "min_success_rate": 0.99,
      "max_error_rate": 0.01,
      "max_memory_usage": 0.8,
      "max_cpu_usage": 0.8
    },
    "webhooks": {
      "alert_webhook": "https://your-monitoring.com/webhooks",
      "performance_webhook": "https://your-metrics.com/webhooks",
      "events": ["provider_failure", "high_latency", "rate_limit_exceeded"]
    },
    "prometheus": {
      "enabled": true,
      "port": 9090,
      "endpoint": "/metrics",
      "include_labels": true
    },
    "logging": {
      "access_log": "/var/log/aimux/access.log",
      "error_log": "/var/log/aimux/error.log",
      "audit_log": "/var/log/aimux/audit.log",
      "rotation": "daily",
      "retention": 30
    }
  }
}
```

### Monitoring Options

#### Alert Thresholds

| Option | Type | Default | Description |
|---------|-------|---------|-------------|
| `max_response_time_ms` | integer | `1000` | Alert on slow responses |
| `min_success_rate` | float | `0.99` | Alert on low success rate |
| `max_error_rate` | float | `0.01` | Alert on high error rate |
| `max_memory_usage` | float | `0.8` | Alert on high memory usage |
| `max_cpu_usage` | float | `0.8` | Alert on high CPU usage |

#### Webhook Events

| Event | Description |
|-------|-------------|
| `provider_failure` | Provider becomes unhealthy |
| `provider_recovery` | Provider becomes healthy again |
| `high_latency` | Response time exceeds threshold |
| `rate_limit_exceeded` | Rate limit hit |
| `circuit_breaker_open` | Circuit breaker opens |
| `circuit_breaker_close` | Circuit breaker closes |

## 🔄 Daemon Configuration

```json
{
  "daemon": {
    "enabled": true,
    "user": "aimux",
    "group": "aimux",
    "working_directory": "/var/lib/aimux",
    "pid_file": "/var/run/aimux.pid",
    "log_file": "/var/log/aimux/aimux.log",
    "auto_restart": true,
    "restart_delay": 10,
    "max_restarts": 10,
    "umask": "0027",
    "signal_handlers": {
      "sigterm": "graceful_shutdown",
      "sigint": "graceful_shutdown",
      "sighup": "reload_config"
    }
  }
}
```

### Daemon Options

| Option | Type | Default | Description |
|---------|-------|---------|-------------|
| `enabled` | boolean | `true` | Enable daemon mode |
| `user` | string | `""` | Run as this user |
| `group` | string | `""` | Run as this group |
| `auto_restart` | boolean | `true` | Restart on failure |
| `restart_delay` | integer | `10` | Seconds between restarts |
| `max_restarts` | integer | `10` | Maximum restarts per hour |
| `umask` | string | `"0027"` | File creation mask |

## 🌍 Environment Variables

Aimux supports environment variable substitution using `${VAR_NAME}` syntax.

### Common Environment Variables

| Variable | Description | Example |
|----------|-------------|---------|
| `CEREBRAS_API_KEY` | Cerebras API key | `"sk-abc123..."` |
| `ZAI_API_KEY` | Z.ai API key | `"zai-def456..."` |
| `MINIMAX_API_KEY` | MiniMax API key | `"mm-ghi789..."` |
| `AIMUX_ENV` | Runtime environment | `"production"` |
| `AIMUX_LOG_LEVEL` | Override log level | `"warn"` |
| `AIMUX_CONFIG_FILE` | Config file path | `"/etc/aimux/config.json"` |

### Environment Variable File

Create `.env` file in working directory:

```bash
# API Keys
CEREBRAS_API_KEY=sk-your-cerebras-key
ZAI_API_KEY=zai-your-zai-key
MINIMAX_API_KEY=mm-your-minimax-key

# System
AIMUX_ENV=production
AIMUX_LOG_LEVEL=warn

# Database (if used)
AIMUX_DB_HOST=localhost
AIMUX_DB_PORT=5432
AIMUX_DB_NAME=aimux
AIMUX_DB_USER=aimux
AIMUX_DB_PASSWORD=secure_password
```

## 🔧 Configuration Validation

### Built-in Validation

```bash
# Validate configuration file
./aimux --config config.json --validate

# Test with synthetic providers
./aimux --test --dry-run
```

### Configuration Schema

Use JSON Schema for validation:

```json
{
  "$schema": "https://json-schema.org/draft/2020-12/schema",
  "type": "object",
  "properties": {
    "system": {
      "type": "object",
      "properties": {
        "environment": {
          "type": "string",
          "enum": ["development", "staging", "production"]
        },
        "max_concurrent_requests": {
          "type": "integer",
          "minimum": 1,
          "maximum": 10000
        }
      },
      "required": ["environment"]
    }
  },
  "required": ["system", "providers"]
}
```

## 📝 Configuration Examples

### Development Configuration

```json
{
  "system": {
    "environment": "development",
    "log_level": "debug",
    "structured_logging": false,
    "max_concurrent_requests": 100
  },
  "providers": {
    "synthetic": {
      "enabled": true,
      "max_requests_per_minute": 1000,
      "error_rate": 0.1
    }
  },
  "webui": {
    "enabled": true,
    "port": 8080,
    "ssl_enabled": false,
    "cors_enabled": true,
    "cors_origins": ["*"]
  },
  "monitoring": {
    "metrics_collection": true,
    "performance_alerts": false
  }
}
```

### Production Configuration

```json
{
  "system": {
    "environment": "production",
    "log_level": "warn",
    "structured_logging": true,
    "max_concurrent_requests": 2000
  },
  "security": {
    "api_key_encryption": true,
    "rate_limiting": true,
    "ssl_verification": true,
    "cors_origins": ["https://your-domain.com"]
  },
  "providers": {
    "cerebras": {
      "enabled": true,
      "priority": 1
    },
    "zai": {
      "enabled": true,
      "priority": 2
    }
  },
  "load_balancing": {
    "strategy": "adaptive",
    "failover_enabled": true,
    "circuit_breaker": {
      "enabled": true,
      "failure_threshold": 5
    }
  },
  "webui": {
    "enabled": true,
    "port": 8080,
    "ssl_enabled": true,
    "cors_origins": ["https://your-domain.com"]
  },
  "monitoring": {
    "metrics_collection": true,
    "performance_alerts": true,
    "alert_thresholds": {
      "max_response_time_ms": 1000,
      "min_success_rate": 0.99
    }
  }
}
```

## 🔍 Configuration Debugging

### Enable Debug Mode

```json
{
  "system": {
    "log_level": "debug",
    "debug_config": true
  }
}
```

### Test Configuration

```bash
# Test configuration syntax
./aimux --config config.json --syntax-check

# Test providers connectivity
./aimux --test-providers

# Test load balancing
./aimux --test-load-balancing
```

---

## 📞 Support

For configuration assistance:

- **Documentation**: https://docs.aimux.com
- **Configuration Wizard**: `./aimux --config-wizard`
- **Validation Tool**: `./aimux --validate-config`
- **Community Forum**: https://community.aimux.com

---

**Last Updated**: 2025-11-13  
**Version**: Aimux 2.0 Configuration Reference  
**Format**: JSON Schema v2025-11