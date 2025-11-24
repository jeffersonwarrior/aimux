#include "aimux/prettifier/anthropic_formatter.hpp"
#include <regex>
#include <sstream>
#include <chrono>
#include <algorithm>
#include <iomanip>

// TODO: Replace with proper logging when available
#define LOG_ERROR(msg, ...) do { printf("[ANTHROPIC ERROR] " msg "\n", ##__VA_ARGS__); } while(0)
#define LOG_DEBUG(msg, ...) do { printf("[ANTHROPIC DEBUG] " msg "\n", ##__VA_ARGS__); } while(0)

namespace aimux {
namespace prettifier {

// ClaudePatterns implementation
AnthropicFormatter::ClaudePatterns::ClaudePatterns()
    : function_calls_start(R"(<function_calls[^>]*>)", std::regex::optimize | std::regex::icase)
    , function_calls_end(R"(</function_calls>)", std::regex::optimize | std::regex::icase)
    , function_call_pattern(R"(<invoke[^>]*name\s*=\s*\"([^\"]+)\"[^>]*>([\s\S]*?)</invoke>)", std::regex::optimize | std::regex::icase)
    , thinking_start(R"(<thinking>)", std::regex::optimize | std::regex::icase)
    , thinking_end(R"(</thinking>)", std::regex::optimize | std::regex::icase)
    , reflection_pattern(R"(<reflection[^>]*>([\s\S]*?)</reflection>)", std::regex::optimize | std::regex::icase)
    , code_block_pattern(R"(```[\s\S]*?```)", std::regex::optimize)
    , xml_artifact_pattern(R"(&[a-z]+;|&\#[0-9]+;)", std::regex::optimize) {
}

// AnthropicFormatter implementation
AnthropicFormatter::AnthropicFormatter()
    : patterns_(std::make_unique<ClaudePatterns>()) {
    LOG_DEBUG("AnthropicFormatter initialized with Claude XML tool use support");
}

std::string AnthropicFormatter::get_name() const {
    return "anthropic-claude-formatter-v1.0.0";
}

std::string AnthropicFormatter::version() const {
    return "1.0.0";
}

std::string AnthropicFormatter::description() const {
    return "Anthropic Claude formatter with comprehensive XML tool use, thinking blocks, and reasoning support. "
           "Handles Claude's unique response format including <function_calls> XML tags, <thinking> blocks, "
           "and detailed analytical responses. Optimized for preserving Claude's reasoning capabilities while "
           "providing structured TOON format output.";
}

std::vector<std::string> AnthropicFormatter::supported_formats() const {
    return {
        "xml-tool-use",
        "thinking-blocks",
        "reasoning-traces",
        "claude-response",
        "streaming-xml",
        "multimodal-content"
    };
}

std::vector<std::string> AnthropicFormatter::output_formats() const {
    return {"toon", "json", "normalized-text", "structured-data", "reasoning-separated"};
}

std::vector<std::string> AnthropicFormatter::supported_providers() const {
    return {"anthropic", "claude"};
}

std::vector<std::string> AnthropicFormatter::capabilities() const {
    return {
        "xml-tool-calls",
        "thinking-blocks",
        "reasoning-extraction",
        "multimodal-support",
        "streaming-support",
        "xml-validation",
        "content-preservation",
        "code-block-preservation",
        "reasoning-traces"
    };
}

ProcessingResult AnthropicFormatter::preprocess_request(const core::Request& request) {
    auto start_time = std::chrono::high_resolution_clock::now();

    try {
        ProcessingResult result;
        result.success = true;
        result.output_format = "claude-preprocessed";

        // Clone request data for modification
        nlohmann::json optimized_data = request.data;

        // Format tools for Claude's XML-based tool use
        if (optimized_data.contains("tools")) {
            nlohmann::json claude_tools = nlohmann::json::array();

            for (const auto& tool : optimized_data["tools"]) {
                nlohmann::json claude_tool = {
                    {"name", tool["function"]["name"]},
                    {"description", tool["function"]["description"]}
                };

                if (tool["function"].contains("parameters")) {
                    claude_tool["input_schema"] = tool["function"]["parameters"];
                } else {
                    claude_tool["input_schema"] = nlohmann::json::object();
                }

                claude_tools.push_back(claude_tool);
            }

            optimized_data["tools"] = claude_tools;
        }

        // Add Claude-specific system message optimizations
        if (optimized_data.contains("system")) {
            std::string system_msg = optimized_data["system"];

            if (preserve_thinking_) {
                system_msg += "\n\nWhen reasoning through complex problems, use <thinking> blocks to show your step-by-step analysis.";
            }

            if (extract_reasoning_) {
                system_msg += "\n\nExtract and clearly separate your reasoning from your final response.";
            }

            optimized_data["system"] = system_msg;
        }

        // Configure Claude-specific parameters
        if (!optimized_data.contains("max_tokens")) {
            optimized_data["max_tokens"] = 4096; // Claude default
        }

        // Add Claude-specific metadata
        optimized_data["_claude_metadata"] = {
            {"preprocessed_at", std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()},
            {"preserve_thinking", preserve_thinking_},
            {"extract_reasoning", extract_reasoning_},
            {"xml_validation", validate_xml_structure_},
            {"support_multimodal", support_multimodal_}
        };

        result.processed_content = optimized_data.dump();

        // Calculate processing time
        auto end_time = std::chrono::high_resolution_clock::now();
        auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(
            end_time - start_time).count();

        result.processing_time = std::chrono::milliseconds(elapsed_us / 1000);
        result.tokens_processed = request.data.dump().length() / 4;

        update_claude_metrics(static_cast<uint64_t>(elapsed_us), 0, 0, false);

        LOG_DEBUG("Claude request preprocessing completed in %ld us", elapsed_us);
        return result;

    } catch (const std::exception& e) {
        return create_error_result("Claude request preprocessing failed: " + std::string(e.what()), "preprocess_error");
    }
}

ProcessingResult AnthropicFormatter::postprocess_response(
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

        ProcessingResult result;
        result.success = true;
        result.output_format = "toon";

        // Clean content while preserving structure
        std::string cleaned_content = clean_claude_content(response.data);

        // Try JSON tool_use extraction first (Claude v3.5+ format)
        std::vector<ToolCall> tool_calls = extract_claude_json_tool_uses(response.data);

        // Fall back to XML extraction if no JSON tool_use blocks found (older Claude format)
        if (tool_calls.empty()) {
            tool_calls = extract_claude_xml_tool_calls(cleaned_content);
        }

        // Extract thinking blocks and reasoning
        auto [content_without_thinking, reasoning_content] = extract_thinking_blocks(cleaned_content);
        std::string final_content = content_without_thinking;

        // Process multimodal content if supported
        if (support_multimodal_) {
            final_content = process_multimodal_content(final_content);
        }

        // Generate Claude-optimized TOON format
        std::string toon_content = generate_claude_toon(
            final_content, tool_calls, reasoning_content, context);

        result.processed_content = toon_content;
        result.extracted_tool_calls = tool_calls;
        result.streaming_mode = context.streaming_mode;

        // Add reasoning as separate field if extracted
        if (!reasoning_content.empty()) {
            result.reasoning = reasoning_content;
        }

        // Add metadata
        result.metadata["provider"] = "anthropic";
        result.metadata["model_capabilities"] = detect_content_types(response.data);
        result.metadata["tool_calls_count"] = tool_calls.size();
        result.metadata["reasoning_extracted"] = !reasoning_content.empty();
        result.metadata["xml_tool_calls"] = !tool_calls.empty();

        // Calculate final metrics
        auto end_time = std::chrono::high_resolution_clock::now();
        auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(
            end_time - start_time).count();

        result.processing_time = std::chrono::milliseconds(elapsed_us / 1000);
        result.tokens_processed = final_content.length() / 4;

        // Update metrics
        bool has_thinking = response.data.find("<thinking>") != std::string::npos;
        update_claude_metrics(static_cast<uint64_t>(elapsed_us), tool_calls.size(), has_thinking ? 1 : 0, !reasoning_content.empty());

        LOG_DEBUG("Claude response processing completed in %ld us, tools: %zu, reasoning: %s",
                 elapsed_us, tool_calls.size(), reasoning_content.empty() ? "no" : "yes");
        return result;

    } catch (const std::exception& e) {
        xml_validation_errors_.fetch_add(1);
        return create_error_result("Claude response processing failed: " + std::string(e.what()), "postprocess_error");
    }
}

bool AnthropicFormatter::begin_streaming(const ProcessingContext& /*context*/) {
    try {
        streaming_content_.clear();
        streaming_content_.reserve(16384); // Larger buffer for Claude's longer responses
        xml_buffer_.clear();
        xml_buffer_.reserve(4096);
        thinking_buffer_.clear();
        thinking_buffer_.reserve(2048);

        in_xml_block_ = false;
        in_thinking_block_ = false;
        streaming_active_ = true;
        streaming_start_ = std::chrono::steady_clock::now();

        LOG_DEBUG("Claude streaming initialization completed");
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Claude streaming initialization failed: %s", e.what());
        return false;
    }
}

ProcessingResult AnthropicFormatter::process_streaming_chunk(
    const std::string& chunk,
    bool is_final,
    const ProcessingContext& /*context*/) {

    if (!streaming_active_) {
        return create_error_result("Claude streaming not initialized", "streaming_not_active");
    }

    try {
        ProcessingResult result;
        result.success = true;
        result.streaming_mode = true;

        // Process streaming XML reconstruction
        std::string processed_chunk = process_streaming_xml(chunk);

        // Accumulate content
        streaming_content_ += processed_chunk;

        // Check for XML tool calls in streaming buffer
        std::vector<ToolCall> streaming_tools;
        if (in_xml_block_ || streaming_content_.find("<invoke") != std::string::npos) {
            streaming_tools = extract_claude_xml_tool_calls(xml_buffer_);
        }

        // Check for thinking blocks
        if (in_thinking_block_ || streaming_content_.find("<thinking>") != std::string::npos) {
            // Extract partial thinking content
            std::smatch thinking_match;
            if (std::regex_search(streaming_content_, thinking_match, patterns_->thinking_start) &&
                std::regex_search(streaming_content_, thinking_match, patterns_->thinking_end)) {
                std::string full_thinking = thinking_match.str();
                if (full_thinking.length() <= static_cast<size_t>(max_thinking_length_)) {
                    thinking_buffer_ = full_thinking;
                }
            }
        }

        result.processed_content = processed_chunk;
        result.extracted_tool_calls = streaming_tools;

        if (!thinking_buffer_.empty()) {
            result.reasoning = thinking_buffer_;
        }

        if (is_final) {
            result.metadata["final_chunk"] = true;
        }

        return result;

    } catch (const std::exception& e) {
        return create_error_result("Claude streaming chunk processing failed: " + std::string(e.what()), "streaming_error");
    }
}

ProcessingResult AnthropicFormatter::end_streaming(const ProcessingContext& context) {
    if (!streaming_active_) {
        return create_error_result("Claude streaming not active", "streaming_not_active");
    }

    try {
        auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - streaming_start_).count();

        // Final processing of accumulated content
        std::vector<ToolCall> final_tool_calls = extract_claude_xml_tool_calls(streaming_content_);
        auto [final_content, final_reasoning] = extract_thinking_blocks(streaming_content_);

        // Generate final TOON format
        std::string final_toon = generate_claude_toon(
            final_content, final_tool_calls, final_reasoning, context);

        ProcessingResult result;
        result.success = true;
        result.processed_content = final_toon;
        result.extracted_tool_calls = final_tool_calls;
        result.streaming_mode = false;
        result.processing_time = std::chrono::milliseconds(total_time);

        if (!final_reasoning.empty()) {
            result.reasoning = final_reasoning;
        }

        result.metadata["streaming_completed"] = true;
        result.metadata["total_streaming_time_ms"] = total_time;
        result.metadata["final_content_length"] = streaming_content_.length();

        // Reset streaming state
        streaming_active_ = false;
        streaming_content_.clear();
        xml_buffer_.clear();
        thinking_buffer_.clear();
        in_xml_block_ = false;
        in_thinking_block_ = false;

        LOG_DEBUG("Claude streaming completed in %ld ms", total_time);
        return result;

    } catch (const std::exception& e) {
        streaming_active_ = false;
        return create_error_result("Claude streaming end failed: " + std::string(e.what()), "streaming_end_error");
    }
}

bool AnthropicFormatter::configure(const nlohmann::json& config) {
    try {
        if (config.contains("preserve_thinking")) {
            preserve_thinking_ = config["preserve_thinking"].get<bool>();
        }

        if (config.contains("extract_reasoning")) {
            extract_reasoning_ = config["extract_reasoning"].get<bool>();
        }

        if (config.contains("validate_xml_structure")) {
            validate_xml_structure_ = config["validate_xml_structure"].get<bool>();
        }

        if (config.contains("support_multimodal")) {
            support_multimodal_ = config["support_multimodal"].get<bool>();
        }

        if (config.contains("preserve_code_blocks")) {
            preserve_code_blocks_ = config["preserve_code_blocks"].get<bool>();
        }

        if (config.contains("max_thinking_length")) {
            max_thinking_length_ = config["max_thinking_length"].get<int>();
            if (max_thinking_length_ <= 0) {
                max_thinking_length_ = 10000; // Reset to default
            }
        }

        if (config.contains("clean_xml_artifacts")) {
            clean_xml_artifacts_ = config["clean_xml_artifacts"].get<bool>();
        }

        LOG_DEBUG("Claude configuration updated successfully");
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Claude configuration failed: %s", e.what());
        return false;
    }
}

bool AnthropicFormatter::validate_configuration() const {
    return max_thinking_length_ > 0 && max_thinking_length_ <= 100000; // Reasonable limits
}

nlohmann::json AnthropicFormatter::get_configuration() const {
    nlohmann::json config;
    config["preserve_thinking"] = preserve_thinking_;
    config["extract_reasoning"] = extract_reasoning_;
    config["validate_xml_structure"] = validate_xml_structure_;
    config["support_multimodal"] = support_multimodal_;
    config["preserve_code_blocks"] = preserve_code_blocks_;
    config["max_thinking_length"] = max_thinking_length_;
    config["clean_xml_artifacts"] = clean_xml_artifacts_;
    return config;
}

nlohmann::json AnthropicFormatter::get_metrics() const {
    nlohmann::json metrics;

    uint64_t count = total_processing_count_.load();
    uint64_t total_time = total_processing_time_us_.load();

    metrics["total_processing_count"] = count;
    metrics["total_processing_time_us"] = total_time;
    metrics["average_processing_time_us"] = count > 0 ? total_time / count : 0;
    metrics["xml_tool_calls_extracted"] = xml_tool_calls_extracted_.load();
    metrics["thinking_blocks_processed"] = thinking_blocks_processed_.load();
    metrics["reasoning_content_extracted"] = reasoning_content_extracted_.load();
    metrics["xml_validation_errors"] = xml_validation_errors_.load();
    metrics["multimodal_responses_processed"] = multimodal_responses_processed_.load();

    // Calculate rates
    metrics["xml_tool_call_rate"] = count > 0 ?
        static_cast<double>(xml_tool_calls_extracted_.load()) / count : 0.0;
    metrics["thinking_block_rate"] = count > 0 ?
        static_cast<double>(thinking_blocks_processed_.load()) / count : 0.0;
    metrics["reasoning_extraction_rate"] = count > 0 ?
        static_cast<double>(reasoning_content_extracted_.load()) / count : 0.0;

    return metrics;
}

void AnthropicFormatter::reset_metrics() {
    total_processing_count_.store(0);
    total_processing_time_us_.store(0);
    xml_tool_calls_extracted_.store(0);
    thinking_blocks_processed_.store(0);
    reasoning_content_extracted_.store(0);
    xml_validation_errors_.store(0);
    multimodal_responses_processed_.store(0);
}

nlohmann::json AnthropicFormatter::health_check() const {
    nlohmann::json health;

    try {
        // Basic health status
        health["status"] = "healthy";
        health["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        // Test XML tool call extraction
        std::string test_xml = R"(<function_calls>
<invoke name="test_function">
<parameter name="param">value</parameter>
</invoke>
</function_calls>)";

        auto test_calls = extract_claude_xml_tool_calls(test_xml);
        health["xml_tool_call_extraction"] = !test_calls.empty();

        // Test thinking block extraction
        std::string test_thinking = R"(<thinking>
This is a test thinking block with step-by-step reasoning.
</thinking>

Here is the final response.)";

        auto [cleaned, reasoning] = extract_thinking_blocks(test_thinking);
        health["thinking_block_extraction"] = !reasoning.empty();

        // Test XML validation
        std::string valid_xml = R"(<function_calls></function_calls>)";
        health["xml_validation"] = validate_claude_xml(valid_xml);

        // Configuration validation
        health["configuration_valid"] = validate_configuration();

        // Pattern availability
        health["patterns_available"] = patterns_ != nullptr;

        // Recent performance
        auto metrics = get_metrics();
        health["recent_performance"] = {
            {"avg_processing_time_us", metrics["average_processing_time_us"]},
            {"xml_extraction_rate", metrics["xml_tool_call_rate"]},
            {"thinking_processing_rate", metrics["thinking_block_rate"]},
            {"validation_errors", metrics["xml_validation_errors"]}
        };

    } catch (const std::exception& e) {
        health["status"] = "unhealthy";
        health["error"] = e.what();
    }

    return health;
}

nlohmann::json AnthropicFormatter::get_diagnostics() const {
    nlohmann::json diagnostics;

    diagnostics["name"] = get_name();
    diagnostics["version"] = version();
    diagnostics["status"] = "active";
    diagnostics["configuration"] = get_configuration();
    diagnostics["metrics"] = get_metrics();

    // Capabilities matrix
    diagnostics["capabilities"] = {
        {"xml_tool_calls", true},
        {"thinking_blocks", preserve_thinking_},
        {"reasoning_extraction", extract_reasoning_},
        {"xml_validation", validate_xml_structure_},
        {"multimodal_support", support_multimodal_},
        {"code_preservation", preserve_code_blocks_}
    };

    // Performance analysis
    uint64_t avg_time = total_processing_count_.load() > 0 ?
        total_processing_time_us_.load() / total_processing_count_.load() : 0;

    diagnostics["performance_analysis"] = {
        {"average_processing_time_us", avg_time},
        {"meets_performance_target", avg_time < 45000}, // 45ms target
        {"xml_processing_efficiency", xml_tool_calls_extracted_.load() > 0 ?
            static_cast<double>(xml_tool_calls_extracted_.load()) / total_processing_count_.load() : 0.0},
        {"thinking_processing_efficiency", thinking_blocks_processed_.load() > 0 ?
            static_cast<double>(thinking_blocks_processed_.load()) / total_processing_count_.load() : 0.0}
    };

    // Content processing statistics
    diagnostics["content_processing"] = {
        {"multimodal_responses", multimodal_responses_processed_.load()},
        {"reasoning_extractions", reasoning_content_extracted_.load()},
        {"xml_validation_errors", xml_validation_errors_.load()}
    };

    return diagnostics;
}

// Private helper methods implementation

std::vector<ToolCall> AnthropicFormatter::extract_claude_xml_tool_calls(const std::string& content) const {
    std::vector<ToolCall> tool_calls;

    try {
        std::smatch match;
        std::string search_content = content;

        // Find function_calls blocks
        while (std::regex_search(search_content, match, patterns_->function_calls_start)) {
            size_t start_pos = static_cast<size_t>(match.position());
            size_t end_pos = search_content.find("</function_calls>", start_pos);

            if (end_pos != std::string::npos) {
                std::string function_block = search_content.substr(start_pos, end_pos - start_pos + 17);

                // Validate XML structure if enabled
                if (validate_xml_structure_ && !validate_claude_xml(function_block)) {
                    xml_validation_errors_.fetch_add(1);
                    search_content = match.suffix();
                    continue;
                }

                // Extract individual function calls
                std::smatch invoke_match;
                std::string block_search = function_block;

                while (std::regex_search(block_search, invoke_match, patterns_->function_call_pattern)) {
                    ToolCall call;
                    call.status = "completed";
                    call.timestamp = std::chrono::system_clock::now();

                    call.name = invoke_match[1].str();

                    // Extract parameters from invoke content
                    std::string param_content = invoke_match[2].str();
                    nlohmann::json params = nlohmann::json::object();

                    // Parse parameter tags
                    static const std::regex param_pattern(R"(<parameter[^>]*name\s*=\s*\"([^\"]+)\"[^>]*>(.*?)</parameter>)", std::regex::icase);
                    std::smatch param_match;
                    std::string param_search = param_content;

                    while (std::regex_search(param_search, param_match, param_pattern)) {
                        std::string param_name = param_match[1].str();
                        std::string param_value = param_match[2].str();

                        try {
                            // Try to parse as JSON, fallback to string
                            params[param_name] = nlohmann::json::parse(param_value);
                        } catch (...) {
                            params[param_name] = param_value;
                        }

                        param_search = param_match.suffix();
                    }

                    call.parameters = params;
                    tool_calls.push_back(call);

                    block_search = invoke_match.suffix();
                }

                search_content = search_content.substr(end_pos + 17);
            } else {
                break;
            }
        }

        xml_tool_calls_extracted_.fetch_add(tool_calls.size());

    } catch (const std::exception& e) {
        LOG_DEBUG("Claude XML tool call extraction failed: %s", e.what());
        xml_validation_errors_.fetch_add(1);
    }

    return tool_calls;
}

std::pair<std::string, std::string> AnthropicFormatter::extract_thinking_blocks(const std::string& content) const {
    std::string cleaned_content = content;
    std::string reasoning_content;

    try {
        std::smatch thinking_match;

        // Find thinking blocks
        if (std::regex_search(cleaned_content, thinking_match, patterns_->thinking_start)) {
            size_t think_start = static_cast<size_t>(thinking_match.position());

            // Find corresponding end tag
            size_t think_end = cleaned_content.find("</thinking>", think_start);

            if (think_end != std::string::npos) {
                // Extract thinking content
                std::string thinking_block = cleaned_content.substr(
                    think_start, think_end - think_start + 12); // Include closing tag

                // Remove thinking tags for cleaner content
                std::string thinking_inner = cleaned_content.substr(
                    think_start + 10, think_end - think_start - 10); // Content between tags

                if (thinking_inner.length() <= static_cast<size_t>(max_thinking_length_)) {
                    reasoning_content = thinking_inner;

                    // Remove the thinking block from main content
                    cleaned_content.erase(think_start, think_end - think_start + 12);

                    thinking_blocks_processed_.fetch_add(1);

                    // Clean up extra whitespace
                    static const std::regex whitespace_clean(R"(\s*\n\s*\n\s*\n)");
                    cleaned_content = std::regex_replace(cleaned_content, whitespace_clean, "\n\n");
                }
            }
        }

        // Extract reflection blocks if present
        if (extract_reasoning_) {
            std::smatch reflection_match;
            std::string search_content = cleaned_content;

            while (std::regex_search(search_content, reflection_match, patterns_->reflection_pattern)) {
                std::string reflection_content = reflection_match[1].str();
                if (!reasoning_content.empty()) {
                    reasoning_content += "\n\n";
                }
                reasoning_content += "Reflection: " + reflection_content;

                // Remove reflection block from main content
                size_t refl_start = search_content.find(reflection_match.str());
                search_content.erase(refl_start, reflection_match.str().length());
            }

            if (!reasoning_content.empty()) {
                cleaned_content = search_content;
                reasoning_content_extracted_.fetch_add(1);
            }
        }

    } catch (const std::exception& e) {
        LOG_DEBUG("Thinking block extraction failed: %s", e.what());
    }

    return {cleaned_content, reasoning_content};
}

bool AnthropicFormatter::validate_claude_xml(const std::string& xml_content) const {
    try {
        // Basic XML validation - check for well-formed tags
        size_t pos = 0;
        std::vector<std::string> open_tags;

        while (pos < xml_content.length()) {
            // Find next tag
            size_t tag_start = xml_content.find('<', pos);
            if (tag_start == std::string::npos) break;

            size_t tag_end = xml_content.find('>', tag_start);
            if (tag_end == std::string::npos) return false; // Unclosed tag

            std::string tag = xml_content.substr(tag_start, tag_end - tag_start + 1);

            if (tag[1] == '/') { // Closing tag
                std::string tag_name = tag.substr(2, tag.find('>', 2) - 2);
                if (open_tags.empty() || open_tags.back() != tag_name) {
                    return false; // Unmatched closing tag
                }
                open_tags.pop_back();
            } else if (tag[tag.length() - 2] == '/') { // Self-closing tag
                // No stack operation needed
            } else { // Opening tag
                size_t name_end = tag.find(' ');
                std::string tag_name = (name_end == std::string::npos) ?
                    tag.substr(1, tag.length() - 2) : tag.substr(1, name_end - 1);
                open_tags.push_back(tag_name);
            }

            pos = tag_end + 1;
        }

        return open_tags.empty(); // All tags should be closed

    } catch (...) {
        return false; // Validation failed on exception
    }
}

std::string AnthropicFormatter::clean_claude_content(const std::string& content) const {
    try {
        std::string cleaned = content;

        // Clean XML artifacts if enabled
        if (clean_xml_artifacts_) {
            cleaned = std::regex_replace(cleaned, patterns_->xml_artifact_pattern, "");
        }

        // Normalize whitespace while preserving structure
        // Preserve code blocks if enabled
        std::vector<std::string> code_blocks;
        if (preserve_code_blocks_) {
            std::smatch code_match;
            std::string search_content = cleaned;

            while (std::regex_search(search_content, code_match, patterns_->code_block_pattern)) {
                code_blocks.push_back(code_match.str());
                search_content = code_match.suffix();
            }

            // Replace code blocks with placeholders
            for (size_t i = 0; i < code_blocks.size(); ++i) {
                cleaned = std::regex_replace(cleaned, patterns_->code_block_pattern,
                    "CODE_BLOCK_PLACEHOLDER_" + std::to_string(i));
            }
        }

        // Normalize line endings and excess whitespace
        static const std::regex line_endings(R"(\r\n|\r)");
        cleaned = std::regex_replace(cleaned, line_endings, "\n");

        static const std::regex excess_whitespace(R"(\n\s*\n\s*\n)");
        cleaned = std::regex_replace(cleaned, excess_whitespace, "\n\n");

        // Restore code blocks if they were extracted
        if (preserve_code_blocks_ && !code_blocks.empty()) {
            for (size_t i = 0; i < code_blocks.size(); ++i) {
                std::string placeholder = "CODE_BLOCK_PLACEHOLDER_" + std::to_string(i);
                size_t pos = cleaned.find(placeholder);
                if (pos != std::string::npos) {
                    cleaned.replace(pos, placeholder.length(), code_blocks[i]);
                }
            }
        }

        return cleaned;

    } catch (const std::exception& e) {
        LOG_DEBUG("Content cleaning failed: %s", e.what());
        return content; // Return original content on error
    }
}

std::string AnthropicFormatter::generate_claude_toon(
    const std::string& content,
    const std::vector<ToolCall>& tool_calls,
    const std::string& reasoning,
    const ProcessingContext& context) const {

    try {
        nlohmann::json toon;

        // Basic TOON structure
        toon["format"] = "toon";
        toon["version"] = "1.0.0";
        toon["provider"] = "anthropic";
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

        // Claude-specific metadata
        nlohmann::json claude_metadata = {
            {"processed_at", std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()},
            {"preserved_thinking", preserve_thinking_},
            {"extracted_reasoning", !reasoning.empty()},
            {"xml_validation_enabled", validate_xml_structure_},
            {"multimodal_support", support_multimodal_},
            {"capabilities", capabilities()}
        };

        // Add reasoning if extracted
        if (!reasoning.empty()) {
            toon["reasoning"] = reasoning;
            claude_metadata["reasoning_length"] = reasoning.length();
        }

        toon["metadata"] = claude_metadata;

        return toon.dump();

    } catch (const std::exception& e) {
        LOG_DEBUG("Claude TOON generation failed: %s", e.what());

        // Fallback TOON
        nlohmann::json fallback;
        fallback["format"] = "toon";
        fallback["provider"] = "anthropic";
        fallback["content"] = content;
        fallback["error"] = "claude_toon_generation_failed";
        return fallback.dump();
    }
}

std::string AnthropicFormatter::process_streaming_xml(const std::string& chunk) {
    std::string processed = chunk;

    // Track XML block boundaries
    if (!in_xml_block_) {
        size_t start_pos = processed.find("<function_calls");
        if (start_pos != std::string::npos) {
            in_xml_block_ = true;
            xml_buffer_ += processed.substr(start_pos);

            // Return content before XML block
            return processed.substr(0, start_pos);
        }
    } else {
        xml_buffer_ += processed;

        size_t end_pos = processed.find("</function_calls>");
        if (end_pos != std::string::npos) {
            in_xml_block_ = false;

            // Process complete XML buffer
            auto tools = extract_claude_xml_tool_calls(xml_buffer_);

            // Return content after XML block
            return processed.substr(end_pos + 18);
        }
    }

    // Track thinking block boundaries
    if (!in_thinking_block_) {
        size_t think_start = processed.find("<thinking>");
        if (think_start != std::string::npos) {
            in_thinking_block_ = true;
            thinking_buffer_ += processed.substr(think_start);
            return processed.substr(0, think_start);
        }
    } else {
        thinking_buffer_ += processed;

        size_t think_end = processed.find("</thinking>");
        if (think_end != std::string::npos) {
            in_thinking_block_ = false;
            return processed.substr(think_end + 12);
        }
    }

    // Return chunk as-is if not in special blocks
    return (!in_xml_block_ && !in_thinking_block_) ? processed : "";
}

void AnthropicFormatter::update_claude_metrics(
    uint64_t processing_time_us,
    uint64_t xml_tool_calls_count,
    uint64_t thinking_blocks_count,
    bool reasoning_extracted) const {

    total_processing_count_.fetch_add(1);
    total_processing_time_us_.fetch_add(processing_time_us);

    if (xml_tool_calls_count > 0) {
        xml_tool_calls_extracted_.fetch_add(xml_tool_calls_count);
    }

    if (thinking_blocks_count > 0) {
        thinking_blocks_processed_.fetch_add(thinking_blocks_count);
    }

    if (reasoning_extracted) {
        reasoning_content_extracted_.fetch_add(1);
    }
}

std::vector<std::string> AnthropicFormatter::detect_content_types(const std::string& content) const {
    std::vector<std::string> types;

    if (content.find("<function_calls>") != std::string::npos) {
        types.push_back("xml_tool_use");
    }

    if (content.find("<thinking>") != std::string::npos) {
        types.push_back("thinking_blocks");
    }

    if (content.find("<reflection>") != std::string::npos) {
        types.push_back("reflection_blocks");
    }

    if (content.find("```") != std::string::npos) {
        types.push_back("code_blocks");
    }

    if (content.find("![image]") != std::string::npos || content.find("image_analysis") != std::string::npos) {
        types.push_back("multimodal");
    }

    if (types.empty()) {
        types.push_back("text_response");
    }

    return types;
}

std::string AnthropicFormatter::process_multimodal_content(const std::string& content) const {
    // Basic multimodal processing - could be expanded significantly
    try {
        std::string processed = content;

        // Handle image analysis results
        static const std::regex image_pattern(R"(\[Image: ([^\]]+)\])");
        processed = std::regex_replace(processed, image_pattern, "🖼️ Image Analysis: $1");

        // Handle document processing results
        static const std::regex doc_pattern(R"(Document Analysis:|Document Content:)");
        processed = std::regex_replace(processed, doc_pattern, "📄 Document Content:");

        if (processed != content) {
            multimodal_responses_processed_.fetch_add(1);
        }

        return processed;

    } catch (const std::exception& e) {
        LOG_DEBUG("Multimodal processing failed: %s", e.what());
        return content;
    }
}

std::string AnthropicFormatter::extract_reasoning_traces(const std::string& content) const {
    try {
        std::string traces;

        // Extract numbered reasoning steps
        static const std::regex step_pattern(R"(\d+\.\s*([^.\n]+))");
        std::sregex_iterator iter(content.begin(), content.end(), step_pattern);
        std::sregex_iterator end;

        for (std::sregex_iterator i = iter; i != end; ++i) {
            if (!traces.empty()) traces += "\n";
            traces += i->str();
        }

        // Extract analysis keywords
        static const std::regex analysis_pattern(R"((analysis|examination|evaluation|consideration)[^.,]*[.,])", std::regex::icase);
        std::smatch match;
        std::string search_content = content;

        while (std::regex_search(search_content, match, analysis_pattern)) {
            if (!traces.empty()) traces += "\n";
            traces += match.str();
            search_content = match.suffix();
        }

        return traces;

    } catch (const std::exception& e) {
        LOG_DEBUG("Reasoning trace extraction failed: %s", e.what());
        return "";
    }
}

bool AnthropicFormatter::validate_claude_tool_call(const ToolCall& tool_call) const {
    if (tool_call.name.empty()) {
        return false;
    }

    if (!tool_call.parameters.is_object()) {
        return false;
    }

    // Additional validation could be added here based on tool schemas
    return true;
}

std::vector<ToolCall> AnthropicFormatter::extract_claude_json_tool_uses(const std::string& content) const {
    std::vector<ToolCall> tool_calls;

    try {
        // Try to parse content as JSON - this handles API response format
        nlohmann::json response_json;

        // First, try to parse the entire content as JSON (v3.5+ format with content array)
        try {
            response_json = nlohmann::json::parse(content);
        } catch (...) {
            // If not valid JSON, try to extract a JSON object from the content
            // This handles cases where content has surrounding text
            std::string::size_type start = content.find('{');
            std::string::size_type end = content.rfind('}');

            if (start != std::string::npos && end != std::string::npos && start < end) {
                std::string json_str = content.substr(start, end - start + 1);
                try {
                    response_json = nlohmann::json::parse(json_str);
                } catch (...) {
                    LOG_DEBUG("Failed to parse JSON from content");
                    return tool_calls;
                }
            } else {
                LOG_DEBUG("No JSON object found in content");
                return tool_calls;
            }
        }

        // Check for content array (Claude v3.5+ format)
        if (response_json.contains("content") && response_json["content"].is_array()) {
            const auto& content_array = response_json["content"];

            for (const auto& content_item : content_array) {
                // Look for tool_use type blocks
                if (content_item.is_object() &&
                    content_item.contains("type") &&
                    content_item["type"].is_string() &&
                    content_item["type"].get<std::string>() == "tool_use") {

                    ToolCall call;
                    call.status = "completed";
                    call.timestamp = std::chrono::system_clock::now();

                    // Extract tool name
                    if (content_item.contains("name") && content_item["name"].is_string()) {
                        call.name = content_item["name"].get<std::string>();
                    } else {
                        LOG_DEBUG("Tool use block missing 'name' field");
                        continue;
                    }

                    // Extract tool ID if present
                    if (content_item.contains("id") && content_item["id"].is_string()) {
                        call.id = content_item["id"].get<std::string>();
                    }

                    // Extract parameters from input field
                    if (content_item.contains("input")) {
                        try {
                            // Input can be an object or a string representation of JSON
                            if (content_item["input"].is_object()) {
                                call.parameters = content_item["input"];
                            } else if (content_item["input"].is_string()) {
                                // Try to parse string as JSON
                                try {
                                    call.parameters = nlohmann::json::parse(content_item["input"].get<std::string>());
                                } catch (...) {
                                    // If not JSON, store as a single "value" parameter
                                    call.parameters = nlohmann::json::object();
                                    call.parameters["value"] = content_item["input"].get<std::string>();
                                }
                            } else {
                                call.parameters = nlohmann::json::object();
                            }
                        } catch (const std::exception& e) {
                            LOG_DEBUG("Failed to parse tool input: %s", e.what());
                            call.parameters = nlohmann::json::object();
                        }
                    } else {
                        // No input field, create empty parameters object
                        call.parameters = nlohmann::json::object();
                    }

                    // Validate and add the tool call
                    if (validate_claude_tool_call(call)) {
                        tool_calls.push_back(call);
                        LOG_DEBUG("Extracted JSON tool_use: %s (id: %s)", call.name.c_str(), call.id.c_str());
                    }
                }
            }
        }

        // Also check for direct tool_use field at root level (alternative format)
        if (tool_calls.empty() && response_json.contains("tool_use") && response_json["tool_use"].is_array()) {
            const auto& tool_uses = response_json["tool_use"];

            for (const auto& tool_use : tool_uses) {
                if (tool_use.is_object()) {
                    ToolCall call;
                    call.status = "completed";
                    call.timestamp = std::chrono::system_clock::now();

                    if (tool_use.contains("name") && tool_use["name"].is_string()) {
                        call.name = tool_use["name"].get<std::string>();
                    } else {
                        continue;
                    }

                    if (tool_use.contains("id") && tool_use["id"].is_string()) {
                        call.id = tool_use["id"].get<std::string>();
                    }

                    if (tool_use.contains("input")) {
                        try {
                            if (tool_use["input"].is_object()) {
                                call.parameters = tool_use["input"];
                            } else if (tool_use["input"].is_string()) {
                                try {
                                    call.parameters = nlohmann::json::parse(tool_use["input"].get<std::string>());
                                } catch (...) {
                                    call.parameters = nlohmann::json::object();
                                    call.parameters["value"] = tool_use["input"].get<std::string>();
                                }
                            } else {
                                call.parameters = nlohmann::json::object();
                            }
                        } catch (const std::exception& e) {
                            LOG_DEBUG("Failed to parse tool input: %s", e.what());
                            call.parameters = nlohmann::json::object();
                        }
                    } else {
                        call.parameters = nlohmann::json::object();
                    }

                    if (validate_claude_tool_call(call)) {
                        tool_calls.push_back(call);
                        LOG_DEBUG("Extracted JSON tool_use (alt format): %s", call.name.c_str());
                    }
                }
            }
        }

        if (!tool_calls.empty()) {
            LOG_DEBUG("Extracted %zu JSON tool_use blocks", tool_calls.size());
        }

        // Update metrics
        const_cast<std::atomic<uint64_t>&>(xml_tool_calls_extracted_).fetch_add(tool_calls.size());

    } catch (const std::exception& e) {
        LOG_DEBUG("JSON tool_use extraction failed: %s", e.what());
        const_cast<std::atomic<uint64_t>&>(xml_validation_errors_).fetch_add(1);
    }

    return tool_calls;
}

// Factory function for plugin registration
extern "C" {
    std::shared_ptr<aimux::prettifier::PrettifierPlugin> create_anthropic_formatter() {
        return std::make_shared<aimux::prettifier::AnthropicFormatter>();
    }
}

} // namespace prettifier
} // namespace aimux