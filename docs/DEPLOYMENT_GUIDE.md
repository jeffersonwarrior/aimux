# Aimux2 Production Deployment Guide

## Overview

This guide provides comprehensive instructions for deploying Aimux2 in production environments. Aimux2 is a high-performance AI multiplexer built in C++ that provides intelligent routing across multiple AI providers.

## Table of Contents

1. [Prerequisites](#prerequisites)
2. [System Requirements](#system-requirements)
3. [Installation](#installation)
4. [Configuration](#configuration)
5. [Service Management](#service-management)
6. [Security](#security)
7. [Monitoring](#monitoring)
8. [Backup and Recovery](#backup-and-recovery)
9. [Troubleshooting](#troubleshooting)
10. [Performance Tuning](#performance-tuning)

## Prerequisites

### Operating Systems

- **Linux**: Ubuntu 20.04+, CentOS 8+, RHEL 8+, Debian 11+
- **macOS**: 11.0+ (Big Sur or later)

### Required Software

- **C++23 compatible compiler**: GCC 11+, Clang 13+
- **CMake**: version 3.20 or higher
- **OpenSSL**: 1.1.1+
- **Curl**: 7.0+
- **vcpkg**: for dependency management

### System Dependencies

```bash
# Ubuntu/Debian
sudo apt update
sudo apt install -y build-essential cmake libssl-dev libcurl4-openssl-dev

# CentOS/RHEL
sudo yum install -y gcc-c++ cmake openssl-devel libcurl-devel

# macOS (with Homebrew)
brew install cmake openssl curl
```

## System Requirements

### Hardware

- **CPU**: 2+ cores recommended
- **Memory**: Minimum 512MB, Recommended 1GB+
- **Storage**: 100MB for application, + storage for logs
- **Network**: Stable internet connection for AI provider APIs

### Software Architecture

- **Memory Usage**: ~50MB baseline (vs 200MB+ TypeScript version)
- **CPU Usage**: ~10x faster performance than TypeScript version
- **Concurrent Requests**: Up to 1000 concurrent requests
- **Rate Limiting**: Configurable per provider (default: 300 requests/minute)

## Installation

### Method 1: Pre-built Binary (Recommended)

1. **Download the latest release**
   ```bash
   wget https://github.com/aimux/aimux2/releases/latest/download/aimux2-production-linux-x64.tar.gz
   tar -xzf aimux2-production-linux-x64.tar.gz
   cd aimux2-production
   ```

2. **Install the service**
   ```bash
   sudo ./scripts/install_service.sh
   ```

### Method 2: Build from Source

1. **Clone and build**
   ```bash
   git clone https://github.com/aimux/aimux2.git
   cd aimux2/aimux
   ./scripts/build_production.sh
   ```

2. **Install the service**
   ```bash
   sudo ./scripts/install_service.sh
   ```

## Configuration

### Basic Configuration

The main configuration file is located at `/etc/aimux/config.json`. Here's a production-ready example:

```json
{
  "providers": [
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
  ],
  "webui": {
    "enabled": true,
    "port": 8080,
    "ssl_port": 8443,
    "ssl_enabled": false,
    "cors_enabled": true,
    "cors_origins": ["your-domain.com"],
    "api_docs": true,
    "real_time_metrics": true
  },
  "security": {
    "api_key_encryption": true,
    "audit_logging": true,
    "input_validation": true,
    "rate_limiting": true,
    "require_https": true
  },
  "system": {
    "environment": "production",
    "log_level": "warn",
    "structured_logging": true,
    "max_concurrent_requests": 1000
  }
}
```

### Environment Variables

Create `/etc/aimux/.env` for environment-specific configuration:

```bash
# Security
AIMUX_REQUIRE_HTTPS=true
AIMUX_ENCRYPT_KEYS=true
AIMUX_AUDIT_LOGGING=true

# API Keys (alternative to config file)
AIMUX_MINIMAX_API_KEY=your-minimax-api-key
AIMUX_ZAI_API_KEY=your-zai-api-key

# WebUI
AIMUX_WEBUI_PORT=8080
AIMUX_WEBUI_SSL_PORT=8443

# Performance
AIMUX_MAX_CONCURRENT_REQUESTS=1000
AIMUX_REQUEST_TIMEOUT_MS=30000
```

### SSL/TLS Configuration (Optional but Recommended)

1. **Generate self-signed certificate** (for development):
   ```bash
   sudo mkdir -p /etc/ssl/aimux
   cd /etc/ssl/aimux
   sudo openssl req -x509 -newkey rsa:4096 -keyout aimux.key -out aimux.crt -days 365 -nodes
   ```

2. **Update configuration**:
   ```json
   {
     "webui": {
       "ssl_enabled": true,
       "cert_file": "/etc/ssl/aimux/aimux.crt",
       "key_file": "/etc/ssl/aimux/aimux.key"
     }
   }
   ```

## Service Management

### Starting and Stopping

```bash
# Start the service
sudo systemctl start aimux2

# Stop the service
sudo systemctl stop aimux2

# Restart the service
sudo systemctl restart aimux2

# Check status
sudo systemctl status aimux2

# Enable auto-start on boot
sudo systemctl enable aimux2
```

### Using the Management Script

```bash
# All-purpose service management
./scripts/manage_service.sh {start|stop|restart|status|logs|health}

# View logs with filtering
./scripts/manage_service.sh logs
./scripts/manage_service.sh logs -f  # Follow logs

# Check service health
./scripts/manage_service.sh health

# Configuration management
./scripts/manage_service.sh config
```

## Security

### Production Security Checklist

- [ ] **API Key Encryption**: Ensure API keys are encrypted at rest
- [ ] **HTTPS/TLS**: Enable SSL for WebUI in production
- [ ] **Firewall**: Configure firewall rules to restrict access
- [ ] **User Permissions**: Run as non-privileged user (aimux)
- [ ] **Log Security**: Configure log rotation and secure permissions

### Security Hardening

1. **File Permissions**
   ```bash
   sudo chmod 640 /etc/aimux/config.json
   sudo chmod 640 /etc/aimux/.env
   sudo chown aimux:aimux /etc/aimux/*
   ```

2. **Firewall Configuration** (Ubuntu/Debian)
   ```bash
   sudo ufw allow 8080/tcp  # WebUI HTTP
   sudo ufw allow 8443/tcp  # WebUI HTTPS
   ```

3. **SELinux/AppArmor** (if applicable)
   - Ensure appropriate security policies are in place
   - Test with permissive mode first

### API Key Management

- Store API keys in encrypted format in the config file
- Use environment variables for sensitive configuration
- Implement regular key rotation policies
- Monitor for API key usage and anomalies

## Monitoring

### WebUI Dashboard

Access the monitoring dashboard at:
- HTTP: `http://localhost:8080`
- HTTPS: `https://localhost:8443` (if SSL enabled)

### Key Monitoring Endpoints

- **Health Check**: `/health`
- **Metrics (JSON)**: `/metrics`
- **Metrics (Prometheus)**: `/metrics/prometheus`
- **Logs**: `/logs`
- **Alerts**: `/alerts`
- **Configuration**: `/config`

### Command Line Monitoring

```bash
# Service health
curl http://localhost:8080/health

# Metrics
curl http://localhost:8080/metrics

# Recent logs
curl "http://localhost:8080/logs?limit=50&level=error"
```

### System Logs

```bash
# Service logs via systemd
sudo journalctl -u aimux2 -f

# Application logs
sudo tail -f /var/log/aimux/aimux.log

# Log rotation
sudo logrotate -f /etc/logrotate.d/aimux2
```

### Alerting

Configure alerts through the WebUI or programmatically:

```bash
# Check active alerts
curl http://localhost:8080/alerts

# Example metrics to monitor:
- Error rate > 1%
- Response time > 1000ms
- Memory usage > 80%
- Provider failures
```

## Backup and Recovery

### Automated Backups

The system automatically creates daily backups by default:
- Location: `/var/backups/aimux/`
- Retention: 30 days (configurable)

### Manual Backup

```bash
# Backup configuration and data
sudo ./scripts/manage_service.sh backup

# Custom backup location
sudo cp -r /etc/aimux /path/to/backup/config
sudo cp -r /var/lib/aimux /path/to/backup/data
```

### Recovery Procedures

1. **Stop the service**
   ```bash
   sudo systemctl stop aimux2
   ```

2. **Restore configuration**
   ```bash
   sudo cp /path/to/backup/config.json /etc/aimux/
   sudo chown aimux:aimux /etc/aimux/config.json
   ```

3. **Start the service**
   ```bash
   sudo systemctl start aimux2
   ```

## Troubleshooting

### Common Issues

#### Service Won't Start

1. **Check configuration syntax**:
   ```bash
   /opt/aimux2/aimux --validate-config
   ```

2. **Check permissions**:
   ```bash
   ls -la /etc/aimux/
   ls -la /var/log/aimux/
   ```

3. **Check systemd logs**:
   ```bash
   sudo journalctl -u aimux2 -n 50
   ```

#### API Connection Issues

1. **Verify network connectivity**:
   ```bash
   curl -I https://api.provider.com
   ```

2. **Check API key validity**:
   - Verify API keys in configuration
   - Test with provider's API tools

3. **Check rate limits**:
   - Monitor provider rate limits
   - Review request timing

#### Performance Issues

1. **Monitor resources**:
   ```bash
   top -p $(pgrep aimux2)
   iostat 1 5
   ```

2. **Check configuration**:
   - Review `max_concurrent_requests`
   - Monitor provider-specific timeouts

3. **Review logs for errors**:
   ```bash
   grep -i "error\|timeout\|failed" /var/log/aimux/aimux.log
   ```

### Debug Mode

Enable debug logging temporarily:

```bash
# Update log level
sudo sed -i 's/"log_level": "warn"/"log_level": "debug"/' /etc/aimux/config.json
sudo systemctl restart aimux2

# Revert to production level after debugging
sudo sed -i 's/"log_level": "debug"/"log_level": "warn"/' /etc/aimux/config.json
```

## Performance Tuning

### Memory Optimization

- **Default**: ~50MB baseline usage
- **Tuning**: Adjust `max_concurrent_requests` based on available memory
- **Monitoring**: Use `/metrics` endpoint to track memory usage

### Request Handling

- **Concurrent Requests**: Adjust `max_concurrent_requests` (default: 1000)
- **Timeouts**: Configure per-provider timeouts based on SLA requirements
- **Rate Limiting**: Set provider-specific rate limits

### Caching

- **Model Caching**: Automatic provider model discovery caching
- **Configuration Caching**: In-memory configuration with auto-reload
- **DNS Caching**: System-level DNS caching for provider endpoints

### Network Optimization

```bash
# System TCP tuning (Linux)
echo 'net.core.somaxconn = 65535' >> /etc/sysctl.conf
echo 'net.ipv4.tcp_max_syn_backlog = 65535' >> /etc/sysctl.conf
echo 'net.ipv4.tcp_fin_timeout = 5' >> /etc/sysctl.conf
sysctl -p
```

## Container Deployment (Optional)

### Dockerfile

```dockerfile
FROM ubuntu:22.04

# Install dependencies
RUN apt-get update && apt-get install -y \
    libssl-dev \
    libcurl4-openssl-dev \
    && rm -rf /var/lib/apt/lists/*

# Copy application
COPY aimux /usr/local/bin/
COPY config.json /etc/aimux/

# Create user
RUN useradd -r -s /bin/false aimux

# Expose ports
EXPOSE 8080 8443

# Health check
HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
  CMD curl -f http://localhost:8080/health || exit 1

USER aimux
CMD ["aimux", "--config=/etc/aimux/config.json"]
```

### Docker Compose

```yaml
version: '3.8'
services:
  aimux2:
    build: .
    ports:
      - "8080:8080"
      - "8443:8443"
    volumes:
      - ./config:/etc/aimux:ro
      - ./data:/var/lib/aimux
      - ./logs:/var/log/aimux
    environment:
      - AIMUX_ENVIRONMENT=production
    restart: unless-stopped
```

## Support

### Getting Help

1. **Documentation**: Check the full documentation at `/docs`
2. **WebUI**: Use the built-in help and diagnostics
3. **Logs**: Review application logs for detailed error information
4. **Community**: GitHub Issues and Discussions

### Reporting Issues

When reporting issues, please include:

- Aimux2 version
- Operating system and version
- Configuration (redacted for security)
- Error logs
- Steps to reproduce
- Expected vs actual behavior

### Version Information

Check your version:
```bash
/opt/aimux2/aimux --version
```

The output includes:
- Version number
- Git commit
- Build date
- Production build status

---

This deployment guide ensures Aimux2 runs optimally in production environments with proper security, monitoring, and operational procedures. For additional details, see the [Configuration Reference](CONFIGURATION.md) and [API Documentation](API.md).