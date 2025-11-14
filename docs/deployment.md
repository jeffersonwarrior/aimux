# Aimux 2.0 - Production Deployment Guide

## 🚀 Overview

This guide covers production deployment of Aimux 2.0 with best practices for security, performance, and reliability.

## 📋 Prerequisites

### System Requirements

| Requirement | Minimum | Recommended |
|-------------|-----------|-------------|
| **OS** | Ubuntu 20.04+ / CentOS 8+ | Ubuntu 22.04 LTS |
| **CPU** | 2 cores | 4+ cores |
| **Memory** | 4 GB RAM | 8 GB+ RAM |
| **Storage** | 20 GB SSD | 50 GB+ SSD |
| **Network** | 100 Mbps | 1 Gbps |
| **Ports** | 8080 (HTTP) | 443 (HTTPS) |

### Software Dependencies

```bash
# Ubuntu/Debian
sudo apt update
sudo apt install -y build-essential cmake gcc-15 g++-15
sudo apt install -y libcurl4-openssl-dev libssl-dev
sudo apt install -y nginx certbot python3-certbot-nginx

# CentOS/RHEL
sudo yum groupinstall -y "Development Tools"
sudo yum install -y cmake gcc15 gcc15-c++
sudo yum install -y libcurl-devel openssl-devel
sudo yum install -y nginx certbot
```

### API Keys Configuration

Ensure you have API keys for all providers:

| Provider | Required | Configuration |
|----------|-----------|----------------|
| **Cerebras** | Yes | `CEREBRAS_API_KEY` |
| **Z.ai** | Yes | `ZAI_API_KEY` |
| **MiniMax** | Yes | `MINIMAX_API_KEY` |
| **Synthetic** | No | For testing only |

## 🔧 Build for Production

### 1. Clone Repository

```bash
git clone https://github.com/aimux/aimux.git
cd aimux
git checkout v2.0  # Production branch
```

### 2. Configure Build

```bash
mkdir build-prod && cd build-prod
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=/opt/aimux \
      ..
```

### 3. Build and Install

```bash
make -j$(nproc)
sudo make install
```

### 4. Verify Installation

```bash
/opt/aimux/bin/aimux --version
```

## ⚙️ Production Configuration

### 1. Generate Production Config

```bash
cd /opt/aimux
./bin/aimux --prod
```

This creates `production-config.json` with optimal settings.

### 2. Customize Configuration

Edit `/opt/aimux/etc/production-config.json`:

```json
{
  "system": {
    "environment": "production",
    "log_level": "warn",
    "structured_logging": true,
    "max_concurrent_requests": 1000,
    "working_directory": "/var/lib/aimux",
    "pid_file": "/var/run/aimux.pid"
  },
  "security": {
    "api_key_encryption": true,
    "rate_limiting": true,
    "ssl_verification": true,
    "input_validation": true,
    "audit_logging": true
  },
  "providers": {
    "cerebras": {
      "endpoint": "https://api.cerebras.ai",
      "enabled": true,
      "max_requests_per_minute": 300,
      "timeout_ms": 30000,
      "retry_attempts": 3,
      "priority": 1,
      "api_key": "${CEREBRAS_API_KEY}"
    },
    "zai": {
      "endpoint": "https://api.z.ai",
      "enabled": true,
      "max_requests_per_minute": 200,
      "timeout_ms": 25000,
      "retry_attempts": 3,
      "priority": 2,
      "api_key": "${ZAI_API_KEY}"
    }
  },
  "load_balancing": {
    "strategy": "adaptive",
    "health_check_interval": 30,
    "failover_enabled": true,
    "retry_on_failure": true,
    "circuit_breaker": {
      "enabled": true,
      "failure_threshold": 5,
      "recovery_timeout": 60
    }
  },
  "webui": {
    "enabled": true,
    "port": 8080,
    "ssl_enabled": true,
    "cors_enabled": true,
    "api_docs": true,
    "real_time_metrics": true
  },
  "monitoring": {
    "metrics_collection": true,
    "health_checks": true,
    "performance_alerts": true,
    "dashboard_enabled": true,
    "alert_thresholds": {
      "max_response_time_ms": 1000,
      "min_success_rate": 0.99,
      "max_error_rate": 0.01
    },
    "webhooks": {
      "alert_webhook": "https://your-monitoring.com/webhooks",
      "events": ["provider_failure", "high_latency", "rate_limit_exceeded"]
    }
  }
}
```

### 3. Environment Variables

Create `/opt/aimux/.env`:

```bash
# API Keys
CEREBRAS_API_KEY=your_cerebras_api_key_here
ZAI_API_KEY=your_zai_api_key_here
MINIMAX_API_KEY=your_minimax_api_key_here

# System
AIMUX_ENV=production
AIMUX_LOG_LEVEL=warn

# Database (if needed)
AIMUX_DB_HOST=localhost
AIMUX_DB_PORT=5432
AIMUX_DB_NAME=aimux
AIMUX_DB_USER=aimux
AIMUX_DB_PASSWORD=secure_password_here
```

Set permissions:

```bash
chmod 600 /opt/aimux/.env
chown aimux:aimux /opt/aimux/.env
```

## 🔐 Security Configuration

### 1. Create System User

```bash
sudo useradd -r -s /bin/false -d /var/lib/aimux aimux
sudo mkdir -p /var/lib/aimux /var/log/aimux
sudo chown -R aimux:aimux /opt/aimux /var/lib/aimux /var/log/aimux
sudo chmod 750 /var/lib/aimux /var/log/aimux
```

### 2. SSL/TLS Certificate

#### Option A: Let's Encrypt (Recommended)

```bash
sudo certbot --nginx -d your-domain.com
```

#### Option B: Self-Signed

```bash
sudo mkdir -p /etc/aimux/ssl
sudo openssl req -x509 -nodes -days 365 -newkey rsa:2048 \
    -keyout /etc/aimux/ssl/privkey.pem \
    -out /etc/aimux/ssl/cert.pem
sudo chmod 600 /etc/aimux/ssl/privkey.pem
```

### 3. Firewall Configuration

```bash
sudo ufw allow 22/tcp    # SSH
sudo ufw allow 80/tcp    # HTTP
sudo ufw allow 443/tcp   # HTTPS
sudo ufw enable
```

### 4. Systemd Service

Create `/etc/systemd/system/aimux.service`:

```ini
[Unit]
Description=Aimux 2.0 AI Service Router
Documentation=https://docs.aimux.com
After=network.target
Wants=network.target

[Service]
Type=simple
User=aimux
Group=aimux
WorkingDirectory=/var/lib/aimux
EnvironmentFile=/opt/aimux/.env
ExecStart=/opt/aimux/bin/aimux --config /opt/aimux/etc/production-config.json --daemon
ExecReload=/bin/kill -HUP $MAINPID
Restart=always
RestartSec=10
StandardOutput=append:/var/log/aimux/aimux.log
StandardError=append:/var/log/aimux/aimux.error
SyslogIdentifier=aimux

# Security
NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=strict
ProtectHome=true
ReadWritePaths=/var/lib/aimux /var/log/aimux

# Resource limits
LimitNOFILE=65536
LimitNPROC=4096

[Install]
WantedBy=multi-user.target
```

Enable and start:

```bash
sudo systemctl daemon-reload
sudo systemctl enable aimux
sudo systemctl start aimux
```

## 🌐 Nginx Reverse Proxy

### Configuration

Create `/etc/nginx/sites-available/aimux`:

```nginx
upstream aimux_backend {
    server 127.0.0.1:8080;
    keepalive 32;
}

server {
    listen 80;
    server_name your-domain.com;
    return 301 https://$server_name$request_uri;
}

server {
    listen 443 ssl http2;
    server_name your-domain.com;

    # SSL Configuration
    ssl_certificate /etc/letsencrypt/live/your-domain.com/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/your-domain.com/privkey.pem;
    ssl_protocols TLSv1.2 TLSv1.3;
    ssl_ciphers ECDHE-RSA-AES256-GCM-SHA512:DHE-RSA-AES256-GCM-SHA512;
    ssl_prefer_server_ciphers off;

    # Security Headers
    add_header X-Frame-Options DENY;
    add_header X-Content-Type-Options nosniff;
    add_header X-XSS-Protection "1; mode=block";
    add_header Strict-Transport-Security "max-age=63072000; includeSubDomains; preload";

    # Rate Limiting
    limit_req_zone $binary_remote_addr zone=aimux_api:10m rate=10r/s;
    limit_req zone=aimux_api burst=20 nodelay;

    # Main Location
    location / {
        proxy_pass http://aimux_backend;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection 'upgrade';
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
        proxy_cache_bypass $http_upgrade;
        
        # Timeouts
        proxy_connect_timeout 60s;
        proxy_send_timeout 60s;
        proxy_read_timeout 60s;
    }

    # API Rate Limiting
    location /api/ {
        limit_req zone=aimux_api burst=50 nodelay;
        proxy_pass http://aimux_backend;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
    }

    # WebSocket Support
    location /ws {
        proxy_pass http://aimux_backend;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "upgrade";
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
    }

    # Health Check
    location /health {
        access_log off;
        proxy_pass http://aimux_backend;
    }

    # Logging
    access_log /var/log/nginx/aimux.access.log;
    error_log /var/log/nginx/aimux.error.log;
}
```

Enable site:

```bash
sudo ln -s /etc/nginx/sites-available/aimux /etc/nginx/sites-enabled/
sudo nginx -t
sudo systemctl reload nginx
```

## 📊 Monitoring Setup

### 1. Prometheus Metrics

Create `/opt/aimux/etc/prometheus.yml`:

```yaml
global:
  scrape_interval: 15s

scrape_configs:
  - job_name: 'aimux'
    static_configs:
      - targets: ['localhost:8080']
    metrics_path: '/metrics'
    scrape_interval: 5s
```

### 2. Grafana Dashboard

Import Aimux dashboard template:
- Dashboard ID: `aimux-production`
- Metrics: Prometheus data source
- Auto-refresh: 30s

### 3. Alert Manager

Create `/opt/aimux/etc/alertmanager.yml`:

```yaml
global:
  smtp_smarthost: 'localhost:587'
  smtp_from: 'alerts@your-domain.com'

route:
  group_by: ['alertname']
  group_wait: 10s
  group_interval: 10s
  repeat_interval: 1h
  receiver: 'web.hook'

receivers:
  - name: 'web.hook'
    email_configs:
      - to: 'admin@your-domain.com'
        subject: 'Aimux Alert: {{ .GroupLabels.alertname }}'
        body: |
          {{ range .Alerts }}
          Alert: {{ .Annotations.summary }}
          Description: {{ .Annotations.description }}
          {{ end }}
```

## 🚦 Health Checks

### 1. System Health Script

Create `/opt/aimux/bin/health-check.sh`:

```bash
#!/bin/bash

# Check if Aimux is running
if ! systemctl is-active --quiet aimux; then
    echo "ERROR: Aimux service is not running"
    exit 1
fi

# Check API health
if ! curl -s -f http://localhost:8080/health > /dev/null; then
    echo "ERROR: Aimux API health check failed"
    exit 1
fi

# Check log file
if [ ! -f "/var/log/aimux/aimux.log" ]; then
    echo "ERROR: Log file not found"
    exit 1
fi

# Check recent errors
ERROR_COUNT=$(journalctl -u aimux --since "1 hour ago" | grep -i error | wc -l)
if [ "$ERROR_COUNT" -gt 10 ]; then
    echo "WARNING: High error rate: $ERROR_COUNT errors in last hour"
fi

echo "OK: All health checks passed"
exit 0
```

### 2. Cron Job

```bash
sudo crontab -e
# Add this line for health check every 5 minutes
*/5 * * * * /opt/aimux/bin/health-check.sh
```

## 🔄 Deployment Process

### 1. Pre-Deployment Checklist

```bash
# Backup current configuration
sudo cp /opt/aimux/etc/production-config.json /opt/aimux/etc/production-config.json.backup

# Run health checks
/opt/aimux/bin/aimux --prod | grep "SYSTEM READY FOR PRODUCTION"

# Test new binary
sudo systemctl stop aimux
sudo systemctl start aimux
sleep 10
/opt/aimux/bin/health-check.sh
```

### 2. Deployment Steps

```bash
# 1. Maintenance mode (optional)
sudo systemctl stop nginx
sudo systemctl stop aimux

# 2. Backup
sudo tar -czf /opt/aimux/backups/backup-$(date +%Y%m%d-%H%M%S).tar.gz \
    /opt/aimux/bin /opt/aimux/etc /var/lib/aimux

# 3. Update binary
sudo cp /path/to/new/aimux /opt/aimux/bin/aimux
sudo chown aimux:aimux /opt/aimux/bin/aimux
sudo chmod 750 /opt/aimux/bin/aimux

# 4. Update configuration
sudo cp /opt/aimux/etc/production-config.json.new \
        /opt/aimux/etc/production-config.json

# 5. Start services
sudo systemctl start aimux
sudo systemctl start nginx

# 6. Verify deployment
sleep 30
sudo systemctl status aimux
curl -s -f https://your-domain.com/health
```

### 3. Post-Deployment Verification

```bash
# Check service status
sudo systemctl status aimux nginx

# Check logs
sudo journalctl -u aimux -f --since "1 minute ago"

# Test API
curl -X POST https://your-domain.com/chat/completions \
  -H "Authorization: Bearer YOUR_API_KEY" \
  -d '{"model":"test","messages":[{"role":"user","content":"test"}]}'

# Check metrics
curl -s https://your-domain.com/metrics | jq '.performance'
```

## 🔧 Troubleshooting

### Common Issues

#### Service Won't Start

```bash
# Check logs
sudo journalctl -u aimux -f

# Check configuration
sudo -u aimux /opt/aimux/bin/aimux --config /opt/aimux/etc/production-config.json --test

# Check permissions
ls -la /opt/aimux/
ls -la /var/lib/aimux/
```

#### High Memory Usage

```bash
# Check memory usage
ps aux | grep aimux
sudo systemctl status aimux

# Adjust limits in service file
sudo systemctl edit aimux
# Add:
# [Service]
# MemoryLimit=2G
```

#### Connection Issues

```bash
# Test provider connectivity
curl -v -H "Authorization: Bearer YOUR_KEY" https://api.cerebras.ai/v1/chat/completions

# Check firewall
sudo ufw status
sudo iptables -L

# Check SSL
openssl s_client -connect api.cerebras.ai:443
```

## 📈 Performance Tuning

### 1. System Optimization

```bash
# Increase file limits
echo "* soft nofile 65536" >> /etc/security/limits.conf
echo "* hard nofile 65536" >> /etc/security/limits.conf

# Optimize network
echo "net.core.somaxconn = 65536" >> /etc/sysctl.conf
echo "net.ipv4.tcp_max_syn_backlog = 65536" >> /etc/sysctl.conf
sysctl -p
```

### 2. Application Tuning

Adjust configuration values based on load testing:

```json
{
  "system": {
    "max_concurrent_requests": 2000,
    "worker_threads": 8
  },
  "providers": {
    "cerebras": {
      "max_requests_per_minute": 500,
      "timeout_ms": 45000
    }
  },
  "monitoring": {
    "alert_thresholds": {
      "max_response_time_ms": 1500,
      "min_success_rate": 0.95
    }
  }
}
```

## 🎯 Production Best Practices

### 1. Security

- ✅ Use HTTPS with valid certificates
- ✅ Rotate API keys regularly
- ✅ Monitor failed authentication attempts
- ✅ Implement rate limiting
- ✅ Keep software updated

### 2. Reliability

- ✅ Set up health monitoring
- ✅ Configure automatic failover
- ✅ Implement proper logging
- ✅ Test disaster recovery
- ✅ Use circuit breakers

### 3. Performance

- ✅ Monitor response times
- ✅ Optimize database queries
- ✅ Use connection pooling
- ✅ Implement caching
- ✅ Profile bottlenecks

### 4. Operations

- ✅ Automated deployments
- ✅ Configuration management
- ✅ Monitoring and alerting
- ✅ Backup procedures
- ✅ Documentation maintenance

---

## 📞 Support

For production deployment support:

- **Documentation**: https://docs.aimux.com
- **Issues**: https://github.com/aimux/aimux/issues
- **Community**: https://community.aimux.com
- **Support**: support@aimux.com

---

**Last Updated**: 2025-11-13  
**Version**: Aimux 2.0 Production Guide  
**Compatibility**: Ubuntu 20.04+, CentOS 8+, Debian 10+