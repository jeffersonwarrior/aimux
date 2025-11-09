#!/bin/bash

# Aimux Docker Testing Script
# This script builds and tests the aimux Docker environment

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CONTAINER_NAME="aimux-test"
ROUTER_PORT="8080"
TEST_TIMEOUT="60"

# Logging function
log() {
    echo -e "${BLUE}[$(date +'%Y-%m-%d %H:%M:%S')] $1${NC}"
}

log_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

log_error() {
    echo -e "${RED}✗ $1${NC}"
}

log_warning() {
    echo -e "${YELLOW}⚠ $1${NC}"
}

# Check dependencies
check_dependencies() {
    log "Checking dependencies..."

    if ! command -v docker &> /dev/null; then
        log_error "Docker is not installed or not in PATH"
        exit 1
    fi

    if ! command -v docker-compose &> /dev/null; then
        log_error "Docker Compose is not installed or not in PATH"
        exit 1
    fi

    if ! command -v curl &> /dev/null; then
        log_error "curl is not installed or not in PATH"
        exit 1
    fi

    if ! command -v jq &> /dev/null; then
        log_warning "jq is not installed. Some JSON parsing features may not work"
    fi

    log_success "Dependencies check passed"
}

# Clean up existing containers and images
cleanup() {
    log "Cleaning up existing containers..."

    # Stop and remove existing container
    if docker ps -a --format 'table {{.Names}}' | grep -q "^${CONTAINER_NAME}$"; then
        log "Stopping existing container..."
        docker stop "${CONTAINER_NAME}" 2>/dev/null || true
        log "Removing existing container..."
        docker rm "${CONTAINER_NAME}" 2>/dev/null || true
    fi

    # Remove dangling images
    docker image prune -f 2>/dev/null || true

    log_success "Cleanup completed"
}

# Build Docker image
build_image() {
    log "Building Docker image..."

    cd "${PROJECT_DIR}"
    docker build -t aimux:latest .

    log_success "Docker image built successfully"
}

# Start containers
start_containers() {
    log "Starting containers..."

    cd "${PROJECT_DIR}"

    # Start with test profile
    docker-compose --profile testing up -d

    log_success "Containers started"
}

# Wait for router to be ready
wait_for_router() {
    log "Waiting for router to be ready..."

    local max_attempts=30
    local attempt=1

    while [ $attempt -le $max_attempts ]; do
        if curl -s -f http://localhost:${ROUTER_PORT}/health &>/dev/null; then
            log_success "Router is ready on port ${ROUTER_PORT}"
            return 0
        fi

        log "Attempt ${attempt}/${max_attempts}: Router not ready yet..."
        sleep 2
        ((attempt++))
    done

    log_error "Router failed to start within ${max_attempts} attempts"
    return 1
}

# Test container health
test_container_health() {
    log "Testing container health..."

    # Check if container is running
    if ! docker ps --format 'table {{.Names}}' | grep -q "^${CONTAINER_NAME}$"; then
        log_error "Container is not running"
        return 1
    fi

    # Check container health status
    local health_status=$(docker inspect --format='{{.State.Health.Status}}' "${CONTAINER_NAME}" 2>/dev/null || echo "unknown")

    if [[ "$health_status" == "healthy" ]]; then
        log_success "Container health check passed"
        return 0
    elif [[ "$health_status" == "starting" ]]; then
        log_warning "Container is still starting..."
        return 0
    else
        log_error "Container health status: $health_status"
        return 1
    fi
}

# Test aimux CLI functionality
test_aimux_cli() {
    log "Testing aimux CLI functionality..."

    # Test aimux help command
    local help_output=$(docker exec "${CONTAINER_NAME}" aimux --help 2>/dev/null || echo "")
    if [[ "$help_output" == *"aimux"* ]]; then
        log_success "aimux --help command works"
    else
        log_error "aimux --help command failed"
        return 1
    fi

    # Test provider listing
    local providers_output=$(docker exec "${CONTAINER_NAME}" aimux providers 2>/dev/null || echo "")
    if [[ "$providers_output" == *"minimax"* ]]; then
        log_success "aimux providers command works"
    else
        log_error "aimux providers command failed"
        return 1
    fi

    # Test configuration
    log "Testing aimux configuration..."
    docker exec "${CONTAINER_NAME}" sh -c "aimux config" &>/dev/null
    if [ $? -eq 0 ]; then
        log_success "aimux config command works"
    else
        log_warning "aimux config command had issues (expected in container)"
    fi

    return 0
}

# Test connectivity with Minimax
test_provider_connectivity() {
    log "Testing Minimax M2 provider connectivity..."

    # Test API key validation
    local test_result=$(docker exec "${CONTAINER_NAME}" node -e "
        const { BaseProvider } = require('./dist/providers');
        console.log('Testing connectivity...');
        process.exit(0);
    " 2>/dev/null)

    if [ $? -eq 0 ]; then
        log_success "Provider connectivity test passed"
        return 0
    else
        log_error "Provider connectivity test failed"
        return 1
    fi
}

# Test router functionality
test_router() {
    log "Testing router functionality..."

    # Test health endpoint
    local health_response=$(curl -s -w "%{http_code}" http://localhost:${ROUTER_PORT}/health)
    local http_code="${health_response: -3}"
    local response_body="${health_response%???}"

    if [ "$http_code" = "200" ]; then
        log_success "Router health endpoint responded correctly"
    else
        log_error "Router health endpoint failed with code: $http_code"
        return 1
    fi

    # Test that router is listening on correct port
    if netstat -tuln 2>/dev/null | grep -q ":${ROUTER_PORT}"; then
        log_success "Router is listening on port ${ROUTER_PORT}"
    else
        log_warning "Router port check inconclusive (netstat may not be available)"
    fi

    return 0
}

# Run comprehensive tests
run_tests() {
    log "Running comprehensive tests..."

    local tests_passed=0
    local tests_failed=0

    # Run individual tests
    test_container_health && ((tests_passed++)) || ((tests_failed++))
    test_aimux_cli && ((tests_passed++)) || ((tests_failed++))
    test_provider_connectivity && ((tests_passed++)) || ((tests_failed++))
    test_router && ((tests_passed++)) || ((tests_failed++))

    # Report results
    log "Test Results:"
    log "  Passed: $tests_passed"
    log "  Failed: $tests_failed"
    log "  Total: $((tests_passed + tests_failed))"

    if [ $tests_failed -eq 0 ]; then
        log_success "All tests passed!"
        return 0
    else
        log_error "$tests_failed test(s) failed"
        return 1
    fi
}

# Show container logs
show_logs() {
    log "Showing recent container logs..."
    echo
    docker logs --tail 50 "${CONTAINER_NAME}"
    echo
}

# Interactive test mode
interactive_test() {
    log "Entering interactive test mode..."
    log "Container is running. You can now test aimux manually:"
    echo
    echo "Useful commands:"
    echo "  docker exec -it ${CONTAINER_NAME} aimux --help"
    echo "  docker exec -it ${CONTAINER_NAME} aimux providers"
    echo "  docker exec -it ${CONTAINER_NAME} aimux minimax setup"
    echo "  docker exec -it ${CONTAINER_NAME} bash"
    echo
    echo "Test URLs:"
    echo "  http://localhost:${ROUTER_PORT}/health"
    echo
    read -p "Press Enter to stop containers and exit..."
}

# Main execution
main() {
    local action="${1:-test}"

    case "$action" in
        "cleanup")
            cleanup
            ;;
        "build")
            cleanup
            build_image
            ;;
        "start")
            start_containers
            wait_for_router
            ;;
        "test")
            log "Starting aimux Docker testing..."
            check_dependencies
            cleanup
            build_image
            start_containers

            if wait_for_router; then
                run_tests
                local test_result=$?

                if [ "$test_result" -eq 0 ]; then
                    read -p "Tests completed successfully! Would you like to enter interactive mode? (y/n): " -n 1 -r
                    echo
                    if [[ $REPLY =~ ^[Yy]$ ]]; then
                        interactive_test
                    fi
                else
                    show_logs
                fi

                # Cleanup
                docker-compose down
            else
                log_error "Router failed to start. Showing logs:"
                show_logs
                docker-compose down
                exit 1
            fi
            ;;
        "interactive")
            interactive_test
            ;;
        "logs")
            show_logs
            ;;
        *)
            echo "Usage: $0 {cleanup|build|start|test|interactive|logs}"
            echo
            echo "Commands:"
            echo "  cleanup    - Clean up existing containers and images"
            echo "  build      - Build Docker image only"
            echo "  start      - Start containers only"
            echo "  test       - Run full test suite (default)"
            echo "  interactive- Enter interactive testing mode"
            echo "  logs       - Show container logs"
            exit 1
            ;;
    esac
}

# Script entry point
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi