# Aimux2 Troubleshooting Guide

## Overview

This guide provides comprehensive troubleshooting procedures for common issues encountered with Aimux2 deployments.

## Table of Contents

1. [Installation Issues](#installation-issues)
2. [Service Management Problems](#service-management-problems)
3. [Configuration Problems](#configuration-problems)
4. [Provider Connection Issues](#provider-connection-issues)
5. [Performance Problems](#performance-problems)
6. [Security Issues](#security-issues)
7. [WebUI Problems](#webui-problems)
8. [Monitoring and Logging Issues](#monitoring-and-logging-issues)
9. [Network Issues](#network-issues)
10. [Backup and Recovery Issues](#backup-and-recovery-issues)

## Installation Issues

### Build Fails with Missing Dependencies

**Problem**: Build process fails with missing library errors

**Symptoms**:
```
error: 'openssl/ssl.h' file not found
fatal error: curl/curl.h: No such file or directory
```

**Solutions**:

#### Ubuntu/Debian:
```bash
sudo apt update
sudo apt install -y build-essential cmake libssl-dev libcurl4-openssl-dev pkg-config
```

#### CentOS/RHEL:
```bash
sudo yum groupinstall -y "Development Tools"
sudo yum install -y cmake openssl-devel libcurl-devel pkgconfig
```

#### macOS:
```bash
brew install cmake openssl curl
export PKG_CONFIG_PATH="/usr/local/opt/openssl/lib/pkgconfig:$PKG_CONFIG_PATH"
```

### vcpkg Installation Fails

**Problem**: vcpkg cannot fetch dependencies

**Symptoms**:
```
Error: Failed to download package
vcpkg install failed
```

**Solutions**:

1. **Update vcpkg**:
   ```bash
   cd vcpkg
   git pull origin master
   ./bootstrap-vcpkg.sh
   ```

2. **Clear vcpkg cache**:
   ```bash
   rm -rf vcpkg_installed
   ./vcpkg install --force-system-binaries
   ```

3. **Check network connectivity**:
   ```bash
   curl -I https://github.com
   ```

### Production Build Fails

**Problem**: Release build fails with optimization errors

**Solutions**:

```bash
# Try without aggressive optimizations
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=OFF ..

# Check compiler compatibility
gcc --version
clang --version
```

## Service Management Problems

### Service Won't Start

**Problem**: `systemctl start aimux2` fails

**Diagnostic Steps**:

1. **Check service status**:
   ```bash
   sudo systemctl status aimux2
   sudo journalctl -u aimux2 -n 50
   ```

2. **Check configuration**:
   ```bash
   sudo /opt/aimux2/aimux --validate-config
   ```

3. **Check file permissions**:
   ```bash
   ls -la /etc/aimux/
   ls -la /var/log/aimux/
   ls -la /var/lib/aimux/
   ```

**Common Solutions**:

#### Fix Permissions:
```bash
sudo chown -R aimux:aimux /etc/aimux/
sudo chown -R aimux:aimux /var/lib/aimux/
sudo chown -R aimux:aimux /var/log/aimux/
sudo chmod 640 /etc/aimux/config.json
```

#### Fix Configuration:
```bash
# Create default config if missing
sudo mkdir -p /etc/aimux
sudo /opt/aimux2/aimux --create-default-config
```

#### Create Required Directories:
```bash
sudo mkdir -p /var/lib/aimux /var/log/aimux /var/backups/aimux
sudo chown aimux:aimux /var/lib/aimux /var/log/aimux /var/backups/aimux
```

### Service Starts but Crashes Immediately

**Problem**: Service starts then exits with error

**Diagnostic Steps**:

1. **Enable debug logging**:
   ```bash
   sudo sed -i 's/"log_level": "warn"/"log_level": "debug"/' /etc/aimux/config.json
   sudo systemctl restart aimux2
   sudo journalctl -u aimux2 -f
   ```

2. **Run manually**:
   ```bash
   sudo -u aimux /opt/aimux2/aimux --config=/etc/aimux/config.json
   ```

3. **Check core dumps**:
   ```bash
   sudo coredumpctl list
   sudo coredumpctl info aimux
   ```

### Service Auto-restart Loop

**Problem**: Service continuously restarts

**Solutions**:

1. **Check restart count**:
   ```bash
   sudo systemctl status aimux2 | grep "restart"
   ```

2. **Temporarily disable auto-restart**:
   ```bash
   sudo systemctl edit aimux2
   # Add:
   # [Service]
   # Restart=on-failure
   # RestartSec=30
   ```

3. **Check for race conditions**:
   ```bash
   # Check if another instance is running
   sudo lsof -i :8080
   sudo pkill -f aimux
   ```

## Configuration Problems

### Invalid JSON Configuration

**Problem**: Configuration file has JSON syntax errors

**Diagnostic Steps**:

```bash
# Validate JSON syntax
python3 -m json.tool /etc/aimux/config.json
jq . /etc/aimux/config.json
```

**Common Issues**:

#### Trailing Commas:
```json
{
  "providers": [
    {
      "name": "minimax",  // <- Remove comma
    }
  ],
}
```

#### Missing Quotes:
```json
{
  name: "minimax",  // <- Should be "name"
}
```

#### Invalid Booleans:
```json
{
  "enabled": True,  // <- Should be true
}
```

### Configuration Validation Errors

**Problem**: Valid JSON but fails validation

**Diagnostic Command**:
```bash
sudo /opt/aimux2/aimux --validate-full-config
```

**Common Validation Errors**:

#### Invalid Field Values:
```json
{
  "port": 70000,  // Must be 1-65535
  "priority": -1,  // Should be positive
  "timeout_ms": 0  // Should be > 0
}
```

#### Missing Required Fields:
```json
{
  "providers": [
    {
      "name": "minimax",
      // Missing: api_key, endpoint, models
    }
  ]
}
```

### Environment Variables Not Loading

**Problem**: Environment variables not overriding configuration

**Solutions**:

1. **Check environment file permissions**:
   ```bash
   sudo chmod 640 /etc/aimux/.env
   sudo chown aimux:aimux /etc/aimux/.env
   ```

2. **Verify service environment**:
   ```bash
   sudo systemctl show aimux2 --property=Environment
   ```

3. **Load environment manually**:
   ```bash
   set -a
   source /etc/aimux/.env
   /opt/aimux2/aimux
   ```

## Provider Connection Issues

### API Authentication Fails

**Problem**: 401/403 errors from provider APIs

**Diagnostic Steps**:

1. **Test API manually**:
   ```bash
   # Test Minimax (with your actual key)
   curl -H "Authorization: Bearer YOUR_KEY" https://api.minimax.io/heartbeat

   # Test with verbose output
   curl -v -H "Authorization: Bearer YOUR_KEY" https://api.minimax.io/anthropic
   ```

2. **Check API key format**:
   ```bash
   echo "$API_KEY" | wc -c  # Should be 16-256 characters
   echo "$API_KEY" | base64 -d  # Verify no encoding issues
   ```

3. **Review provider-specific requirements**:
   - Minimax: Requires group_id
   - Z.AI: Different endpoint format
   - Cerebras: Specific header requirements

**Solutions**:

#### Rotate API Keys:
```bash
# Update config with new key
sudo nano /etc/aimux/config.json
sudo systemctl restart aimux2
```

#### Use Environment Variables:
```bash
export AIMUX_MINIMAX_API_KEY="new-key-here"
sudo systemctl restart aimux2
```

### Rate Limiting Issues

**Problem**: Too many requests (429) errors

**Diagnostic Steps**:

1. **Check current rate limits**:
   ```bash
   curl http://localhost:8080/metrics | grep rate_limit
   ```

2. **Monitor provider response times**:
   ```bash
   curl http://localhost:8080/profiling | grep providers
   ```

**Solutions**:

#### Adjust Rate Limits:
```json
{
  "providers": [
    {
      "name": "minimax",
      "max_requests_per_minute": 100,  // Increase from 60
    }
  ]
}
```

#### Implement Client-Side Rate Limiting:
```json
{
  "system": {
    "max_concurrent_requests": 500,  // Reduce from 1000
  }
}
```

### Provider Endpoint Changes

**Problem**: Provider updates their API endpoints

**Solutions**:

1. **Update endpoint configuration**:
   ```json
   {
     "providers": [
       {
         "name": "minimax",
         "endpoint": "https://api.new-endpoint.com/anthropic"
       }
     ]
   }
   ```

2. **Test new endpoint**:
   ```bash
   curl -I https://api.new-endpoint.com/anthropic
   ```

3. **Monitor for changes**:
   ```bash
   # Set up endpoint monitoring
   monitor-endpoint.sh https://api.minimax.io/health
   ```

## Performance Problems

### High Memory Usage

**Problem**: Memory usage grows continuously

**Diagnostic Steps**:

1. **Monitor memory usage**:
   ```bash
   # Real-time monitoring
   watch -n 1 'ps -o pid,ppid,cmd,%mem,%cpu --sort=-%mem -C aimux'

   # Memory statistics
   cat /proc/$(pgrep aimux)/status | grep -E "(VmRSS|VmSize|VmPeak)"
   ```

2. **Check for memory leaks**:
   ```bash
   valgrind --leak-check=full --show-leak-kinds=all /opt/aimux2/aimux
   ```

**Solutions**:

#### Optimize Configuration:
```json
{
  "system": {
    "max_concurrent_requests": 500,  // Reduce if memory is limited
  }
}
```

#### Enable Garbage Collection:
```json
{
  "system": {
    "structured_logging": false,  // Disable if not needed
  }
}
```

#### Monitor Memory Metrics:
```bash
# Memory usage chart
curl http://localhost:8080/metrics | grep memory
```

### Slow Response Times

**Problem**: High latency for AI requests

**Diagnostic Steps**:

1. **Check network connectivity**:
   ```bash
   # Test latency to providers
   ping api.minimax.io
   curl -w "@curl-format.txt" https://api.minimax.io/antrhropic

   # curl-format.txt contents:
   #      time_namelookup:  %{time_namelookup}\n
   #         time_connect:  %{time_connect}\n
   #      time_appconnect:  %{time_appconnect}\n
   #     time_pretransfer:  %{time_pretransfer}\n
   #        time_redirect:  %{time_redirect}\n
   #   time_starttransfer:  %{time_starttransfer}\n
   #                      ----------\n
   #           time_total:  %{time_total}\n
   ```

2. **Check provider performance**:
   ```bash
   curl http://localhost:8080/profiling
   ```

**Solutions**:

#### Optimize Timeouts:
```json
{
  "providers": [
    {
      "name": "minimax",
      "timeout_ms": 15000,  // Reduce from 30000
    }
  ]
}
```

#### Use Closer Provider:
```json
{
  "providers": [
    {
      "name": "provider-with-low-latency",
      "priority": 1,  // Higher priority
    }
  ]
}
```

### High CPU Usage

**Problem**: CPU usage spikes or remains high

**Diagnostic Steps**:

1. **Profile CPU usage**:
   ```bash
   # CPU sampling
   perf record -p $(pgrep aimux) -g -- sleep 10
   perf report

   # Top usage
   top -p $(pgrep aimux)
   ```

2. **Check request patterns**:
   ```bash
   curl http://localhost:8080/metrics | grep requests_per_second
   ```

**Solutions**:

#### Limit Concurrent Requests:
```json
{
  "system": {
    "max_concurrent_requests": 100,  // Reduce for CPU-limited systems
  }
}
```

#### Enable Request Caching (if available):
```json
{
  "system": {
    "enable_request_cache": true,
    "cache_ttl_seconds": 300,
  }
}
```

## Security Issues

### SSL/TLS Certificate Problems

**Problem**: HTTPS/SSL configuration fails

**Diagnostic Steps**:

1. **Verify certificate**:
   ```bash
   openssl x509 -in /etc/ssl/aimux/aimux.crt -text -noout
   openssl x509 -in /etc/ssl/aimux/aimux.crt -dates -noout
   ```

2. **Test SSL connection**:
   ```bash
   openssl s_client -connect localhost:8443 -servername localhost
   ```

3. **Check permissions**:
   ```bash
   ls -la /etc/ssl/aimux/
   # Should be: root:root 600 for key, 644 for cert
   ```

**Solutions**:

#### Generate New Certificate:
```bash
sudo mkdir -p /etc/ssl/aimux
cd /etc/ssl/aimux
sudo openssl req -x509 -newkey rsa:4096 -keyout aimux.key -out aimux.crt -days 365 -nodes -subj "/CN=localhost"
sudo chown root:root aimux.key aimux.crt
sudo chmod 600 aimux.key
sudo chmod 644 aimux.crt
```

#### Update Configuration:
```json
{
  "webui": {
    "ssl_enabled": true,
    "cert_file": "/etc/ssl/aimux/aimux.crt",
    "key_file": "/etc/ssl/aimux/aimux.key"
  }
}
```

### API Key Exposure

**Problem**: API keys visible in logs or memory

**Diagnostic Steps**:

1. **Check log files for keys**:
   ```bash
   sudo grep -i "api_key\|token\|secret" /var/log/aimux/aimux.log
   ```

2. **Check memory dumps**:
   ```bash
   sudo strings /proc/$(pgrep aimux)/mem | grep -i "api_key"
   ```

**Solutions**:

#### Enable Key Encryption:
```json
{
  "security": {
    "api_key_encryption": true,
  }
}
```

#### Use Environment Variables:
```bash
# Move keys out of config
unset AIMUX_MINIMAX_API_KEY  # Clear from shell history
export AIMUX_MINIMAX_API_KEY="secure-key-here"
```

#### Sanitize Logs:
```json
{
  "system": {
    "structured_logging": true,  # Better redaction
  }
}
```

### Unauthorized Access

**Problem**: WebUI accessible from unauthorized sources

**Diagnostic Steps**:

1. **Check current access**:
   ```bash
   sudo ss -tulpn | grep :8080
   curl -I http://localhost:8080
   ```

2. **Review firewall rules**:
   ```bash
   sudo ufw status
   sudo iptables -L | grep 8080
   ```

**Solutions**:

#### Restrict Access:
```json
{
  "webui": {
    "cors_origins": ["your-domain.com"],  // Not "*"
  }
}
```

#### Configure Firewall:
```bash
# Allow only specific IP
sudo ufw allow from 192.168.1.0/24 to any port 8080
sudo ufw allow from 192.168.1.0/24 to any port 8443
```

#### Enable HTTPS Only:
```json
{
  "security": {
    "require_https": true,
  },
  "webui": {
    "ssl_enabled": true,
  }
}
```

## WebUI Problems

### WebUI Not Accessible

**Problem**: Browser cannot connect to WebUI

**Diagnostic Steps**:

1. **Check service status**:
   ```bash
   systemctl status aimux2
   curl http://localhost:8080/health
   ```

2. **Check port binding**:
   ```bash
   sudo netstat -tulpn | grep :8080
   sudo lsof -i :8080
   ```

3. **Test locally vs remotely**:
   ```bash
   # Local test
   curl http://localhost:8080

   # Remote test
   curl http://$(hostname -I | awk '{print $1}'):8080
   ```

**Solutions**:

#### Check Configuration:
```json
{
  "webui": {
    "enabled": true,
    "port": 8080,
  }
}
```

#### Check Firewall:
```bash
sudo ufw allow 8080/tcp
sudo firewall-cmd --add-port=8080/tcp --permanent  # firewalld
```

#### Check Network Interface:
```bash
# Binding to specific interface only?
sudo ss -tulpn 'sport = :8080'
```

### WebUI Features Not Working

**Problem**: Some WebUI pages or features don't load

**Diagnostic Steps**:

1. **Check browser console**:
   - Open developer tools (F12)
   - Look for JavaScript errors
   - Check network requests

2. **Test API endpoints directly**:
   ```bash
   curl http://localhost:8080/metrics
   curl http://localhost:8080/provider-status
   ```

3. **Check CORS configuration**:
   ```bash
   curl -H "Origin: https://your-domain.com" \
        -H "Access-Control-Request-Method: GET" \
        -H "Access-Control-Request-Headers: X-Requested-With" \
        -X OPTIONS http://localhost:8080/metrics
   ```

**Solutions**:

#### Fix CORS:
```json
{
  "webui": {
    "cors_enabled": true,
    "cors_origins": ["your-domain.com", "http://localhost:8080"],
  }
}
```

#### Enable Features:
```json
{
  "webui": {
    "api_docs": true,
    "real_time_metrics": true,
  }
}
```

## Monitoring and Logging Issues

### Logs Not Generated

**Problem**: No log files created

**Diagnostic Steps**:

1. **Check log directory**:
   ```bash
   ls -la /var/log/aimux/
   sudo touch /var/log/aimux/test.log  # Test write permissions
   ```

2. **Check log configuration**:
   ```bash
   curl http://localhost:8080/config | grep log
   ```

3. **Check systemd journal**:
   ```bash
   journalctl -u aimux2
   ```

**Solutions**:

#### Create Log Directory:
```bash
sudo mkdir -p /var/log/aimux
sudo chown aimux:adm /var/log/aimux
sudo chmod 750 /var/log/aimux
```

#### Configure Logging:
```json
{
  "system": {
    "log_level": "info",  // Not "off"
    "log_dir": "/var/log/aimux",
  }
}
```

### Log Rotation Not Working

**Problem**: Log files growing too large

**Solutions**:

#### Configure Logrotate:
```bash
sudo nano /etc/logrotate.d/aimux2
```

Contents:
```
/var/log/aimux/*.log {
    daily
    missingok
    rotate 30
    compress
    delaycompress
    notifempty
    create 640 aimux adm
    postrotate
        systemctl reload aimux2 || true
    endscript
}
```

Test:
```bash
sudo logrotate -f /etc/logrotate.d/aimux2
```

#### Configure Size Limits in Aimux2:
```json
{
  "system": {
    "max_log_size_mb": 100,
  }
}
```

## Network Issues

### DNS Resolution Problems

**Problem**: Cannot resolve provider hostnames

**Diagnostic Steps**:

```bash
# Test DNS resolution
nslookup api.minimax.io
dig api.minimax.io
host api.minimax.io

# Test connectivity
curl -v https://api.minimax.io/health
```

**Solutions**:

#### Use IP Addresses:
```json
{
  "providers": [
    {
      "endpoint": "https://1.2.3.4/anthropic",  // Direct IP
    }
  ]
}
```

#### Configure DNS:
```bash
# Use reliable DNS
echo "nameserver 8.8.8.8" | sudo tee /etc/resolv.conf
```

### Connection Timeouts

**Problem**: Requests to providers timeout

**Solutions**:

#### Adjust Timeouts:
```json
{
  "providers": [
    {
      "timeout_ms": 60000,  // Increase to 60 seconds
    }
  ]
}
```

#### Enable Keep-alive:
```json
{
  "network": {
    "keep_alive": true,
    "keep_alive_timeout": 30,
  }
}
```

## Backup and Recovery Issues

### Backup Fails

**Problem**: Automatic backup creation fails

**Diagnostic Steps**:

1. **Check backup directory permissions**:
   ```bash
   ls -la /var/backups/aimux/
   sudo touch /var/backups/aimux/test
   ```

2. **Check disk space**:
   ```bash
   df -h /var/backups/
   ```

3. **Check configuration**:
   ```bash
   curl http://localhost:8080/config | grep backup
   ```

**Solutions**:

#### Create Backup Directory:
```bash
sudo mkdir -p /var/backups/aimux
sudo chown aimux:aimux /var/backups/aimux
sudo chmod 750 /var/backups/aimux
```

#### Configure Backup Settings:
```json
{
  "system": {
    "backup_dir": "/var/backups/aimux",
    "backup_retention_days": 30,
  }
}
```

### Restore Fails

**Problem**: Cannot restore from backup

**Solutions**:

#### Manual Restore:
```bash
# Stop service
sudo systemctl stop aimux2

# Restore configuration
sudo cp /var/backups/aimux/config_latest.json /etc/aimux/config.json

# Restore permissions
sudo chown aimux:aimux /etc/aimux/config.json
sudo chmod 640 /etc/aimux/config.json

# Start service
sudo systemctl start aimux2
```

#### Validate Backup Before Restore:
```bash
/opt/aimux2/aimux --validate-config --config=/var/backups/aimux/config_20231201.json
```

## Emergency Procedures

### Complete Service Recovery

1. **Emergency restart**:
   ```bash
   sudo systemctl restart aimux2
   sudo systemctl status aimux2
   ```

2. **Use last known good configuration**:
   ```bash
   sudo cp /var/backups/aimux/config_latest.json /etc/aimux/config.json
   sudo systemctl restart aimux2
   ```

3. **Factory reset** (last resort):
   ```bash
   # Backup current config
   sudo cp /etc/aimux/config.json /tmp/config_backup.json

   # Generate new config
   sudo /opt/aimux2/aimux --create-default-config

   # Reconfigure providers
   sudo nano /etc/aimux/config.json
   ```

### Security Incident Response

1. **Secure the system**:
   ```bash
   sudo systemctl stop aimux2
   sudo ufw deny 8080
   sudo ufw deny 8443
   ```

2. **Investigate**:
   ```bash
   # Check for unauthorized access
   sudo journalctl -u aimux2 | grep -i "error\|fail\|unauthorized"

   # Check configuration changes
   find /etc/aimux -mtime -1 -ls
   ```

3. **Recover**:
   ```bash
   # Restore from secure backup
   sudo /opt/aimux2/aimux --restore-from-backup /var/backups/aimux/secure_backup.json

   # Rotate API keys
   # Update all provider API keys
   ```

## Getting Help

When the above solutions don't resolve your issue:

### Log Collection

1. **Gather diagnostic information**:
   ```bash
   # Service status
   sudo systemctl status aimux2 > aimux_status.txt

   # Recent logs
   sudo journalctl -u aimux2 -n 1000 > aimux_logs.txt

   # Configuration (redacted)
   curl http://localhost:8080/config > aimux_config.json

   # System info
   uname -a > system_info.txt
   dpkg -l | grep aimux >> system_info.txt  # Ubuntu/Debian
   rpm -qa | grep aimux >> system_info.txt   # RHEL/CentOS
   ```

2. **Create support bundle**:
   ```bash
   tar -czf aimux_support_$(date +%Y%m%d_%H%M%S).tar.gz \
     aimux_status.txt aimux_logs.txt aimux_config.json system_info.txt
   ```

### Report Issues

Include in your issue report:
- Aimux2 version: `/opt/aimux2/aimux --version`
- Operating system and version
- Complete error messages
- Steps to reproduce
- Support bundle (without sensitive data)

---

This troubleshooting guide should resolve most common issues with Aimux2. For additional help, consult the [Deployment Guide](DEPLOYMENT_GUIDE.md) or the [Configuration Reference](CONFIGURATION_REFERENCE.md).