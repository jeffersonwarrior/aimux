#!/bin/bash

# Aimux2 Service Management Script
# Provides easy commands to manage the aimux2 service

set -e

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
SERVICE_NAME="aimux2"
SERVICE_USER="aimux"
CONFIG_DIR="/etc/aimux"
LOG_DIR="/var/log/aimux"

# Detect OS and service type
OS="$(uname -s)"
case "$OS" in
    Linux*)     SERVICE_TYPE="systemd";;
    Darwin*)    SERVICE_TYPE="launchd";;
    *)          echo -e "${RED}Unsupported OS: $OS${NC}"; exit 1;;
esac

# Function to show usage
show_usage() {
    echo -e "${BLUE}Aimux2 Service Management${NC}"
    echo
    echo "Usage: $0 {start|stop|restart|status|logs|enable|disable|config|health|backup|version}"
    echo
    echo "Commands:"
    echo "  start    - Start the aimux2 service"
    echo "  stop     - Stop the aimux2 service"
    echo "  restart  - Restart the aimux2 service"
    echo "  status   - Show service status"
    echo "  logs     - Show service logs"
    echo "  enable   - Enable service auto-start"
    echo "  disable  - Disable service auto-start"
    echo "  config   - Show/edit configuration"
    echo "  health   - Check service health"
    echo "  backup   - Backup configuration and data"
    echo "  version  - Show aimux2 version"
}

# Function to run command with privilege check
run_with_privilege() {
    if [[ $EUID -ne 0 ]]; then
        echo -e "${YELLOW}This command requires root privileges. Using sudo...${NC}"
        sudo "$@"
    else
        "$@"
    fi
}

# Systemd functions
systemd_start() {
    run_with_privilege systemctl start "$SERVICE_NAME"
    echo -e "${GREEN}Service started${NC}"
}

systemd_stop() {
    run_with_privilege systemctl stop "$SERVICE_NAME"
    echo -e "${GREEN}Service stopped${NC}"
}

systemd_restart() {
    run_with_privilege systemctl restart "$SERVICE_NAME"
    echo -e "${GREEN}Service restarted${NC}"
}

systemd_status() {
    run_with_privilege systemctl status "$SERVICE_NAME"
}

systemd_logs() {
    if command -v journalctl &> /dev/null; then
        if [[ "$1" == "-f" ]]; then
            journalctl -u "$SERVICE_NAME" -f
        else
            journalctl -u "$SERVICE_NAME" --no-pager -n 100
        fi
    else
        tail -n 100 "$LOG_DIR/aimux.log"
    fi
}

systemd_enable() {
    run_with_privilege systemctl enable "$SERVICE_NAME"
    echo -e "${GREEN}Service enabled for auto-start${NC}"
}

systemd_disable() {
    run_with_privilege systemctl disable "$SERVICE_NAME"
    echo -e "${GREEN}Service disabled from auto-start${NC}"
}

# Launchd functions
launchd_start() {
    run_with_privilege launchctl load -w "/Library/LaunchDaemons/com.aimux.daemon.plist"
    echo -e "${GREEN}Service started${NC}"
}

launchd_stop() {
    run_with_privilege launchctl unload "/Library/LaunchDaemons/com.aimux.daemon.plist"
    echo -e "${GREEN}Service stopped${NC}"
}

launchd_restart() {
    launchd_stop
    sleep 2
    launchd_start
}

launchd_status() {
    if launchctl list | grep -q "com.aimux.daemon"; then
        echo -e "${GREEN}Service is running${NC}"
        launchctl list | grep "com.aimux.daemon"
    else
        echo -e "${RED}Service is not running${NC}"
    fi
}

launchd_logs() {
    local log_file="$LOG_DIR/aimux.log"
    if [[ -f "$log_file" ]]; then
        if [[ "$1" == "-f" ]]; then
            tail -f "$log_file"
        else
            tail -n 100 "$log_file"
        fi
    else
        echo -e "${YELLOW}Log file not found: $log_file${NC}"
    fi
}

# Generic functions
show_config() {
    echo -e "${BLUE}Configuration files:${NC}"
    echo "Main config:   $CONFIG_DIR/config.json"
    echo "Environment:   $CONFIG_DIR/.env"

    if [[ -f "$CONFIG_DIR/config.json" ]]; then
        echo -e "\n${GREEN}Current configuration:${NC}"
        run_with_privilege cat "$CONFIG_DIR/config.json" | head -20
        echo "... (truncated)"
    fi
}

check_health() {
    echo -e "${BLUE}Service Health Check${NC}"
    echo

    # Check service status
    echo "Service status:"
    if [[ "$SERVICE_TYPE" == "systemd" ]]; then
        systemctl is-active "$SERVICE_NAME" >/dev/null 2>&1 && echo -e "  ✓ Running" || echo -e "  ✗ Stopped"
    else
        launchd_status
    fi

    # Check configuration
    echo "Configuration:"
    [[ -f "$CONFIG_DIR/config.json" ]] && echo -e "  ✓ Config file exists" || echo -e "  ✗ Config file missing"
    [[ -r "$CONFIG_DIR/config.json" ]] && echo -e "  ✓ Config file readable" || echo -e "  ✗ Config file not readable"

    # Check directories
    echo "Directories:"
    [[ -d "/var/lib/aimux" ]] && echo -e "  ✓ Working directory exists" || echo -e "  ✗ Working directory missing"
    [[ -d "$LOG_DIR" ]] && echo -e "  ✓ Log directory exists" || echo -e "  ✗ Log directory missing"

    # Check WebUI
    echo "WebUI:"
    if curl -s http://localhost:8080/health >/dev/null 2>&1; then
        echo -e "  ✓ WebUI responding"
    else
        echo -e "  ✗ WebUI not responding"
    fi

    # Check recent errors in logs
    local error_count=$(grep -c -i "error\|exception\|failed" "$LOG_DIR/aimux.log" 2>/dev/null | head -1)
    if [[ "$error_count" -gt 0 ]]; then
        echo "Recent errors: $error_count"
    else
        echo -e "  ✓ No recent errors"
    fi

    echo
}

backup_config() {
    local backup_dir="/var/backups/aimux"
    local backup_file="$backup_dir/aimux-backup-$(date +%Y%m%d-%H%M%S).tar.gz"

    echo -e "${BLUE}Creating backup...${NC}"

    mkdir -p "$backup_dir"
    run_with_privilege tar -czf "$backup_file" \
        "$CONFIG_DIR/config.json" "$CONFIG_DIR/.env" \
        "/var/lib/aimux" 2>/dev/null || true

    echo -e "${GREEN}Backup created: $backup_file${NC}"

    # Clean old backups (keep last 10)
    run_with_privilege find "$backup_dir" -name "aimux-backup-*.tar.gz" -mtime +30 -delete 2>/dev/null || true
}

show_version() {
    local binary="/opt/aimux2/aimux"
    if [[ -f "$binary" ]]; then
        echo -e "${BLUE}Aimux2 Version Information:${NC}"
        "$binary" --version 2>/dev/null || echo "Version: unknown"
        echo "Binary location: $binary"
        echo "Build date: $(stat -c %y "$binary" 2>/dev/null || stat -f %Sm "$binary" 2>/dev/null)"
    else
        echo -e "${RED}Aimux2 binary not found at $binary${NC}"
    fi
}

# Command routing
case "$1" in
    start)
        echo -e "${BLUE}Starting aimux2 service...${NC}"
        [[ "$SERVICE_TYPE" == "systemd" ]] && systemd_start || launchd_start
        ;;
    stop)
        echo -e "${BLUE}Stopping aimux2 service...${NC}"
        [[ "$SERVICE_TYPE" == "systemd" ]] && systemd_stop || launchd_stop
        ;;
    restart)
        echo -e "${BLUE}Restarting aimux2 service...${NC}"
        [[ "$SERVICE_TYPE" == "systemd" ]] && systemd_restart || launchd_restart
        ;;
    status)
        echo -e "${BLUE}Aimux2 service status:${NC}"
        [[ "$SERVICE_TYPE" == "systemd" ]] && systemd_status || launchd_status
        ;;
    logs)
        echo -e "${BLUE}Aimux2 service logs:${NC}"
        [[ "$SERVICE_TYPE" == "systemd" ]] && systemd_logs "$2" || launchd_logs "$2"
        ;;
    enable)
        echo -e "${BLUE}Enabling aimux2 service...${NC}"
        [[ "$SERVICE_TYPE" == "systemd" ]] && systemd_enable || echo -e "${YELLOW}Auto-start not available on macOS${NC}"
        ;;
    disable)
        echo -e "${BLUE}Disabling aimux2 service...${NC}"
        [[ "$SERVICE_TYPE" == "systemd" ]] && systemd_disable || echo -e "${YELLOW}Auto-start not available on macOS${NC}"
        ;;
    config)
        show_config
        ;;
    health)
        check_health
        ;;
    backup)
        backup_config
        ;;
    version)
        show_version
        ;;
    *)
        show_usage
        exit 1
        ;;
esac