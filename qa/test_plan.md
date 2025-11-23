# Aimux Test Plan

## Purpose
Prevent the embarrassing situation where QA finds a sophisticated system that doesn't actually work.

## Test Levels

### 1. Smoke Tests (5 minutes)
**Goal:** Does the build actually work?

```bash
# Build test
cmake --build build || FAIL "Build fails"

# Binary exists
[ -f build/aimux ] || FAIL "Main binary missing"
[ -f build/claude_gateway ] || FAIL "Gateway binary missing"

# Basic help doesn't crash
./build/aimux --help || FAIL "Help command crashes"
./build/claude_gateway --help || FAIL "Gateway help crashes"
```

### 2. Basic Functionality Tests (15 minutes)
**Goal:** Does it do the ONE thing it's supposed to do?

**Test: Provider Actually Exists**
```bash
# Start with valid config
./build/claude_gateway --config test_config.json &

# Check if providers are actually loaded
curl -s http://localhost:8080/providers | jq '.providers | length' | grep -q "^[1-9]" || FAIL "No providers loaded"
```

**Test: Real Request Routing**
```bash
# Send a real request
curl -X POST http://localhost:8080/anthropic/v1/messages \
  -H "Content-Type: application/json" \
  -d '{"model":"claude-3-sonnet-20240229","max_tokens":10,"messages":[{"role":"user","content":"hi"}]}'

# Response should NOT be "PROVIDER_NOT_FOUND"
[ "$HTTP_STATUS" -ne 400 ] || FAIL "Request routing broken"
```

### 3. Configuration Tests (10 minutes)
**Goal:** Config validation must work correctly

```bash
# Missing config should abort with error code != 0
timeout 5 ./build/claude_gateway --config /nonexistent.json
[ "$?" -ne 0 ] || FAIL "Should abort on missing config"

# Invalid JSON should abort
echo "invalid json" > bad_config.json
timeout 5 ./build/claude_gateway --config bad_config.json
[ "$?" -ne 0 ] || FAIL "Should abort on invalid config"
```

### 4. Provider Integration Tests (30 minutes)
**Goal:** Each provider actually works

**For each provider (Cerebras, Z.AI, MiniMax, Synthetic):**
```bash
# Test provider is reachable
./build/provider_integration_tests --provider <name> || FAIL "Provider <name> unreachable"

# Test real request flows through
curl -X POST "http://localhost:8080/anthropic/v1/messages" \
  -H "X-Provider: <name>" \
  -d '{"model":"test","max_tokens":5,"messages":[{"role":"user","content":"test"}]}'

# Should get actual response, not error
echo "$RESPONSE" | grep -q "content" || FAIL "Provider <name> returns errors"
```

### 5. Negative Tests (15 minutes)
**Goal:** Fail gracefully, crash safely

```bash
# Invalid request should 400, not 500
curl -X POST http://localhost:8080/anthropic/v1/messages \
  -d "invalid json"
[ "$HTTP_STATUS" -eq 400 ] || FAIL "Should 400 on invalid JSON"

# Missing fields should 400
curl -X POST http://localhost:8080/anthropic/v1/messages \
  -H "Content-Type: application/json" \
  -d '{}'
[ "$HTTP_STATUS" -eq 400 ] || FAIL "Should 400 on missing fields"
```

### 6. Performance Smoke (10 minutes)
**Goal:** Not embarrassingly slow

```bash
# Basic load test
for i in {1..10}; do
  curl -s http://localhost:8080/health > /dev/null &
done
wait

# Should complete in reasonable time
sleep 5
# If still running processes, FAIL "Too slow"
```

### 7. End-to-End Test (20 minutes)
**Goal:** Complete user workflow

```bash
# 1. Fresh install scenario
cd /tmp
git clone aimux
cd aimux
cmake -S . -B build
cmake --build build

# 2. Configure with at least one real provider
cat > config.json <<EOF
{
  "providers": {
    "synthetic": {
      "name": "synthetic",
      "api_key": "test-key",
      "base_url": "http://localhost:8081",
      "enabled": true
    }
  }
}
EOF

# 3. Start services
./build/claude_gateway --config config.json &
GATE_PID=$!
./build/test_dashboard &
DASH_PID=$!

# 4. Test dashboard accessible
curl -s http://localhost:18080/health || FAIL "Dashboard inaccessible"

# 5. Test gateway working
curl -X POST http://localhost:8080/anthropic/v1/messages \
  -d '{"model":"claude-3-sonnet-20240229","max_tokens":5,"messages":[{"role":"user","content":"hi"}]}' \
  | grep -q "content" || FAIL "Gateway not routing"

# 6. Cleanup
kill $GATE_PID $DASH_PID
```

## Test Execution

### Before Every Commit
```bash
./qa/run_smoke_tests.sh
```

### Before Every PR
```bash
./qa/run_full_tests.sh
```

### Before Every Release
```bash
./qa/run_release_tests.sh
```

## Success Criteria

- ✅ Build completes with zero warnings
- ✅ All services start with valid config, abort with invalid config
- ✅ Real requests route through to actual providers
- ✅ Error responses are appropriate (400/500/503)
- ✅ Dashboard shows real provider status
- ✅ Fresh install scenario works from scratch

### Failure Criteria

- ❌ Build fails or has warnings
- ❌ Services start with no config (should abort)
- ❌ All requests return "PROVIDER_NOT_FOUND"
- ❌ Config validation inconsistent
- ❌ Services crash on invalid input
- ❌ Fresh install scenario fails

## Reality Check

If these tests fail, **fix the core functionality** before adding more features. There's no point in advanced error handling, monitoring, testing frameworks, or documentation if the basic routing doesn't work.

**Core functionality first, polish later.**