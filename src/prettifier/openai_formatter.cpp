#include "aimux/prettifier/openai_formatter.hpp"
#include "aimux/core/model_registry.hpp"
#include <map>
#include <regex>
#include <sstream>
#include <chrono>
#include <algorithm>
#include <iomanip>

// TODO: Replace with proper logging when available
#define LOG_ERROR(msg, ...) do { printf("[OPENAI ERROR] " msg "\n", ##__VA_ARGS__); } while(0)
#define LOG_DEBUG(msg, ...) do { printf("[OPENAI DEBUG] " msg "\n", ##__VA_ARGS__); } while(0)

// Forward declaration for global model configuration
namespace aimux {
namespace config {
    extern std::map<std::string, aimux::core::ModelRegistry::ModelInfo> g_selected_models;
}
}


namespace aimux {
namespace prettifier {

// OpenAIPatterns implementation
OpenAIFormatter::OpenAIPatterns::OpenAIPatterns()
    : function_call_pattern(R"(\"tool_calls\":\s*\[\s*\{[^}]*\"function\"[^}]*\}\s*\])", std::regex::optimize)
    , tool_calls_pattern(R"(\"function\":\s*\{[^}]*\"name\"[^}]*\"arguments\"[^}]*\})", std::regex::optimize)
    , legacy_function_pattern(R"(\"function_call\":\s*\{[^}]*\"name\"[^}]*\"arguments\"[^}]*\})", std::regex::optimize)
    , json_schema_pattern(R"(\"response_format\":\s*\{[^}]*\"type\"[^}]*\"schema\"[^}]*\})", std::regex::optimize)
    , structured_output_pattern(R"(^\{[\s\S]*\}$)", std::regex::optimize) // Basic JSON structure
    , streaming_delta_pattern(R"(\"delta\":\s*\{[^}]*\})", std::regex::optimize)
    , streaming_function_delta(R"(\"function_call\":\s*\{[^}]*\})", std::regex::optimize) {
}

// OpenAIFormatter implementation
std::string OpenAIFormatter::get_default_model() {
    
    auto it = aimux::config::g_selected_models.find("openai");
    if (it != aimux::config::g_selected_models.end()) {
        return it->second.model_id;
    }
    return "gpt-4o";
}

OpenAIFormatter::OpenAIFormatter(const std::string& model_name)
    : patterns_(std::make_unique<OpenAIPatterns>())
    , model_name_(model_name.empty() ? get_default_model() : model_name) {
    LOG_DEBUG("OpenAIFormatter initialized with comprehensive format support (model: %s)", model_name_.c_str());
}

std::string OpenAIFormatter::get_name() const {
    return "openai-gpt-formatter-v1.0.0";
}

std::string OpenAIFormatter::version() const {
    return "1.0.0";
}

std::string OpenAIFormatter::description() const {
    return "OpenAI GPT formatter with comprehensive function calling, structured outputs, and legacy format support. "
           "Handles all OpenAI response formats including GPT-4 function calling, JSON structured outputs, "
           "and legacy GPT-3.5 completion formats. Provides robust validation and compatibility across OpenAI API versions.";
}

std::vector<std::string> OpenAIFormatter::supported_formats() const {
    return {
        "chat-completion",
        "function-calling",
        "structured-output",
        "legacy-completion",
        "streaming-delta",
        "json-mode",
        "tool-results"
    };
}

std::vector<std::string> OpenAIFormatter::output_formats() const {
    return {"toon", "json", "normalized-text", "structured-data", "function-calls"};
}

std::vector<std::string> OpenAIFormatter::supported_providers() const {
    return {"openai", "openai-compatibility"};
}

std::vector<std::string> OpenAIFormatter::capabilities() const {
    return {
        "function-calling",
        "structured-outputs",
        "legacy-compatibility",
        "streaming-support",
        "json-validation",
        "schema-validation",
        "tool-call-parsing",
        "format-detection",
        "error-recovery"
    };
}

ProcessingResult OpenAIFormatter::preprocess_request(const core::Request& request) {
    auto start_time = std::chrono::high_resolution_clock::now();

    try {
        ProcessingResult result;
        result.success = true;
        result.output_format = "openai-preprocessed";

        // Clone request data for modification
        nlohmann::json optimized_data = request.data;

        // Format tool definitions for OpenAI function calling
        if (optimized_data.contains("tools")) {
            for (auto& tool : optimized_data["tools"]) {
                if (tool.contains("function")) {
                    auto& func = tool["function"];

                    // Ensure required fields are present
                    if (!func.contains("name")) {
                        throw std::invalid_argument("Tool function missing required 'name' field");
                    }

                    // Set function type if not specified
                    if (!func.contains("type")) {
                        func["type"] = "function";
                    }

                    // Validate function parameters if present
                    if (func.contains("parameters")) {
                        if (!func["parameters"].is_object()) {
                            func["parameters"] = nlohmann::json::object();
                        }
                    } else {
                        func["parameters"] = nlohmann::json::object();
                    }
                }
            }
        }

        // Configure structured outputs if enabled
        if (enable_structured_outputs_ && optimized_data.contains("response_format")) {
            auto& response_format = optimized_data["response_format"];
            if (response_format.contains("type") && response_format["type"] == "json_object") {
                response_format["schema"] = response_format.value("schema", nlohmann::json::object());
            }
        }

        // Add OpenAI-specific metadata
        optimized_data["_openai_metadata"] = {
            {"preprocessed_at", std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()},
            {"legacy_support", support_legacy_formats_},
            {"strict_validation", strict_function_validation_},
            {"structured_outputs", enable_structured_outputs_}
        };

        result.processed_content = optimized_data.dump();

        // Calculate processing time
        auto end_time = std::chrono::high_resolution_clock::now();
        auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(
            end_time - start_time).count();

        result.processing_time = std::chrono::milliseconds(elapsed_us / 1000);
        result.tokens_processed = request.data.dump().length() / 4;

        update_openai_metrics(static_cast<uint64_t>(elapsed_us), "preprocessing", 0, true);

        LOG_DEBUG("Request preprocessing completed in %ld us", elapsed_us);
        return result;

    } catch (const std::exception& e) {
        return create_error_result("Request preprocessing failed: " + std::string(e.what()), "preprocess_error");
    }
}

ProcessingResult OpenAIFormatter::postprocess_response(
    const core::Response& response,
    const ProcessingContext& context) {

    auto start_time = std::chrono::high_resolution_clock::now();

    try {
        // Input size validation to prevent crashes on very large inputs
        const size_t MAX_INPUT_SIZE = 10 * 1024 * 1024; // 10MB limit for local development
        if (response.data.size() > MAX_INPUT_SIZE) {
            LOG_ERROR("Input size %zu exceeds maximum allowed size %zu", response.data.size(), MAX_INPUT_SIZE);
            return create_error_result("Input size too large (max 10MB)", "input_too_large");
        }

        // Security validation: check for malicious patterns
        if (contains_malicious_patterns(response.data)) {
            LOG_ERROR("Malicious patterns detected in response");
            return create_error_result("Content contains malicious patterns", "security_violation");
        }

        ProcessingResult result;
        result.success = true;
        result.output_format = "toon";

        // Detect format type
        std::string format_type = detect_format_type(response.data);
        LOG_DEBUG("Detected format type: %s", format_type.c_str());

        // Clean content based on detected format
        std::string cleaned_content = clean_openai_content(response.data);

        // Extract function calls based on format
        std::vector<ToolCall> tool_calls;
        std::string final_content = cleaned_content;

        if (format_type == "function-calling" || format_type == "legacy-function") {
            tool_calls = extract_openai_function_calls(cleaned_content);
        } else if (format_type == "structured-output") {
            // Validate structured output JSON
            final_content = validate_structured_output(cleaned_content);
        } else if (format_type == "legacy-format" && support_legacy_formats_) {
            // Process legacy formats
            final_content = process_legacy_format(cleaned_content);
            tool_calls = extract_openai_function_calls(final_content);
        }

        // Generate OpenAI-compatible TOON format
        std::string toon_content = generate_openai_toon(final_content, tool_calls, context);

        result.processed_content = toon_content;
        result.extracted_tool_calls = tool_calls;
        result.streaming_mode = context.streaming_mode;

        // Add metadata
        result.metadata["provider"] = "openai";
        result.metadata["format_type"] = format_type;
        result.metadata["function_calls_count"] = tool_calls.size();
        result.metadata["structured_output"] = (format_type == "structured-output");
        result.metadata["legacy_processed"] = (format_type == "legacy-format");

        // Calculate final metrics
        auto end_time = std::chrono::high_resolution_clock::now();
        auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(
            end_time - start_time).count();

        result.processing_time = std::chrono::milliseconds(elapsed_us / 1000);
        result.tokens_processed = final_content.length() / 4;

        // Update metrics based on processing results
        bool validation_passed = true;
        if (strict_function_validation_ && !tool_calls.empty()) {
            validation_passed = tool_calls.size() <= static_cast<size_t>(max_function_calls_);
        }

        update_openai_metrics(static_cast<uint64_t>(elapsed_us), format_type, tool_calls.size(), validation_passed);

        LOG_DEBUG("Response postprocessing completed in %ld us, format: %s, tools: %zu",
                 elapsed_us, format_type.c_str(), tool_calls.size());
        return result;

    } catch (const std::exception& e) {
        validation_errors_.fetch_add(1);
        return create_error_result("Response postprocessing failed: " + std::string(e.what()), "postprocess_error");
    }
}

bool OpenAIFormatter::begin_streaming(const ProcessingContext& /*context*/) {
    try {
        streaming_content_.clear();
        streaming_content_.reserve(8192); // Pre-allocate for streaming
        streaming_function_call_ = nlohmann::json::object();
        streaming_active_ = true;
        streaming_start_ = std::chrono::steady_clock::now();

        LOG_DEBUG("OpenAI streaming initialization completed");
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("OpenAI streaming initialization failed: %s", e.what());
        return false;
    }
}

ProcessingResult OpenAIFormatter::process_streaming_chunk(
    const std::string& chunk,
    bool is_final,
    const ProcessingContext& /*context*/) {

    if (!streaming_active_) {
        return create_error_result("Streaming not initialized", "streaming_not_active");
    }

    auto start_time = std::chrono::high_resolution_clock::now();

    try {
        ProcessingResult result;
        result.success = true;
        result.streaming_mode = true;

        // Parse streaming chunk
        auto chunk_json = validate_json(chunk);
        if (!chunk_json) {
            // Try to extract JSON from SSE format
            static const std::regex sse_pattern(R"(data:\s*\{[^}]*\})");
            std::smatch match;
            if (std::regex_search(chunk, match, sse_pattern)) {
                chunk_json = validate_json(match[0].str());
            }
        }

        if (!chunk_json) {
            return create_error_result("Invalid streaming chunk format", "invalid_chunk");
        }

        // Process content delta
        if (chunk_json->contains("choices") && !chunk_json->at("choices").empty()) {
            const auto& choice = chunk_json->at("choices")[0];
            if (choice.contains("delta")) {
                const auto& delta = choice.at("delta");

                // Accumulate content
                if (delta.contains("content")) {
                    streaming_content_ += delta.at("content").get<std::string>();
                    result.processed_content = delta.at("content").get<std::string>();
                }

                // Process function call streaming
                if (delta.contains("function_call")) {
                    streaming_function_call_ = process_function_delta(delta.at("function_call"));

                    // Check if function call is complete
                    if (streaming_function_call_.contains("name") &&
                        streaming_function_call_.contains("arguments") &&
                        streaming_function_call_["arguments"].get<std::string>().find("}") != std::string::npos) {

                        ToolCall call;
                        call.name = streaming_function_call_["name"];

                        // Parse arguments
                        try {
                            if (streaming_function_call_["arguments"].is_string()) {
                                call.parameters = nlohmann::json::parse(streaming_function_call_["arguments"].get<std::string>());
                            } else {
                                call.parameters = streaming_function_call_["arguments"];
                            }
                        } catch (...) {
                            call.parameters = nlohmann::json::object();
                        }

                        call.status = "completed";
                        call.timestamp = std::chrono::system_clock::now();

                        result.extracted_tool_calls.push_back(call);
                    }
                }
            }
        }

        streaming_chunks_processed_.fetch_add(1);

        // Calculate metrics
        auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now() - start_time).count();

        update_openai_metrics(static_cast<uint64_t>(elapsed_us), "streaming", result.extracted_tool_calls.size(), true);

        if (is_final) {
            result.metadata["final_chunk"] = true;
        }

        return result;

    } catch (const std::exception& e) {
        return create_error_result("Streaming chunk processing failed: " + std::string(e.what()), "streaming_error");
    }
}

ProcessingResult OpenAIFormatter::end_streaming(const ProcessingContext& context) {
    if (!streaming_active_) {
        return create_error_result("Streaming not active", "streaming_not_active");
    }

    try {
        auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - streaming_start_).count();

        // Extract any remaining tool calls from accumulated content
        std::vector<ToolCall> final_tool_calls = extract_openai_function_calls(streaming_content_);

        // Generate final TOON format
        std::string final_toon = generate_openai_toon(streaming_content_, final_tool_calls, context);

        ProcessingResult result;
        result.success = true;
        result.processed_content = final_toon;
        result.extracted_tool_calls = final_tool_calls;
        result.streaming_mode = false;
        result.processing_time = std::chrono::milliseconds(total_time);
        result.metadata["streaming_completed"] = true;
        result.metadata["total_streaming_time_ms"] = total_time;
        result.metadata["final_content_length"] = streaming_content_.length();

        // Reset streaming state
        streaming_active_ = false;
        streaming_content_.clear();
        streaming_function_call_ = nlohmann::json::object();

        LOG_DEBUG("OpenAI streaming completed in %ld ms", total_time);
        return result;

    } catch (const std::exception& e) {
        streaming_active_ = false;
        return create_error_result("Streaming end failed: " + std::string(e.what()), "streaming_end_error");
    }
}

bool OpenAIFormatter::configure(const nlohmann::json& config) {
    try {
        if (config.contains("support_legacy_formats")) {
            support_legacy_formats_ = config["support_legacy_formats"].get<bool>();
        }

        if (config.contains("strict_function_validation")) {
            strict_function_validation_ = config["strict_function_validation"].get<bool>();
        }

        if (config.contains("enable_structured_outputs")) {
            enable_structured_outputs_ = config["enable_structured_outputs"].get<bool>();
        }

        if (config.contains("validate_tool_schemas")) {
            validate_tool_schemas_ = config["validate_tool_schemas"].get<bool>();
        }

        if (config.contains("preserve_thinking")) {
            preserve_thinking_ = config["preserve_thinking"].get<bool>();
        }

        if (config.contains("max_function_calls")) {
            max_function_calls_ = config["max_function_calls"].get<int>();
            if (max_function_calls_ <= 0) {
                max_function_calls_ = 10; // Reset to default
            }
        }

        LOG_DEBUG("OpenAI configuration updated successfully");
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("OpenAI configuration failed: %s", e.what());
        return false;
    }
}

bool OpenAIFormatter::validate_configuration() const {
    return max_function_calls_ > 0 && max_function_calls_ <= 50; // Reasonable limits
}

nlohmann::json OpenAIFormatter::get_configuration() const {
    nlohmann::json config;
    config["support_legacy_formats"] = support_legacy_formats_;
    config["strict_function_validation"] = strict_function_validation_;
    config["enable_structured_outputs"] = enable_structured_outputs_;
    config["validate_tool_schemas"] = validate_tool_schemas_;
    config["preserve_thinking"] = preserve_thinking_;
    config["max_function_calls"] = max_function_calls_;
    return config;
}

nlohmann::json OpenAIFormatter::get_metrics() const {
    nlohmann::json metrics;

    uint64_t count = total_processing_count_.load();
    uint64_t total_time = total_processing_time_us_.load();

    metrics["total_processing_count"] = count;
    metrics["total_processing_time_us"] = total_time;
    metrics["average_processing_time_us"] = count > 0 ? total_time / count : 0;
    metrics["function_calls_processed"] = function_calls_processed_.load();
    metrics["structured_outputs_validated"] = structured_outputs_validated_.load();
    metrics["legacy_formats_processed"] = legacy_formats_processed_.load();
    metrics["validation_errors"] = validation_errors_.load();
    metrics["streaming_chunks_processed"] = streaming_chunks_processed_.load();
    metrics["success_rate"] = count > 0 ?
        static_cast<double>(count - validation_errors_.load()) / count : 1.0;

    return metrics;
}

void OpenAIFormatter::reset_metrics() {
    total_processing_count_.store(0);
    total_processing_time_us_.store(0);
    function_calls_processed_.store(0);
    structured_outputs_validated_.store(0);
    legacy_formats_processed_.store(0);
    validation_errors_.store(0);
    streaming_chunks_processed_.store(0);
}

nlohmann::json OpenAIFormatter::health_check() const {
    nlohmann::json health;

    try {
        // Basic health status
        health["status"] = "healthy";
        health["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        // Test function call extraction
        std::string test_func = R"({
            "choices": [{
                "message": {
                    "tool_calls": [{
                        "function": {"name": "test_function", "arguments": "{\"param\": \"value\"}"}
                    }]
                }
            }]
        })";

        auto test_calls = extract_openai_function_calls(test_func);
        health["function_call_extraction"] = !test_calls.empty();

        // Test structured output validation
        std::string test_json = R"({"test": "validation", "number": 42})";
        std::string validated = validate_structured_output(test_json);
        health["structured_output_validation"] = !validated.empty();

        // Configuration validation
        health["configuration_valid"] = validate_configuration();

        // Pattern availability
        health["patterns_available"] = patterns_ != nullptr;

        // Recent performance
        auto metrics = get_metrics();
        health["recent_performance"] = {
            {"avg_processing_time_us", metrics["average_processing_time_us"]},
            {"success_rate", metrics["success_rate"]},
            {"validation_errors", metrics["validation_errors"]}
        };

    } catch (const std::exception& e) {
        health["status"] = "unhealthy";
        health["error"] = e.what();
    }

    return health;
}

nlohmann::json OpenAIFormatter::get_diagnostics() const {
    nlohmann::json diagnostics;

    diagnostics["name"] = get_name();
    diagnostics["version"] = version();
    diagnostics["status"] = "active";
    diagnostics["configuration"] = get_configuration();
    diagnostics["metrics"] = get_metrics();

    // Capabilities matrix
    diagnostics["capabilities"] = {
        {"function_calling", true},
        {"structured_outputs", enable_structured_outputs_},
        {"legacy_support", support_legacy_formats_},
        {"streaming", true},
        {"json_validation", true},
        {"schema_validation", validate_tool_schemas_}
    };

    // Performance analysis
    uint64_t avg_time = total_processing_count_.load() > 0 ?
        total_processing_time_us_.load() / total_processing_count_.load() : 0;

    diagnostics["performance_analysis"] = {
        {"average_processing_time_us", avg_time},
        {"meets_performance_target", avg_time < 40000}, // 40ms target
        {"function_call_rate", function_calls_processed_.load() > 0 ?
            static_cast<double>(function_calls_processed_.load()) / total_processing_count_.load() : 0.0},
        {"structured_output_rate", structured_outputs_validated_.load() > 0 ?
            static_cast<double>(structured_outputs_validated_.load()) / total_processing_count_.load() : 0.0}
    };

    // Known limitations
    std::vector<std::string> limitations;
    if (!support_legacy_formats_) limitations.push_back("Legacy format support disabled");
    if (!enable_structured_outputs_) limitations.push_back("Structured output validation disabled");
    if (validation_errors_.load() > total_processing_count_.load() * 0.1) {
        limitations.push_back("High validation error rate");
    }

    diagnostics["limitations"] = limitations;

    return diagnostics;
}

// Private helper methods implementation

std::vector<ToolCall> OpenAIFormatter::extract_openai_function_calls(const std::string& content) const {
    std::vector<ToolCall> tool_calls;

    try {
        auto content_json = validate_json(content);
        if (!content_json) {
            return tool_calls;
        }

        // Extract from standard tool_calls format
        if (content_json->contains("choices") && !content_json->at("choices").empty()) {
            const auto& choice = content_json->at("choices")[0];
            if (choice.contains("message") && choice.at("message").contains("tool_calls")) {
                const auto& tool_calls_array = choice.at("message")["tool_calls"];

                for (const auto& tool_call : tool_calls_array) {
                    if (tool_call.contains("function")) {
                        ToolCall call;
                        call.status = "completed";
                        call.timestamp = std::chrono::system_clock::now();

                        const auto& func = tool_call.at("function");
                        if (func.contains("name")) {
                            call.name = func["name"].get<std::string>();
                        }

                        if (func.contains("arguments")) {
                            try {
                                call.parameters = nlohmann::json::parse(func["arguments"].get<std::string>());
                            } catch (...) {
                                // Store as string if JSON parsing fails
                                call.parameters = func["arguments"];
                            }
                        }

                        if (tool_call.contains("id")) {
                            call.id = tool_call["id"].get<std::string>();
                        }

                        tool_calls.push_back(call);
                    }
                }
            }
        }

        // Extract from legacy function_call format
        if (tool_calls.empty() && support_legacy_formats_) {
            if (content_json->contains("choices") && !content_json->at("choices").empty()) {
                const auto& choice = content_json->at("choices")[0];
                if (choice.contains("message") && choice.at("message").contains("function_call")) {
                    const auto& func_call = choice.at("message")["function_call"];

                    ToolCall call;
                    call.status = "completed";
                    call.timestamp = std::chrono::system_clock::now();

                    if (func_call.contains("name")) {
                        call.name = func_call["name"].get<std::string>();
                    }

                    if (func_call.contains("arguments")) {
                        try {
                            call.parameters = nlohmann::json::parse(func_call["arguments"].get<std::string>());
                        } catch (...) {
                            call.parameters = func_call["arguments"];
                        }
                    }

                    tool_calls.push_back(call);
                    legacy_formats_processed_.fetch_add(1);
                }
            }
        }

        function_calls_processed_.fetch_add(tool_calls.size());

    } catch (const std::exception& e) {
        LOG_DEBUG("Function call extraction failed: %s", e.what());
    }

    return tool_calls;
}

std::string OpenAIFormatter::validate_structured_output(
    const std::string& content,
    const std::optional<nlohmann::json>& schema) const {

    try {
        // Try to parse as JSON
        auto json = nlohmann::json::parse(content);

        // Basic validation
        if (!json.is_object()) {
            throw std::invalid_argument("Structured output must be a JSON object");
        }

        // Schema validation if provided
        if (schema && validate_tool_schemas_) {
            // Basic schema validation - could be expanded with full JSON schema support
            if (schema->contains("required")) {
                for (const auto& required_field : schema->at("required")) {
                    if (!json.contains(required_field.get<std::string>())) {
                        throw std::invalid_argument("Missing required field: " + required_field.get<std::string>());
                    }
                }
            }
        }

        structured_outputs_validated_.fetch_add(1);
        return json.dump();

    } catch (const nlohmann::json::parse_error& e) {
        // Try to repair common JSON issues
        try {
            std::string repaired = content;

            // Remove trailing commas
            static const std::regex trailing_comma(R"(,\s*([}\]]))");
            repaired = std::regex_replace(repaired, trailing_comma, "$1");

            auto json = nlohmann::json::parse(repaired);
            structured_outputs_validated_.fetch_add(1);
            return json.dump();

        } catch (...) {
            throw std::invalid_argument("Invalid structured output JSON: " + std::string(e.what()));
        }
    }
}

std::string OpenAIFormatter::process_legacy_format(const std::string& content) const {
    try {
        auto content_json = validate_json(content);
        if (!content_json) {
            return content; // Return as-is if not valid JSON
        }

        // Map legacy fields to modern format
        nlohmann::json modern_format = *content_json;

        // Handle legacy completion format
        if (content_json->contains("text") && !content_json->contains("choices")) {
            modern_format["choices"] = nlohmann::json::array({
                {
                    {"message", {
                        {"content", content_json->at("text")}
                    }}
                }
            });
        }

        return modern_format.dump();

    } catch (const std::exception& e) {
        LOG_DEBUG("Legacy format processing failed: %s", e.what());
        return content; // Return original content on error
    }
}

std::string OpenAIFormatter::generate_openai_toon(
    const std::string& content,
    const std::vector<ToolCall>& tool_calls,
    const ProcessingContext& context) const {

    try {
        nlohmann::json toon;

        // Basic TOON structure
        toon["format"] = "toon";
        toon["version"] = "1.0.0";
        toon["provider"] = "openai";
        toon["model"] = context.model_name;

        // Content
        toon["content"] = content;

        // Tool calls
        if (!tool_calls.empty()) {
            nlohmann::json tool_array = nlohmann::json::array();
            for (const auto& tool : tool_calls) {
                tool_array.push_back(tool.to_json());
            }
            toon["tool_calls"] = tool_array;
        }

        // OpenAI-specific metadata
        toon["metadata"] = {
            {"processed_at", std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()},
            {"openai_capabilities", capabilities()},
            {"format_type", detect_format_type(content)},
            {"legacy_support_enabled", support_legacy_formats_},
            {"structured_outputs_enabled", enable_structured_outputs_},
            {"function_validation", strict_function_validation_}
        };

        return toon.dump();

    } catch (const std::exception& e) {
        LOG_DEBUG("TOON generation failed: %s", e.what());

        // Fallback TOON
        nlohmann::json fallback;
        fallback["format"] = "toon";
        fallback["provider"] = "openai";
        fallback["content"] = content;
        fallback["error"] = "toon_generation_failed";
        return fallback.dump();
    }
}

bool OpenAIFormatter::validate_tool_call_schema(
    const ToolCall& tool_call,
    const nlohmann::json& schema) const {

    try {
        // Basic validation - could be expanded with full JSON schema validation
        if (schema.contains("properties") && !tool_call.parameters.is_null()) {
            const auto& properties = schema.at("properties");

            for (auto& [key, value] : properties.items()) {
                if (schema.contains("required")) {
                    for (const auto& required : schema.at("required")) {
                        if (required.get<std::string>() == key &&
                            !tool_call.parameters.contains(key)) {
                            return false;
                        }
                    }
                }
            }
        }

        return true;
    } catch (...) {
        return false; // Fail validation on errors
    }
}

nlohmann::json OpenAIFormatter::process_function_delta(const nlohmann::json& delta) {
    // Update streaming function call with delta
    if (delta.contains("name")) {
        streaming_function_call_["name"] = delta.at("name").get<std::string>();
    }

    if (delta.contains("arguments")) {
        if (!streaming_function_call_.contains("arguments")) {
            streaming_function_call_["arguments"] = "";
        }
        streaming_function_call_["arguments"] =
            streaming_function_call_["arguments"].get<std::string>() +
            delta.at("arguments").get<std::string>();
    }

    return streaming_function_call_;
}

void OpenAIFormatter::update_openai_metrics(
    uint64_t processing_time_us,
    const std::string& format_type,
    uint64_t function_calls_count,
    bool validation_passed) const {

    total_processing_count_.fetch_add(1);
    total_processing_time_us_.fetch_add(processing_time_us);

    if (!validation_passed) {
        validation_errors_.fetch_add(1);
    }

    // Track format-specific metrics
    if (format_type == "function-calling" || format_type == "legacy-function") {
        function_calls_processed_.fetch_add(function_calls_count);
    } else if (format_type == "structured-output") {
        structured_outputs_validated_.fetch_add(1);
    } else if (format_type == "legacy-format") {
        legacy_formats_processed_.fetch_add(1);
    }
}

std::string OpenAIFormatter::detect_format_type(const std::string& content) const {
    try {
        auto content_json = validate_json(content);
        if (!content_json) {
            return "unknown";
        }

        // Check for function calling
        if (content_json->contains("choices") && !content_json->at("choices").empty()) {
            const auto& choice = content_json->at("choices")[0];
            if (choice.contains("message")) {
                const auto& message = choice.at("message");

                if (message.contains("tool_calls")) {
                    return "function-calling";
                } else if (message.contains("function_call")) {
                    return "legacy-function";
                }
            }
        }

        // Check for structured output
        if (content_json->is_object() && !content_json->contains("choices")) {
            return "structured-output";
        }

        // Check for legacy format
        if (content_json->contains("text") ||
            (content_json->contains("choices") &&
             content_json->at("choices")[0].contains("text"))) {
            return "legacy-format";
        }

        return "chat-completion";

    } catch (...) {
        return "text"; // Assume plain text if JSON parsing fails
    }
}

std::string OpenAIFormatter::clean_openai_content(const std::string& content) const {
    try {
        // Skip regex operations on very large inputs to prevent stack overflow
        const size_t MAX_REGEX_SIZE = 100 * 1024; // 100KB limit for regex operations

        // Try to parse as JSON first
        auto content_json = validate_json(content);
        if (content_json) {
            return content_json->dump(); // Return cleaned JSON
        }

        // For large non-JSON content, return as-is without regex processing
        if (content.size() > MAX_REGEX_SIZE) {
            LOG_DEBUG("Content too large for regex cleaning (%zu bytes), returning as-is", content.size());
            return content;
        }

        // Clean plain text content
        std::string cleaned = content;

        // Remove common OpenAI artifacts
        static const std::regex artifacts_pattern(R"(\[DONE\]|data:\s*\{|:\s*\"?DONE\"?)", std::regex::optimize);
        cleaned = std::regex_replace(cleaned, artifacts_pattern, "");

        // Normalize whitespace
        static const std::regex whitespace_pattern(R"(\s+)", std::regex::optimize);
        cleaned = std::regex_replace(cleaned, whitespace_pattern, " ");

        // Trim
        cleaned.erase(0, cleaned.find_first_not_of(" \t\n\r"));
        cleaned.erase(cleaned.find_last_not_of(" \t\n\r") + 1);

        return cleaned;

    } catch (const std::exception& e) {
        LOG_DEBUG("Content cleaning failed: %s", e.what());
        return content; // Return original content on error
    }
}

bool OpenAIFormatter::contains_malicious_patterns(const std::string& content) const {
    std::vector<std::regex> malicious_patterns = {
        // XSS patterns
        std::regex(R"(<script[^>]*>)", std::regex::icase),
        std::regex(R"(javascript\s*:)", std::regex::icase),
        std::regex(R"(onerror\s*=)", std::regex::icase),
        std::regex(R"(onload\s*=)", std::regex::icase),
        std::regex(R"(</script>)", std::regex::icase),

        // Code execution patterns
        std::regex(R"(eval\s*\()", std::regex::icase),
        std::regex(R"(system\s*\()", std::regex::icase),
        std::regex(R"(exec\s*\()", std::regex::icase),

        // SQL injection patterns
        std::regex(R"('\s+OR\s+'1'\s*=\s*'1)", std::regex::icase),
        std::regex(R"("\s+OR\s+"1"\s*=\s*"1)", std::regex::icase),
        std::regex(R"(;\s*DROP\s+TABLE)", std::regex::icase),
        std::regex(R"(UNION\s+SELECT)", std::regex::icase),

        // Path traversal patterns
        std::regex(R"(\.\./)", std::regex::icase),
        std::regex(R"(\.\.[\\\/])", std::regex::icase),
        std::regex(R"(/etc/passwd)", std::regex::icase),
        std::regex(R"(c:\\windows)", std::regex::icase)
    };

    for (const auto& pattern : malicious_patterns) {
        if (std::regex_search(content, pattern)) {
            return true;
        }
    }

    return false;
}

// Factory function for plugin registration
extern "C" {
    std::shared_ptr<aimux::prettifier::PrettifierPlugin> create_openai_formatter() {
        return std::make_shared<aimux::prettifier::OpenAIFormatter>();
    }
}

} // namespace prettifier
} // namespace aimux