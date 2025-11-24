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
 * @brief OpenAI GPT response formatter with comprehensive function calling support
 *
 * This formatter specializes in handling responses from OpenAI GPT models, with full
 * support for function calling, structured outputs, and legacy OpenAI response formats.
 * It provides robust processing for both modern GPT-4 responses and legacy GPT-3.5 formats.
 *
 * Key features:
 * - Complete OpenAI function calling format support
 * - Compatibility layer for legacy OpenAI response structures
 * - JSON tool response and structured output handling
 * - General-purpose OpenAI pattern optimization
 * - Comprehensive error handling for API variations
 * - Support for streaming and non-streaming responses
 *
 * Supported formats:
 * - Standard OpenAI ChatCompletion responses
 * - Function calling with multiple tools
 * - Structured outputs (JSON mode)
 * - Legacy GPT-3.5 completion formats
 * - Code generation responses
 * - Multi-modal GPT-4 Vision responses
 *
 * Performance targets:
 * - <40ms response processing time
 * - <15ms function call extraction
 * - <20ms JSON structured output validation
 * - Comprehensive format compatibility
 *
 * Usage example:
 * @code
 * auto formatter = std::make_shared<OpenAIFormatter>();
 * formatter->configure({
 *     {"support_legacy_formats", true},
 *     {"strict_function_validation", true},
 *     {"enable_structured_outputs", true}
 * });
 *
 * ProcessingContext context;
 * context.provider_name = "openai";
 * context.model_name = "gpt-4";
 *
 * ProcessingResult result = formatter->postprocess_response(response, context);
 * @endcode
 */
class OpenAIFormatter : public PrettifierPlugin {
public:
    /**
     * @brief Constructor
     *
     * Initializes the OpenAI formatter with comprehensive support for all OpenAI
     * response formats. Sets up pattern recognition for function calls, structured outputs,
     * and legacy compatibility modes.
     */
    OpenAIFormatter();

    /**
     * @brief Destructor
     *
     * Cleans up resources and ensures thread-safe shutdown of any background processing.
     */
    ~OpenAIFormatter() override = default;

    // Plugin identification and metadata

    /**
     * @brief Get the unique identifier for this plugin
     * @return "openai-gpt-formatter-v1.0.0"
     */
    std::string get_name() const override;

    /**
     * @brief Get the plugin version
     * @return "1.0.0"
     */
    std::string version() const override;

    /**
     * @brief Get human-readable description
     * @return Detailed description of OpenAI formatter capabilities
     */
    std::string description() const override;

    /**
     * @brief Get supported input formats
     * @return Vector of formats OpenAI typically outputs
     */
    std::vector<std::string> supported_formats() const override;

    /**
     * @brief Get output formats this plugin generates
     * @return Vector of standardized output formats
     */
    std::vector<std::string> output_formats() const override;

    /**
     * @brief Get supported providers
     * @return {"openai", "openai-compatibility"}
     */
    std::vector<std::string> supported_providers() const override;

    /**
     * @brief Get plugin capabilities
     * @return List of specialized capabilities
     */
    std::vector<std::string> capabilities() const override;

    // Core processing methods

    /**
     * @brief Preprocess request for OpenAI compatibility
     *
     * Optimizes requests before sending to OpenAI by:
     * - Formatting tool definitions for OpenAI function calling
     * - Setting appropriate parameters for different GPT models
     * - Adding OpenAI-specific metadata and validation
     * - Configuring structured output modes when requested
     *
     * @param request Original AIMUX request
     * @return Processing result with OpenAI-optimized request
     */
    ProcessingResult preprocess_request(const core::Request& request) override;

    /**
     * @brief Postprocess OpenAI response with comprehensive format support
     *
     * Processes OpenAI responses with full compatibility:
     * - Standard ChatCompletion format handling
     * - Function calling extraction and validation
     * - Structured output JSON validation
     * - Legacy format compatibility processing
     * - Content cleanup and normalization
     *
     * @param response Response from OpenAI API
     * @param context Processing context information
     * @return Processing result with formatted content
     */
    ProcessingResult postprocess_response(
        const core::Response& response,
        const ProcessingContext& context) override;

    // Streaming support (OpenAI-compatible)

    /**
     * @brief Begin streaming processing for OpenAI
     *
     * Initializes streaming state for OpenAI's streaming format,
     * which includes delta updates and function call streaming.
     *
     * @param context Processing context for streaming session
     * @return true if initialization successful
     */
    bool begin_streaming(const ProcessingContext& context) override;

    /**
     * @brief Process OpenAI streaming chunk
     *
     * Handles OpenAI streaming chunks with delta processing:
     * - Partial content accumulation
     * - Streaming function call assembly
     * - Delta-based tool call updates
     * - Chunk validation and error recovery
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
     * Finalizes OpenAI streaming response with complete
     * function call reconstruction and final formatting.
     *
     * @param context Processing context
     * @return Final processing result
     */
    ProcessingResult end_streaming(const ProcessingContext& context) override;

    // Configuration and customization

    /**
     * @brief Configure formatter with OpenAI-specific settings
     *
     * Supported configuration options:
     * - "support_legacy_formats": boolean - Support GPT-3.5 legacy formats (default: true)
     * - "strict_function_validation": boolean - Strict validation of function calls (default: true)
     * - "enable_structured_outputs": boolean - Enable JSON structured output support (default: true)
     * - "validate_tool_schemas": boolean - Validate tool schemas against response (default: false)
     * - "preserve_thinking": boolean - Preserve reasoning traces from GPT-4 (default: false)
     * - "max_function_calls": number - Maximum function calls per response (default: 10)
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
     * @brief Get performance metrics focused on OpenAI compatibility
     *
     * Returns metrics emphasizing format compatibility and processing:
     * - Average processing time by format type
     * - Function call success rate
     * - Structured output validation metrics
     * - Legacy format compatibility rate
     * - Error distribution by format type
     *
     * @return JSON object with OpenAI-focused metrics
     */
    nlohmann::json get_metrics() const override;

    /**
     * @brief Reset all metrics
     */
    void reset_metrics() override;

    // Health and diagnostics

    /**
     * @brief Perform OpenAI-specific health check
     *
     * Tests formatter functionality with OpenAI-specific test cases:
     * - Function calling format validation
     * - Structured output JSON validation
     * - Legacy compatibility verification
     * - Streaming delta processing test
     *
     * @return JSON health check result with compatibility metrics
     */
    nlohmann::json health_check() const override;

    /**
     * @brief Get diagnostic information
     *
     * Returns comprehensive diagnostic information including:
     * - Supported OpenAI models and their capabilities
     * - Format compatibility matrix
     * - Known limitations and workarounds
     * - Performance optimization recommendations
     *
     * @return JSON diagnostic information
     */
    nlohmann::json get_diagnostics() const override;

private:
    // Configuration settings
    bool support_legacy_formats_ = true;
    bool strict_function_validation_ = true;
    bool enable_structured_outputs_ = true;
    bool validate_tool_schemas_ = false;
    bool preserve_thinking_ = false;
    int max_function_calls_ = 10;

    // Performance metrics (thread-safe)
    mutable std::atomic<uint64_t> total_processing_count_{0};
    mutable std::atomic<uint64_t> total_processing_time_us_{0};
    mutable std::atomic<uint64_t> function_calls_processed_{0};
    mutable std::atomic<uint64_t> structured_outputs_validated_{0};
    mutable std::atomic<uint64_t> legacy_formats_processed_{0};
    mutable std::atomic<uint64_t> validation_errors_{0};
    mutable std::atomic<uint64_t> streaming_chunks_processed_{0};

    // Streaming state
    std::string streaming_content_;
    nlohmann::json streaming_function_call_;
    bool streaming_active_ = false;
    std::chrono::steady_clock::time_point streaming_start_;

    // OpenAI-specific patterns
    struct OpenAIPatterns {
        // Function call patterns
        std::regex function_call_pattern;
        std::regex tool_calls_pattern;
        std::regex legacy_function_pattern;

        // Structured output patterns
        std::regex json_schema_pattern;
        std::regex structured_output_pattern;

        // Streaming patterns
        std::regex streaming_delta_pattern;
        std::regex streaming_function_delta;

        OpenAIPatterns();
    };

    std::unique_ptr<OpenAIPatterns> patterns_;

    // Private helper methods

    /**
     * @brief Extract and validate OpenAI function calls
     *
     * Processes OpenAI function calling format with comprehensive validation:
     * - Standard tool_calls array format
     * - Legacy function_call format
     * - Streaming function call reconstruction
     * - Schema validation when enabled
     *
     * @param content Content to process
     * @return Vector of validated tool calls
     */
    std::vector<ToolCall> extract_openai_function_calls(const std::string& content) const;

    /**
     * @brief Validate structured JSON output
     *
     * Validates and cleans structured JSON outputs from OpenAI:
     * - JSON schema validation
     * - Format normalization
     * - Error recovery and repair
     *
     * @param content JSON content to validate
     * @param schema Optional JSON schema for validation
     * @return Validated and cleaned JSON string
     */
    std::string validate_structured_output(
        const std::string& content,
        const std::optional<nlohmann::json>& schema = std::nullopt) const;

    /**
     * @brief Process legacy OpenAI formats
     *
     * Handles legacy OpenAI response formats for backward compatibility:
     * - GPT-3.5 completion formats
     * - Legacy function calling
     * - Older structured output formats
     *
     * @param content Legacy format content
     * @return Normalized modern format content
     */
    std::string process_legacy_format(const std::string& content) const;

    /**
     * @brief Generate OpenAI-compatible TOON format
     *
     * Creates TOON format with OpenAI-specific metadata:
     * - Model capability information
     * - Function call tracking metadata
     * - Structured output validation status
     * - OpenAI API version compatibility
     *
     * @param content Processed content
     * @param tool_calls Extracted tool calls
     * @param context Processing context
     * @return TOON format JSON string
     */
    std::string generate_openai_toon(
        const std::string& content,
        const std::vector<ToolCall>& tool_calls,
        const ProcessingContext& context) const;

    /**
     * @brief Validate OpenAI tool call against schema
     *
     * Validates tool call parameters against the tool schema when available.
     * Used when strict validation is enabled.
     *
     * @param tool_call Tool call to validate
     * @param schema Tool schema for validation
     * @return True if tool call is valid
     */
    bool validate_tool_call_schema(
        const ToolCall& tool_call,
        const nlohmann::json& schema) const;

    /**
     * @brief Process streaming function delta
     *
     * Processes streaming function call deltas for reconstruction:
     * - Accumulates function name deltas
     * - Builds arguments string progressively
     * - Handles function call completion
     *
     * @param delta Streaming delta content
     * @return Updated function call state
     */
    nlohmann::json process_function_delta(const nlohmann::json& delta);

    /**
     * @brief Update OpenAI-specific metrics
     *
     * Updates metrics with OpenAI-specific categories:
     * - Format type tracking
     * - Function call processing metrics
     * - Structured output validation metrics
     * - Compatibility metrics
     *
     * @param processing_time_us Processing time in microseconds
     * @param format_type Type of format processed
     * @param function_calls_count Number of function calls
     * @param validation_passed Whether validation passed
     */
    void update_openai_metrics(
        uint64_t processing_time_us,
        const std::string& format_type,
        uint64_t function_calls_count,
        bool validation_passed) const;

    /**
     * @brief Detect OpenAI response format type
     *
     * Analyzes content to determine which OpenAI format is being used:
     * - Standard chat completion
     * - Function calling
     * - Structured output
     * - Legacy format
     *
     * @param content Content to analyze
     * @return Detected format type
     */
    std::string detect_format_type(const std::string& content) const;

    /**
     * @brief Clean and normalize OpenAI content
     *
     * Performs OpenAI-specific content cleaning:
     * - Removes API artifacts
     * - Normalizes whitespace and formatting
     * - Preserves important code blocks and structure
     *
     * @param content Raw content from OpenAI
     * @return Cleaned and normalized content
     */
    std::string clean_openai_content(const std::string& content) const;

    /**
     * @brief Check for malicious patterns in content
     *
     * Security validation to detect and block:
     * - XSS (Cross-Site Scripting) patterns
     * - SQL injection attempts
     * - Path traversal attempts
     * - Code execution patterns
     *
     * @param content Content to check
     * @return True if malicious patterns found, false otherwise
     */
    bool contains_malicious_patterns(const std::string& content) const;
};

} // namespace prettifier
} // namespace aimux