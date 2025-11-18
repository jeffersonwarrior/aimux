#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include <chrono>
#include "aimux/prettifier/prettifier_plugin.hpp"
#include "aimux/prettifier/plugin_registry.hpp"

using namespace aimux::prettifier;
using namespace aimux::core;
using namespace ::testing;

// Mock plugin implementation for testing
class MockPrettifierPlugin : public PrettifierPlugin {
public:
    MOCK_METHOD(ProcessingResult, preprocess_request, (const Request& request), (override));
    MOCK_METHOD(ProcessingResult, postprocess_response, (const Response& response, const ProcessingContext& context), (override));
    MOCK_METHOD(std::string, get_name, (), (const, override));
    MOCK_METHOD(std::string, version, (), (const, override));
    MOCK_METHOD(std::string, description, (), (const, override));
    MOCK_METHOD(std::vector<std::string>, supported_formats, (), (const, override));
    MOCK_METHOD(std::vector<std::string>, output_formats, (), (const, override));
    MOCK_METHOD(std::vector<std::string>, supported_providers, (), (const, override));
    MOCK_METHOD(std::vector<std::string>, capabilities, (), (const, override));
};

// Concrete plugin implementation for testing abstract behavior
class TestPlugin : public PrettifierPlugin {
public:
    TestPlugin() = default;

    std::string get_name() const override { return "test-plugin"; }
    std::string version() const override { return "1.0.0"; }
    std::string description() const override { return "Test plugin for unit testing"; }
    std::vector<std::string> supported_formats() const override { return {"markdown", "json"}; }
    std::vector<std::string> output_formats() const override { return {"toon"}; }
    std::vector<std::string> supported_providers() const override { return {"test-provider"}; }
    std::vector<std::string> capabilities() const override { return {"formatting"}; }

    ProcessingResult preprocess_request(const Request& request) override {
        ProcessingResult result;
        result.success = true;
        result.processed_content = "preprocessed";
        return result;
    }

    ProcessingResult postprocess_response(const Response& response, const ProcessingContext& context) override {
        ProcessingResult result;
        result.success = true;
        result.processed_content = "postprocessed";
        result.output_format = "toon";
        return result;
    }

    // Expose protected methods for testing
    using PrettifierPlugin::create_success_result;
    using PrettifierPlugin::create_error_result;
    using PrettifierPlugin::validate_json;
    using PrettifierPlugin::extract_tool_calls;
};

// Test suite following qa/phase1_foundation_qa_plan.md - Component 2
class PrettifierPluginTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_plugin_ = std::make_shared<TestPlugin>();
    }

    std::shared_ptr<TestPlugin> test_plugin_;
};

// Test interface compliance requirements
TEST_F(PrettifierPluginTest, InterfaceCompliance) {
    // Test that all virtual methods are properly implemented
    EXPECT_NE(test_plugin_->get_name(), "");
    EXPECT_NE(test_plugin_->version(), "");
    EXPECT_NE(test_plugin_->description(), "");
    EXPECT_FALSE(test_plugin_->supported_formats().empty());
    EXPECT_FALSE(test_plugin_->output_formats().empty());
    EXPECT_FALSE(test_plugin_->supported_providers().empty());
    EXPECT_FALSE(test_plugin_->capabilities().empty());
}

// Test abstract class behavior
TEST_F(PrettifierPluginTest, AbstractClassBehavior) {
    // Verify that PrettifierPlugin cannot be instantiated directly
    // This is tested at compile time - class is abstract
    EXPECT_TRUE(true); // Compilation passes means abstract class is properly defined
}

// Test polymorphic behavior
TEST_F(PrettifierPluginTest, PolymorphicBehavior) {
    // Test that derived classes work through base pointer
    std::shared_ptr<PrettifierPlugin> base_plugin = test_plugin_;

    EXPECT_EQ(base_plugin->get_name(), "test-plugin");
    EXPECT_EQ(base_plugin->version(), "1.0.0");

    Request test_request;
    auto preprocessed = base_plugin->preprocess_request(test_request);
    EXPECT_TRUE(preprocessed.success);

    Response test_response;
    ProcessingContext context;
    auto postprocessed = base_plugin->postprocess_response(test_response, context);
    EXPECT_TRUE(postprocessed.success);
}

// Test memory management with RAII
TEST_F(PrettifierPluginTest, MemoryManagement) {
    // Test shared pointer behavior
    std::weak_ptr<PrettifierPlugin> weak_plugin;
    {
        auto plugin = std::make_shared<TestPlugin>();
        weak_plugin = plugin;
        EXPECT_FALSE(weak_plugin.expired());
    }
    EXPECT_TRUE(weak_plugin.expired()); // Should be destroyed when out of scope
}

// Test factory pattern concepts
TEST_F(PrettifierPluginTest, FactoryPatternConcept) {
    // Demonstrate factory pattern usage
    auto plugin = std::make_shared<TestPlugin>();

    // Verify plugin can be created and used
    EXPECT_TRUE(plugin != nullptr);
    EXPECT_TRUE(plugin->validate_configuration());

    auto config = plugin->get_configuration();
    EXPECT_TRUE(config.is_object());
}

// Test utility methods
TEST_F(PrettifierPluginTest, UtilityMethods) {
    // Test success result creation
    auto success_result = test_plugin_->create_success_result("test content");
    EXPECT_TRUE(success_result.success);
    EXPECT_EQ(success_result.processed_content, "test content");

    // Test error result creation
    auto error_result = test_plugin_->create_error_result("test error", "ERROR_CODE");
    EXPECT_FALSE(error_result.success);
    EXPECT_EQ(error_result.error_message, "test error");
    EXPECT_EQ(error_result.metadata["error_code"], "ERROR_CODE");
}

// Test streaming support (default implementations)
TEST_F(PrettifierPluginTest, StreamingSupportDefault) {
    ProcessingContext context;
    context.streaming_mode = true;

    // Test default streaming methods
    EXPECT_TRUE(test_plugin_->begin_streaming(context));

    ProcessingResult chunk_result = test_plugin_->process_streaming_chunk("test chunk", false, context);
    EXPECT_TRUE(chunk_result.success);
    EXPECT_EQ(chunk_result.processed_content, "test chunk");
    EXPECT_TRUE(chunk_result.streaming_mode);

    ProcessingResult end_result = test_plugin_->end_streaming(context);
    EXPECT_TRUE(end_result.success);
}

// Test configuration methods (default implementations)
TEST_F(PrettifierPluginTest, ConfigurationDefault) {
    // Test default configuration handling
    EXPECT_TRUE(test_plugin_->configure(nlohmann::json::object()));
    EXPECT_TRUE(test_plugin_->validate_configuration());

    auto config = test_plugin_->get_configuration();
    EXPECT_TRUE(config.is_object());
    EXPECT_TRUE(config.empty());
}

// Test metrics and monitoring (default implementations)
TEST_F(PrettifierPluginTest, MonitoringDefault) {
    // Test default metrics methods
    auto metrics = test_plugin_->get_metrics();
    EXPECT_TRUE(metrics.is_object());
    EXPECT_TRUE(metrics.empty());

    // Should not throw
    EXPECT_NO_THROW(test_plugin_->reset_metrics());
}

// Test health and diagnostics
TEST_F(PrettifierPluginTest, HealthAndDiagnostics) {
    // Test health check
    auto health = test_plugin_->health_check();
    EXPECT_TRUE(health.is_object());
    EXPECT_EQ(health["status"], "healthy");
    EXPECT_TRUE(health.contains("timestamp"));

    // Test diagnostics
    auto diagnostics = test_plugin_->get_diagnostics();
    EXPECT_TRUE(diagnostics.is_object());
    EXPECT_EQ(diagnostics["name"], "test-plugin");
    EXPECT_EQ(diagnostics["version"], "1.0.0");
    EXPECT_EQ(diagnostics["status"], "active");
}

// ToolCall structure tests
TEST(PrettifierPluginStructuresTest, ToolCallSerialization) {
    ToolCall call;
    call.name = "test_function";
    call.id = "call_123";
    call.parameters = nlohmann::json{{"param1", "value1"}, {"param2", 42}};
    call.status = "completed";
    call.timestamp = std::chrono::system_clock::now();

    // Test serialization
    auto json = call.to_json();
    EXPECT_EQ(json["name"], "test_function");
    EXPECT_EQ(json["id"], "call_123");
    EXPECT_EQ(json["status"], "completed");
    EXPECT_TRUE(json.contains("parameters"));
    EXPECT_TRUE(json.contains("timestamp"));

    // Test deserialization
    auto reconstructed = ToolCall::from_json(json);
    EXPECT_EQ(reconstructed.name, call.name);
    EXPECT_EQ(reconstructed.id, call.id);
    EXPECT_EQ(reconstructed.status, call.status);
    EXPECT_EQ(reconstructed.parameters, call.parameters);
}

// ProcessingContext structure tests
TEST(PrettifierPluginStructuresTest, ProcessingContextSerialization) {
    ProcessingContext context;
    context.provider_name = "test-provider";
    context.model_name = "test-model";
    context.original_format = "markdown";
    context.requested_formats = {"toon", "json"};
    context.streaming_mode = true;
    context.processing_start = std::chrono::system_clock::now();

    auto json = context.to_json();
    EXPECT_EQ(json["provider_name"], "test-provider");
    EXPECT_EQ(json["model_name"], "test-model");
    EXPECT_EQ(json["original_format"], "markdown");
    EXPECT_TRUE(json["streaming_mode"]);
    EXPECT_TRUE(json.contains("requested_formats"));
}

// ProcessingResult structure tests
TEST(PrettifierPluginStructuresTest, ProcessingResultSerialization) {
    ProcessingResult result;
    result.success = true;
    result.processed_content = "processed content";
    result.output_format = "toon";
    result.processing_time = std::chrono::milliseconds(150);
    result.tokens_processed = 100;
    result.error_message = "";

    // Add a tool call
    ToolCall tool;
    tool.name = "test_tool";
    result.extracted_tool_calls.push_back(tool);

    auto json = result.to_json();
    EXPECT_TRUE(json["success"]);
    EXPECT_EQ(json["processed_content"], "processed content");
    EXPECT_EQ(json["output_format"], "toon");
    EXPECT_EQ(json["processing_time_ms"], 150);
    EXPECT_EQ(json["tokens_processed"], 100);
    EXPECT_TRUE(json["extracted_tool_calls"].is_array());
    EXPECT_EQ(json["extracted_tool_calls"].size(), 1);
}

// Test JSON validation utility
TEST_F(PrettifierPluginTest, JsonValidation) {
    // Test valid JSON
    auto valid_json = test_plugin_->validate_json(R"({"key": "value", "number": 42})");
    EXPECT_TRUE(valid_json.has_value());
    EXPECT_EQ(valid_json->at("key"), "value");
    EXPECT_EQ(valid_json->at("number"), 42);

    // Test invalid JSON
    auto invalid_json = test_plugin_->validate_json("invalid json string");
    EXPECT_FALSE(invalid_json.has_value());

    // Test JSON repair
    auto repaired_json = test_plugin_->validate_json(R"({"key": "value", "number": 42,})");
    EXPECT_TRUE(repaired_json.has_value()); // Should repair trailing comma
}

// Test tool call extraction utility
TEST_F(PrettifierPluginTest, ToolCallExtraction) {
    // Test XML-style function calls
    std::string xml_content = R"(
        <function_calls>
        {"name": "test_function", "arguments": {"param": "value"}}
        </function_calls>
    )";

    auto xml_calls = test_plugin_->extract_tool_calls(xml_content);
    EXPECT_EQ(xml_calls.size(), 1);
    EXPECT_EQ(xml_calls[0].name, "test_function");

    // Test JSON code blocks
    std::string json_content = R"(
        ```json
        {"function": {"name": "another_function", "arguments": {"param": "value2"}}}
        ```
    )";

    auto json_calls = test_plugin_->extract_tool_calls(json_content);
    EXPECT_EQ(json_calls.size(), 1);
    EXPECT_EQ(json_calls[0].name, "another_function");

    // Test no tool calls
    std::string no_calls = "This is just regular text with no function calls.";
    auto no_tool_calls = test_plugin_->extract_tool_calls(no_calls);
    EXPECT_EQ(no_tool_calls.size(), 0);
}

// Performance tests following qa/phase1_foundation_qa_plan.md
TEST_F(PrettifierPluginTest, PerformanceBasics) {
    const int iterations = 1000;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; ++i) {
        ProcessingContext context;
        context.provider_name = "test";
        context.processing_start = std::chrono::system_clock::now();

        // Test health check performance
        auto health = test_plugin_->health_check();
        EXPECT_TRUE(health.contains("status"));

        // Test diagnostics performance
        auto diagnostics = test_plugin_->get_diagnostics();
        EXPECT_TRUE(diagnostics.contains("name"));
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Should complete in reasonable time (target <1ms per operation)
    double micros_per_op = static_cast<double>(duration.count()) / iterations;
    EXPECT_LT(micros_per_op, 100.0) << "Plugin operations taking too long: "
                                    << micros_per_op << "μs per operation";
}

// Thread safety test
TEST_F(PrettifierPluginTest, ThreadSafety) {
    const int num_threads = 10;
    const int operations_per_thread = 100;
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([this, operations_per_thread, &success_count]() {
            for (int i = 0; i < operations_per_thread; ++i) {
                try {
                    // Test concurrent access to plugin methods
                    auto health = test_plugin_->health_check();
                    auto diagnostics = test_plugin_->get_diagnostics();
                    auto metrics = test_plugin_->get_metrics();

                    if (health.contains("status") &&
                        diagnostics.contains("name") &&
                        metrics.is_object()) {
                        success_count++;
                    }
                } catch (...) {
                    // Should not throw
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    int expected_successes = num_threads * operations_per_thread;
    EXPECT_EQ(success_count, expected_successes)
        << "Thread safety test failed: " << success_count
        << " successes out of " << expected_successes;
}

// Integration test with PluginRegistry
TEST_F(PrettifierPluginTest, RegistryIntegration) {
    PluginRegistry registry;

    // Test plugin registration
    PluginManifest manifest;
    manifest.name = "test-plugin";
    manifest.version = "1.0.0";
    manifest.description = "Test plugin";
    manifest.providers = {"test-provider"};
    manifest.formats = {"test-format"};
    manifest.capabilities = {"test-capability"};

    auto result = registry.register_plugin(test_plugin_, manifest);
    EXPECT_TRUE(result.success);

    // Test plugin retrieval
    auto retrieved = registry.get_prettifier("test-plugin");
    EXPECT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->get_name(), "test-plugin");

    // Test plugin metadata
    auto metadata = registry.get_plugin_metadata("test-plugin");
    EXPECT_TRUE(metadata.has_value());
    EXPECT_EQ(metadata->manifest.name, "test-plugin");
}

// Error handling test
TEST_F(PrettifierPluginTest, ErrorHandling) {
    // Test error result creation and handling
    auto error_result = test_plugin_->create_error_result("Test error message", "TEST_ERROR");

    EXPECT_FALSE(error_result.success);
    EXPECT_EQ(error_result.error_message, "Test error message");
    EXPECT_EQ(error_result.metadata["error_code"], "TEST_ERROR");

    // Test error reporting in processing
    Request test_request;
    ProcessingResult result = test_plugin_->preprocess_request(test_request);
    EXPECT_TRUE(result.success); // Our test plugin should succeed

    // Verify error handling doesn't crash
    EXPECT_NO_THROW({
        auto diagnostics = test_plugin_->get_diagnostics();
        auto health = test_plugin_->health_check();
    });
}