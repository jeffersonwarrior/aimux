# Aimux Core Functionality TODO

## Mission Statement
Make the router actually route requests to AI providers. Everything else is secondary.

## CURRENT STATUS
- Sophisticated architecture: ✅ Complete
- Beautiful documentation: ✅ Complete
- Advanced testing framework: ✅ Complete
- Enterprise-grade error handling: ✅ Complete
- **Actually routes requests:** ❌ BROKEN

### Core Problem
All requests return "PROVIDER_NOT_FOUND" because the provider system doesn't actually work.

---

## 🚨 IMMEDIATE CRITICAL FIXES (BLOCKER)

### 1. Fix ProviderFactory::create_provider
**Issue:** Provider creation doesn't work - returns no providers
**Files:** `src/providers/provider_impl.cpp`
**Action Required:**
- [ ] Debug ProviderFactory::create_provider implementation
- [ ] Ensure providers are actually registered in factory
- [ ] Fix provider instantiation logic
- [ ] Test that providers are created successfully

### 2. Fix Provider Configuration Loading
**Issue:** Config shows "loaded" but providers remain empty
**Files:** Configuration loading logic
**Action Required:**
- [ ] Debug configuration parsing
- [ ] Ensure provider configs are correctly loaded
- [ ] Fix provider JSON parsing logic
- [ ] Test config leads to actual provider objects

### 3. Fix GatewayManager Provider Selection
**Issue:** GatewayManager can't find loaded providers
**Files:** `src/gateway/gateway_manager.cpp`
**Action Required:**
- [ ] Debug provider selection logic
- [ ] Ensure GatewayManager can access loaded providers
- [ ] Fix provider lookup mechanism
- [ ] Test routing attempts work

### 4. Fix Real API Implementations
**Issue:** Provider implementations may be mocks or incomplete
**Files:** Individual provider files
**Action Required:**
- [ ] Review CerebrasProvider implementation
- [ ] Review ZAIProvider implementation
- [ ] Review MiniMaxProvider implementation
- [ ] Review SyntheticProvider implementation
- [ ] Ensure they make real HTTP requests

---

## 🔧 BASIC FUNCTIONALITY VERIFICATION

### 5. End-to-End Request Test
**Goal:** One actual request flows through the system
**Test:**
```bash
# Start gateway with working config
./build/claude_gateway --config config.json &

# Send real request
curl -X POST http://localhost:8080/anthropic/v1/messages \
  -H "Content-Type: application/json" \
  -d '{"model":"claude-3-sonnet-20240229","max_tokens":5,"messages":[{"role":"user","content":"hi"}]}'

# Should get response, not PROVIDER_NOT_FOUND
```

### 6. Provider Status Verification
**Goal:** Dashboard shows real provider status
**Test:**
```bash
curl http://localhost:8080/providers
# Should show loaded providers, not empty array
```

### 7. Configuration Validation
**Goal:** Config system works correctly
**Test:**
```bash
# Valid config works
./build/claude_gateway --config config.json

# Invalid config fails
./build/claude_gateway --config invalid.json
# Should abort with error code != 0
```

---

## 🎯 SUCCESS CRITERIA

### Must Achieve Before Any Other Work:
1. ✅ **Request Routing Works** - Real requests reach real providers
2. ✅ **Providers Load** - Config leads to actual provider objects
3. ✅ **Responses Return** - Get content, not errors
4. ✅ **Providers Visible** - Dashboard shows loaded providers
5. ✅ **Config Validation** - Good configs work, bad configs fail

### Test Evidence Required:
- [ ] Curl request returns response with "content" field
- [ ] `/providers` endpoint shows non-empty provider list
- [ ] Test suite passes actual integration tests
- [ ] No "PROVIDER_NOT_FOUND" in logs with valid config

---

## 🚫 WORK STOPPED UNTIL

**ALL of the above CRITICAL fixes are complete AND verified working.**

No new features, no documentation, no "performance optimizations", no "security hardening".

**Nothing else matters until the router actually routes.**

---

## 🧪 DEBUGGING APPROACH

### Step 1: Verify Factory
```cpp
// Add debugging to ProviderFactory::create_provider
std::cout << "Creating provider: " << name << std::endl;
// Debug provider registration map
std::cout << "Available providers: " << registered_providers_.size() << std::endl;
```

### Step 2: Verify Config Loading
```cpp
// Add debugging to config parsing
std::cout << "Loading config from: " << config_path << std::endl;
// Debug provider configs
std::cout << "Found providers in config: " << provider_configs.size() << std::endl;
```

### Step 3: Verify Gateway Integration
```cpp
// Add debugging to GatewayManager
std::cout << "GatewayManager providers: " << providers_.size() << std::endl;
// Debug provider selection
std::cout << "Selected provider: " << selected_provider_name << std::endl;
```

### Step 4: Test With Synthetic
Start with simplest provider (Synthetic) to get basic routing working, then expand to real providers.

---

## 📋 WORK ORDER

1. **FIX PROVIDER CREATION** - Make factory work
2. **FIX CONFIG LOADING** - Get providers from config to factory
3. **FIX GATEWAY INTEGRATION** - Connect factory to gateway
4. **TEST WITH SYNTHETIC** - Get one provider working
5. **INTEGRATION TEST** - Verify full request flow
6. **EXPAND TO REAL PROVIDERS** - Add real API implementations

---

## 🎊 DEFINITION OF DONE

**System is ready when:**
A new person can:
1. Clone the repo
2. Build successfully
3. Create simple config with synthetic provider
4. Start the service
5. Send a request
6. Get a response (not "PROVIDER_NOT_FOUND")

That's it. That's the entire definition of "done" for this TODO.

---

**Priority:** FIX CORE FUNCTIONALITY FIRST
**Timeline:** Until it works
**Anything else:** IGNORED

*Created: November 14, 2025*
*Priority: CRITICAL - FIXES BEFORE ANY OTHER WORK*