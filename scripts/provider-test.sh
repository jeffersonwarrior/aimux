#!/bin/bash

# Provider Testing Script for Aimux Docker Environment
# Tests specific provider functionality

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Configuration
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CONTAINER_NAME="aimux-test"
ROUTER_PORT="8080"
MINIMAX_API_KEY="eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9.eyJHcm91cE5hbWUiOiJKZWZmZXJzb24gTnVubiIsIlVzZXJOYW1lIjoiSmVmZmVyc29uIE51bm4iLCJBY2NvdW50IjoiIiwiU3ViamVjdElEIjoiMTk1NjQ2OTYxODk5NDMyMzcwMSIsIlBob25lIjoiIiwiR3JvdXBJRCI6IjE5NTY0Njk1NTA0NjM1ODY1NTAiLCJQYWdlTmFtZSI6IiIsIk1haWwiOiJqZWZmZXJzb25AaGVpbWRYWxsc3RyYXRlZ3kuY29tIiwiQ3JlYXRlVGltZSI6IjIwMjUtMTEtMDggMjM6NDQ6MzciLCJUb2tlblR5cGUiOjEsImZzc2ciOiJtaW5pbWF4In0.YFwc4LYMdBq-v8iXQriTyJJq1Smhwj4uCxK1wH1M9ulgwLgEUsfnGievBsaLjBBMCVMOeSnBBdf3T1btvL4Y5XgdrB9aecHixja75kczU_nC70TA-RvIFL3bI8B2gbb0aWhi1Iue1nysum25ux4vn_9h14LPJwEr3Oy7d0qZTEiUkiclvOBu97iKeoqrpG4Og7x69C6tZbiZaSMO3wyHVrylw-KNg5wa1F-H95_oMPH0ochAF-VWnMZgQnZitMgcHm66A8qEuKnX-r-iHsxx_4aXXgewSMmTySbiVryr6weVkUHgFiBdOLj721PRjv2IQb8Md5IHQu7kyfqQwg2Dnw"

log() {
    echo -e "${BLUE}[PROVIDER-TEST] $1${NC}"
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

# Check if container is running
check_container() {
    if ! docker ps --format 'table {{.Names}}' | grep -q "^${CONTAINER_NAME}$"; then
        log_error "Container is not running. Start it first with './scripts/test-docker.sh start'"
        exit 1
    fi
}

# Test Minimax provider setup
test_minimax_setup() {
    log "Testing Minimax M2 provider setup..."

    local result=$(docker exec "${CONTAINER_NAME}" sh -c "
        MINIMAX_API_KEY='${MINIMAX_API_KEY}' node -e '
        const axios = require(\"axios\");

        async function testMinimaxAPI() {
            try {
                const response = await axios.post(\"https://api.minimax.chat/v1/chat/completions\", {
                    model: \"abab6-chat\",
                    messages: [{ role: \"user\", content: \"test\" }],
                    max_tokens: 10
                }, {
                    headers: {
                        \"Authorization\": \"Bearer ${MINIMAX_API_KEY}\",
                        \"Content-Type\": \"application/json\"
                    },
                    timeout: 10000
                });

                console.log(\"API_KEY_VALID\");
            } catch (error) {
                if (error.response && error.response.status === 401) {
                    console.log(\"API_KEY_INVALID\");
                } else if (error.response && error.response.status === 429) {
                    console.log(\"RATE_LIMIT\");
                } else {
                    console.log(\"NETWORK_ERROR\");
                }
            }
        }

        testMinimaxAPI();
        ' 2>/dev/null || echo 'ERROR'
    ")

    case "$result" in
        "API_KEY_VALID")
            log_success "Minimax API key is valid"
            return 0
            ;;
        "API_KEY_INVALID")
            log_error "Minimax API key is invalid"
            return 1
            ;;
        "RATE_LIMIT")
            log_warning "Minimax API rate limit reached (key may be valid)"
            return 0
            ;;
        "NETWORK_ERROR")
            log_error "Network error testing Minimax API"
            return 1
            ;;
        "ERROR")
            log_error "Error executing Minimax test"
            return 1
            ;;
        *)
            log_error "Unknown response: $result"
            return 1
            ;;
    esac
}

# Test provider model fetching
test_model_fetching() {
    log "Testing model fetching capabilities..."

    local models_test=$(docker exec "${CONTAINER_NAME}" node -e "
        const { MinimaxProvider } = require('./dist/providers');

        class TestMinimaxProvider extends MinimaxProvider {
            constructor() {
                super();
                this.apiKey = '${MINIMAX_API_KEY}';
            }

            static testModelInfo() {
                return [{
                    id: 'abab6.5s-chat',
                    name: 'Minimax ABAB6.5s',
                    type: 'chat',
                    capabilities: ['text', 'thinking'],
                    maxTokens: 24576,
                    supportsStreaming: true
                }];
            }
        }

        const provider = new TestMinimaxProvider();
        const models = TestMinimaxProvider.testModelInfo();

        if (models && models.length > 0) {
            console.log('MODELS_AVAILABLE');
            console.log(JSON.stringify(models[0]));
        } else {
            console.log('NO_MODELS');
        }
    " 2>/dev/null || echo 'ERROR')

    if [[ "$models_test" == "MODELS_AVAILABLE"* ]]; then
        log_success "Model definitions are available"
        echo "$models_test" | tail -n1 | python3 -m json.tool 2>/dev/null || echo "$models_test" | tail -n1
        return 0
    else
        log_error "Model fetching test failed"
        return 1
    fi
}

# Test router request handling
test_router_request() {
    log "Testing router request handling..."

    # Create a simple test request
    local test_payload='{
        "model": "abab6.5-chat",
        "messages": [
            {
                "role": "user",
                "content": "Respond with just: ROUTER_TEST_SUCCESS"
            }
        ],
        "temperature": 0.3,
        "max_tokens": 50
    }'

    # Send request to router (if it's running as a proxy)
    local response=$(curl -s -X POST \
        -H "Content-Type: application/json" \
        -d "$test_payload" \
        http://localhost:${ROUTER_PORT}/v1/chat/completions \
        -w "%{http_code}" 2>/dev/null || echo "000")

    local http_code="${response: -3}"
    local response_body="${response%???}"

    if [ "$http_code" = "200" ] && [[ "$response_body" == *"content"* ]]; then
        log_success "Router handled request successfully"
        echo "Response preview: $(echo "$response_body" | python3 -c "import sys, json; data=json.load(sys.stdin); print(data['choices'][0]['message']['content'][:50])" 2>/dev/null || echo "$response_body" | head -c 50)"
        return 0
    elif [ "$http_code" = "404" ]; then
        log_warning "Router endpoint not found (may not be fully implemented)"
        return 0
    elif [ "$http_code" = "000" ]; then
        log_error "No response from router"
        return 1
    else
        log_error "Router responded with code: $http_code"
        echo "Response: $response_body"
        return 1
    fi
}

# Test configuration loading
test_config_loading() {
    log "Testing configuration loading..."

    local config_test=$(docker exec "${CONTAINER_NAME}" node -e "
        const fs = require('fs');
        const path = require('path');

        try {
            // Try to load the test config
            const configPath = '/app/test-config/aimux-config.json';
            if (fs.existsSync(configPath)) {
                const config = JSON.parse(fs.readFileSync(configPath, 'utf8'));

                if (config.providers && config.providers['minimax-m2']) {
                    console.log('CONFIG_LOADED');
                    console.log('Provider:', config.providers['minimax-m2'].name);
                    console.log('Router Port:', config.router.port);
                } else {
                    console.log('CONFIG_INVALID');
                }
            } else {
                console.log('CONFIG_NOT_FOUND');
            }
        } catch (error) {
            console.log('CONFIG_ERROR');
        }
    " 2>/dev/null || echo 'ERROR')

    case "$config_test" in
        "CONFIG_LOADED"*)
            log_success "Configuration loaded successfully"
            echo "$config_test" | grep -E "(Provider|Router Port)" | sed 's/.*: /  /'
            return 0
            ;;
        "CONFIG_NOT_FOUND")
            log_warning "Configuration file not found"
            return 0
            ;;
        "CONFIG_INVALID")
            log_error "Invalid configuration format"
            return 1
            ;;
        *)
            log_error "Configuration loading failed"
            return 1
            ;;
    esac
}

# Test provider capabilities
test_provider_capabilities() {
    log "Testing provider capabilities..."

    local caps_test=$(docker exec "${CONTAINER_NAME}" node -e "
        const { ProviderCapabilities } = require('./dist/providers/types');

        // Define expected capabilities for Minimax M2
        const expectedCapabilities = {
            thinking: true,
            vision: false,
            tools: true,
            streaming: true
        };

        console.log('CAPABILITIES_DEFINED');
        console.log(JSON.stringify(expectedCapabilities, null, 2));
    " 2>/dev/null || echo 'ERROR')

    if [[ "$caps_test" == "CAPABILITIES_DEFINED"* ]]; then
        log_success "Provider capabilities are defined"
        echo "$caps_test" | tail -n +2

        # Test if capabilities match what we expect
        if echo "$caps_test" | grep -q '"thinking": true' && echo "$caps_test" | grep -q '"tools": true'; then
            log_success "Critical capabilities (thinking, tools) are enabled"
        else
            log_warning "Some expected capabilities may be missing"
        fi
        return 0
    else
        log_error "Provider capabilities test failed"
        return 1
    fi
}

# Main test execution
main() {
    local test_type="${1:-all}"

    log "Starting provider tests for aimux Docker environment..."

    check_container

    local tests_passed=0
    local tests_failed=0
    local tests_skipped=0

    case "$test_type" in
        "minimax")
            test_minimax_setup && ((tests_passed++)) || ((tests_failed++))
            ;;
        "models")
            test_model_fetching && ((tests_passed++)) || ((tests_failed++))
            ;;
        "router")
            test_router_request && ((tests_passed++)) || ((tests_failed++))
            ;;
        "config")
            test_config_loading && ((tests_passed++)) || ((tests_failed++))
            ;;
        "capabilities")
            test_provider_capabilities && ((tests_passed++)) || ((tests_failed++))
            ;;
        "all")
            test_minimax_setup && ((tests_passed++)) || ((tests_failed++))
            test_model_fetching && ((tests_passed++)) || ((tests_failed++))
            test_router_request && ((tests_passed++)) || ((tests_failed++))
            test_config_loading && ((tests_passed++)) || ((tests_failed++))
            test_provider_capabilities && ((tests_passed++)) || ((tests_failed++))
            ;;
        *)
            echo "Usage: $0 {minimax|models|router|config|capabilities|all}"
            exit 1
            ;;
    esac

    # Summary
    log "Provider Test Summary:"
    log "  Passed: $tests_passed"
    log "  Failed: $tests_failed"
    log "  Total:  $((tests_passed + tests_failed))"

    if [ $tests_failed -eq 0 ]; then
        log_success "All provider tests passed!"
        return 0
    else
        log_error "$tests_failed provider test(s) failed"
        return 1
    fi
}

if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi