# Phase 1 Foundation QA Plan

## Overview
This document outlines comprehensive testing procedures for Phase 1 of the Prettifier Plugin System implementation. Phase 1 establishes the core infrastructure upon which all subsequent phases depend.

## Test Environment Setup
- **Build System**: CMake with vcpkg dependencies
- **Test Framework**: Google Test + Google Mock
- **Coverage**: Minimum 90% code coverage required
- **Performance**: Baseline benchmarks established
- **Regression**: Automated baseline comparisons

## Component 1: PluginRegistry Class Testing

### Unit Tests
```cpp
// test/plugin_registry_test.cpp
class PluginRegistryTest : public ::testing::Test {
protected:
    void SetUp() override {
        registry_ = std::make_unique<PluginRegistry>();
        test_plugin_path_ = std::filesystem::temp_directory_path() / "test_plugins";
        std::filesystem::create_directories(test_plugin_path_);
    }

    void TearDown() override {
        std::filesystem::remove_all(test_plugin_path_);
    }

    std::unique_ptr<PluginRegistry> registry_;
    std::filesystem::path test_plugin_path_;
};
```

#### Test Cases:
1. **JSON Manifest Validation**
   - Valid manifest files load successfully
   - Invalid JSON is rejected with appropriate error codes
   - Missing required fields are detected
   - Schema validation catches malformed structures
   - Semantic version validation (semver compliance)

2. **Plugin Discovery**
   - Registry discovers plugins in configured directories
   - Recursive scanning works for nested plugin structures
   - Duplicate plugin names are handled appropriately
   - Plugin permissions are validated before loading

3. **Plugin Registration**
   - Successfully registered plugins appear in registry
   - Version conflicts are detected and reported
   - Provider compatibility is validated
   - Disabled plugins are ignored but tracked

### Integration Tests
1. **Registry Persistence**
   - Plugin state persists across registry restarts
   - Configuration changes are applied correctly
   - Cache invalidation works on plugin updates

2. **Multi-threading Safety**
   - Concurrent plugin registration is thread-safe
   - Race conditions in plugin discovery are eliminated
   - Deadlocks do not occur under high load

### Performance Tests
```cpp
BENCHMARK_F(PluginRegistryBenchmark, Load100Plugins)(benchmark::State& state) {
    for (auto _ : state) {
        registry_->scan_directory(test_plugin_path_);
    }
}
```

**Acceptance Criteria**:
- Plugin discovery: <100ms for 100 plugins
- Memory usage: <10MB for registry with 200 plugins
- Thread safety: No data races under 100 concurrent operations

## Component 2: PrettifierPlugin Interface Testing

### Unit Tests
```cpp
// test/prettifier_plugin_test.cpp
class MockPrettifierPlugin : public PrettifierPlugin {
public:
    MOCK_METHOD(Response, preprocess_request, (const Request& request), (override));
    MOCK_METHOD(Response, postprocess_response, (const Response& response), (override));
    MOCK_METHOD(std::vector<std::string>, supported_formats, (), (const, override));
    MOCK_METHOD(std::string, version, (), (const, override));
};
```

#### Test Cases:
1. **Interface Compliance**
   - All plugins implement required virtual methods
   - Pure virtual functions are properly overridden
   - Interface contracts are maintained

2. **Abstract Class Behavior**
   - PrettifierPlugin cannot be instantiated directly
   - Derived classes properly inherit interface
   - Virtual function calls work correctly through base pointers

3. **Memory Management**
   - No memory leaks in plugin creation/destruction
   - RAII principles are followed
   - Smart pointers work correctly with plugin hierarchy

### Integration Tests
1. **Plugin Polymorphism**
   - Different plugin types work through common interface
   - Runtime plugin switching works correctly
   - Plugin factories create correct derived types

2. **Error Handling**
   - Exceptions in plugin methods are caught appropriately
   - Graceful degradation when plugins fail
   - Error propagation follows established patterns

## Component 3: TOON Format Serializer/Deserializer Testing

### Unit Tests
```cpp
// test/toon_formatter_test.cpp
class ToonFormatterTest : public ::testing::Test {
protected:
    ToonFormatter formatter_;

    SampleResponse create_sample_response() {
        return Response{
            .provider = "cerebras",
            .model = "zai-glm-4.6",
            .content = "```python\nprint('hello')\n```",
            .timestamp = std::chrono::system_clock::now(),
            .tokens_used = 245
        };
    }
};
```

#### Test Cases:
1. **TOON Serialization**
   - Valid responses serialize to correct TOON format
   - Special characters are properly escaped
   - Unicode content is handled correctly
   - Large responses (>1MB) serialize without issues

2. **TOON Deserialization**
   - Valid TOON format parses correctly
   - Invalid TOON syntax is rejected with helpful error messages
   - Missing required sections are detected
   - Extra unknown sections are ignored or flagged

3. **Format Validation**
   - TOON schema validation catches all violations
   - Line ending normalization works correctly
   - Whitespace handling matches specification

### Integration Tests
1. **Round-trip Conversion**
   - Response → TOON → Response preserves all data
   - Metadata integrity is maintained through conversion
   - Binary data (if any) survives serialization

2. **Cross-platform Compatibility**
   - TOON files work across Windows/Linux/macOS
   - File encoding issues are resolved
   - Path separators are handled correctly

### Performance Tests
**Acceptance Criteria**:
- Serialization: <10ms for typical 1KB response
- Deserialization: <5ms for typical TOON document
- Memory overhead: <2x original response size

## Component 4: Configuration Integration Testing

### Unit Tests
```cpp
// test/prettifier_config_test.cpp
class PrettifierConfigTest : public ::testing::Test {
protected:
    PrettifierConfig config_;

    void SetUp() override {
        config_.load_from_string(R"({
            "prettifiers": {
                "enabled": true,
                "default_prettifier": "markdown",
                "providers": {
                    "cerebras": {"prettifier": "enhanced_markdown"}
                }
            }
        })");
    }
};
```

#### Test Cases:
1. **Configuration Loading**
   - Valid JSON configuration loads successfully
   - Invalid configuration is rejected with clear error messages
   - Default settings are applied when configuration is missing
   - Environment variable overrides work correctly

2. **Configuration Validation**
   - All configuration parameters are validated against schema
   - Invalid provider names are rejected
   - Incompatible settings are detected

3. **Hot Reloading**
   - Configuration changes apply without restart
   - Invalid configuration changes are rolled back
   - Plugin reloading respects new configuration

### Integration Tests
1. **System Integration**
   - Prettifier configuration integrates with existing AIMUX config
   - Plugin registry respects configuration settings
   - CLI commands correctly update prettifier settings

2. **WebUI Integration**
   - Web dashboard can modify prettifier settings
   - Configuration changes persist across UI sessions
   - Real-time configuration validation in UI

## Regression Testing Plan

### Baseline Establishment
1. **Existing AIMUX Functionality**
   - All current tests must pass without modification
   - Performance benchmarks must not degrade more than 5%
   - Memory usage must not increase more than 10%

2. **API Compatibility**
   - Existing Router::route() method signature unchanged
   - Provider implementations maintain current behavior
   - Configuration file format remains backward compatible

### Automated Regression Suite
```bash
#!/bin/bash
# regression_test_phase1.sh

# Run existing test suite
ctest --output-on-failure

# Run new prettifier tests
./test_prettifier_phase1

# Performance benchmarks
./benchmark_prettifier_overhead

# Memory leak detection
valgrind --leak-check=full ./test_prettifier_phase1

# Integration tests
./test_prettifier_integration
```

### Critical Regression Tests
1. **No Breaking Changes**
   - All existing CLI commands work unchanged
   - WebUI functionality remains intact
   - Provider failover mechanisms unaffected

2. **Performance Regression**
   - Request latency increase <5ms
   - CPU usage increase <10%
   - Memory usage increase <20MB

3. **Stability Regression**
   - No increase in crash frequency
   - No memory leaks introduced
   - No deadlock situations created

## Test Data and Mock Objects

### Sample Plugin Manifests
```json
{
    "name": "test-plugin",
    "version": "1.0.0",
    "provider": "cerebras",
    "formats": ["markdown", "json"],
    "capabilities": ["syntax-highlighting"]
}
```

### Sample AI Responses for Testing
```cpp
std::vector<Response> create_test_responses() {
    return {
        Response{.provider="cerebras", .content="```python\nprint('hello')\n```"},
        Response{.provider="openai", .content="Here's the code:\n\n```js\nconsole.log('world');\n```"},
        Response{.provider="anthropic", .content="Let me explain:\n\n<reasoning>\nAnalysis...\n</reasoning>\n\n```cpp\n// code\n```"}
    };
}
```

## Continuous Integration Requirements

### GitHub Actions Pipeline
```yaml
name: Phase 1 Prettifier Tests
on: [push, pull_request]
jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - name: Setup Dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y cmake build-essential valgrind
      - name: Build and Test
        run: |
          mkdir build && cd build
          cmake ..
          make -j$(nproc)
          ctest --output-on-failure
      - name: Run Coverage
        run: |
          gcov -r ..
          lcov --capture --directory . --output-file coverage.info
      - name: Performance Benchmark
        run: ./benchmark_phase1_performance
```

## Success Criteria for Phase 1

### Functional Requirements
- [ ] PluginRegistry discovers and loads plugins correctly
- [ ] PrettifierPlugin interface is properly implemented
- [ ] TOON format serialization works bidirectionally
- [ ] Configuration integration is seamless
- [ ] All existing functionality remains intact

### Quality Requirements
- [ ] 90%+ code coverage on new components
- [ ] Zero memory leaks in Valgrind testing
- [ ] Performance degradation <5%
- [ ] All regression tests pass
- [ ] Documentation is complete and accurate

### Security Requirements
- [ ] No security vulnerabilities introduced
- [ ] Plugin sandboxing prevents malicious code execution
- [ ] Configuration validation prevents injection attacks
- [ ] File system access is properly restricted

## Final Sign-off Requirements

Before moving to Phase 2, the following must be completed:
1. All unit tests passing with 90%+ coverage
2. All integration tests passing
3. Performance benchmarks meeting criteria
4. Code review completed and approved
5. Security audit performed
6. Documentation updated
7. Regression test suite updated and passing
8. Demo of all Phase 1 functionality working end-to-end