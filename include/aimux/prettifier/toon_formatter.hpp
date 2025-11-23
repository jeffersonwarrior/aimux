#pragma once

#include <string>
#include <optional>
#include <chrono>
#include <nlohmann/json.hpp>
#include "aimux/prettifier/prettifier_plugin.hpp"

namespace aimux {
namespace prettifier {

/**
 * @brief TOON Format Standard for AI Communication Standardization
 *
 * TOON (Tabular Object-Oriented Notation) is a lightweight, human-readable format
 * specifically designed for standardizing AI communications across different providers.
 * It's optimized for both machine parsing and human readability while maintaining
 * compatibility with existing markdown and JSON formats.
 *
 * Key Principles:
 * - Tabular structure for clear organization
 * - Simple section-based format
 * - Preserves all metadata from original requests
 * - Provider-agnostic normalization
 * - Extensible through custom sections
 *
 * Format Structure:
 * ```
 * # META
 * key: value
 * timestamp: 2024-01-15T10:30:00Z
 *
 * # CONTENT
 * [TYPE: markdown]
 * [FORMAT: enhanced_markdown]
 * [CONTENT: response_content...]
 *
 * # TOOLS
 * [CALL: function_name]
 * [PARAMS: {"key": "value"}]
 * [RESULT: success/error]
 *
 * # THINKING
 * [REASONING: step-by-step analysis...]
 * ```
 *
 * Performance Requirements (from qa/phase1_foundation_qa_plan.md):
 * - Serialization: <10ms for typical 1KB response
 * - Deserialization: <5ms for typical TOON document
 * - Memory overhead: <2x original response size
 * - 100% round-trip data preservation
 *
 * Usage:
 * ```cpp
 * ToonFormatter formatter;
 *
 * // Serialize response to TOON
 * std::string toon_format = formatter.serialize_response(response, context);
 *
 * // Parse TOON back to structured data
 * auto parsed = formatter.deserialize_toon(toon_format);
 * ```
 */
class ToonFormatter {
public:
    /**
     * @brief Default constructor with standard configuration
     */
    ToonFormatter();

    /**
     * @brief Configuration options for TOON formatting
     */
    struct Config {
        bool include_metadata = true;        // Include META section
        bool include_tools = true;           // Include TOOLS section
        bool include_thinking = true;        // Include THINKING section
        bool preserve_timestamps = true;     // Keep original timestamps
        bool enable_compression = false;     // Compact output format
        size_t max_content_length = 1000000; // Max content size (1MB)
        std::string indent = "    ";         // Standard indentation
    };

    /**
     * @brief Construct with custom configuration
     *
     * @param config Custom formatting configuration
     */
    explicit ToonFormatter(const Config& config);

    // Core Serialization Methods

    /**
     * @brief Convert AI response to TOON format
     *
     * Transforms a provider response into standardized TOON format with
     * proper section organization and metadata preservation.
     *
     * @param response The response to convert
     * @param context Processing context metadata
     * @param tool_calls Extracted tool calls (optional)
     * @param thinking Reasoning content (optional)
     * @return TOON formatted string
     *
     * Performance target: <10ms for typical 1KB response
     */
    std::string serialize_response(
        const core::Response& response,
        const ProcessingContext& context,
        const std::vector<ToolCall>& tool_calls = {},
        const std::string& thinking = "");

    /**
     * @brief Convert structured data to TOON format
     *
     * More flexible serialization for arbitrary data structures.
     *
     * @param data JSON data to format
     * @param metadata Additional metadata to include
     * @return TOON formatted string
     */
    std::string serialize_data(
        const nlohmann::json& data,
        const std::map<std::string, std::string>& metadata = {});

    // Core Deserialization Methods

    /**
     * @brief Parse TOON format to structured data
     *
     * Extracts all sections and converts them back to structured format
     * with full metadata preservation.
     *
     * @param toon_content TOON formatted content
     * @return Structured representation of TOON content
     *
     * Performance target: <5ms for typical TOON document
     */
    std::optional<nlohmann::json> deserialize_toon(const std::string& toon_content);

    /**
     * @brief Extract specific section from TOON content
     *
     * @param toon_content TOON formatted content
     * @param section_name Name of section to extract (META, CONTENT, TOOLS, THINKING)
     * @return Section content or empty optional
     */
    std::optional<std::string> extract_section(
        const std::string& toon_content,
        const std::string& section_name);

    // Validation and Analysis

    /**
     * @brief Validate TOON format syntax and structure
     *
     * Checks for proper section organization, valid section names,
     * and well-formed metadata.
     *
     * @param toon_content Content to validate
     * @param error_message Error description if validation fails
     * @return true if content is valid TOON format
     */
    bool validate_toon(const std::string& toon_content, std::string& error_message);

    /**
     * @brief Get statistics about TOON content
     *
     * Analyzes TOON content and returns metadata about structure,
     * size, sections, etc.
     *
     * @param toon_content TOON content to analyze
     * @return Analysis statistics
     */
    nlohmann::json analyze_toon(const std::string& toon_content);

    // Utility Methods

    /**
     * @brief Convert JSON to TOON recursively
     *
     * Utility for converting arbitrary JSON objects to TOON format.
     *
     * @param json_data JSON data to convert
     * @param indent Current indentation level
     * @return TOON formatted string
     */
    std::string json_to_toon(const nlohmann::json& json_data, int indent = 0);

    /**
     * @brief Escape special characters for TOON format
     *
     * Escapes characters that could interfere with TOON section parsing.
     *
     * @param input String to escape
     * @return Escaped string
     */
    std::string escape_toon_content(const std::string& input);

    /**
     * @brief Unescape special characters from TOON content
     *
     * @param input Escaped string from TOON
     * @return Unescaped string
     */
    std::string unescape_toon_content(const std::string& input);

    // Section Creation Helpers

    /**
     * @brief Create META section from JSON
     */
    std::string create_meta_section(const nlohmann::json& metadata);

    /**
     * @brief Create CONTENT section with typing information
     */
    std::string create_content_section(
        const std::string& content,
        const std::string& type = "markdown",
        const std::string& format = "");

    /**
     * @brief Create TOOLS section from tool calls
     */
    std::string create_tools_section(
        const std::vector<ToolCall>& tool_calls);

    /**
     * @brief Create THINKING section from reasoning
     */
    std::string create_thinking_section(const std::string& reasoning);

    // Configuration Management

    /**
     * @brief Update formatting configuration
     *
     * @param new_config New configuration to apply
     */
    void update_config(const Config& new_config);

    /**
     * @brief Get current configuration
     *
     * @return Current formatting configuration
     */
    const Config& get_config() const { return config_; }

private:
    Config config_;

    // Internal parsing helpers
    std::vector<std::pair<std::string, std::string>> parse_sections(
        const std::string& toon_content);

    nlohmann::json parse_meta_section(const std::string& content);
    nlohmann::json parse_content_section(const std::string& content);
    nlohmann::json parse_tools_section(const std::string& content);
    std::string parse_thinking_section(const std::string& content);

    // Validation helpers
    bool is_valid_section_name(const std::string& name);
    bool is_valid_meta_line(const std::string& line);
    bool is_valid_content_tag(const std::string& tag);

    // Utility helpers
    std::string generate_timestamp();
    std::string format_json_value(const nlohmann::json& value);
    std::string indent_string(const std::string& input, const std::string& indent);
    size_t estimate_size(const std::string& content);

    // Error handling
    void log_error(const std::string& operation, const std::string& message);
    void log_debug(const std::string& operation, const std::string& message);
};

/**
 * @brief Result of TOON parsing operation
 */
struct ToonParseResult {
    bool success = false;
    nlohmann::json data;
    std::string metadata;
    std::string content;
    std::vector<ToolCall> tools;
    std::string thinking;
    std::string error_message;
    std::chrono::milliseconds parse_time{0};

    nlohmann::json to_json() const;
};

} // namespace prettifier
} // namespace aimux