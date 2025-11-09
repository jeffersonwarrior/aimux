#!/bin/bash

# Setup Docker Environment for Aimux Testing
# This script prepares the Docker environment with proper configuration

set -e

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Configuration
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

log() {
    echo -e "${BLUE}[SETUP] $1${NC}"
}

log_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

log_warning() {
    echo -e "${YELLOW}⚠ $1${NC}"
}

# Create environment file
create_env_file() {
    log "Creating .env file for Docker..."

    cat > "${PROJECT_DIR}/.env" << EOF
# Aimux Docker Environment Configuration
NODE_ENV=production

# Router Configuration
AIMUX_ROUTER_PORT=8080
AIMUX_HOST=0.0.0.0

# Logging
AIMUX_DEBUG=1
AIMUX_LOG_LEVEL=debug

# Minimax M2 Configuration
MINIMAX_API_KEY=eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9.eyJHcm91cE5hbWUiOiJKZWZmZXJzb24gTnVubiIsIlVzZXJOYW1lIjoiSmVmZmVyc29uIE51bm4iLCJBY2NvdW50IjoiIiwiU3ViamVjdElEIjoiMTk1NjQ2OTYxODk5NDMyMzcwMSIsIlBob25lIjoiIiwiR3JvdXBJRCI6IjE5NTY0Njk1NTA0NjM1ODY1NTAiLCJQYWdlTmFtZSI6IiIsIk1haWwiOiJqZWZmZXJzb25AaGVpbWRYWxsc3RyYXRlZ3kuY29tIiwiQ3JlYXRlVGltZSI6IjIwMjUtMTEtMDggMjM6NDQ6MzciLCJUb2tlblR5cGUiOjEsImZzc2ciOiJtaW5pbWF4In0.YFwc4LYMdBq-v8iXQriTyJJq1Smhwj4uCxK1wH1M9ulgwLgEUsfnGievBsaLjBBMCVMOeSnBBdf3T1btvL4Y5XgdrB9aecHixja75kczU_nC70TA-RvIFL3bI8B2gbb0aWhi1Iue1nysum25ux4vn_9h14LPJwEr3Oy7d0qZTEiUkiclvOBu97iKeoqrpG4Og7x69C6tZbiZaSMO3wyHVrylw-KNj5wa1F-H95_oMPH0ochAF-VWnMZgQnZitMgcHm66A8qEuKnX-r-iHsxx_4aXXgewSMmTySbiVryr6weVkUHgFiBdOLj721PRjv2IQb8Md5IHQu7kyfqQwg2Dnw
MINIMAX_BASE_URL=https://api.minimax.chat

# Additional Provider Keys (Optional)
ZAI_API_KEY=
SYNTHETIC_API_KEY=

# Network Configuration (Optional)
HTTP_PROXY=
HTTPS_PROXY=
NO_PROXY=localhost,127.0.0.1

# Docker Configuration
COMPOSE_PROJECT_NAME=aimux
COMPOSE_FILE=docker-compose.yml
EOF

    log_success ".env file created"
}

# Setup configuration directory
setup_config_directory() {
    log "Setting up configuration directory..."

    # Create .dockerignore file
    cat > "${PROJECT_DIR}/.dockerignore" << EOF
# Git
.git
.gitignore

# Node modules
node_modules
npm-debug.log*

# Distro
dist/

# Coverage
coverage/

# Logs
logs
*.log

# Runtime
.DS_Store
.env.local
.env.development.local
.env.test.local
.env.production.local

# IDE
.vscode
.idea
*.swp
*.swo

# OS
Thumbs.db

# Testing
test-results/
temp/
.tmp/

# Source maps
*.js.map

# Optional documentation (keep it clean)
*.md
!README.md
EOF

    log_success ".dockerignore file created"
}

# Create test data directory structure
create_test_data_structure() {
    log "Creating test data structure..."

    mkdir -p "${PROJECT_DIR}/test-data/samples"
    mkdir -p "${PROJECT_DIR}/test-data/expected"
    mkdir -p "${PROJECT_DIR}/logs"

    # Create sample test files
    cat > "${PROJECT_DIR}/test-data/samples/simple-chat.json" << 'EOF'
{
  "model": "abab6.5s-chat",
  "messages": [
    {
      "role": "user",
      "content": "Hello! Please respond with your model name and provider."
    }
  ],
  "temperature": 0.7,
  "max_tokens": 150
}
EOF

    cat > "${PROJECT_DIR}/test-data/samples/thinking-test.json" << 'EOF'
{
  "model": "abab6.5s-chat",
  "messages": [
    {
      "role": "user",
      "content": "Think step by step: If I have 3 apples and I give away 1, then buy 2 more, how many apples do I have? Show your reasoning."
    }
  ],
  "temperature": 0.5,
  "max_tokens": 300
}
EOF

    log_success "Test data structure created"
}

# Create helper scripts
create_helper_scripts() {
    log "Creating helper scripts..."

    # Quick test script
    cat > "${PROJECT_DIR}/scripts/quick-test.sh" << 'EOF'
#!/bin/bash

# Quick test script for aimux Docker environment
set -e

CONTAINER_NAME="aimux-test"
ROUTER_PORT="8080"

echo "=== Aimux Quick Test ==="

# Check if container is running
if ! docker ps --format '{{.Names}}' | grep -q "^${CONTAINER_NAME}$"; then
    echo "❌ Container is not running. Run './scripts/test-docker.sh test' first."
    exit 1
fi

echo "✅ Container is running"

# Test router health
if curl -s -f http://localhost:${ROUTER_PORT}/health >/dev/null; then
    echo "✅ Router is healthy"
else
    echo "❌ Router health check failed"
fi

# Test aimux CLI
if docker exec "${CONTAINER_NAME}" aimux --help >/dev/null 2>&1; then
    echo "✅ aimux CLI is working"
else
    echo "❌ aimux CLI test failed"
fi

echo "=== Quick Test Complete ==="
EOF

    chmod +x "${PROJECT_DIR}/scripts/quick-test.sh"

    # Log viewer script
    cat > "${PROJECT_DIR}/scripts/view-logs.sh" << 'EOF'
#!/bin/bash

# View aimux container logs
CONTAINER_NAME="aimux-test"

if docker ps --format '{{.Names}}' | grep -q "^${CONTAINER_NAME}$"; then
    docker logs -f "${CONTAINER_NAME}"
else
    echo "Container ${CONTAINER_NAME} is not running"
    exit 1
fi
EOF

    chmod +x "${PROJECT_DIR}/scripts/view-logs.sh"

    log_success "Helper scripts created"
}

# Main setup
main() {
    log "Setting up aimux Docker environment..."

    cd "${PROJECT_DIR}"

    create_env_file
    setup_config_directory
    create_test_data_structure
    create_helper_scripts

    log "🎉 Docker environment setup completed!"
    log
    log "Next steps:"
    log "  1. Run './scripts/test-docker.sh test' to build and test"
    log "  2. Use './scripts/quick-test.sh' for quick health checks"
    log "  3. Use './scripts/view-logs.sh' to monitor logs"
    log
    log "Configuration files created:"
    log "  - .env (Docker environment variables)"
    log "  - .dockerignore (Docker build exclusions)"
    log "  - test-data/ (Test data and samples)"
    log "  - scripts/ (Helper scripts)"
}

if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi