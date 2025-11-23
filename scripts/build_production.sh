#!/bin/bash

# Aimux2 Production Build Script
# Creates optimized release build with full production deployment setup

set -e

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Project root
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build-release"
INSTALL_PREFIX="/opt/aimux2"

echo -e "${BLUE}=== Aimux2 Production Build ===${NC}"
echo "Project root: ${PROJECT_ROOT}"
echo "Build directory: ${BUILD_DIR}"
echo "Install prefix: ${INSTALL_PREFIX}"
echo

# Clean previous builds
if [ -d "${BUILD_DIR}" ]; then
    echo -e "${YELLOW}Cleaning previous build...${NC}"
    rm -rf "${BUILD_DIR}"
fi

# Create build directory
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

# Configure for production build
echo -e "${BLUE}Configuring production build...${NC}"
cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}" \
    -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=TRUE \
    -DBUILD_SHARED_LIBS=OFF \
    ..

# Build
echo -e "${BLUE}Building aimux2 components...${NC}"
make -j$(nproc) VERBOSE=1

# Run tests to ensure quality
echo -e "${BLUE}Running production tests...${NC}"
if [ -f "./provider_integration_tests" ]; then
    ./provider_integration_tests
fi
if [ -f "./memory_performance_test" ]; then
    ./memory_performance_test
fi

# Install
echo -e "${BLUE}Installing to ${INSTALL_PREFIX}...${NC}"
sudo mkdir -p "${INSTALL_PREFIX}"
sudo make install

# Create deployment package
echo -e "${BLUE}Creating deployment package...${NC}"
DEPLOY_DIR="${PROJECT_ROOT}/deploy"
mkdir -p "${DEPLOY_DIR}"

# Package binary
cp aimux "${DEPLOY_DIR}/"
cp "${PROJECT_ROOT}/production-config.json" "${DEPLOY_DIR}/"

# Copy configuration files
mkdir -p "${DEPLOY_DIR}/config"
cp -r "${PROJECT_ROOT}/config/"* "${DEPLOY_DIR}/config/" 2>/dev/null || true

# Copy service files
cp "${PROJECT_ROOT}/aimux.service" "${DEPLOY_DIR}/"

# Copy documentation
cp "${PROJECT_ROOT}/README.md" "${DEPLOY_DIR}/"

# Create version info
echo "Build Information:" > "${DEPLOY_DIR}/BUILD_INFO.txt"
echo "=================" >> "${DEPLOY_DIR}/BUILD_INFO.txt"
echo "Build Date: $(date)" >> "${DEPLOY_DIR}/BUILD_INFO.txt"
echo "Build Type: Production Release" >> "${DEPLOY_DIR}/BUILD_INFO.txt"
echo "Git Commit: $(git rev-parse HEAD)" >> "${DEPLOY_DIR}/BUILD_INFO.txt"
echo "Git Branch: $(git rev-parse --abbrev-ref HEAD)" >> "${DEPLOY_DIR}/BUILD_INFO.txt"
echo "Binary Size: $(stat -c%s "${DEPLOY_DIR}/aimux") bytes" >> "${DEPLOY_DIR}/BUILD_INFO.txt"

# Create deployment tarball
cd "${DEPLOY_DIR}"
tar -czf "aimux2-production-$(date +%Y%m%d-%H%M%S).tar.gz" *.json aimux aimux.service README.md BUILD_INFO.txt config/

echo
echo -e "${GREEN}=== Production Build Complete ===${NC}"
echo "Binary: ${DEPLOY_DIR}/aimux"
echo "Deployment package: ${DEPLOY_DIR}/aimux2-production-*.tar.gz"
echo
echo -e "${YELLOW}Next steps:${NC}"
echo "1. Review BUILD_INFO.txt in ${DEPLOY_DIR}"
echo "2. Test deployment: cd ${DEPLOY_DIR} && ./aimux --help"
echo "3. Install system service: sudo cp aimux.service /etc/systemd/system/"
echo

# Show binary information if it exists
if [ -f "${DEPLOY_DIR}/aimux" ]; then
    echo -e "${BLUE}Binary Information:${NC}"
    ls -lh "${DEPLOY_DIR}/aimux"
    file "${DEPLOY_DIR}/aimux"
    echo
fi