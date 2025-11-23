#!/bin/bash

# Aimux2 Service Installation Script
# Installs aimux2 as a system service with proper permissions and configuration

set -e

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Constants
SERVICE_NAME="aimux2"
SERVICE_USER="aimux"
SERVICE_GROUP="aimux"
INSTALL_PREFIX="/opt/aimux2"
CONFIG_DIR="/etc/aimux"
VAR_DIR="/var/lib/aimux"
LOG_DIR="/var/log/aimux"

# Detect OS
OS="$(uname -s)"
case "$OS" in
    Linux*)     SERVICE_TYPE="systemd";;
    Darwin*)    SERVICE_TYPE="launchd";;
    *)          echo -e "${RED}Unsupported OS: $OS${NC}"; exit 1;;
esac

echo -e "${BLUE}=== Aimux2 Service Installation ===${NC}"
echo "OS: $OS"
echo "Service Type: $SERVICE_TYPE"
echo "Install Prefix: $INSTALL_PREFIX"
echo

# Function to check if running as root
check_root() {
    if [[ $EUID -ne 0 ]]; then
        echo -e "${RED}This script must be run as root${NC}"
        exit 1
    fi
}

# Function to create system user
create_user() {
    echo -e "${BLUE}Creating service user...${NC}"

    if id "$SERVICE_USER" &>/dev/null; then
        echo -e "${YELLOW}User $SERVICE_USER already exists${NC}"
    else
        if [[ "$SERVICE_TYPE" == "systemd" ]]; then
            useradd --system --no-create-home --shell /usr/sbin/nologin --user-group "$SERVICE_USER"
        else
            # macOS
            sysadminctl -addUser "$SERVICE_USER" -shell /usr/bin/false -home /var/empty
        fi
        echo -e "${GREEN}Created user: $SERVICE_USER${NC}"
    fi
}

# Function to create directories
create_directories() {
    echo -e "${BLUE}Creating directories...${NC}"

    mkdir -p "$CONFIG_DIR"
    mkdir -p "$VAR_DIR"
    mkdir -p "$LOG_DIR"
    mkdir -p "/var/backups/aimux"

    # Set ownership and permissions
    chown -R "$SERVICE_USER:$SERVICE_GROUP" "$CONFIG_DIR" "$VAR_DIR" "$LOG_DIR" "/var/backups/aimux"
    chmod 755 "$CONFIG_DIR" "$VAR_DIR"
    chmod 750 "$LOG_DIR"
    chmod 750 "/var/backups/aimux"

    echo -e "${GREEN}Directories created and configured${NC}"
}

# Function to install binary
install_binary() {
    echo -e "${BLUE}Installing binary...${NC}"

    mkdir -p "$INSTALL_PREFIX"
    cp aimux "$INSTALL_PREFIX/"
    chmod 755 "$INSTALL_PREFIX/aimux"
    chown root:root "$INSTALL_PREFIX/aimux"

    # Create symlink for easier access
    ln -sf "$INSTALL_PREFIX/aimux" "/usr/local/bin/aimux"

    echo -e "${GREEN}Binary installed to $INSTALL_PREFIX/aimux${NC}"
}

# Function to install configuration
install_config() {
    echo -e "${BLUE}Installing configuration...${NC}"

    if [[ ! -f "$CONFIG_DIR/config.json" ]]; then
        cp production-config.json "$CONFIG_DIR/config.json"
        chown "$SERVICE_USER:$SERVICE_GROUP" "$CONFIG_DIR/config.json"
        chmod 640 "$CONFIG_DIR/config.json"
        echo -e "${GREEN}Configuration installed${NC}"
    else
        echo -e "${YELLOW}Configuration already exists, skipping${NC}"
    fi

    # Install environment template
    if [[ ! -f "$CONFIG_DIR/.env" ]]; then
        cp config/production.env "$CONFIG_DIR/.env.template"
        chown "$SERVICE_USER:$SERVICE_GROUP" "$CONFIG_DIR/.env.template"
        chmod 640 "$CONFIG_DIR/.env.template"
        echo -e "${GREEN}Environment template installed${NC}"
    fi
}

# Function to install systemd service
install_systemd_service() {
    echo -e "${BLUE}Installing systemd service...${NC}"

    cp aimux.service "/etc/systemd/system/$SERVICE_NAME.service"
    systemctl daemon-reload
    systemctl enable "$SERVICE_NAME"

    echo -e "${GREEN}Systemd service installed and enabled${NC}"
    echo -e "${YELLOW}Start with: systemctl start $SERVICE_NAME${NC}"
}

# Function to install launchd service
install_launchd_service() {
    echo -e "${BLUE}Installing launchd service...${NC}"

    # Create macOS-style paths
    MAC_INSTALL_PREFIX="/usr/local/opt/aimux"
    MAC_CONFIG_DIR="/usr/local/etc/aimux"
    MAC_VAR_DIR="/usr/local/var/lib/aimux"
    MAC_LOG_DIR="/usr/local/var/log/aimux"

    mkdir -p "$MAC_INSTALL_PREFIX" "$MAC_CONFIG_DIR" "$MAC_VAR_DIR" "$MAC_LOG_DIR"

    # Update plist with correct paths
    sed "s|/opt/aimux2/aimux|$MAC_INSTALL_PREFIX/aimux|g" com.aimux.daemon.plist > /tmp/com.aimux.daemon.plist
    sed -i.tmp "s|/usr/local/etc/aimux|$MAC_CONFIG_DIR|g" /tmp/com.aimux.daemon.plist
    sed -i.tmp "s|/usr/local/var/lib/aimux|$MAC_VAR_DIR|g" /tmp/com.aimux.daemon.plist
    sed -i.tmp "s|/usr/local/var/log/aimux|$MAC_LOG_DIR|g" /tmp/com.aimux.daemon.plist

    cp /tmp/com.aimux.daemon.plist "/Library/LaunchDaemons/com.aimux.daemon.plist"
    rm -f /tmp/com.aimux.daemon.plist /tmp/com.aimux.daemon.plist.tmp

    launchctl load -w "/Library/LaunchDaemons/com.aimux.daemon.plist"

    echo -e "${GREEN}Launchd service installed and loaded${NC}"
}

# Function to setup log rotation
setup_log_rotation() {
    echo -e "${BLUE}Setting up log rotation...${NC}"

    if [[ "$SERVICE_TYPE" == "systemd" ]]; then
        cat > "/etc/logrotate.d/aimux2" << EOF
$LOG_DIR/*.log {
    daily
    missingok
    rotate 30
    compress
    delaycompress
    notifempty
    create 640 $SERVICE_USER $SERVICE_GROUP
    postrotate
        systemctl reload $SERVICE_NAME || true
    endscript
}
EOF
        echo -e "${GREEN}Log rotation configured${NC}"
    else
        echo -e "${YELLOW}Log rotation should be configured manually on macOS${NC}"
    fi
}

# Function to show post-install info
show_post_install_info() {
    echo
    echo -e "${GREEN}=== Installation Complete ===${NC}"
    echo
    echo -e "${YELLOW}Next steps:${NC}"
    echo "1. Configure your API keys in: $CONFIG_DIR/config.json"
    echo "2. Review security settings in: $CONFIG_DIR/.env.template"
    echo "3. Start the service:"

    if [[ "$SERVICE_TYPE" == "systemd" ]]; then
        echo "   systemctl start $SERVICE_NAME"
        echo "   systemctl status $SERVICE_NAME"
        echo "   journalctl -u $SERVICE_NAME -f  # View logs"
    else
        echo "   launchctl load -w /Library/LaunchDaemons/com.aimux.daemon.plist"
        echo "   tail -f $LOG_DIR/aimux.log  # View logs"
    fi

    echo
    echo -e "${YELLOW}Configuration files:${NC}"
    echo "  Main config: $CONFIG_DIR/config.json"
    echo "  Environment: $CONFIG_DIR/.env.template"
    echo "  Logs: $LOG_DIR/"
    echo "  Working dir: $VAR_DIR/"

    echo
    echo -e "${YELLOW}Web UI:${NC}"
    echo "  HTTP: http://localhost:8080"
    echo "  HTTPS: https://localhost:8443 (if configured)"
}

# Main installation flow
main() {
    check_root

    # Check if aimux binary exists
    if [[ ! -f "aimux" ]]; then
        echo -e "${RED}aimux binary not found. Build it first with ./scripts/build_production.sh${NC}"
        exit 1
    fi

    create_user
    create_directories
    install_binary
    install_config

    # Install service based on OS
    if [[ "$SERVICE_TYPE" == "systemd" ]]; then
        install_systemd_service
    else
        install_launchd_service
    fi

    setup_log_rotation
    show_post_install_info
}

# Run main function
main "$@"