# Phase 2 Core Plugins QA Plan

## Overview
This document outlines comprehensive testing procedures for Phase 2 of the Prettifier Plugin System implementation. Phase 2 builds upon the foundation established in Phase 1 to implement the core prettification plugins.

## Test Environment Setup
- **Dependencies**: Phase 1 components fully tested and integrated
- **Test Data**: Real AI responses from all supported providers
- **Mock Framework**: Provider-specific response generators
- **Performance**: Multi-provider throughput testing
- **Security**: Content sanitization and validation testing

## Component 1: Markdown Normalization Plugin Testing

### Unit Tests
```cpp
// test/markdown_normalizer_test.cpp
class MarkdownNormalizerTest : public ::testing::Test {
protected:
    MarkdownNormalizerPlugin normalizer_;

    struct TestCase {
        std::string input;
        std::string expected;
        std::string description;
    };

    std::vector<TestCase> create_markdown_test_cases() {
        return {
            {
                .input = "```python\nprint('hello')\n```",
                .expected = "```python\nprint('hello')\n```",
                .description = "Well-formed code block"
            },
            {
                .input = "```\ncode without language\n```",
                .expected = "```text\ncode without language\n```",
                .description = "Code block without language"
            },
            {
                .input = "Here's some code:\n\n```python\nprint('test')\n\n```\n\nMore text.",
                .expected = "Here's some code:\n\n```python\nprint('test')\n```\n\nMore text.",
                .description = "Code block with extra whitespace"
            }
        };
    }
};
```

#### Test Cases:
1. **Code Block Normalization**
   - Detects and standardizes code block fences
   - Handles nested code blocks correctly
   - Preserves language identifiers while normalizing syntax
   - Fixes incomplete or malformed code blocks

2. **Markdown Structure Cleanup**
   - Removes excessive blank lines
   - Normalizes heading levels
   - Fixes list formatting inconsistencies
   - Handles mixed markdown/plain text correctly

3. **Provider-Specific Patterns**
   - Cerebras: Fast but sometimes incomplete code blocks
   - OpenAI: Well-structured but sometimes overly verbose
   - Anthropic: Mixed reasoning and code blocks
   - Synthetic: Complex nested markdown structures

### Integration Tests
1. **Real Provider Responses**
   - Test with actual responses from all supported providers
   - Verify normalization improves code extraction
   - Ensure no valid content is lost during normalization

2. **Multi-language Support**
   - Python, JavaScript, C++, Rust, Go, etc.
   - Language-specific syntax highlighting integration
   - Mixed-language documents handled correctly

### Performance Tests
**Acceptance Criteria**:
- Normalization: <5ms for typical 1KB markdown
- Memory usage: <5MB for normalization buffers
- Throughput: 1000+ responses/second on single thread

## Component 2: Tool Call Extraction Plugin Testing

### Unit Tests
```cpp
// test/tool_call_extractor_test.cpp
class ToolCallExtractorTest : public ::testing::Test {
protected:
    ToolCallExtractorPlugin extractor_;

    struct ToolCallTestCase {
        std::string input;
        std::vector<ToolCall> expected_calls;
        std::string description;
    };

    std::vector<ToolCallTestCase> create_tool_call_test_cases() {
        return {
            {
                .input = R"(I'll call the function:
<function_calls>
{"name": "search_files", "arguments": {"pattern": "*.cpp", "directory": "/src"}}
</function_calls>)",
                .expected_calls = {
                    ToolCall{.name="search_files", .args={{"pattern", "*.cpp"}, {"directory", "/src"}}}
                },
                .description = "Standard tool call format"
            },
            {
                .input = R"(Using the tool:
```json
{"function": "execute_command", "parameters": {"command": "ls -la"}}
```)",
                .expected_calls = {
                    ToolCall{.name="execute_command", .args={{"command", "ls -la"}}}
                },
                .description = "JSON tool call in code block"
            }
        };
    }
};
```

#### Test Cases:
1. **Tool Call Format Detection**
   - Detects various tool call formats (XML, JSON, markdown)
   - Handles malformed tool calls gracefully
   - Extracts partial information from incomplete calls
   - Normalizes parameter naming conventions

2. **Parameter Extraction**
   - Properly parses complex nested JSON parameters
   - Handles string escaping in parameters
   - Validates required vs optional parameters
   - Type conversion and validation

3. **Multi-call Scenarios**
   - Extracts multiple tool calls from single response
   - Maintains call order and dependencies
   - Handles concurrent vs sequential call patterns

### Integration Tests
1. **Real-world Tool Call Patterns**
   - Test with actual tool calls from Claude, OpenAI, etc.
   - Verify extraction accuracy across provider formats
   - Validate parameter preservation through extraction

2. **Error Scenarios**
   - Malformed JSON doesn't crash extractor
   - Missing required parameters detected
   - Invalid parameter types handled gracefully

### Security Tests
1. **Injection Prevention**
   - SQL injection attempts in tool parameters
   - Command injection in execute calls
   - Path traversal in file operations
   - XXE attacks in XML tool calls

## Component 3: Provider-Specific Formatters Testing

### Unit Tests
```cpp
// test/provider_formatters_test.cpp
class ProviderFormattersTest : public ::testing::Test {
protected:
    std::map<std::string, std::unique_ptr<PrettifierPlugin>> formatters_;

    void SetUp() override {
        formatters_["cerebras"] = std::make_unique<CerebrasFormatter>();
        formatters_["openai"] = std::make_unique<OpenAIFormatter>();
        formatters_["anthropic"] = std::make_unique<AnthropicFormatter>();
        formatters_["synthetic"] = std::make_unique<SyntheticFormatter>();
    }
};
```

#### Test Cases per Provider:

**Cerebras Formatter**
1. **Speed-Optimized Responses**
   - Handles rapid fire responses with minimal formatting
   - Fixes incomplete code blocks common in fast responses
   - Preserves the speed advantage while ensuring quality

2. **zai-glm-4.6 Specific Patterns**
   - Chinese/English mixed content handling
   - Technical documentation formatting
   - Code snippet completion

**OpenAI Formatter**
1. **gpt-4o-mini Optimization**
   - Handles well-structured but verbose responses
   - Condenses explanations while preserving technical details
   - Maintains chat format compatibility

**Anthropic Formatter**
1. **Claude Response Patterns**
   - Separates thinking from final responses
   - Extracts reasoning blocks appropriately
   - Handles Claude's structured output format

**Synthetic Formatter**
1. **Kimi K2 Complex Reasoning**
   - Handles nested markdown structures
   - Preserves complex reasoning chains
   - Manages multi-step analysis formatting

### Integration Tests
1. **Cross-Provider Consistency**
   - Same request produces consistently formatted output across providers
   - TOON format is standardized regardless of source provider
   - Tool calls are normalized to common format

2. **Provider Switching**
   - Mid-conversation provider switches maintain formatting consistency
   - Mixed provider conversations are handled gracefully
   - Context preservation during provider changes

## Component 4: Streaming Format Support Testing

### Unit Tests
```cpp
// test/streaming_formatter_test.cpp
class StreamingFormatterTest : public ::testing::Test {
protected:
    StreamingFormatterPlugin streamer_;

    struct StreamChunk {
        std::string content;
        bool is_final;
        std::optional<ToolCall> tool_call;
    };

    std::vector<StreamChunk> create_stream_chunks() {
        return {
            {"Here's the", false, {}},
            {" ", false, {}},
            {"solution:", false, {}},
            {"\n\n", false, {}},
            {"```python", false, {}},
            {"\nprint('hello')", false, {}},
            {"\n```", true, {}}
        };
    }
};
```

#### Test Cases:
1. **Chunk Processing**
   - Partial markdown chunks don't break formatting
   - Code fences spanning multiple chunks handled correctly
   - Tool calls distributed across chunks are reconstructed
   - Final chunk triggers format validation

2. **Async Processing**
   - Stream processing doesn't block network I/O
   - Backpressure handling prevents memory overflow
   - Concurrent stream processing works correctly

3. **Progress Indicators**
   - Real-time formatting progress updates
   - Error detection during streaming
   - Partial format validation

### Integration Tests
1. **Real Streaming Scenarios**
   - Test with actual streaming responses from providers
   - Verify formatting quality matches non-streaming responses
   - Ensure tool calls work correctly in streaming mode

2. **Performance Under Load**
   - Multiple concurrent streams handled efficiently
   - Memory usage remains bounded during streaming
   - Latency impact of streaming formatting is minimal

## Regression Testing Plan

### Phase 1 Functionality Preservation
1. **Plugin Registry Integration**
   - All new plugins register correctly with Phase 1 registry
   - Plugin discovery finds new plugins
   - Configuration works with new plugin types

2. **TOON Format Consistency**
   - All plugins output valid TOON format
   - TOON schema validation catches all plugin outputs
   - Round-trip conversion works for all plugins

### Performance Regression Prevention
```bash
#!/bin/bash
# regression_test_phase2.sh

# Phase 1 regression tests
./run_phase1_regression_tests

# New plugin performance tests
./benchmark_plugin_performance --all-providers

# Streaming performance tests
./benchmark_streaming_overhead

# Memory usage under load
./stress_test_memory_usage --concurrent-streams 100
```

### Provider Behavior Regression
1. **Existing Provider Integrations**
   - No changes to existing provider implementations required
   - All existing tests continue to pass
   - Provider failover mechanisms unaffected

2. **API Compatibility**
   - Router::route() behavior unchanged when prettifiers disabled
   - Response format unchanged when prettifiers disabled
   - Configuration backward compatibility maintained

## End-to-End Testing Scenarios

### Scenario 1: Complete Request Flow
1. Request enters Router
2. Provider processes and returns raw response
3. Prettifier plugin normalizes response
4. TOON format applied
5. Response delivered to client

```cpp
TEST(E2ETest, CompleteRequestFlow) {
    // Setup
    Router router;
    MockProvider provider;
    MockPrettifierPlugin prettifier;

    // Execute
    auto response = router.route(create_test_request());

    // Verify
    EXPECT_TRUE(response.is_prettified);
    EXPECT_TRUE(is_valid_toon(response.toon_content));
    EXPECT_EQ(response.provider, "cerebras");
}
```

### Scenario 2: Multi-Provider Conversation
1. Start conversation with Cerebras
2. Switch to OpenAI mid-conversation
3. Include tool calls from multiple providers
4. Verify consistent output format throughout

### Scenario 3: Streaming with Prettification
1. Initiate streaming request
2. Provider streams response chunks
3. Prettifier processes chunks in real-time
4. Client receives consistently formatted stream

## Load Testing

### Concurrent Request Testing
```cpp
TEST(LoadTest, ConcurrentPrettification) {
    const int num_threads = 50;
    const int requests_per_thread = 100;

    std::vector<std::thread> threads;
    std::atomic<int> successful_requests{0};

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < requests_per_thread; ++j) {
                auto response = router.route(create_random_request());
                if (response.is_prettified && is_valid_toon(response.content)) {
                    successful_requests++;
                }
            }
        });
    }

    for (auto& t : threads) t.join();

    EXPECT_EQ(successful_requests, num_threads * requests_per_thread);
}
```

### Performance Benchmarks
- **Throughput**: 500+ requests/second with all plugins enabled
- **Latency**: Additional overhead <20ms per request
- **Memory**: <50MB increase in memory usage
- **CPU**: <15% increase in CPU usage

## Security Testing

### Content Sanitization
```cpp
TEST(SecurityTest, MaliciousContentSanitization) {
    std::string malicious_content = R"(
    <script>alert('xss')</script>
    ```javascript
    eval('malicious code');
    ```
    <function_calls>
    {"name": "system", "arguments": {"command": "rm -rf /"}}
    </function_calls>
    )";

    auto response = normalizer_.process(malicious_content);

    EXPECT_FALSE(response.content.contains("<script>"));
    EXPECT_FALSE(response.content.contains("eval("));
    EXPECT_FALSE(response.content.contains("rm -rf"));
}
```

### Plugin Security Validation
1. **No Arbitrary Code Execution**
   - Plugins cannot execute external commands
   - No network access from plugins
   - File system access is properly sandboxed

2. **Memory Safety**
   - All buffer operations are bounds-checked
   - No use-after-free vulnerabilities
   - Proper string handling prevents buffer overflows

## Continuous Integration Requirements

### Automated Test Pipeline
```yaml
name: Phase 2 Core Plugins Tests
on: [push, pull_request]
jobs:
  test:
    runs-on: ubuntu-latest
    strategy:
      matrix:
        provider: [cerebras, openai, anthropic, synthetic]
    steps:
      - uses: actions/checkout@v3
      - name: Build
        run: |
          mkdir build && cd build
          cmake .. -DWITH_${{ matrix.provider }}_TESTS=ON
          make -j$(nproc)
      - name: Run Provider Tests
        run: ctest -R "${{ matrix.provider }}.*" --output-on-failure
      - name: Performance Benchmarks
        run: ./benchmark_${{ matrix.provider }}_performance
      - name: Security Scans
        run: |
          valgrind --leak-check=full ./test_${{ matrix.provider }}_security
          cppcheck --enable=all src/prettifiers/${{ matrix.provider }}/
```

## Success Criteria for Phase 2

### Functional Requirements
- [ ] Markdown normalization works across all providers
- [ ] Tool call extraction maintains accuracy >95%
- [ ] Provider-specific formatters handle provider quirks
- [ ] Streaming support works with real-time formatting
- [ ] All Phase 1 functionality preserved

### Quality Requirements
- [ ] 90%+ code coverage on new components
- [ ] Performance regression <20% compared to Phase 1
- [ ] Zero security vulnerabilities in new plugins
- [ ] Load testing meets benchmark criteria

### Integration Requirements
- [ ] All plugins integrate seamlessly with PluginRegistry
- [ ] TOON format output is consistent across all plugins
- [ ] Configuration system supports all new plugin options
- [ ] WebUI can control all new plugin features

## Final Sign-off Requirements

Before moving to Phase 3, the following must be completed:
1. All unit and integration tests passing
2. Performance benchmarks meeting criteria
3. Security audit of all plugin code
4. Real-world testing with actual AI providers
5. Documentation of all plugin APIs and configurations
6. Demo of complete prettifier system working with all providers
7. Load testing with 100+ concurrent requests
8. Compatibility testing with existing AIMUX deployments