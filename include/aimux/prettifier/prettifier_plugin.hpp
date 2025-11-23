#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <chrono>
#include <nlohmann/json.hpp>
#include "aimux/core/router.hpp" // For Request/Response structures

namespace aimux {
namespace prettifier {

// Forward declarations
struct ProcessingContext;
struct ProcessingResult;

/**
 * @brief Tool call structure for AI agent interactions
 */
struct ToolCall {
    std::string name;
    std::string id;
    nlohmann::json parameters;
    std::optional<nlohmann::json> result;
    std::string status; // "pending", "executing", "completed", "failed"
    std::chrono::system_clock::time_point timestamp;

    nlohmann::json to_json() const;
    static ToolCall from_json(const nlohmann::json& j);
};

/**
 * @brief Context information for prettification processing
 */
struct ProcessingContext {
    std::string provider_name;
    std::string model_name;
    std::string original_format;
    std::vector<std::string> requested_formats;
    bool streaming_mode = false;
    std::optional<nlohmann::json> provider_config;
    std::chrono::system_clock::time_point processing_start;

    nlohmann::json to_json() const;
};

/**
 * @brief Result of prettification processing
 */
struct ProcessingResult {
    bool success = false;
    std::string processed_content;
    std::string output_format;
    std::vector<ToolCall> extracted_tool_calls;
    std::optional<std::string> reasoning;
    std::chrono::milliseconds processing_time{0};
    size_t tokens_processed = 0;
    std::string error_message;
    nlohmann::json metadata;
    bool streaming_mode = false;

    nlohmann::json to_json() const;
};

/**
 * @brief Abstract base class for all prettifier plugins
 *
 * This interface defines the contract that all prettifier plugins must implement.
 * It provides a standardized way to process AI responses from various providers
 * and convert them to consistent formats suitable for the AIMUX system.
 *
 * The interface follows the Strategy pattern, allowing different formatting
 * strategies to be plugged into the system at runtime. Each plugin specializes
 * in handling specific types of content or provider-specific response formats.
 *
 * Key design principles:
 * - Pure virtual interface prevents direct instantiation
 * - RAII memory management with smart pointers
 * - Thread-safe operations for concurrent processing
 * - Comprehensive error reporting and validation
 * - Performance monitoring and metrics collection
 * - Extensible metadata and configuration support
 *
 * Implementation requirements (from qa/phase1_foundation_qa_plan.md):
 * - All virtual methods must be properly overridden in derived classes
 * - RAII principles with smart pointers for memory management
 * - Thread safety for concurrent plugin access
 * - No memory leaks in plugin creation/destruction
 *
 * Usage example:
 * @code
 * class MarkdownPlugin : public PrettifierPlugin {
 * public:
 *     std::string version() const override { return "1.0.0"; }
 *     ProcessingResult preprocess_request(const core::Request& request) override;
 *     ProcessingResult postprocess_response(const core::Response& response, const ProcessingContext& context) override;
 * };
 *
 * auto plugin = std::make_shared<MarkdownPlugin>();
 * auto result = plugin->postprocess_response(response, context);
 * @endcode
 */
class PrettifierPlugin {
public:
    virtual ~PrettifierPlugin() = default;

    // Core processing methods (must be implemented by all plugins)

    /**
     * @brief Preprocess a request before sending to provider
     *
     * Allows plugins to modify or enhance requests before they are sent
     * to the AI provider. This can include format conversion, parameter
     * validation, metadata injection, or request optimization.
     *
     * @param requestOriginal AIMUX request to be processed
     * @return Processing result with potentially modified request
     *
     * Thread safety: This method must be thread-safe for concurrent calls
     * Error handling: Should return ProcessingResult with error details on failure
     */
    virtual ProcessingResult preprocess_request(const core::Request& request) = 0;

    /**
     * @brief Postprocess a response from the provider
     *
     * Main prettification method that converts provider-specific response
     * formats into standardized AIMUX formats. Handles content normalization,
     * tool call extraction, format conversion, and quality improvements.
     *
     * @param response Response received from AI provider
     * @param context Processing context with provider and format information
     * @return Processing result with prettified content and metadata
     *
     * Thread safety: This method must be thread-safe for concurrent calls
     * Performance: Should complete processing within the plugin's performance target
     */
    virtual ProcessingResult postprocess_response(
        const core::Response& response,
        const ProcessingContext& context) = 0;

    // Plugin metadata and capabilities (must be implemented by all plugins)

    /**
     * @brief Get the unique identifier for this plugin
     *
     * @return Unique plugin name in format [provider]-[format]-[version]
     */
    virtual std::string get_name() const = 0;

    /**
     * @brief Get the semantic version of this plugin
     *
     * @return Version string following semantic versioning (e.g., "1.2.3")
     */
    virtual std::string version() const = 0;

    /**
     * @brief Get human-readable description of plugin functionality
     *
     * @return Detailed description of what this plugin does and when to use it
     */
    virtual std::string description() const = 0;

    /**
     * @brief Get list of supported input formats
     *
     * @return Vector of format identifiers this plugin can process as input
     */
    virtual std::vector<std::string> supported_formats() const = 0;

    /**
     * @brief Get list of output formats this plugin can generate
     *
     * @return Vector of format identifiers this plugin can produce as output
     */
    virtual std::vector<std::string> output_formats() const = 0;

    /**
     * @brief Get list of providers this plugin is compatible with
     *
     * @return Vector of provider names this plugin supports
     */
    virtual std::vector<std::string> supported_providers() const = 0;

    /**
     * @brief Get list of capabilities this plugin provides
     *
     * @return Vector of capability descriptors (e.g., "tool-calls", "formatting", "validation")
     */
    virtual std::vector<std::string> capabilities() const = 0;

    // Optional streaming support (default implementations provided)

    /**
     * @brief Begin streaming response processing
     *
     * Called when streaming response processing begins. Allows plugins
     * to initialize state for multi-chunk processing.
     *
     * @param context Processing context for the streaming session
     * @return true if initialization successful
     */
    virtual bool begin_streaming(const ProcessingContext& /*context*/) {
        // Default implementation - streaming not required
        return true;
    }

    /**
     * @brief Process a streaming response chunk
     *
     * Processes individual chunks of streaming responses. Plugins that
     * support streaming should implement this to handle partial responses.
     *
     * @param chunk Partial response chunk from provider
     * @param is_final True if this is the last chunk
     * @param context Processing context
     * @return Processing result for the chunk
     */
    virtual ProcessingResult process_streaming_chunk(
        const std::string& chunk,
        bool /*is_final*/,
        const ProcessingContext& /*context*/) {
        // Default implementation - just return the chunk as-is
        ProcessingResult result;
        result.success = true;
        result.processed_content = chunk;
        result.streaming_mode = true;
        return result;
    }

    /**
     * @brief End streaming response processing
     *
     * Called when streaming response processing is complete. Allows
     * plugins to cleanup state and finalize processing.
     *
     * @param context Processing context for the completed session
     * @return Final processing result
     */
    virtual ProcessingResult end_streaming(const ProcessingContext& /*context*/) {
        // Default implementation - return empty result
        ProcessingResult result;
        result.success = true;
        return result;
    }

    // Optional configuration and validation

    /**
     * @brief Configure plugin with custom settings
     *
     * Allows runtime configuration of plugin behavior. Plugins can
     * implement this to accept custom configuration parameters.
     *
     * @param config Configuration data as JSON object
     * @return true if configuration was applied successfully
     */
    virtual bool configure(const nlohmann::json& /*config*/) {
        // Default implementation - no configuration needed
        return true;
    }

    /**
     * @brief Validate plugin configuration
     *
     * Checks if the plugin is properly configured and ready for use.
     * Should validate all required dependencies and settings.
     *
     * @return true if plugin is properly configured
     */
    virtual bool validate_configuration() const {
        // Default implementation - always valid
        return true;
    }

    /**
     * @brief Get current plugin configuration
     *
     * @return Current configuration as JSON object
     */
    virtual nlohmann::json get_configuration() const {
        // Default implementation - empty configuration
        return nlohmann::json::object();
    }

    // Optional metrics and monitoring

    /**
     * @brief Get plugin performance metrics
     *
     * @return JSON object with performance counters and statistics
     */
    virtual nlohmann::json get_metrics() const {
        // Default implementation - no metrics
        return nlohmann::json::object();
    }

    /**
     * @brief Reset plugin metrics
     *
     * Resets all performance counters to zero.
     */
    virtual void reset_metrics() {
        // Default implementation - no metrics to reset
    }

    // Optional health and diagnostics

    /**
     * @brief Perform health check
     *
     * Checks if the plugin is functioning correctly. Should test
     * core functionality and report any issues.
     *
     * @return JSON health check result
     */
    virtual nlohmann::json health_check() const {
        nlohmann::json health;
        health["status"] = "healthy";
        health["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        return health;
    }

    /**
     * @brief Get diagnostic information
     *
     * Returns detailed diagnostic information useful for debugging
     * and troubleshooting plugin issues.
     *
     * @return JSON diagnostic information
     */
    virtual nlohmann::json get_diagnostics() const {
        nlohmann::json diagnostics;
        diagnostics["name"] = get_name();
        diagnostics["version"] = version();
        diagnostics["status"] = "active";
        return diagnostics;
    }

protected:
    // Utility methods for derived classes

    /**
     * @brief Create a basic successful result
     *
     * @param content Processed content
     * @return Basic successful ProcessingResult
     */
    ProcessingResult create_success_result(const std::string& content) const {
        ProcessingResult result;
        result.success = true;
        result.processed_content = content;
        return result;
    }

    /**
     * @brief Create an error result
     *
     * @param error_message Description of the error
     * @param error_code Optional error code for categorization
     * @return ProcessingResult indicating failure
     */
    ProcessingResult create_error_result(const std::string& error_message,
                                       const std::string& error_code = "") const {
        ProcessingResult result;
        result.success = false;
        result.error_message = error_message;
        if (!error_code.empty()) {
            result.metadata["error_code"] = error_code;
        }
        return result;
    }

    /**
     * @brief Extract tool calls from content using common patterns
     *
     * @param content Content to search for tool calls
     * @return Vector of extracted tool calls
     */
    std::vector<ToolCall> extract_tool_calls(const std::string& content) const;

    /**
     * @brief Validate JSON content safely
     *
     * @param content String to validate as JSON
     * @return Valid JSON object or empty optional
     */
    std::optional<nlohmann::json> validate_json(const std::string& content) const;

protected:
    // Allow derived classes to construct the base class
    PrettifierPlugin() = default;

    // Make plugin registry a friend for factory access
    friend class PluginRegistry;
};

/**
 * @brief Factory function type for plugin creation
 *
 * Each plugin must export a function with this signature that creates
 * and returns a new instance of the plugin.
 */
using PluginFactory = std::shared_ptr<PrettifierPlugin>(*)();

} // namespace prettifier
} // namespace aimux