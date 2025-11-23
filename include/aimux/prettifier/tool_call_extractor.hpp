#pragma once

#include "aimux/prettifier/prettifier_plugin.hpp"
#include <regex>
#include <chrono>
#include <atomic>
#include <optional>

namespace aimux {
namespace prettifier {

/**
 * @brief Configuration for tool call extraction behavior
 */
struct ToolCallExtractorConfig {
    bool enable_security_validation = true;
    bool enable_json_parsing = true;
    bool enable_xml_parsing = true;
    bool enable_error_recovery = true;
    size_t max_content_size = 1024 * 1024; // 1MB max
    size_t max_tool_calls = 50; // Maximum number of tool calls to extract
    std::vector<std::string> allowed_tool_names = {
        "search", "calculate", "execute", "analyze", "fetch", "process",
        "validate", "transform", "query", "scan", "parse", "format"
    };

    nlohmann::json to_json() const {
        nlohmann::json j;
        j["enable_security_validation"] = enable_security_validation;
        j["enable_json_parsing"] = enable_json_parsing;
        j["enable_xml_parsing"] = enable_xml_parsing;
        j["enable_error_recovery"] = enable_error_recovery;
        j["max_content_size"] = max_content_size;
        j["max_tool_calls"] = max_tool_calls;
        j["allowed_tool_names"] = allowed_tool_names;
        return j;
    }

    static ToolCallExtractorConfig from_json(const nlohmann::json& j) {
        ToolCallExtractorConfig config;
        if (j.contains("enable_security_validation"))
            j.at("enable_security_validation").get_to(config.enable_security_validation);
        if (j.contains("enable_json_parsing"))
            j.at("enable_json_parsing").get_to(config.enable_json_parsing);
        if (j.contains("enable_xml_parsing"))
            j.at("enable_xml_parsing").get_to(config.enable_xml_parsing);
        if (j.contains("enable_error_recovery"))
            j.at("enable_error_recovery").get_to(config.enable_error_recovery);
        if (j.contains("max_content_size"))
            j.at("max_content_size").get_to(config.max_content_size);
        if (j.contains("max_tool_calls"))
            j.at("max_tool_calls").get_to(config.max_tool_calls);
        if (j.contains("allowed_tool_names"))
            j.at("allowed_tool_names").get_to(config.allowed_tool_names);
        return config;
    }
};

/**
 * @brief Statistics for tool call extraction operations
 */
struct ToolCallExtractorStats {
    std::atomic<uint64_t> total_extractions{0};
    std::atomic<uint64_t> successful_extractions{0};
    std::atomic<uint64_t> security_blocks{0};
    std::atomic<uint64_t> json_parse_failures{0};
    std::atomic<uint64_t> xml_parse_failures{0};
    std::atomic<uint64_t> tools_extracted{0};
    std::atomic<uint64_t> average_time_us{0};
    std::atomic<uint64_t> max_time_us{0};

    nlohmann::json to_json() const {
        nlohmann::json j;
        j["total_extractions"] = total_extractions.load();
        j["successful_extractions"] = successful_extractions.load();
        j["security_blocks"] = security_blocks.load();
        j["json_parse_failures"] = json_parse_failures.load();
        j["xml_parse_failures"] = xml_parse_failures.load();
        j["tools_extracted"] = tools_extracted.load();
        j["average_time_us"] = average_time_us.load();
        j["max_time_us"] = max_time_us.load();
        return j;
    }

    void reset() {
        total_extractions = 0;
        successful_extractions = 0;
        security_blocks = 0;
        json_parse_failures = 0;
        xml_parse_failures = 0;
        tools_extracted = 0;
        average_time_us = 0;
        max_time_us = 0;
    }
};


/**
 * @brief Provider-specific tool call patterns
 */
class ProviderToolPatterns {
public:
    /**
     * @brief Get Cerebras tool call patterns
     *
     * Cerebras often uses fast JSON-based tool calls with minimal formatting.
     * Pattern: {"tool_calls": [{"name": "...", "arguments": "..."}]}
     */
    static std::vector<std::regex> get_cerebras_patterns();

    /**
     * @brief Get OpenAI tool call patterns
     *
     * OpenAI uses structured function calling format with JSON.
     * Pattern: {"function": {"name": "...", "arguments": "..."}}
     */
    static std::vector<std::regex> get_openai_patterns();

    /**
     * @brief Get Anthropic tool call patterns
     *
     * Anthropic uses XML-based tool calls in Claude format.
     * Pattern: <function_calls><invoke name="...">...args...</invoke></function_calls>
     */
    static std::vector<std::regex> get_anthropic_patterns();

    /**
     * @brief Get Synthetic tool call patterns
     *
     * Synthetic uses mixed formats for testing and diagnostics.
     * Pattern: Multiple formats including JSON, XML, and text.
     */
    static std::vector<std::regex> get_synthetic_patterns();

    /**
     * @brief Get common tool call patterns applicable to all providers
     */
    static std::vector<std::regex> get_common_patterns();
};

/**
 * @brief Tool Call Extraction Plugin
 *
 * Extracts tool calls from AI responses with security validation and error recovery.
 * Supports multiple JSON/XML patterns and provider-specific formats.
 *
 * Performance targets:
 * - <20ms for 50 tool calls
 * - <2ms for typical single tool call
 * - Memory usage: <2MB for extraction buffers
 *
 * Security features:
 * - Tool name validation against allowlist
 * - JSON injection prevention
 * - XML entity expansion protection
 * - Size and count limits
 */
class ToolCallExtractorPlugin : public PrettifierPlugin {
public:
    /**
     * @brief Constructor with default configuration
     */
    ToolCallExtractorPlugin();

    /**
     * @brief Constructor with custom configuration
     */
    explicit ToolCallExtractorPlugin(const ToolCallExtractorConfig& config);

    // PrettifierPlugin interface implementation

    ProcessingResult preprocess_request(const core::Request& request) override;

    ProcessingResult postprocess_response(
        const core::Response& response,
        const ProcessingContext& context) override;

    std::string get_name() const override;
    std::string version() const override;
    std::string description() const override;
    std::vector<std::string> supported_formats() const override;
    std::vector<std::string> output_formats() const override;
    std::vector<std::string> supported_providers() const override;
    std::vector<std::string> capabilities() const override;

    // Streaming support
    bool begin_streaming(const ProcessingContext& context) override;
    ProcessingResult process_streaming_chunk(
        const std::string& chunk,
        bool is_final,
        const ProcessingContext& context) override;
    ProcessingResult end_streaming(const ProcessingContext& context) override;

    // Configuration and validation
    bool configure(const nlohmann::json& config) override;
    bool validate_configuration() const override;
    nlohmann::json get_configuration() const override;

    // Metrics and monitoring
    nlohmann::json get_metrics() const override;
    void reset_metrics() override;

    // Health and diagnostics
    nlohmann::json health_check() const override;
    nlohmann::json get_diagnostics() const override;

private:
    ToolCallExtractorConfig config_;
    mutable ToolCallExtractorStats stats_;

    // Streaming state
    std::string streaming_buffer_;
    bool streaming_active_ = false;
    std::string current_provider_;

    // Pre-compiled regex patterns for performance
    mutable std::mutex patterns_mutex_;
    std::map<std::string, std::vector<std::regex>> provider_patterns_;
    std::vector<std::regex> common_patterns_;

    // Core extraction methods

    /**
     * @brief Main tool call extraction entry point
     */
    std::vector<ToolCall> extract_tool_calls(
        const std::string& content,
        const std::string& provider) const;

    /**
     * @brief Provider-specific extraction
     */
    std::vector<ToolCall> extract_provider_tool_calls(
        const std::string& content,
        const std::string& provider) const;

    /**
     * @brief Extract JSON tool calls
     */
    std::vector<ToolCall> extract_json_tool_calls(const std::string& content) const;

    /**
     * @brief Extract XML tool calls
     */
    std::vector<ToolCall> extract_xml_tool_calls(const std::string& content) const;

    /**
     * @brief Parse structured tool call from JSON object
     */
    std::optional<ToolCall> parse_tool_call_from_json(const nlohmann::json& json_obj) const;

    /**
     * @brief Parse tool call from XML element
     */
    std::optional<ToolCall> parse_tool_call_from_xml(const std::string& xml_element) const;

    // Security and validation methods

    /**
     * @brief Security validation for extracted tool calls
     */
    bool validate_tool_call(const ToolCall& tool_call) const;

    /**
     * @brief Check for malicious tool injection
     */
    bool contains_malicious_patterns(const std::string& content) const;

    /**
     * @brief Validate tool name against allowlist
     */
    bool is_valid_tool_name(const std::string& tool_name) const;

    /**
     * @brief Sanitize tool arguments
     */
    std::string sanitize_tool_arguments(const std::string& arguments) const;

    // Error recovery methods

    /**
     * @brief Attempt to recover from JSON parse errors
     */
    std::vector<ToolCall> recover_from_json_error(const std::string& content) const;

    /**
     * @brief Attempt to recover from XML parse errors
     */
    std::vector<ToolCall> recover_from_xml_error(const std::string& content) const;

    // Performance optimization

    /**
     * @brief Initialize regex patterns for all providers
     */
    void initialize_patterns();

    /**
     * @brief Get cached patterns for provider
     */
    std::vector<std::regex> get_provider_patterns(const std::string& provider) const;

    /**
     * @brief Update performance statistics
     */
    void update_stats(std::chrono::microseconds duration, bool success, size_t tools_extracted = 0);

    /**
     * @brief Check content size limits
     */
    bool check_content_limits(const std::string& content) const;

    // Streaming-specific methods

    /**
     * @brief Reset streaming state
     */
    void reset_streaming_state();

    /**
     * @brief Process partial content in streaming mode
     */
    std::string process_streaming_extraction(const std::string& chunk, bool is_final);

    // Utility methods

    /**
     * @brief Generate unique tool call ID
     */
    std::string generate_call_id() const;

    /**
     * @brief Serialize tool calls to TOON format
     */
    std::string serialize_tool_calls_to_toon(const std::vector<ToolCall>& tool_calls) const;

    /**
     * @brief Log debug information
     */
    void log_debug(const std::string& operation, const std::string& message) const;

    /**
     * @brief Log error information
     */
    void log_error(const std::string& operation, const std::string& message) const;
};

} // namespace prettifier
} // namespace aimux