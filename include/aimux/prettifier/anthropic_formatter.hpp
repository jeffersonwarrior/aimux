#pragma once

#include "aimux/prettifier/prettifier_plugin.hpp"
#include <chrono>
#include <atomic>
#include <memory>
#include <string>
#include <regex>

namespace aimux {
namespace prettifier {

/**
 * @brief Anthropic Claude response formatter with XML tool use and reasoning support
 *
 * This formatter specializes in handling responses from Anthropic Claude models, with full
 * support for Claude's unique XML-based tool use tags, thinking/reasoning blocks, and
 * detailed response style. It optimizes TOON format for Claude's characteristic output
 * while preserving Claude's advanced reasoning capabilities.
 *
 * Key features:
 * - Claude-specific XML tool use tag parsing (function_calls)
 * - Support for Claude's thinking/reasoning blocks separation
 * - Optimized TOON format for Claude's detailed response style
 * - Claude-specific content validation and cleanup
 * - Support for Claude's multi-modal outputs
 * - Handling Claude's <thinking> and <reflection> blocks
 *
 * Supported Claude features:
 * - Standard XML tool use format: <function_calls>...</function_calls>
 * - Thinking blocks: <thinking>...</thinking>
 * - Reasoning traces and analysis
 * - Claude's detailed analytical responses
 * - Tool result handling with XML formatting
 * - Text and code block preservation
 *
 * Performance targets:
 * - <45ms response processing time (Claude responses can be longer)
 * - <20ms XML tool call extraction
 * - <25ms reasoning block separation
 * - Preservation of Claude's analytical depth
 *
 * Usage example:
 * @code
 * auto formatter = std::make_shared<AnthropicFormatter>();
 * formatter->configure({
 *     {"preserve_thinking", true},
 *     {"extract_reasoning", true},
 *     {"validate_xml_structure", true},
 *     {"support_multimodal", true}
 * });
 *
 * ProcessingContext context;
 * context.provider_name = "anthropic";
 * context.model_name = "claude-3-sonnet";
 *
 * ProcessingResult result = formatter->postprocess_response(response, context);
 * @endcode
 */
class AnthropicFormatter : public PrettifierPlugin {
public:
    /**
     * @brief Constructor
     *
     * Initializes the Anthropic formatter with comprehensive support for Claude's
     * unique response formats. Sets up XML parsing for tool use tags, handling of
     * thinking blocks, and optimization for Claude's detailed response style.
     *
     * @param model_name Optional model name to use (empty = use global default from config)
     */
    AnthropicFormatter(const std::string& model_name = "");

    /**
     * @brief Destructor
     *
     * Cleans up resources and ensures thread-safe shutdown of any background processing.
     */
    ~AnthropicFormatter() override = default;

    // Plugin identification and metadata

    /**
     * @brief Get the unique identifier for this plugin
     * @return "anthropic-claude-formatter-v1.0.0"
     */
    std::string get_name() const override;

    /**
     * @brief Get the plugin version
     * @return "1.0.0"
     */
    std::string version() const override;

    /**
     * @brief Get human-readable description
     * @return Detailed description of Anthropic formatter capabilities
     */
    std::string description() const override;

    /**
     * @brief Get supported input formats
     * @return Vector of formats Claude typically outputs
     */
    std::vector<std::string> supported_formats() const override;

    /**
     * @brief Get output formats this plugin generates
     * @return Vector of standardized output formats
     */
    std::vector<std::string> output_formats() const override;

    /**
     * @brief Get supported providers
     * @return {"anthropic", "claude"}
     */
    std::vector<std::string> supported_providers() const override;

    /**
     * @brief Get plugin capabilities
     * @return List of specialized capabilities
     */
    std::vector<std::string> capabilities() const override;

    // Core processing methods

    /**
     * @brief Preprocess request for Claude compatibility
     *
     * Optimizes requests before sending to Claude by:
     * - Formatting tool definitions for Claude's XML-based tool use
     * - Adding Claude-specific system message optimizations
     * - Configuring thinking and reasoning parameters
     * - Setting up multimodal content handling
     *
     * @param request Original AIMUX request
     * @return Processing result with Claude-optimized request
     */
    ProcessingResult preprocess_request(const core::Request& request) override;

    /**
     * @brief Postprocess Claude response with XML tool use support
     *
     * Processes Claude responses with full format support:
     * - XML tool use tag parsing and validation
     * - Thinking and reasoning block extraction
     * - Content cleanup preserving Claude's analytical style
     * - Tool call extraction from XML format
     * - Multimodal content handling
     *
     * @param response Response from Claude API
     * @param context Processing context information
     * @return Processing result with formatted content
     */
    ProcessingResult postprocess_response(
        const core::Response& response,
        const ProcessingContext& context) override;

    // Streaming support (Claude-compatible)

    /**
     * @brief Begin streaming processing for Claude
     *
     * Initializes streaming state for Claude's streaming format,
     * which includes partial XML tag reconstruction and thinking blocks.
     *
     * @param context Processing context for streaming session
     * @return true if initialization successful
     */
    bool begin_streaming(const ProcessingContext& context) override;

    /**
     * @brief Process Claude streaming chunk
     *
     * Handles Claude streaming chunks with XML reconstruction:
     * - Partial XML tag accumulation
     * - Incremental thinking block assembly
     * - Tool call streaming reconstruction
     * - Content preservation during streaming
     *
     * @param chunk Streaming response chunk
     * @param is_final True if this is the last chunk
     * @param context Processing context
     * @return Processing result for the chunk
     */
    ProcessingResult process_streaming_chunk(
        const std::string& chunk,
        bool is_final,
        const ProcessingContext& context) override;

    /**
     * @brief End streaming processing
     *
     * Finalizes Claude streaming response with complete
     * XML reconstruction and final reasoning block processing.
     *
     * @param context Processing context
     * @return Final processing result
     */
    ProcessingResult end_streaming(const ProcessingContext& context) override;

    // Configuration and customization

    /**
     * @brief Configure formatter with Claude-specific settings
     *
     * Supported configuration options:
     * - "preserve_thinking": boolean - Preserve thinking blocks in output (default: true)
     * - "extract_reasoning": boolean - Extract and separate reasoning content (default: true)
     * - "validate_xml_structure": boolean - Validate XML tool use structure (default: true)
     * - "support_multimodal": boolean - Handle Claude's multimodal outputs (default: true)
     * - "preserve_code_blocks": boolean - Preserve code and formatting (default: true)
     * - "max_thinking_length": number - Maximum thinking block length (default: 10000)
     * - "clean_xml_artifacts": boolean - Remove XML processing artifacts (default: true)
     *
     * @param config Configuration JSON object
     * @return true if configuration was applied successfully
     */
    bool configure(const nlohmann::json& config) override;

    /**
     * @brief Validate current configuration
     * @return true if configuration is valid and ready for use
     */
    bool validate_configuration() const override;

    /**
     * @brief Get current configuration
     * @return Current configuration as JSON
     */
    nlohmann::json get_configuration() const override;

    // Metrics and monitoring

    /**
     * @brief Get performance metrics focused on Claude processing
     *
     * Returns metrics emphasizing Claude-specific processing:
     * - Average processing time by content type
     * - XML tool call extraction metrics
     * - Thinking block processing statistics
     * - Reasoning extraction success rate
     * - Content preservation metrics
     *
     * @return JSON object with Claude-focused metrics
     */
    nlohmann::json get_metrics() const override;

    /**
     * @brief Reset all metrics
     */
    void reset_metrics() override;

    // Health and diagnostics

    /**
     * @brief Perform Claude-specific health check
     *
     * Tests formatter functionality with Claude-specific test cases:
     * - XML tool use parsing validation
     * - Thinking block extraction test
     * - Reasoning content separation test
     * - XML structure validation test
     *
     * @return JSON health check result with processing metrics
     */
    nlohmann::json health_check() const override;

    /**
     * @brief Get diagnostic information
     *
     * Returns comprehensive diagnostic information including:
     * - Supported Claude models and their capabilities
     * - XML processing performance analysis
     * - Thinking block handling statistics
     * - Content preservation effectiveness
     *
     * @return JSON diagnostic information
     */
    nlohmann::json get_diagnostics() const override;

private:
    /**
     * @brief Get default model from global config
     */
    static std::string get_default_model();

    // Model configuration
    std::string model_name_;

    // Configuration settings
    bool preserve_thinking_ = true;
    bool extract_reasoning_ = true;
    bool validate_xml_structure_ = true;
    bool support_multimodal_ = true;
    bool preserve_code_blocks_ = true;
    int max_thinking_length_ = 10000;
    bool clean_xml_artifacts_ = true;

    // Performance metrics (thread-safe)
    mutable std::atomic<uint64_t> total_processing_count_{0};
    mutable std::atomic<uint64_t> total_processing_time_us_{0};
    mutable std::atomic<uint64_t> xml_tool_calls_extracted_{0};
    mutable std::atomic<uint64_t> thinking_blocks_processed_{0};
    mutable std::atomic<uint64_t> reasoning_content_extracted_{0};
    mutable std::atomic<uint64_t> xml_validation_errors_{0};
    mutable std::atomic<uint64_t> multimodal_responses_processed_{0};

    // Streaming state
    std::string streaming_content_;
    std::string xml_buffer_;
    std::string thinking_buffer_;
    bool in_xml_block_ = false;
    bool in_thinking_block_ = false;
    bool streaming_active_ = false;
    std::chrono::steady_clock::time_point streaming_start_;

    // Claude-specific patterns
    struct ClaudePatterns {
        // XML tool use patterns
        std::regex function_calls_start;
        std::regex function_calls_end;
        std::regex function_call_pattern;
        std::regex thinking_start;
        std::regex thinking_end;
        std::regex reflection_pattern;

        // Content patterns
        std::regex code_block_pattern;
        std::regex xml_artifact_pattern;

        ClaudePatterns();
    };

    std::unique_ptr<ClaudePatterns> patterns_;

    // Private helper methods

    /**
     * @brief Extract XML tool calls from Claude response
     *
     * Parses Claude's XML-based tool use format:
     * - <function_calls>...</function_calls> blocks
     * - Individual function call elements
     * - Parameter extraction and validation
     * - XML structure validation when enabled
     *
     * @param content Content containing XML tool calls
     * @return Vector of extracted tool calls
     */
    std::vector<ToolCall> extract_claude_xml_tool_calls(const std::string& content) const;

    /**
     * @brief Extract JSON tool_use blocks from Claude response (v3.5+ format)
     *
     * Parses Claude's modern JSON-based tool use format:
     * - Content array parsing
     * - tool_use block extraction from content objects
     * - Parameter extraction from tool_use.input JSON
     * - ID mapping and status handling
     *
     * @param content Content containing tool_use blocks in JSON format
     * @return Vector of extracted tool calls
     */
    std::vector<ToolCall> extract_claude_json_tool_uses(const std::string& content) const;

    /**
     * @brief Extract and process thinking blocks
     *
     * Extracts Claude's thinking/reasoning blocks:
     * - <thinking>...</thinking> blocks
     * - <reflection>...</reflection> blocks
     * - Reasoning trace preservation
     * - Content separation and organization
     *
     * @param content Content containing thinking blocks
     * @return Pair of (cleaned content, extracted reasoning)
     */
    std::pair<std::string, std::string> extract_thinking_blocks(const std::string& content) const;

    /**
     * @brief Validate Claude XML structure
     *
     * Validates XML tool use structure when validation is enabled:
     * - Well-formed XML checking
     * - Proper tag nesting verification
     * - Required attribute presence
     * - Schema validation for tool calls
     *
     * @param xml_content XML content to validate
     * @return True if XML structure is valid
     */
    bool validate_claude_xml(const std::string& xml_content) const;

    /**
     * @brief Clean Claude content while preserving structure
     *
     * Performs Claude-specific content cleaning:
     * - Removes XML processing artifacts
     * - Preserves code and formatting blocks
     * - Normalizes whitespace while maintaining structure
     * - Handles Claude's analytical content style
     *
     * @param content Raw content from Claude
     * @return Cleaned and structured content
     */
    std::string clean_claude_content(const std::string& content) const;

    /**
     * @brief Generate Claude-optimized TOON format
     *
     * Creates TOON format with Claude-specific metadata:
     * - Thinking block tracking information
     * - Reasoning content separation
     * - XML tool call metadata
     * - Claude model capability information
     * - Content preservation metrics
     *
     * @param content Processed content
     * @param tool_calls Extracted tool calls
     * @param reasoning Extracted reasoning content
     * @param context Processing context
     * @return TOON format JSON string
     */
    std::string generate_claude_toon(
        const std::string& content,
        const std::vector<ToolCall>& tool_calls,
        const std::string& reasoning,
        const ProcessingContext& context) const;

    /**
     * @brief Process streaming XML reconstruction
     *
     * Handles streaming XML tag reconstruction:
     * - Tracks XML tag boundaries
     * - Accumulates partial XML content
     * - Validates XML structure during streaming
     * - Handles incomplete tags gracefully
     *
     * @param chunk Streaming chunk content
     * @return Updated XML buffer state
     */
    std::string process_streaming_xml(const std::string& chunk);

    /**
     * @brief Update Claude-specific metrics
     *
     * Updates metrics with Claude-specific categories:
     * - XML processing metrics
     * - Thinking block processing metrics
     * - Reasoning extraction metrics
     * - Content preservation metrics
     *
     * @param processing_time_us Processing time in microseconds
     * @param xml_tool_calls_count Number of XML tool calls
     * @param thinking_blocks_count Number of thinking blocks
     * @param reasoning_extracted Whether reasoning was extracted
     */
    void update_claude_metrics(
        uint64_t processing_time_us,
        uint64_t xml_tool_calls_count,
        uint64_t thinking_blocks_count,
        bool reasoning_extracted) const;

    /**
     * @brief Detect Claude response content type
     *
     * Analyzes content to determine Claude response characteristics:
     * - Tool use presence and type
     * - Thinking/reasoning blocks
     * - Multimodal content indicators
     * - Code and structured content
     *
     * @param content Content to analyze
     * @return Detected content characteristics
     */
    std::vector<std::string> detect_content_types(const std::string& content) const;

    /**
     * @brief Handle multimodal content processing
     *
     * Processes Claude's multimodal outputs:
     * - Image analysis results
     * - Document processing outputs
     * - Mixed content type handling
     * - Media content preservation
     *
     * @param content Content to process
     * @return Processed multimodal content
     */
    std::string process_multimodal_content(const std::string& content) const;

    /**
     * @brief Extract reasoning traces from content
     *
     * Extracts detailed reasoning traces from Claude responses:
     * - Step-by-step analysis
     * - Problem-solving approaches
     * - Decision-making processes
     * - Analytical chains of thought
     *
     * @param content Content containing reasoning
     * @return Extracted reasoning traces
     */
    std::string extract_reasoning_traces(const std::string& content) const;

    /**
     * @brief Validate Claude tool call format
     *
     * Validates individual Claude tool calls:
     * - Required field presence
     * - Parameter format validation
     * - Name validation
     * - Structure compliance
     *
     * @param tool_call Tool call to validate
     * @return True if tool call is valid
     */
    bool validate_claude_tool_call(const ToolCall& tool_call) const;
};

} // namespace prettifier
} // namespace aimux