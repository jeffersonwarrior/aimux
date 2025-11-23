#pragma once

#include "aimux/prettifier/prettifier_plugin.hpp"
#include <regex>
#include <chrono>
#include <atomic>

namespace aimux {
namespace prettifier {

/**
 * @brief Configuration for markdown normalization behavior
 */
struct MarkdownNormalizerConfig {
    bool enable_security_validation = true;
    bool enable_code_block_fixing = true;
    bool enable_whitespace_cleanup = true;
    bool enable_list_normalization = true;
    bool enable_heading_normalization = true;
    size_t max_content_size = 1024 * 1024; // 1MB max
    size_t max_line_length = 10000; // Prevent DoS with extremely long lines
    std::vector<std::string> allowed_languages = {
        "python", "javascript", "cpp", "c", "rust", "go", "java",
        "typescript", "bash", "shell", "json", "xml", "yaml",
        "markdown", "text", "sql", "html", "css", "dockerfile"
    };

    nlohmann::json to_json() const {
        nlohmann::json j;
        j["enable_security_validation"] = enable_security_validation;
        j["enable_code_block_fixing"] = enable_code_block_fixing;
        j["enable_whitespace_cleanup"] = enable_whitespace_cleanup;
        j["enable_list_normalization"] = enable_list_normalization;
        j["enable_heading_normalization"] = enable_heading_normalization;
        j["max_content_size"] = max_content_size;
        j["max_line_length"] = max_line_length;
        j["allowed_languages"] = allowed_languages;
        return j;
    }

    static MarkdownNormalizerConfig from_json(const nlohmann::json& j) {
        MarkdownNormalizerConfig config;
        if (j.contains("enable_security_validation"))
            j.at("enable_security_validation").get_to(config.enable_security_validation);
        if (j.contains("enable_code_block_fixing"))
            j.at("enable_code_block_fixing").get_to(config.enable_code_block_fixing);
        if (j.contains("enable_whitespace_cleanup"))
            j.at("enable_whitespace_cleanup").get_to(config.enable_whitespace_cleanup);
        if (j.contains("enable_list_normalization"))
            j.at("enable_list_normalization").get_to(config.enable_list_normalization);
        if (j.contains("enable_heading_normalization"))
            j.at("enable_heading_normalization").get_to(config.enable_heading_normalization);
        if (j.contains("max_content_size"))
            j.at("max_content_size").get_to(config.max_content_size);
        if (j.contains("max_line_length"))
            j.at("max_line_length").get_to(config.max_line_length);
        if (j.contains("allowed_languages"))
            j.at("allowed_languages").get_to(config.allowed_languages);
        return config;
    }
};

/**
 * @brief Statistics for markdown normalization operations
 */
struct MarkdownNormalizerStats {
    std::atomic<uint64_t> total_normalizations{0};
    std::atomic<uint64_t> successful_normalizations{0};
    std::atomic<uint64_t> security_blocks{0};
    std::atomic<uint64_t> code_blocks_fixed{0};
    std::atomic<uint64_t> whitespace_cleaned{0};
    std::atomic<uint64_t> average_time_us{0};
    std::atomic<uint64_t> max_time_us{0};

    nlohmann::json to_json() const {
        nlohmann::json j;
        j["total_normalizations"] = total_normalizations.load();
        j["successful_normalizations"] = successful_normalizations.load();
        j["security_blocks"] = security_blocks.load();
        j["code_blocks_fixed"] = code_blocks_fixed.load();
        j["whitespace_cleaned"] = whitespace_cleaned.load();
        j["average_time_us"] = average_time_us.load();
        j["max_time_us"] = max_time_us.load();
        return j;
    }

    void reset() {
        total_normalizations = 0;
        successful_normalizations = 0;
        security_blocks = 0;
        code_blocks_fixed = 0;
        whitespace_cleaned = 0;
        average_time_us = 0;
        max_time_us = 0;
    }
};

/**
 * @brief Provider-specific markdown patterns and normalization rules
 */
class ProviderPatterns {
public:
    /**
     * @brief Get normalization patterns for Cerebras provider
     *
     * Handles fast responses with incomplete code blocks and JSON tool responses.
     * Optimizes for speed while ensuring quality.
     */
    static std::vector<std::regex> get_cerebras_patterns();

    /**
     * @brief Get normalization patterns for OpenAI provider
     *
     * Handles well-structured but sometimes verbose responses.
     * Normalizes fenced code blocks and function calling formats.
     */
    static std::vector<std::regex> get_openai_patterns();

    /**
     * @brief Get normalization patterns for Anthropic provider
     *
     * Manages Claude-specific XML tags and tool use format.
     * Separates reasoning from final responses appropriately.
     */
    static std::vector<std::regex> get_anthropic_patterns();

    /**
     * @brief Get normalization patterns for Synthetic provider
     *
     * Handles mixed markdown with structured tool outputs.
     * Manages complex nested reasoning patterns.
     */
    static std::vector<std::regex> get_synthetic_patterns();

    /**
     * @brief Get common patterns applicable to all providers
     */
    static std::vector<std::regex> get_common_patterns();
};

/**
 * @brief Markdown Normalization Plugin
 *
 * Provides provider-specific markdown normalization with security validation.
 * Normalizes code blocks, fixes syntax issues, and prevents injection attacks.
 *
 * Performance targets:
 * - <50ms for 10KB markdown content
 * - <5ms for typical 1KB responses
 * - Memory usage: <5MB for normalization buffers
 *
 * Security features:
 * - Content sanitization to prevent XSS
 * - Length limits to prevent DoS attacks
 * - Language validation for code blocks
 * - Pattern-based injection detection
 */
class MarkdownNormalizerPlugin : public PrettifierPlugin {
public:
    /**
     * @brief Constructor with default configuration
     */
    MarkdownNormalizerPlugin();

    /**
     * @brief Constructor with custom configuration
     */
    explicit MarkdownNormalizerPlugin(const MarkdownNormalizerConfig& config);

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
    MarkdownNormalizerConfig config_;
    mutable MarkdownNormalizerStats stats_;

    // Streaming state
    std::string streaming_buffer_;
    bool streaming_active_ = false;
    std::string current_provider_;

    // Pre-compiled regex patterns for performance
    mutable std::mutex patterns_mutex_;
    std::map<std::string, std::vector<std::regex>> provider_patterns_;
    std::vector<std::regex> common_patterns_;

    // Core normalization methods

    /**
     * @brief Main normalization entry point
     */
    ProcessingResult normalize_markdown(
        const std::string& content,
        const ProcessingContext& context);

    /**
     * @brief Provider-specific normalization
     */
    std::string apply_provider_normalization(
        const std::string& content,
        const std::string& provider) const;

    /**
     * @brief Security validation and content sanitization
     */
    std::optional<std::string> validate_content(const std::string& content) const;

    /**
     * @brief Fix and normalize code blocks
     */
    std::string normalize_code_blocks(const std::string& content) const;

    /**
     * @brief Clean up excessive whitespace
     */
    std::string cleanup_whitespace(const std::string& content) const;

    /**
     * @brief Normalize list formatting
     */
    std::string normalize_lists(const std::string& content) const;

    /**
     * @brief Normalize heading hierarchy
     */
    std::string normalize_headings(const std::string& content) const;

    /**
     * @brief Apply common markdown improvements
     */
    std::string apply_common_fixes(const std::string& content) const;

    // Security methods

    /**
     * @brief Detect and prevent injection attacks
     */
    bool contains_injection_patterns(const std::string& content) const;

    /**
     * @brief Validate code block languages
     */
    bool is_valid_language(const std::string& language) const;

    /**
     * @brief Sanitize dangerous content
     */
    std::string sanitize_content(const std::string& content) const;

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
    void update_stats(std::chrono::microseconds duration, bool success, bool security_block = false);

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
     * @brief Process partial markdown in streaming mode
     */
    std::string process_streaming_markdown(const std::string& chunk, bool is_final);

    // Utility methods

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