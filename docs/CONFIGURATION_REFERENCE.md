# Aimux2 Configuration Reference

## Overview

This document provides a comprehensive reference for all Aimux2 configuration options, including default values, validation rules, and best practices.

## Configuration File Structure

The main configuration file uses JSON format and is located at `/etc/aimux/config.json` by default.

```json
{
  "providers": [...],
  "webui": {...},
  "daemon": {...},
  "security": {...},
  "system": {...},
  "load_balancing": {...},
  "monitoring": {...}
}
```

## Provider Configuration

### Provider Object

```json
{
  "name": "string",
  "api_key": "string",
  "endpoint": "string",
  "group_id": "string(optional)",
  "models": ["string"],
  "enabled": boolean,
  "max_requests_per_minute": integer,
  "priority": integer,
  "retry_attempts": integer,
  "timeout_ms": integer,
  "custom_settings": {...}
}
```

#### Fields

| Field | Type | Default | Required | Description |
|-------|------|---------|----------|-------------|
| `name` | string | - | Yes | Unique provider identifier |
| `api_key` | string | - | Yes | Provider API key (encrypted at rest) |
| `endpoint` | string | - | Yes | Provider API endpoint URL |
| `group_id` | string | null | No | Provider group ID (if required) |
| `models` | array | [] | Yes | Supported model names |
| `enabled` | boolean | true | No | Enable/disable provider |
| `max_requests_per_minute` | integer | 60 | No | Rate limit per minute |
| `priority` | integer | 1 | No | Route priority (lower = higher priority) |
| `retry_attempts` | integer | 3 | No | Number of retry attempts |
| `timeout_ms` | integer | 30000 | No | Request timeout in milliseconds |
| `custom_settings` | object | {} | No | Provider-specific settings |

#### Validation Rules

- `name`: Must be unique, alphanumeric with hyphens
- `api_key`: 16-256 characters when present
- `endpoint`: Valid HTTPS URL
- `models`: At least one model required
- `max_requests_per_minute`: 1-10000
- `priority`: 1-100 (lower numbers have higher priority)
- `retry_attempts`: 0-10
- `timeout_ms`: 1000-300000

### Example Configurations

#### Minimax
```json
{
  "name": "minimax",
  "api_key": "your-minimax-api-key",
  "endpoint": "https://api.minimax.io/anthropic",
  "group_id": "your-group-id",
  "models": ["minimax-m2-100k"],
  "enabled": true,
  "max_requests_per_minute": 60,
  "priority": 1,
  "retry_attempts": 3,
  "timeout_ms": 30000
}
```

#### Z.AI
```json
{
  "name": "zai",
  "api_key": "your-zai-api-key",
  "endpoint": "https://api.z.ai",
  "models": ["zai-claude-3"],
  "enabled": true,
  "max_requests_per_minute": 200,
  "priority": 2,
  "retry_attempts": 3,
  "timeout_ms": 25000
}
```

## WebUI Configuration

```json
{
  "enabled": boolean,
  "port": integer,
  "ssl_port": integer,
  "ssl_enabled": boolean,
  "cert_file": "string",
  "key_file": "string",
  "cors_enabled": boolean,
  "cors_origins": ["string"],
  "api_docs": boolean,
  "real_time_metrics": boolean
}
```

#### Fields

| Field | Type | Default | Required | Description |
|-------|------|---------|----------|-------------|
| `enabled` | boolean | true | No | Enable WebUI interface |
| `port` | integer | 8080 | No | HTTP port |
| `ssl_port` | integer | 8443 | No | HTTPS port |
| `ssl_enabled` | boolean | false | No | Enable HTTPS |
| `cert_file` | string | - | No | SSL certificate file path |
| `key_file` | string | - | No | SSL private key file path |
| `cors_enabled` | boolean | true | No | Enable CORS |
| `cors_origins` | array | ["*"] | No | Allowed CORS origins |
| `api_docs` | boolean | true | No | Enable API documentation |
| `real_time_metrics` | boolean | true | No | Enable real-time metrics |

#### Validation Rules

- `port`: 1-65535, not equal to `ssl_port`
- `ssl_port`: 1-65535, not equal to `port`
- `cert_file`/`key_file`: Required when `ssl_enabled` is true
- `cors_origins`: Valid domains or "*" for all

## Daemon Configuration

```json
{
  "enabled": boolean,
  "user": "string",
  "group": "string",
  "working_directory": "string",
  "log_file": "string",
  "pid_file": "string",
  "auto_restart": boolean
}
```

#### Fields

| Field | Type | Default | Required | Description |
|-------|------|---------|----------|-------------|
| `enabled` | boolean | true | No | Enable daemon mode |
| `user` | string | "aimux" | No | Run as user |
| `group` | string | "aimux" | No | Run as group |
| `working_directory` | string | "/var/lib/aimux" | No | Working directory |
| `log_file` | string | "/var/log/aimux/aimux.log" | No | Log file path |
| `pid_file` | string | "/var/run/aimux.pid" | No | PID file path |
| `auto_restart` | boolean | true | No | Auto-restart on crash |

#### Validation Rules

- `user`/`group`: Must exist on system
- Paths: Must be valid writable directories for specified user

## Security Configuration

```json
{
  "api_key_encryption": boolean,
  "audit_logging": boolean,
  "input_validation": boolean,
  "rate_limiting": boolean,
  "ssl_verification": boolean,
  "require_https": boolean,
  "allowed_origins": ["string"]
}
```

#### Fields

| Field | Type | Default | Required | Description |
|-------|------|---------|----------|-------------|
| `api_key_encryption` | boolean | true | No | Encrypt API keys at rest |
| `audit_logging` | boolean | true | No | Enable security audit logging |
| `input_validation` | boolean | true | No | Strict input validation |
| `rate_limiting` | boolean | true | No | Enable rate limiting |
| `ssl_verification` | boolean | true | No | Verify SSL certificates |
| `require_https` | boolean | false | No | Require HTTPS for WebUI |
| `allowed_origins` | array | ["*"] | No | Allowed origins for API |

#### Security Best Practices

- Set `api_key_encryption` to `true` in production
- Set `require_https` to `true` when `ssl_enabled` is true
- Configure specific `allowed_origins` instead of "*"
- Keep `ssl_verification` enabled for security

## System Configuration

```json
{
  "environment": "string",
  "log_level": "string",
  "structured_logging": boolean,
  "max_concurrent_requests": integer,
  "log_dir": "string",
  "backup_dir": "string",
  "backup_retention_days": integer
}
```

#### Fields

| Field | Type | Default | Required | Description |
|-------|------|---------|----------|-------------|
| `environment` | string | "production" | No | Environment name |
| `log_level` | string | "info" | No | Logging level |
| `structured_logging` | boolean | true | No | Use JSON structured logging |
| `max_concurrent_requests` | integer | 1000 | No | Maximum concurrent requests |
| `log_dir` | string | "/var/log/aimux" | No | Log directory |
| `backup_dir` | string | "/var/backups/aimux" | No | Backup directory |
| `backup_retention_days` | integer | 30 | No | Backup retention period |

#### Valid Values

- `environment`: "development", "staging", "production"
- `log_level`: "trace", "debug", "info", "warn", "error", "fatal"

#### Performance Tuning

- `max_concurrent_requests`: Adjust based on available memory
- Typical values: 100 (low memory), 500 (medium), 1000+ (high performance)

## Load Balancing Configuration

```json
{
  "strategy": "string",
  "failover_enabled": boolean,
  "retry_on_failure": boolean,
  "health_check_interval": integer,
  "circuit_breaker": {
    "enabled": boolean,
    "failure_threshold": integer,
    "recovery_timeout": integer
  }
}
```

#### Fields

| Field | Type | Default | Required | Description |
|-------|------|---------|----------|-------------|
| `strategy` | string | "adaptive" | No | Load balancing strategy |
| `failover_enabled` | boolean | true | No | Enable automatic failover |
| `retry_on_failure` | boolean | true | No | Retry failed requests |
| `health_check_interval` | integer | 30 | No | Health check interval (seconds) |
| `circuit_breaker` | object | - | No | Circuit breaker settings |

#### Strategy Options

- `"adaptive"`: Intelligent routing based on performance
- `"round-robin"`: Distribute requests evenly
- `"weighted"`: Based on provider weights/priorities
- `"failover"`: Primary provider with fallback

#### Circuit Breaker

| Field | Type | Default | Required | Description |
|-------|------|---------|----------|-------------|
| `enabled` | boolean | true | No | Enable circuit breaker |
| `failure_threshold` | integer | 5 | No | Failure threshold |
| `recovery_timeout` | integer | 60 | No | Recovery timeout (seconds) |

## Monitoring Configuration

```json
{
  "metrics_collection": boolean,
  "health_checks": boolean,
  "dashboard_enabled": boolean,
  "performance_alerts": boolean,
  "alert_thresholds": {
    "max_error_rate": number,
    "max_response_time_ms": integer,
    "min_success_rate": number
  }
}
```

#### Fields

| Field | Type | Default | Required | Description |
|-------|------|---------|----------|-------------|
| `metrics_collection` | boolean | true | No | Collect performance metrics |
| `health_checks` | boolean | true | No | Enable health checks |
| `dashboard_enabled` | boolean | true | No | Enable monitoring dashboard |
| `performance_alerts` | boolean | true | No | Enable performance alerts |
| `alert_thresholds` | object | - | No | Alert thresholds |

#### Alert Thresholds

| Field | Type | Default | Required | Description |
|-------|------|---------|----------|-------------|
| `max_error_rate` | number | 0.01 | No | Max error rate (0.0-1.0) |
| `max_response_time_ms` | integer | 1000 | No | Max response time (ms) |
| `min_success_rate` | number | 0.99 | No | Min success rate (0.0-1.0) |

## Environment Variables

### Security Variables

| Variable | Type | Default | Description |
|----------|------|---------|-------------|
| `AIMUX_REQUIRE_HTTPS` | boolean | false | Require HTTPS for WebUI |
| `AIMUX_ENCRYPT_KEYS` | boolean | true | Encrypt API keys |
| `AIMUX_AUDIT_LOGGING` | boolean | true | Enable audit logging |
| `AIMUX_ENCRYPTION_KEY` | string | - | Master encryption key |

### API Key Variables

| Variable | Type | Default | Description |
|----------|------|---------|-------------|
| `AIMUX_MINIMAX_API_KEY` | string | - | Minimax API key |
| `AIMUX_ZAI_API_KEY` | string | - | Z.AI API key |
| `AIMUX_CEREBRAS_API_KEY` | string | - | Cerebras API key |
| `AIMUX_SYNTHETIC_API_KEY` | string | - | Synthetic API key |

### WebUI Variables

| Variable | Type | Default | Description |
|----------|------|---------|-------------|
| `AIMUX_WEBUI_PORT` | integer | 8080 | WebUI HTTP port |
| `AIMUX_WEBUI_SSL_PORT` | integer | 8443 | WebUI HTTPS port |
| `AIMUX_WEBUI_SSL_ENABLED` | boolean | false | Enable WebUI SSL |
| `AIMUX_CORS_ORIGINS` | string | "*" | Comma-separated CORS origins |

### Performance Variables

| Variable | Type | Default | Description |
|----------|------|---------|-------------|
| `AIMUX_MAX_CONCURRENT_REQUESTS` | integer | 1000 | Max concurrent requests |
| `AIMUX_REQUEST_TIMEOUT_MS` | integer | 30000 | Default request timeout |
| `AIMUX_RATE_LIMIT_ENABLED` | boolean | true | Enable rate limiting |
| `AIMUX_MAX_REQUESTS_PER_MINUTE` | integer | 300 | Global rate limit |

### System Variables

| Variable | Type | Default | Description |
|----------|------|---------|-------------|
| `AIMUX_ENVIRONMENT` | string | "production" | Environment name |
| `AIMUX_LOG_LEVEL` | string | "info" | Log level |
| `AIMUX_DEBUG` | boolean | false | Enable debug mode |
| `AIMUX_VERBOSE_LOGGING` | boolean | false | Enable verbose logging |

## Configuration Validation

### Syntax Validation

Validate configuration syntax:

```bash
/opt/aimux2/aimux --validate-config [--config /path/to/config.json]
```

### Full Validation

```bash
/opt/aimux2/aimux --validate-full-config
```

This checks:
- JSON syntax
- Field types and ranges
- Required fields
- Logical consistency
- Security policy compliance

### Configuration Migrations

The system supports automatic migrations between configuration versions:

```bash
# Migrate from v1 to v2
/opt/aimux2/aimux --migrate-config --from-version 1.0 --to-version 2.0

# Auto-migrate to latest
/opt/aimux2/aimux --migrate-config --auto
```

## Best Practices

### Security

1. **API Key Management**
   - Always use `api_key_encryption: true`
   - Rotate API keys regularly
   - Use environment variables for sensitive data

2. **Network Security**
   - Enable `require_https` in production
   - Configure specific `allowed_origins`
   - Use firewall rules to restrict access

3. **File Permissions**
   - Config files: 640 (owner:group)
   - Log files: 640 (aimux:adm)
   - SSL certificates: 600 (root:root)

### Performance

1. **Resource Allocation**
   - Set `max_concurrent_requests` based on available memory
   - Monitor memory usage and adjust accordingly
   - Use structured logging for easier parsing

2. **Rate Limiting**
   - Configure per-provider rate limits
   - Use circuit breaker to prevent cascade failures
   - Monitor provider-specific performance

3. **Load Balancing**
   - Use `"adaptive"` strategy for automatic optimization
   - Enable health checks for provider availability
   - Configure appropriate retry strategies

### Reliability

1. **Failover**
   - Enable `failover_enabled` for high availability
   - Configure at least 2 providers
   - Test failover scenarios

2. **Monitoring**
   - Enable all monitoring features
   - Configure alert thresholds appropriately
   - Set up external alerting

3. **Backup**
   - Regular configuration backups
   - Rotate backups based on retention policy
   - Test restore procedures

## Troubleshooting Configuration

### Common Issues

1. **Provider Not Working**
   - Check `api_key` and `endpoint` values
   - Verify `models` list matches provider
   - Check rate limits and timeouts

2. **WebUI Not Accessible**
   - Verify `port` and `ssl_port` are available
   - Check firewall rules
   - Review SSL configuration if enabled

3. **High Memory Usage**
   - Reduce `max_concurrent_requests`
   - Check for memory leaks in logs
   - Monitor system resources

### Debug Configuration

Enable debug mode:

```bash
# Temporary debug
./scripts/manage_service.sh logs --level debug

# Configuration dump
curl http://localhost:8080/config
```

### Configuration Reset

Reset to factory defaults:

```bash
# Backup current config
sudo ./scripts/manage_service.sh backup

# Reset to defaults
sudo /opt/aimux2/aimux --reset-config
```

---

This reference provides all configuration options and best practices for running Aimux2 in production. For deployment instructions, see the [Deployment Guide](DEPLOYMENT_GUIDE.md).