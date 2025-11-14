# Aimux Regression Test Plan

## Why This Exists
We built a sophisticated router that looked great but couldn't route requests. This prevents repeating that embarrassment.

## Core Regression Tests

### 1. Build Regression
**Never break the build.**

```bash
#!/bin/bash
# qa/regression/test_build.sh

cmake --build build
if [ $? -ne 0 ]; then
    echo "REGRESSION: Build broken"
    exit 1
fi

echo "✅ Build regression passed"
```

### 2. Startup Regression
**Never break basic startup.**

```bash
#!/bin/bash
# qa/regression/test_startup.sh

# Test default startup should fail gracefully (no config)
timeout 5 ./build/claude_gateway 2>/dev/null
if [ $? -eq 0 ]; then
    echo "REGRESSION: Should abort without config"
    exit 1
fi

# Test startup with valid config
cat > qa/regression/test_config.json <<EOF
{
  "providers": {
    "synthetic": {
      "name": "synthetic",
      "api_key": "test",
      "base_url": "http://localhost:8081",
      "enabled": true
    }
  }
}
EOF

timeout 10 ./build/claude_gateway --config qa/regression/test_config.json &
PID=$!
sleep 2

if ! kill -0 $PID 2>/dev/null; then
    echo "REGRESSION: Failed to start with valid config"
    exit 1
fi

kill $PID
echo "✅ Startup regression passed"
```

### 3. Core Routing Regression
**Never break the one thing that matters.**

```bash
#!/bin/bash
# qa/regression/test_routing.sh

# Start service with test config
./build/claude_gateway --config qa/regression/test_config.json &
PID=$!
sleep 3

# Test basic request routing
RESPONSE=$(curl -s -w "%{http_code}" -X POST http://localhost:8080/anthropic/v1/messages \
  -H "Content-Type: application/json" \
  -d '{"model":"claude-3-sonnet-20240229","max_tokens":5,"messages":[{"role":"user","content":"hi"}]}')

HTTP_CODE="${RESPONSE: -3}"
BODY="${RESPONSE%???}"

kill $PID 2>/dev/null

if [ "$HTTP_CODE" = "400" ] && echo "$BODY" | grep -q "PROVIDER_NOT_FOUND"; then
    echo "REGRESSION: Core routing broken - PROVIDER_NOT_FOUND"
    exit 1
fi

if [ "$HTTP_CODE" -ge 500 ]; then
    echo "REGRESSION: Core routing broken - server error"
    exit 1
fi

echo "✅ Routing regression passed"
```

### 4. Configuration Validation Regression
**Never break config validation.**

```bash
#!/bin/bash
# qa/regression/test_config_validation.sh

# Test 1: Missing config should abort
timeout 3 ./build/claude_gateway --config /nonexistent.json >& /dev/null
if [ $? -eq 0 ]; then
    echo "REGRESSION: Should abort on missing config"
    exit 1
fi

# Test 2: Invalid JSON should abort
echo "{invalid" > qa/regression/bad.json
timeout 3 ./build/claude_gateway --config qa/regression/bad.json >& /dev/null
if [ $? -eq 0 ]; then
    echo "REGRESSION: Should abort on invalid JSON"
    exit 1
fi

echo "✅ Config validation regression passed"
```

### 5. Provider Loading Regression
**Never break provider loading.**

```bash
#!/bin/bash
# qa/regression/test_provider_loading.sh

./build/claude_gateway --config qa/regression/test_config.json &
PID=$!
sleep 3

PROVIDERS=$(curl -s http://localhost:8080/providers)

if echo "$PROVIDERS" | jq '.providers | length' | grep -q "0"; then
    echo "REGRESSION: No providers loaded"
    kill $PID
    exit 1
fi

kill $PID
echo "✅ Provider loading regression passed"
```

## Automated Regression Pipeline

### Pre-commit Hook
```bash
#!/bin/bash
# .git/hooks/pre-commit

echo "Running regression tests..."
./qa/regression/test_build.sh || exit 1
./qa/regression/test_startup.sh || exit 1
./qa/regression/test_routing.sh || exit 1
./qa/regression/test_config_validation.sh || exit 1
echo "All regression tests passed"
```

### CI Pipeline
```yaml
# .github/workflows/regression.yml
name: Regression Tests
on: [push, pull_request]
jobs:
  regression:
    runs-on: ubuntu-latest
    steps:
    - uses: actions/checkout@v2
    - name: Build
      run: cmake -S . -B build && cmake --build build
    - name: Regression Tests
      run: |
        ./qa/regression/test_build.sh
        ./qa/regression/test_startup.sh
        ./qa/regression/test_routing.sh
        ./qa/regression/test_config_validation.sh
        ./qa/regression/test_provider_loading.sh
```

## Nightly Full Regression
```bash
#!/bin/bash
# qa/regression/nightly_full.sh

echo "Running full regression suite..."

# Core regressions (all must pass)
./qa/regression/test_build.sh || exit 1
./qa/regression/test_startup.sh || exit 1
./qa/regression/test_routing.sh || exit 1
./qa/regression/test_config_validation.sh || exit 1
./qa/regression/test_provider_loading.sh || exit 1

# Extended regressions (should pass)
./qa/regression/test_performance.sh || echo "WARN: Performance regression"
./qa/regression/test_memory.sh || echo "WARN: Memory regression"
./qa/regression/test_endpoints.sh || echo "WARN: Endpoint regression"

echo "Regression complete"
```

## Performance Regression Tests

### Response Time Regression
```bash
#!/bin/bash
# qa/regression/test_performance.sh

./build/claude_gateway --config qa/regression/test_config.json &
PID=$!
sleep 3

# Measure response time
START=$(date +%s%N)
curl -s http://localhost:8080/health > /dev/null
END=$(date +%s%N)

RESPONSE_TIME=$(( (END - START) / 1000000 ))
kill $PID

if [ $RESPONSE_TIME -gt 1000 ]; then
    echo "REGRESSION: Health check too slow (${RESPONSE_TIME}ms > 1000ms)"
    exit 1
fi

echo "✅ Performance regression passed (${RESPONSE_TIME}ms)"
```

### Memory Regression
```bash
#!/bin/bash
# qa/regression/test_memory.sh

./build/claude_gateway --config qa/regression/test_config.json &
PID=$!
sleep 5

# Check memory usage
MEMORY=$(ps -p $PID -o rss= | tr -d ' ')
kill $PID

# Convert to MB
MEMORY_MB=$((MEMORY / 1024))

if [ $MEMORY_MB -gt 100 ]; then
    echo "REGRESSION: Memory usage too high (${MEMORY_MB}MB > 100MB)"
    exit 1
fi

echo "✅ Memory regression passed (${MEMORY_MB}MB)"
```

## Endpoint Regression
```bash
#!/bin/bash
# qa/regression/test_endpoints.sh

./build/claude_gateway --config qa/regression/test_config.json &
PID=$!
sleep 3

# Test all documented endpoints
ENDPOINTS=(
    "/health:200"
    "/metrics:200"
    "/providers:200"
    "/config:200"
)

for endpoint in "${ENDPOINTS[@]}"; do
    ENDPOINT_PATH=${endpoint%:*}
    EXPECTED_CODE=${endpoint#*:}

    CODE=$(curl -s -o /dev/null -w "%{http_code}" "http://localhost:8080${ENDPOINT_PATH}")

    if [ "$CODE" -ne "$EXPECTED_CODE" ]; then
        echo "REGRESSION: Endpoint ${ENDPOINT_PATH} returned ${CODE}, expected ${EXPECTED_CODE}"
        kill $PID
        exit 1
    fi
done

kill $PID
echo "✅ Endpoint regression passed"
```

## Regression Failure Procedures

### Critical Failure (Core Functionality)
1. **Block all merges** immediately
2. **All hands focus** on the regression
3. **Do NOT merge** any other changes
4. **Fix first, optimize later**

### Performance Failure
1. **Document the regression**
2. **Set performance milestone**
3. **Monitor progress**
4. **Consider rollback** if significant

### Extended Test Failure
1. **Log the issue**
2. **Add to backlog**
3. **Fix by next release**
4. **Don't block** core development

## Regression Test Database

Maintain known good responses:
```bash
# qa/regression/baseline/
# health_response.json
# providers_response.json
# metrics_response.json
# routing_response.json
```

Compare current responses against baseline:
```bash
# qa/regression/test_baseline.sh
diff <(curl -s http://localhost:8080/health) qa/regression/baseline/health_response.json
```

## Truth Statements

These must ALWAYS remain true:

1. **Build completes** with zero warnings
2. **Service starts** with valid config, aborts with invalid config
3. **Real requests route** to actual providers
4. **Errors are appropriate** for the situation
5. **Providers load** from configuration
6. **Endpoints respond** as documented

If any truth statement fails, **all work stops** until it's true again.

This regression plan exists because we forgot the basics while building sophisticated features. Never again.