# AIMUX v2.1 C++ Prettifier - Latest Code References

**Reference Date**: November 24, 2025
**Version**: 2.1.0
**Purpose**: Indexed code locations for all major components with precise line numbers

---

## Table of Contents

1. [Core Plugin Infrastructure](#core-plugin-infrastructure)
2. [Provider-Specific Formatters](#provider-specific-formatters)
3. [Gateway Integration](#gateway-integration)
4. [Tool Call Processing](#tool-call-processing)
5. [Streaming Support](#streaming-support)
6. [TOON Format System](#toon-format-system)
7. [Test Infrastructure](#test-infrastructure)
8. [Configuration System](#configuration-system)
9. [Security Features](#security-features)
10. [Utility Functions](#utility-functions)

---

## Core Plugin Infrastructure

### PrettifierPlugin Interface (Abstract Base Class)

**File**: `/home/aimux/include/aimux/prettifier/prettifier_plugin.hpp`

#### Class Definition
- **Lines 104-410**: Complete PrettifierPlugin abstract base class
- **Lines 85-89**: RAII compliance documentation and requirements
- **Lines 106**: Virtual destructor declaration

#### Core Processing Methods
- **Lines 110-123**: `preprocess_request()` method signature and documentation
  - Thread safety requirements (line 120)
  - Error handling contract (line 121)
- **Lines 125-141**: `postprocess_response()` method signature and documentation
  - Thread safety requirements (line 136)
  - Performance requirements (line 137)

#### Plugin Metadata Methods
- **Lines 145-150**: `get_name()` - Unique plugin identifier
- **Lines 152-156**: `version()` - Semantic version string
- **Lines 158-164**: `description()` - Human-readable description
- **Lines 166-171**: `supported_formats()` - Input format list
- **Lines 173-178**: `output_formats()` - Output format list
- **Lines 180-186**: `supported_providers()` - Compatible providers
- **Lines 188-192**: `capabilities()` - Plugin capability list

#### Streaming Support
- **Lines 195-208**: `begin_streaming()` - Initialize streaming session
  - Default implementation (lines 205-208)
- **Lines 210-231**: `process_streaming_chunk()` - Process partial responses
  - Default passthrough implementation (lines 224-231)
- **Lines 233-247**: `end_streaming()` - Finalize streaming
  - Default empty implementation (lines 242-247)

#### Configuration and Validation
- **Lines 249-263**: `configure()` - Runtime configuration
  - Default no-op implementation (lines 260-263)
- **Lines 265-276**: `validate_configuration()` - Config validation
  - Default always-valid implementation (lines 273-276)
- **Lines 278-286**: `get_configuration()` - Current config getter
  - Default empty config (lines 283-286)

#### Metrics and Monitoring
- **Lines 288-298**: `get_metrics()` - Performance metrics
  - Default no metrics (lines 295-298)
- **Lines 300-307**: `reset_metrics()` - Reset counters
  - Default no-op (lines 305-307)

#### Health and Diagnostics
- **Lines 309-325**: `health_check()` - Plugin health status
  - Default healthy status (lines 319-325)
- **Lines 327-341**: `get_diagnostics()` - Diagnostic information
  - Default basic diagnostics (lines 335-341)

#### Protected Utility Methods
- **Lines 346-357**: `create_success_result()` - Success result helper
- **Lines 359-375**: `create_error_result()` - Error result helper
- **Lines 377-383**: `extract_tool_calls()` - Common tool call extraction
- **Lines 385-391**: `validate_json()` - Safe JSON validation

#### Supporting Structures
- **Lines 21-31**: `ToolCall` structure definition
  - Tool name, ID, parameters, result, status, timestamp
  - JSON serialization methods
- **Lines 35-46**: `ProcessingContext` structure
  - Provider name, model, format, streaming mode, config
- **Lines 50-64**: `ProcessingResult` structure
  - Success status, content, format, tool calls, reasoning, metadata

#### Factory Pattern
- **Lines 401-407**: `PluginFactory` typedef for plugin creation

---

### PluginRegistry Implementation

**File**: `/home/aimux/src/prettifier/plugin_registry.cpp`

#### PluginManifest Methods
- **Lines 14-28**: `to_json()` - Serialize manifest to JSON
- **Lines 30-44**: `from_json()` - Deserialize manifest from JSON
- **Lines 46-51**: `validate()` - Manifest validation with regex

#### PluginMetadata Methods
- **Lines 54-65**: `to_json()` - Serialize metadata to JSON
  - Includes load time, usage stats, enabled status

#### PluginRegistry Core
- **Lines 68-75**: Constructor with CacheConfig
  - Cache directory creation (line 72)
  - Cache loading (line 74)
- **Lines 77-84**: Default constructor
- **Lines 86-97**: Destructor with cleanup
  - **Lines 91-92**: **CRITICAL** - Mutex-protected cleanup for thread safety
  - Exception-safe cleanup (lines 94-96)

#### Directory Management
- **Lines 99-112**: `add_plugin_directory()` - Add plugin search path
  - Path existence validation (lines 101-106)
  - Mutex protection (line 107)
  - Duplicate prevention (lines 108-111)
- **Lines 114-122**: `remove_plugin_directory()` - Remove search path
  - Mutex protection (line 115)

#### Plugin Registration
- **Lines 124-169**: `register_plugin()` - Register new plugin
  - Result structure creation (lines 126-132)
  - Manifest validation (lines 135-138)
  - **Line 140**: Mutex lock for thread-safe registration
  - Metadata creation (lines 142-148)
  - Plugin storage (lines 150-151)
  - Callback invocation (lines 156-162)
  - Error handling (lines 164-166)

#### Plugin Retrieval
- **Lines 171-179**: `get_prettifier()` - Get plugin by name
  - **Line 172**: Mutex lock for thread-safe access
  - Usage stats update (line 177)
- **Lines 181-191**: `get_all_plugins()` - Get all enabled plugins
  - **Line 182**: Mutex lock
  - Enabled filter (line 186)

#### Plugin Management
- **Lines 193-214**: `unload_plugin()` - Remove plugin
  - **Line 194**: Mutex lock
  - Callback notification (lines 200-205)
  - Cache cleanup (lines 209-212)
- **Lines 216-223**: `get_plugin_metadata()` - Metadata lookup
  - **Line 217**: Mutex lock
- **Lines 225-228**: `get_all_metadata()` - All plugin metadata
  - **Line 226**: Mutex lock

#### Status and Metrics
- **Lines 230-242**: `get_status()` - Registry status
  - **Line 231**: Mutex lock for safe status read
  - Plugin counts, directory count, cache statistics

#### Internal Methods
- **Lines 244-250**: `update_usage_stats()` - Update plugin usage
  - Last used timestamp, usage counter
- **Lines 252-254**: `persist_cache()` - Save cache (TODO)
- **Lines 256-258**: `load_cache()` - Load cache (TODO)
- **Lines 260-270**: `generate_plugin_id()` - UUID generation
  - Random device initialization (lines 261-263)
  - 32-character hex string (lines 264-269)
- **Lines 272-280**: `is_cache_expired()` - Check cache TTL
- **Lines 282-284**: `cleanup_expired_cache()` - Remove old entries (TODO)
- **Lines 286-288**: `validate_manifest()` - Manifest validation wrapper
- **Lines 290-295**: `load_plugin_from_manifest()` - Load plugin (TODO)
- **Lines 297-310**: `scan_directory_manifests()` - Find manifest files
  - Recursive directory iteration (line 301)
  - manifest.json detection (line 302)

**Header File**: `/home/aimux/include/aimux/prettifier/plugin_registry.hpp`

---

## Provider-Specific Formatters

### OpenAI Formatter

**File**: `/home/aimux/src/prettifier/openai_formatter.cpp`

#### Pattern Definitions
- **Lines 16-24**: `OpenAIPatterns` constructor - Regex pattern initialization
  - **Line 17**: Function call pattern - `tool_calls` array detection
  - **Line 18**: Tool calls pattern - Nested function objects
  - **Line 19**: Legacy function pattern - Old `function_call` format
  - **Line 20**: JSON schema pattern - Structured output validation
  - **Line 21**: Structured output pattern - Basic JSON structure
  - **Line 22**: Streaming delta pattern - Partial response deltas
  - **Line 23**: Streaming function delta - Function call streaming

#### Formatter Core
- **Lines 26-30**: Constructor - Initialize patterns
  - Debug logging (line 29)
- **Lines 32-34**: `get_name()` - Returns "openai-gpt-formatter-v1.0.0"
- **Lines 36-38**: `version()` - Returns "1.0.0"
- **Lines 40-44**: `description()` - Comprehensive formatter description
  - GPT-4 function calling support
  - JSON structured outputs
  - Legacy format compatibility

#### Supported Formats
- **Lines 46-50**: `supported_formats()` - Returns format list:
  1. chat-completion
  2. function-calling
  3. structured-output
  4. json-mode
  5. streaming-chat
  6. streaming-function
  7. legacy-completion
  8. vision
  9. embeddings
  10. moderation

**Header File**: `/home/aimux/include/aimux/prettifier/openai_formatter.hpp`

#### OpenAIFormatter Class Declaration
- Pattern structures
- Method declarations
- Private helper methods

---

### Cerebras Formatter

**File**: `/home/aimux/src/prettifier/cerebras_formatter.cpp`

**Estimated Size**: ~600 lines

**Capabilities**:
- Llama 3.1 70B format support
- XML-based tool call extraction
- Fast inference optimization
- Streaming delta processing

**Header File**: `/home/aimux/include/aimux/prettifier/cerebras_formatter.hpp`

**Test File**: `/home/aimux/test/cerebras_formatter_test.cpp`

---

### Anthropic (Claude) Formatter

**File**: `/home/aimux/src/prettifier/anthropic_formatter.cpp`

**Estimated Size**: ~950 lines

**Capabilities**:
- Claude 3.5 Sonnet response handling
- Thinking mode extraction
- Tool use protocol support
- Markdown-embedded tool calls
- Multi-turn conversation handling

**Header File**: `/home/aimux/include/aimux/prettifier/anthropic_formatter.hpp`

**Test File**: `/home/aimux/test/anthropic_formatter_test.cpp`

---

### Synthetic Formatter

**File**: `/home/aimux/src/prettifier/synthetic_formatter.cpp`

**Estimated Size**: ~975 lines

**Capabilities**:
- Test provider simulation
- Configurable response patterns
- Debugging support
- Format validation reference

**Header File**: `/home/aimux/include/aimux/prettifier/synthetic_formatter.hpp`

**Test File**: `/home/aimux/test/synthetic_formatter_test.cpp`

---

## Gateway Integration

### GatewayManager Prettifier Integration

**File**: `/home/aimux/src/gateway/gateway_manager.cpp`

#### Initialization
- **Line 165**: Prettifier initialization called during startup
  - Called from `GatewayManager::initialize()`
  - After health monitoring starts
  - Before initialization complete flag

#### Prettifier Initialization Method
- **Lines 971-986**: `initialize_prettifier_formatters()` method
  - **Line 979**: Cerebras formatter instantiation
    - `std::make_shared<prettifier::CerebrasFormatter>()`
  - **Line 980**: OpenAI formatter instantiation
    - `std::make_shared<prettifier::OpenAIFormatter>()`
  - **Line 981**: Z.AI formatter mapping
    - Uses OpenAI formatter (format compatibility)
  - **Line 982**: Anthropic formatter instantiation
    - `std::make_shared<prettifier::AnthropicFormatter>()`
  - **Line 983**: Synthetic formatter instantiation
    - `std::make_shared<prettifier::SyntheticFormatter>()`
  - **Line 985**: Initialization logging
    - Logs formatter count
  - **Lines 987-990**: Exception handling
    - Error logging on initialization failure

#### Provider Lookup Method
- **Lines 990-1022**: `get_prettifier_for_provider()` - Provider-to-formatter mapping
  - **Lines 994-996**: Direct provider name lookup
    - Returns formatter if exact match found
  - **Lines 998-1005**: Lowercase name fallback
    - Converts provider name to lowercase
    - Retries lookup with lowercase name
  - **Lines 1007-1019**: Provider-specific fallbacks
    - Cerebras keywords: "cerebras", "llama"
    - OpenAI keywords: "openai", "gpt"
    - Anthropic keywords: "anthropic", "claude"
    - Synthetic keywords: "synthetic", "test"
  - **Line 1021**: Default fallback
    - Returns synthetic formatter if no match

**Header File**: `/home/aimux/include/aimux/gateway/gateway_manager.hpp`

---

## Tool Call Processing

### ToolCall Structure

**File**: `/home/aimux/include/aimux/prettifier/prettifier_plugin.hpp`

#### Structure Definition
- **Lines 21-31**: `ToolCall` struct
  - **Line 22**: `std::string name` - Tool function name
  - **Line 23**: `std::string id` - Unique tool call identifier
  - **Line 24**: `nlohmann::json parameters` - Tool arguments
  - **Line 25**: `std::optional<nlohmann::json> result` - Tool execution result
  - **Line 26**: `std::string status` - Execution status
    - Valid values: "pending", "executing", "completed", "failed"
  - **Line 27**: `std::chrono::system_clock::time_point timestamp` - Call time
  - **Line 29**: `to_json()` method - JSON serialization
  - **Line 30**: `from_json()` static method - JSON deserialization

### Tool Call Extractor

**File**: `/home/aimux/src/prettifier/tool_call_extractor.cpp`

**Estimated Size**: ~650 lines

**Capabilities**:
- XML format extraction (Cerebras)
- JSON format extraction (OpenAI)
- Markdown pattern extraction (Claude)
- Multi-call scenario handling
- Parameter validation
- Type conversion safety

**Header File**: `/home/aimux/include/aimux/prettifier/tool_call_extractor.hpp`

**Test File**: `/home/aimux/test/tool_call_extractor_test.cpp` (if exists)

---

## Streaming Support

### StreamingProcessor

**File**: `/home/aimux/src/prettifier/streaming_processor.cpp`

**Estimated Size**: ~700 lines

**Capabilities**:
- Async/await patterns
- Backpressure handling
- Buffer management
- Chunk boundary handling
- Multi-provider streaming
- Delta accumulation

**Header File**: `/home/aimux/include/aimux/prettifier/streaming_processor.hpp`

**Test File**: `/home/aimux/test/streaming_processor_test.cpp`

### Streaming Interface (PrettifierPlugin)

**File**: `/home/aimux/include/aimux/prettifier/prettifier_plugin.hpp`

#### Streaming Methods
- **Lines 195-208**: `begin_streaming()` - Initialize streaming session
  - Context parameter for session configuration
  - Returns success boolean
  - Default implementation returns true (line 207)

- **Lines 210-231**: `process_streaming_chunk()` - Process partial response
  - Chunk string parameter
  - is_final flag for last chunk detection
  - Processing context
  - Returns ProcessingResult with streaming flag
  - Default passthrough implementation (lines 224-231)

- **Lines 233-247**: `end_streaming()` - Finalize streaming session
  - Context parameter
  - Returns final ProcessingResult
  - Default empty success result (lines 242-247)

**Usage Pattern**:
```cpp
plugin->begin_streaming(context);
while (has_chunks) {
    auto chunk = get_next_chunk();
    auto result = plugin->process_streaming_chunk(chunk, is_final, context);
    // Process result
}
auto final = plugin->end_streaming(context);
```

---

## TOON Format System

### TOON Formatter

**File**: `/home/aimux/src/prettifier/toon_formatter.cpp`

**Estimated Size**: ~475 lines

**Capabilities**:
- Token-efficient serialization (30-60% reduction vs JSON)
- AI-optimized structure
- Indentation-based syntax
- Schema-aware validation
- Type constraints
- Length validation

**Header File**: `/home/aimux/include/aimux/prettifier/toon_formatter.hpp`

**Test File**: `/home/aimux/test/toon_formatter_test.cpp`

### TOON Format Benefits

From `/home/aimux/codedocs/README.md:25-30`:
- **Token-efficient**: 30-60% fewer tokens than JSON for uniform data
- **AI-optimized**: Structured for LLM comprehension with explicit validation
- **Compact**: Human-readable with indentation-based syntax
- **Schema-aware**: Built-in length validation and type constraints

**Example TOON Structure**:
```
provider: cerebras
model: llama-3.1-70b
response:
  content: "Assistant response here"
  tool_calls:
    - name: search_web
      params:
        query: "AI frameworks"
  metadata:
    tokens: 150
    latency_ms: 245
```

---

## Test Infrastructure

### Test File Organization

**Base Path**: `/home/aimux/test/`

#### Core Plugin Tests
1. **prettifier_plugin_test.cpp** - Plugin interface compliance
   - **Lines 72-81**: Interface compliance test
   - **Lines 84-88**: Abstract class behavior test
   - **Lines 91-100**: Polymorphic behavior test
   - Test classes: MockPrettifierPlugin (lines 13-24), TestPlugin (lines 27-59)

2. **prettifier_config_test.cpp** - Configuration validation

3. **toon_formatter_test.cpp** - TOON serialization tests

#### Provider Formatter Tests
4. **cerebras_formatter_test.cpp** - Cerebras formatting tests
5. **openai_formatter_test.cpp** - OpenAI/GPT formatting tests
6. **anthropic_formatter_test.cpp** - Claude formatting tests
7. **synthetic_formatter_test.cpp** - Synthetic provider tests

#### Integration Tests
8. **phase2_integration_test.cpp** - Phase 2 integration
9. **phase3_integration_test.cpp** - Phase 3 integration
10. **streaming_processor_test.cpp** - Streaming functionality

#### Security Tests
11. **security_test.cpp** - General security validation
12. **security_regression_test.cpp** - Security regression tests
13. **cli_security_test.cpp** - CLI security checks

#### Safety Tests
14. **thread_safety_test.cpp** - Concurrency tests
15. **memory_safety_test.cpp** - Memory leak detection

#### Distribution Tests
16. **plugin_downloader_test.cpp** - Plugin download functionality
17. **github_registry_test.cpp** - GitHub registry integration
18. **version_resolver_test.cpp** - Dependency resolution

#### Advanced Feature Tests
19. **ab_testing_framework_test.cpp** - A/B testing
20. **metrics_collector_test.cpp** - Metrics collection
21. **cli_test.cpp** - CLI interface tests

### Test Framework

**Framework**: Google Test (gtest) + Google Mock (gmock)
**Test Pass Rate**: 98.8% (83 of 84 tests passing)
**Total Test Files**: 21
**Estimated Test Code**: ~3,000-4,000 lines

### Test Execution

**Build and Test Commands**:
```bash
cd /home/aimux/build
cmake ..
make -j$(nproc)
ctest --output-on-failure -V
```

---

## Configuration System

### ProductionConfig Integration

**Status**: Integrated but configuration loading incomplete

**Requirements** (from VERSION2.1.TODO.md):
- Config file support for prettifier settings
- Environment variable overrides
- Per-provider format selection
- Output format configuration (TOON, JSON, etc.)

**Environment Variables** (Planned):
- `AIMUX_PRETTIFIER_ENABLED` - Enable/disable prettifier
- `AIMUX_OUTPUT_FORMAT` - Default output format

**Integration Point**: `src/gateway/gateway_manager.cpp:165`

---

## Security Features

### Security Test Coverage

**Test Files**:
1. `/home/aimux/test/security_test.cpp` - Basic security validation
2. `/home/aimux/test/security_regression_test.cpp` - Regression tests
3. `/home/aimux/test/cli_security_test.cpp` - CLI security

### Security Gaps (CRITICAL)

**Missing Features** (from Audit-2.1.md):
- ❌ OWASP Top 10 protection
- ❌ XSS prevention
- ❌ SQL injection blocking
- ❌ PII detection/redaction
- ❌ Command injection prevention
- ⏸️ Input validation (partial)
- ⏸️ Size limits (partial)

**Security Sanitization Plugins**: NOT IMPLEMENTED

**Status**: Security testing exists, but security implementation incomplete

---

## Utility Functions

### JSON Validation

**File**: `/home/aimux/include/aimux/prettifier/prettifier_plugin.hpp`

- **Lines 385-391**: `validate_json()` protected method
  - Safe JSON parsing
  - Returns `std::optional<nlohmann::json>`
  - Handles malformed JSON gracefully

### Tool Call Extraction

**File**: `/home/aimux/include/aimux/prettifier/prettifier_plugin.hpp`

- **Lines 377-383**: `extract_tool_calls()` protected method
  - Common pattern matching
  - Supports multiple formats
  - Returns `std::vector<ToolCall>`

### Result Creation Helpers

**File**: `/home/aimux/include/aimux/prettifier/prettifier_plugin.hpp`

- **Lines 346-357**: `create_success_result()` - Success helper
  - Takes processed content string
  - Returns successful ProcessingResult

- **Lines 359-375**: `create_error_result()` - Error helper
  - Takes error message and optional error code
  - Returns failed ProcessingResult with metadata

---

## File Statistics

### Implementation Code

**Total Implementation Lines**: 7,138 lines
**Total Header Lines**: 4,166 lines
**Total C++ Code**: 11,304 lines

### File Sizes

**Prettifier Implementation Files** (`/home/aimux/src/prettifier/`):
1. plugin_registry.cpp: 313 lines
2. prettifier_plugin.cpp: ~200 lines (estimated)
3. toon_formatter.cpp: ~475 lines
4. markdown_normalizer.cpp: ~500 lines
5. cerebras_formatter.cpp: ~600 lines
6. openai_formatter.cpp: ~850 lines
7. anthropic_formatter.cpp: ~950 lines
8. synthetic_formatter.cpp: ~975 lines
9. tool_call_extractor.cpp: ~650 lines
10. streaming_processor.cpp: ~700 lines

**Prettifier Header Files** (`/home/aimux/include/aimux/prettifier/`):
1. prettifier_plugin.hpp: 410 lines
2. plugin_registry.hpp: ~350 lines (estimated)
3. toon_formatter.hpp: ~300 lines (estimated)
4. markdown_normalizer.hpp: ~250 lines (estimated)
5. cerebras_formatter.hpp: ~400 lines (estimated)
6. openai_formatter.hpp: ~450 lines (estimated)
7. anthropic_formatter.hpp: ~500 lines (estimated)
8. synthetic_formatter.hpp: ~500 lines (estimated)
9. tool_call_extractor.hpp: ~300 lines (estimated)
10. streaming_processor.hpp: ~400 lines (estimated)

---

## Build System References

### CMake Configuration

**File**: `/home/aimux/CMakeLists.txt`

**C++ Standard**: C++23 (line 6: `set(CMAKE_CXX_STANDARD 23)`)

**Prettifier Targets**:
- prettifier_plugin library
- prettifier_plugin_tests executable
- All formatter libraries

**Dependencies**:
- nlohmann/json (JSON parsing)
- Google Test (testing framework)
- Google Mock (mocking framework)

---

## Integration Points

### GatewayManager → Prettifier

**Initialization Flow**:
1. `GatewayManager::initialize()` called
2. Line 165: `initialize_prettifier_formatters()` invoked
3. Lines 979-983: All formatters instantiated
4. Formatters stored in `prettifier_formatters_` map

**Lookup Flow**:
1. Request arrives with provider name
2. `get_prettifier_for_provider(provider_name)` called
3. Lines 994-1021: Formatter lookup with fallbacks
4. Formatter returned for request processing

**Missing Integration**:
- ❌ Prettifier not called in `process_request()` pipeline
- ❌ No end-to-end request → prettify → response flow
- ❌ Performance monitoring not integrated

---

## Key Architectural Patterns

### Design Patterns Used

1. **Strategy Pattern**: PrettifierPlugin interface with multiple implementations
2. **Factory Pattern**: PluginFactory typedef for plugin creation
3. **Registry Pattern**: PluginRegistry for plugin management
4. **RAII**: Smart pointers and automatic resource management
5. **Template Method**: Base class provides default implementations

### Thread Safety

**Mutex Usage**:
- PluginRegistry uses `std::mutex registry_mutex_` for all operations
- All map modifications protected by mutex lock
- Cache access protected by separate `cache_mutex_`

**Lock Pattern**:
```cpp
std::lock_guard<std::mutex> lock(registry_mutex_);
// Critical section
```

### Memory Management

**Smart Pointer Usage**:
- `std::shared_ptr<PrettifierPlugin>` for plugin instances
- `std::unique_ptr` for internal state (patterns, buffers)
- Automatic cleanup in destructors
- Exception-safe resource management

---

## Performance Optimization Points

### Current Optimizations

1. **Regex Precompilation** (openai_formatter.cpp:16-24)
   - All regex patterns compiled during construction
   - `std::regex::optimize` flag used

2. **Cache System** (plugin_registry.cpp:68-84)
   - Plugin metadata cached
   - TTL-based cache expiration
   - Persistent cache support (TODO)

3. **Smart Pointer Sharing** (gateway_manager.cpp:979-983)
   - Single formatter instance per provider
   - Shared ownership model

### Performance Targets

From VERSION2.1.TODO.md:
- Plugin registry lookup: <1ms for 100 plugins
- Markdown normalization: <5ms per 1KB
- Tool extraction: <3ms per 1KB
- TOON serialization: <10ms per 1KB
- End-to-end overhead: <20ms per request

**Status**: Not yet benchmarked

---

## Error Handling Patterns

### Exception Safety

**PluginRegistry Destructor** (plugin_registry.cpp:86-97):
```cpp
try {
    // Cleanup operations
} catch (const std::exception& e) {
    // Error logging, no re-throw
}
```

**Formatter Initialization** (gateway_manager.cpp:987-990):
```cpp
try {
    // Initialize formatters
} catch (const std::exception& e) {
    aimux::error("Failed to initialize: " + std::string(e.what()));
}
```

### Error Result Pattern

**PrettifierPlugin** (prettifier_plugin.hpp:359-375):
```cpp
ProcessingResult create_error_result(
    const std::string& error_message,
    const std::string& error_code = "") const {
    ProcessingResult result;
    result.success = false;
    result.error_message = error_message;
    if (!error_code.empty()) {
        result.metadata["error_code"] = error_code;
    }
    return result;
}
```

---

## Logging and Debugging

### Log Macros

**OpenAI Formatter** (openai_formatter.cpp:8-10):
```cpp
#define LOG_ERROR(msg, ...) do { printf("[OPENAI ERROR] " msg "\n", ##__VA_ARGS__); } while(0)
#define LOG_DEBUG(msg, ...) do { printf("[OPENAI DEBUG] " msg "\n", ##__VA_ARGS__); } while(0)
```

**Usage**:
- Line 29: Debug log on formatter initialization
- Error logging throughout implementation

### Gateway Logging

**GatewayManager** (gateway_manager.cpp):
- Line 985: Info log for formatter initialization count
- Line 989: Error log for initialization failure
- Uses `aimux::info()`, `aimux::error()`, `aimux::warn()` functions

---

## Documentation References

### External Documentation

**Project Documentation** (`/home/aimux/codedocs/`):
1. `README.md` - Documentation index
2. `architecture.toon` - System architecture
3. `components.toon` - Component breakdown
4. `providers.toon` - Provider system
5. `version_2.1_status.toon` - v2.1 status report
6. `IMPLEMENTATION_VERIFICATION.md` - This verification report
7. `LATEST_CODE_REFERENCES.md` - This reference document

**Specification Documents** (`/home/aimux/`):
1. `VERSION2.1.TODO.md` - Master specification
2. `Audit-2.1.md` - System architecture audit
3. `CLAUDE.md` - Project overview
4. `PHASE1_COMPLETION_REPORT.md` - Phase 1 status
5. `PHASE2_COMPLETION_REPORT.md` - Phase 2 status

**QA Documentation** (`/home/aimux/qa/`):
- 19 QA planning and test specification documents
- Comprehensive test plans for all phases
- Security, performance, and regression test specifications

---

## Quick Reference Commands

### Build Commands

```bash
# Clean build
cd /home/aimux
rm -rf build
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)

# Run tests
ctest --output-on-failure -V

# Build specific target
make prettifier_plugin_tests

# Install
sudo make install
```

### Test Commands

```bash
# Run all tests
cd /home/aimux/build
ctest --output-on-failure -V

# Run specific test
./prettifier_plugin_tests

# Run with Valgrind
valgrind --leak-check=full ./prettifier_plugin_tests

# Run with ThreadSanitizer
cmake -DENABLE_TSAN=ON .. && make
./prettifier_plugin_tests
```

### Code Analysis Commands

```bash
# Line count
wc -l /home/aimux/src/prettifier/*.cpp
wc -l /home/aimux/include/aimux/prettifier/*.hpp

# Find function definitions
grep -n "^[a-zA-Z].*::" /home/aimux/src/prettifier/plugin_registry.cpp

# Search for pattern
grep -r "prettifier_formatters_" /home/aimux/src/

# Find test files
find /home/aimux/test -name "*_test.cpp"
```

---

## Index by Component

### A
- **Anthropic Formatter**: src/prettifier/anthropic_formatter.cpp, include/aimux/prettifier/anthropic_formatter.hpp

### C
- **Cerebras Formatter**: src/prettifier/cerebras_formatter.cpp, include/aimux/prettifier/cerebras_formatter.hpp
- **Configuration**: Configuration System section
- **CMake**: CMakeLists.txt (line 6 for C++ standard)

### G
- **Gateway Integration**: src/gateway/gateway_manager.cpp (lines 165, 971-1022)
- **Google Test**: Test Infrastructure section

### M
- **Markdown Normalizer**: src/prettifier/markdown_normalizer.cpp, include/aimux/prettifier/markdown_normalizer.hpp

### O
- **OpenAI Formatter**: src/prettifier/openai_formatter.cpp (lines 16-50), include/aimux/prettifier/openai_formatter.hpp

### P
- **PrettifierPlugin**: include/aimux/prettifier/prettifier_plugin.hpp (lines 104-410)
- **PluginRegistry**: src/prettifier/plugin_registry.cpp (lines 67-313), include/aimux/prettifier/plugin_registry.hpp
- **ProcessingContext**: include/aimux/prettifier/prettifier_plugin.hpp (lines 35-46)
- **ProcessingResult**: include/aimux/prettifier/prettifier_plugin.hpp (lines 50-64)

### S
- **Streaming Processor**: src/prettifier/streaming_processor.cpp, include/aimux/prettifier/streaming_processor.hpp
- **Streaming Interface**: include/aimux/prettifier/prettifier_plugin.hpp (lines 194-247)
- **Synthetic Formatter**: src/prettifier/synthetic_formatter.cpp, include/aimux/prettifier/synthetic_formatter.hpp

### T
- **TOON Formatter**: src/prettifier/toon_formatter.cpp, include/aimux/prettifier/toon_formatter.hpp
- **ToolCall**: include/aimux/prettifier/prettifier_plugin.hpp (lines 21-31)
- **Tool Call Extractor**: src/prettifier/tool_call_extractor.cpp, include/aimux/prettifier/tool_call_extractor.hpp
- **Tests**: /home/aimux/test/ directory (21 test files)

---

**Document Version**: 1.0
**Last Updated**: November 24, 2025
**Maintained By**: AIMUX Development Team
**Purpose**: Code navigation and reference for development and maintenance
