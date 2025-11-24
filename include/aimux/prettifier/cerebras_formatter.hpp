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
 * @brief Cerebras-specific response formatter optimized for speed and tool responses
 *
 * This formatter specializes in handling responses from Cerebras AI, which is known
 * for its fast response times and efficient tool calling capabilities. The formatter
 * is optimized to minimize overhead while preserving the high-speed characteristics
 * that make Cerebras ideal for real-time applications.
 *
 * Key optimizations:
 * - Minimal processing overhead to preserve Cerebras speed advantage
 * - Fast tool call extraction with optimized patterns for Cerebras output
 * - Lightweight TOON serialization for rapid response processing
 * - Provider-specific metrics focused on speed and throughput
 * - Health checks optimized for Cerebras API patterns
 *
 * Performance targets:
 * - <30ms response processing time (faster than general formatters)
 * - <10ms tool call extraction for complex tool chains
 * - <5ms TOON format generation
 * - Sub-millisecond health check responses
 *
 * Usage example:
 * @code
 * auto formatter = std::make_shared<CerebrasFormatter>();
 * formatter->configure({
 *     {"optimize_speed", true},
 *     {"enable_detailed_metrics", false},
 *     {"cache_tool_patterns", true}
 * });
 *
 * ProcessingContext context;
 * context.provider_name = "cerebras";
 * context.model_name = "llama3.1-70b";
 * context.streaming_mode = false;
 *
 * ProcessingResult result = formatter->postprocess_response(response, context);
 * @endcode
 */
class CerebrasFormatter : public PrettifierPlugin {
public:
    /**
     * @brief Constructor
     *
     * Initializes the Cerebras formatter with default settings optimized for speed.
     * Sets up internal metrics collection and compiled regex patterns for efficient processing.
     */
    CerebrasFormatter(const std::string& model_name = "");

    /**
     * @brief Destructor
     *
     * Cleans up resources and ensures thread-safe shutdown of any background processing.
     */
    ~CerebrasFormatter() override = default;

    // Plugin identification and metadata

    /**
     * @brief Get the unique identifier for this plugin
     * @return "cerebras-speed-formatter-v1.0.0"
     */
    std::string get_name() const override;

    /**
     * @brief Get the plugin version
     * @return "1.0.0"
     */
    std::string version() const override;

    /**
     * @brief Get human-readable description
     * @return Detailed description of Cerebras formatter capabilities
     */
    std::string description() const override;

    /**
     * @brief Get supported input formats
     * @return Vector of formats Cerebras typically outputs
     */
    std::vector<std::string> supported_formats() const override;

    /**
     * @brief Get output formats this plugin generates
     * @return Vector of standardized output formats
     */
    std::vector<std::string> output_formats() const override;

    /**
     * @brief Get supported providers
     * @return {"cerebras", "cerebras-ai"}
     */
    std::vector<std::string> supported_providers() const override;

    /**
     * @brief Get plugin capabilities
     * @return List of specialized capabilities
     */
    std::vector<std::string> capabilities() const override;

    // Core processing methods

    /**
     * @brief Preprocess request for Cerebras optimization
     *
     * Optimizes requests before sending to Cerebras by:
     * - Adding Cerebras-specific parameters for speed optimization
     * - Formatting tool calls for maximum efficiency
     * - Setting appropriate temperature and max_tokens for speed
     * - Adding metadata for response processing
     *
     * @param request Original AIMUX request
     * @return Processing result with optimized request
     */
    ProcessingResult preprocess_request(const core::Request& request) override;

    /**
     * @brief Postprocess Cerebras response with speed optimization
     *
     * Processes Cerebras responses with minimal overhead:
     * - Fast content normalization and cleanup
     * - Optimized tool call extraction using Cerebras patterns
     * - Efficient TOON format serialization
     * - Speed-focused metrics collection
     *
     * @param response Response from Cerebras API
     * @param context Processing context information
     * @return Processing result with formatted content
     */
    ProcessingResult postprocess_response(
        const core::Response& response,
        const ProcessingContext& context) override;

    // Streaming support (optimized for Cerebras streaming)

    /**
     * @brief Begin streaming processing for Cerebras
     *
     * Initializes streaming state optimized for Cerebras's fast streaming responses.
     * Pre-compiles patterns and sets up efficient buffering.
     *
     * @param context Processing context for streaming session
     * @return true if initialization successful
     */
    bool begin_streaming(const ProcessingContext& context) override;

    /**
     * @brief Process Cerebras streaming chunk
     *
     * Handles streaming chunks with Cerebras-specific optimizations:
     * - Fast chunk validation and parsing
     * - Incremental tool call assembly
     * - Efficient buffer management
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
     * Finalizes streaming response with cleanup and final formatting.
     *
     * @param context Processing context
     * @return Final processing result
     */
    ProcessingResult end_streaming(const ProcessingContext& context) override;

    // Configuration and customization

    /**
     * @brief Configure formatter with Cerebras-specific settings
     *
     * Supported configuration options:
     * - "optimize_speed": boolean - Enable maximum speed optimizations (default: true)
     * - "enable_detailed_metrics": boolean - Collect detailed performance metrics (default: false)
     * - "cache_tool_patterns": boolean - Cache regex patterns for tool extraction (default: true)
     * - "max_processing_time_ms": number - Maximum processing time before fallback (default: 50)
     * - "enable_fast_failover": boolean - Enable fast fallback to simpler processing (default: true)
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

    // Metrics and monitoring (speed-focused)

    /**
     * @brief Get performance metrics focused on speed
     *
     * Returns metrics emphasizing speed and throughput:
     * - Average processing time (microsecond precision)
     * - Tool call extraction speed
     * - TOON serialization performance
     * - Cache hit rates for patterns
     * - Speed optimization effectiveness
     *
     * @return JSON object with speed-focused metrics
     */
    nlohmann::json get_metrics() const override;

    /**
     * @brief Reset all metrics
     */
    void reset_metrics() override;

    // Health and diagnostics

    /**
     * @brief Perform Cerebras-specific health check
     *
     * Tests formatter functionality with speed benchmarks:
     * - Processing speed validation
     * - Tool call pattern accuracy
     * - Memory efficiency checks
     * - Pattern cache performance
     *
     * @return JSON health check result with speed metrics
     */
    nlohmann::json health_check() const override;

    /**
     * @brief Get diagnostic information
     *
     * Returns comprehensive diagnostic information including:
     * - Configuration status
     * - Pattern cache details
     * - Performance bottlenecks
     * - Optimization recommendations
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
    bool optimize_speed_ = true;
    bool enable_detailed_metrics_ = false;
    bool cache_tool_patterns_ = true;
    int max_processing_time_ms_ = 50;
    bool enable_fast_failover_ = true;

    // Performance metrics (thread-safe)
    mutable std::atomic<uint64_t> total_processing_count_{0};
    mutable std::atomic<uint64_t> total_processing_time_us_{0};
    mutable std::atomic<uint64_t> tool_calls_extracted_{0};
    mutable std::atomic<uint64_t> cache_hits_{0};
    mutable std::atomic<uint64_t> cache_misses_{0};
    mutable std::atomic<uint64_t> fast_failovers_triggered_{0};

    // Streaming state
    std::string streaming_buffer_;
    bool streaming_active_ = false;
    std::chrono::steady_clock::time_point streaming_start_;

    // Pre-compiled regex patterns for speed
    struct CerebrasPatterns {
        // Cerebras-specific tool call patterns (optimized for speed)
        std::regex fast_tool_pattern;
        std::regex cerebras_json_pattern;
        std::regex streaming_tool_pattern;

        CerebrasPatterns();
    };

    std::unique_ptr<CerebrasPatterns> patterns_;

    // Private helper methods

    /**
     * @brief Fast content normalization for Cerebras
     *
     * Performs lightweight cleanup optimized for speed:
     * - Basic whitespace normalization
     * - Removal of Cerebras-specific artifacts
     * - Preserve tool call formatting for fast extraction
     *
     * @param content Raw content from Cerebras
     * @return Normalized content
     */
    std::string fast_normalize_content(const std::string& content) const;

    /**
     * @brief Optimized tool call extraction for Cerebras
     *
     * Extracts tool calls using patterns specifically optimized for Cerebras output:
     * - Fast pattern matching with pre-compiled regex
     * - Efficient JSON parsing and validation
     * - Minimal memory allocations
     *
     * @param content Content to extract tool calls from
     * @return Vector of extracted tool calls
     */
    std::vector<ToolCall> extract_cerebras_tool_calls(const std::string& content) const;

    /**
     * @brief Fast TOON format generation
     *
     * Generates TOON format with minimal overhead:
     * - Lightweight JSON construction
     * - Optimized string operations
     * - Efficient metadata injection
     *
     * @param content Processed content
     * @param tool_calls Extracted tool calls
     * @param context Processing context
     * @return TOON format JSON string
     */
    std::string generate_fast_toon(
        const std::string& content,
        const std::vector<ToolCall>& tool_calls,
        const ProcessingContext& context) const;

    /**
     * @brief Update performance metrics (thread-safe)
     *
     * Updates internal metrics with thread-safe atomic operations.
     *
     * @param processing_time_us Processing time in microseconds
     * @param tool_calls_count Number of tool calls extracted
     * @param cache_hit True if pattern cache was used
     */
    void update_metrics(
        uint64_t processing_time_us,
        uint64_t tool_calls_count,
        bool cache_hit) const;

    /**
     * @brief Check if processing should use fast failover
     *
     * Determines if processing time exceeds thresholds and should trigger
     * fast failover to simpler processing methods.
     *
     * @param elapsed_time_ms Elapsed processing time in milliseconds
     * @return True if fast failover should be triggered
     */
    bool should_trigger_fast_failover(int elapsed_time_ms) const;

    /**
     * @brief Process response with maximum speed optimization
     *
     * Fallback processing method when fast failover is triggered.
     * Uses minimal processing to preserve speed.
     *
     * @param response Original response
     * @param context Processing context
     * @return Basic processing result
     */
    ProcessingResult fast_failover_process(
        const core::Response& response,
        const ProcessingContext& context) const;
};

} // namespace prettifier
} // namespace aimux