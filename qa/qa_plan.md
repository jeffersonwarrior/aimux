# Aimux QA Plan

## Principle
**Core functionality works first, everything else is secondary.**

## QA Gates

### Gate 1: Code Review (Pre-merge)
**Question:** Does this change break the basic promise?

**Checklist:**
- [ ] Code still compiles
- [ ] Existing tests still pass
- [ ] If provider-related: does it still route requests?
- [ ] If config-related: does validation still work?
- [ ] If error handling: does it provide useful error messages?

**Auto-fail if:**
- Adds new features while core routing is broken
- Removes error checking
- Makes configuration validation weaker
- Adds complexity without addressing core issues

### Gate 2: Build Verification (Post-merge)
**Question:** Does the system actually build and run?

**Automated Checks:**
```bash
# 1. Build verification
cmake --build build || BLOCK

# 2. Basic startup
timeout 5 ./build/claude_gateway --help || BLOCK

# 3. Smoke tests
./qa/run_smoke_tests.sh || BLOCK
```

### Gate 3: Functionality Verification (Nightly)
**Question:** Does the core promise still hold?

**Critical Tests:**
```bash
# 1. Provider existence
curl -s http://localhost:8080/providers | jq '.providers | length' | grep -q "^[1-9]" || FAIL

# 2. Request routing
curl -X POST http://localhost:8080/anthropic/v1/messages \
  -d '{"model":"test","max_tokens":5,"messages":[{"role":"user","content":"test"}]}' \
  | grep -q "content" || FAIL

# 3. Config validation
./build/claude_gateway --config /nonexistent.json
[ "$?" -ne 0 ] || FAIL
```

### Gate 4: Integration Testing (Weekly)
**Question:** Can a user actually use this?

**End-to-End Workflow:**
1. Fresh clone and build
2. Configure with real provider
3. Start services
4. Send real request
5. Get real response

If any step fails: **BLOCK deployment**

## QA Process

### Daily Checklist
- [ ] Build completes cleanly
- [ ] Smoke tests pass
- [ ] Core routing works
- [ ] No new critical bugs

### Release Checklist
- [ ] All QA gates passed
- [ ] Full test suite runs
- [ ] Performance not regressed
- [ ] Documentation matches reality
- [ ] Fresh install tested

## Quality Standards

### MUST HAVE (Core Promise)
1. **Buildable Code** - Zero warnings, clean compilation
2. **Working Routing** - Real requests reach real providers
3. **Proper Errors** - Invalid requests get proper HTTP codes
4. **Config Validation** - Bad configs cause startup abort
5. **Useful Logs** - Errors help debug problems

### NICE TO HAVE (Polish)
1. Performance optimization
2. Advanced monitoring
3. Comprehensive testing
4. Beautiful documentation
5. Security hardening

## Block Criteria

**IMMEDIATE BLOCK** if any of these fail:
- Build doesn't complete
- Service crashes on startup
- All requests return "PROVIDER_NOT_FOUND"
- Config validation inconsistent
- Fresh clone doesn't work

## Escalation Path

### Level 1: Build Break
- **Who:** Developer who broke it
- **When:** Immediately
- **Action:** Fix before any other work

### Level 2: Core Functionality Broken
- **Who:** Development team
- **When:** Within 24 hours
- **Action:** All hands on deck

### Level 3: Quality Issue
- **Who:** Core maintainer
- **When:** Next release
- **Action:** Fix or document limitation

## QA Tools

### Automation
```bash
# Continuous integration
./qa/ci_pipeline.sh    # Every PR
./qa/nightly_test.sh   # Every night
./qa/release_test.sh   # Before release
```

### Manual Testing
```bash
# Quick sanity check
./qa/smoke_test.sh

# Full verification
./qa/full_verification.sh

# User scenario testing
./qa/user_scenario_test.sh
```

### Monitoring
```bash
# Health checks
curl http://localhost:8080/health

# Provider status
curl http://localhost:8080/providers

# Metrics
curl http://localhost:8080/metrics
```

## Documentation Updates

### Required with every fix:
- [ ] What was broken
- [ ] How it was fixed
- [ ] How to test it
- [ ] How to prevent regression

### Feature additions:
- [ ] What the feature does
- [ ] How to configure it
- [ ] How to test it works
- [ ] Known limitations

## Reality Check

This QA plan exists because we built a sophisticated system that **looked impressive but didn't work**.

**Rule #1:** Core functionality MUST work before adding anything else.

**Rule #2:** If routing breaks, stop all other work until it's fixed.

**Rule #3:** No "it works on my machine" - must work consistently.

**Rule #4:** Documentation must match reality, not aspirations.

Follow this plan and you'll never again deliver a beautiful system that does nothing useful.