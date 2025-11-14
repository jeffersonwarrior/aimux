#!/bin/bash

# Aimux v2.0.0 Deployment Script
# This script handles the complete deployment of Aimux with Docker, systemd, and monitoring

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
SERVICE_NAME="aimux"
SERVICE_USER="aimux"
INSTALL_DIR="/opt/aimux"
CONFIG_DIR="/etc/aimux"
LOG_DIR="/var/log/aimux"
DATA_DIR="/var/lib/aimux"

# Logging functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if running as root
check_root() {
    if [[ $EUID -ne 0 ]]; then
        log_error "This script must be run as root"
        exit 1
    fi
}

# Create system user
create_user() {
    log_info "Creating system user: $SERVICE_USER"
    if ! id "$SERVICE_USER" &>/dev/null; then
        useradd -r -s /bin/false -d "$DATA_DIR" "$SERVICE_USER"
        log_success "User $SERVICE_USER created"
    else
        log_warning "User $SERVICE_USER already exists"
    fi
}

# Create directories
create_directories() {
    log_info "Creating directories"
    mkdir -p "$INSTALL_DIR"/{bin,scripts}
    mkdir -p "$CONFIG_DIR"
    mkdir -p "$LOG_DIR"
    mkdir -p "$DATA_DIR"/{cache,backups}
    mkdir -p "/var/backups/aimux"

    # Set permissions
    chown -R "$SERVICE_USER:$SERVICE_USER" "$DATA_DIR" "$LOG_DIR"
    chmod 755 "$CONFIG_DIR"

    log_success "Directories created"
}

# Install binaries
install_binaries() {
    log_info "Installing binaries"

    # Copy binaries
    cp "$PROJECT_ROOT/aimux" "$INSTALL_DIR/bin/" 2>/dev/null || {
        log_error "aimux binary not found. Please build the project first."
        exit 1
    }
    cp "$PROJECT_ROOT/claude_gateway" "$INSTALL_DIR/bin/" 2>/dev/null || {
        log_warning "claude_gateway binary not found"
    }

    # Set permissions
    chmod 755 "$INSTALL_DIR/bin/"*
    chown root:root "$INSTALL_DIR/bin/"*

    log_success "Binaries installed"
}

# Install configuration
install_config() {
    log_info "Installing configuration"

    # Copy production config
    if [[ -f "$PROJECT_ROOT/production-config.json" ]]; then
        cp "$PROJECT_ROOT/production-config.json" "$CONFIG_DIR/config.json"
    elif [[ -f "$PROJECT_ROOT/config.json" ]]; then
        cp "$PROJECT_ROOT/config.json" "$CONFIG_DIR/config.json"
    else
        log_warning "No configuration file found, creating default"
        cat > "$CONFIG_DIR/config.json" << 'EOF'
{
  "server": {
    "port": 8080,
    "bind_address": "127.0.0.1",
    "workers": 4
  },
  "logging": {
    "level": "info",
    "file": "/var/log/aimux/aimux.log",
    "rotation": true,
    "max_size": "100MB"
  },
  "providers": [
    {
      "name": "synthetic",
      "endpoint": "https://synthetic.ai",
      "api_key": "synthetic-key",
      "models": ["synthetic-1"],
      "enabled": true
    }
  ]
}
EOF
    fi

    # Set permissions
    chown "$SERVICE_USER:$SERVICE_USER" "$CONFIG_DIR/config.json"
    chmod 640 "$CONFIG_DIR/config.json"

    log_success "Configuration installed"
}

# Install systemd service
install_service() {
    log_info "Installing systemd service"

    # Copy service file
    cp "$PROJECT_ROOT/aimux.service" "/etc/systemd/system/"

    # Reload systemd
    systemctl daemon-reload

    # Enable service
    systemctl enable "$SERVICE_NAME"

    log_success "Systemd service installed and enabled"
}

# Install logrotate configuration
install_logrotate() {
    log_info "Installing logrotate configuration"

    cat > "/etc/logrotate.d/aimux" << EOF
$LOG_DIR/*.log {
    daily
    missingok
    rotate 30
    compress
    delaycompress
    notifempty
    create 640 $SERVICE_USER $SERVICE_USER
    postrotate
        systemctl reload aimux || true
    endscript
}
EOF

    log_success "Logrotate configuration installed"
}

# Setup monitoring
setup_monitoring() {
    log_info "Setting up monitoring"

    # Create monitoring directories
    mkdir -p "$PROJECT_ROOT/monitoring"/{prometheus,grafana/{dashboards,datasources}}

    # Create Prometheus configuration
    cat > "$PROJECT_ROOT/monitoring/prometheus.yml" << EOF
global:
  scrape_interval: 15s
  evaluation_interval: 15s

scrape_configs:
  - job_name: 'aimux'
    static_configs:
      - targets: ['localhost:8081']
    metrics_path: '/metrics'
    scrape_interval: 5s

  - job_name: 'node'
    static_configs:
      - targets: ['localhost:9100']
EOF

    # Create Grafana datasource
    cat > "$PROJECT_ROOT/monitoring/grafana/datasources/prometheus.yml" << EOF
apiVersion: 1

datasources:
  - name: Prometheus
    type: prometheus
    access: proxy
    url: http://prometheus:9090
    isDefault: true
EOF

    log_success "Monitoring configuration created"
}

# Docker deployment
deploy_docker() {
    log_info "Deploying with Docker"

    # Build image
    cd "$PROJECT_ROOT"
    docker build -t aimux:2.0.0 .

    # Create Docker network
    docker network create aimux-network 2>/dev/null || true

    # Run containers
    if command -v docker-compose &>/dev/null; then
        docker-compose up -d
    else
        log_warning "docker-compose not found, using individual commands"

        # Start Redis
        docker run -d --name aimux-redis \
            --network aimux-network \
            -p 6379:6379 \
            redis:7-alpine

        # Start Aimux
        docker run -d --name aimux-app \
            --network aimux-network \
            -p 8080:8080 \
            -p 8081:8081 \
            -v "$CONFIG_DIR:/app/config:ro" \
            -v "$LOG_DIR:/app/logs" \
            aimux:2.0.0
    fi

    log_success "Docker deployment completed"
}

# Systemd deployment
deploy_systemd() {
    log_info "Deploying with systemd"

    create_user
    create_directories
    install_binaries
    install_config
    install_service
    install_logrotate
    setup_monitoring

    # Start service
    systemctl start "$SERVICE_NAME"
    systemctl status "$SERVICE_NAME" --no-pager

    log_success "Systemd deployment completed"
}

# Start services
start_services() {
    log_info "Starting Aimux services"

    if systemctl is-active --quiet "$SERVICE_NAME"; then
        log_warning "Service is already running"
        return
    fi

    systemctl start "$SERVICE_NAME"

    # Wait for service to start
    for i in {1..30}; do
        if systemctl is-active --quiet "$SERVICE_NAME"; then
            log_success "Aimux service started successfully"
            return
        fi
        sleep 1
    done

    log_error "Failed to start Aimux service"
    systemctl status "$SERVICE_NAME" --no-pager
    exit 1
}

# Verify deployment
verify_deployment() {
    log_info "Verifying deployment"

    # Check service status
    if systemctl is-active --quiet "$SERVICE_NAME"; then
        log_success "✓ Service is running"
    else
        log_error "✗ Service is not running"
        return 1
    fi

    # Check API endpoint
    if curl -f "http://localhost:8080/health" &>/dev/null; then
        log_success "✓ API endpoint is responding"
    else
        log_warning "✗ API endpoint is not responding (may need time to start)"
    fi

    # Check logs
    if [[ -f "$LOG_DIR/aimux.log" ]]; then
        log_success "✓ Log file is being created"
    else
        log_warning "✗ Log file not found"
    fi

    log_success "Deployment verification completed"
}

# Show usage
show_usage() {
    cat << EOF
Usage: $0 [OPTIONS] COMMAND

Commands:
    docker      Deploy using Docker and docker-compose
    systemd     Deploy using systemd service
    start       Start Aimux service
    stop        Stop Aimux service
    restart     Restart Aimux service
    status      Show service status
    logs        Show service logs
    verify      Verify deployment
    help        Show this help message

Examples:
    $0 systemd          # Deploy with systemd
    $0 docker           # Deploy with Docker
    $0 start            # Start services
    $0 logs             # View logs
EOF
}

# Main script logic
main() {
    case "${1:-help}" in
        "docker")
            check_root
            deploy_docker
            ;;
        "systemd")
            check_root
            deploy_systemd
            start_services
            verify_deployment
            ;;
        "start")
            check_root
            start_services
            ;;
        "stop")
            check_root
            systemctl stop "$SERVICE_NAME"
            log_success "Aimux service stopped"
            ;;
        "restart")
            check_root
            systemctl restart "$SERVICE_NAME"
            log_success "Aimux service restarted"
            ;;
        "status")
            systemctl status "$SERVICE_NAME" --no-pager
            ;;
        "logs")
            journalctl -u "$SERVICE_NAME" -f --no-pager
            ;;
        "verify")
            verify_deployment
            ;;
        "help"|"--help"|"-h")
            show_usage
            ;;
        *)
            log_error "Unknown command: $1"
            show_usage
            exit 1
            ;;
    esac
}

# Run main function with all arguments
main "$@"