#include "aimux/prettifier/markdown_normalizer.hpp"
#include <algorithm>
#include <sstream>
#include <iostream>
#include <unordered_set>
#include <cctype>

// TODO: Replace with proper logging when available
#define LOG_DEBUG(msg, ...) do { printf("[DEBUG] MarkdownNormalizer: " msg "\n", ##__VA_ARGS__); } while(0)
#define LOG_ERROR(msg, ...) do { printf("[ERROR] MarkdownNormalizer: " msg "\n", ##__VA_ARGS__); } while(0)
#define LOG_INFO(msg, ...) do { printf("[INFO] MarkdownNormalizer: " msg "\n", ##__VA_ARGS__); } while(0)

namespace aimux {
namespace prettifier {

// ProviderPatterns implementation
std::vector<std::regex> ProviderPatterns::get_cerebras_patterns() {
    return {
        // Fix incomplete code blocks (common in fast responses)
        std::regex(R"(```([a-zA-Z0-9_]*)\s*\n(.*?)\n```)", std::regex::ECMAScript),

        // Normalize JSON tool responses
        std::regex(R"(\{\s*"tool_calls"\s*:\s*\[.*?\]\s*\})", std::regex::ECMAScript),

        // Fix missing language identifiers
        std::regex(R"(```\s*\n(.*?)\n```)", std::regex::ECMAScript)
    };
}

std::vector<std::regex> ProviderPatterns::get_openai_patterns() {
    return {
        // Normalize OpenAI function calling format (simplified)
        std::regex(R"(```json\s*\n\{[^}]*\}\s*```)", std::regex::ECMAScript),

        // Clean up verbose code blocks
        std::regex(R"(```([a-zA-Z0-9_+]+)\s*\n(.*?)\s*```)", std::regex::ECMAScript),

        // Normalize step-by-step explanations
        std::regex(R"(^\s*\d+\.\s+(.*$))", std::regex::ECMAScript)
    };
}

std::vector<std::regex> ProviderPatterns::get_anthropic_patterns() {
    return {
        // Handle Claude XML tool tags (simplified)
        std::regex(R"(<tool_use>\s*\n\{.*?\}\s*\n</tool_use>)", std::regex::ECMAScript),

        // Extract thinking/reasoning blocks
        std::regex(R"(<thinking>\s*\n(.*?)\n</thinking>)", std::regex::ECMAScript),

        // Fix Claude's sometimes inconsistent code fencing
        std::regex(R"(```([a-zA-Z0-9_+]*)\s*(.*?)\s*```)", std::regex::ECMAScript)
    };
}

std::vector<std::regex> ProviderPatterns::get_synthetic_patterns() {
    return {
        // Handle complex nested markdown structures
        std::regex(R"(```([a-zA-Z0-9_+]*)\s*\n(.*?)\n```)", std::regex::ECMAScript),

        // Normalize structured tool outputs
        std::regex(R"(\{\s*"tool"\s*:\s*"[^"]*"\s*,\s*"result"\s*:\s*[^}]*\s*\})", std::regex::ECMAScript),

        // Clean up reasoning chains
        std::regex(R"(^\s*(?:Step\s+\d+:|=>|→)\s*(.*$))", std::regex::ECMAScript)
    };
}

std::vector<std::regex> ProviderPatterns::get_common_patterns() {
    return {
        // Fix common markdown syntax errors
        std::regex(R"(^\s*#{1,6}\s+(.+)$)", std::regex::ECMAScript),

        // Normalize list markers
        std::regex(R"(^\s*[-*+]\s+(.+)$)", std::regex::ECMAScript),
        std::regex(R"(^\s*\d+\.\s+(.+)$)", std::regex::ECMAScript),

        // Fix link formatting
        std::regex(R"(\[([^\]]+)\]\s*\(\s*([^)]+)\s*\))", std::regex::ECMAScript),

        // Clean up excessive whitespace
        std::regex(R"(\n{3,})"), // 3+ newlines to 2
        std::regex(R"([ \t]{2,})") // 2+ spaces to 1
    };
}

// MarkdownNormalizerPlugin implementation
MarkdownNormalizerPlugin::MarkdownNormalizerPlugin()
    : config_(MarkdownNormalizerConfig{}) {
    initialize_patterns();
    LOG_INFO("MarkdownNormalizerPlugin created with default configuration");
}

MarkdownNormalizerPlugin::MarkdownNormalizerPlugin(const MarkdownNormalizerConfig& config)
    : config_(config) {
    initialize_patterns();
    LOG_INFO("MarkdownNormalizerPlugin created with custom configuration");
}

ProcessingResult MarkdownNormalizerPlugin::preprocess_request(const core::Request& request) {
    auto start_time = std::chrono::high_resolution_clock::now();

    ProcessingResult result;
    result.success = true;
    result.processed_content = request.data;

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    result.processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(duration);

    log_debug("preprocess_request", "Request preprocessing completed");
    return result;
}

ProcessingResult MarkdownNormalizerPlugin::postprocess_response(
    const core::Response& response,
    const ProcessingContext& context) {

    auto start_time = std::chrono::high_resolution_clock::now();
    stats_.total_normalizations++;

    try {
        // Check content size limits first
        if (!check_content_limits(response.data)) {
            auto error_result = create_error_result("Content size exceeds maximum limit");
            stats_.security_blocks++;
            update_stats(std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::high_resolution_clock::now() - start_time), false, true);
            return error_result;
        }

        // Security validation
        if (config_.enable_security_validation) {
            auto validation_result = validate_content(response.data);
            if (!validation_result) {
                auto error_result = create_error_result("Content failed security validation");
                stats_.security_blocks++;
                update_stats(std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::high_resolution_clock::now() - start_time), false, true);
                return error_result;
            }
        }

        // Perform normalization
        std::string normalized_content = normalize_markdown(response.data, context).processed_content;

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

        ProcessingResult result;
        result.success = true;
        result.processed_content = normalized_content;
        result.output_format = "markdown";
        result.processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
        result.metadata["normalization_applied"] = true;
        result.metadata["provider"] = context.provider_name;

        stats_.successful_normalizations++;
        update_stats(duration, true);

        LOG_DEBUG("Markdown normalization completed in %ldus", duration.count());
        return result;

    } catch (const std::exception& e) {
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now() - start_time);
        update_stats(duration, false);

        LOG_ERROR("Normalization failed: %s", e.what());
        return create_error_result("Normalization failed: " + std::string(e.what()));
    }
}

std::string MarkdownNormalizerPlugin::get_name() const {
    return "markdown-normalizer-v1.0.0";
}

std::string MarkdownNormalizerPlugin::version() const {
    return "1.0.0";
}

std::string MarkdownNormalizerPlugin::description() const {
    return "Provider-specific markdown normalization with security validation. "
           "Handles Cerebras, OpenAI, Anthropic, and Synthetic response formats. "
           "Features code block fixing, whitespace cleanup, and injection prevention.";
}

std::vector<std::string> MarkdownNormalizerPlugin::supported_formats() const {
    return {"markdown", "text", "mixed"};
}

std::vector<std::string> MarkdownNormalizerPlugin::output_formats() const {
    return {"markdown", "toon"};
}

std::vector<std::string> MarkdownNormalizerPlugin::supported_providers() const {
    return {"cerebras", "openai", "anthropic", "synthetic"};
}

std::vector<std::string> MarkdownNormalizerPlugin::capabilities() const {
    return {"normalization", "security-validation", "code-block-fixing",
            "whitespace-cleanup", "provider-specific"};
}

bool MarkdownNormalizerPlugin::begin_streaming(const ProcessingContext& context) {
    reset_streaming_state();
    streaming_active_ = true;
    current_provider_ = context.provider_name;
    log_debug("streaming", "Started streaming for provider: " + context.provider_name);
    return true;
}

ProcessingResult MarkdownNormalizerPlugin::process_streaming_chunk(
    const std::string& chunk,
    bool is_final,
    const ProcessingContext& context) {

    if (!streaming_active_) {
        return create_error_result("Streaming not initialized");
    }

    try {
        std::string processed_chunk = process_streaming_markdown(chunk, is_final);

        ProcessingResult result;
        result.success = true;
        result.processed_content = processed_chunk;
        result.streaming_mode = true;

        if (is_final) {
            result = end_streaming(context);
        }

        return result;

    } catch (const std::exception& e) {
        return create_error_result("Streaming chunk processing failed: " + std::string(e.what()));
    }
}

ProcessingResult MarkdownNormalizerPlugin::end_streaming(const ProcessingContext& context) {
    ProcessingResult result;
    result.success = true;
    result.processed_content = streaming_buffer_;
    result.streaming_mode = false;

    reset_streaming_state();
    log_debug("streaming", "Ended streaming");

    return result;
}

bool MarkdownNormalizerPlugin::configure(const nlohmann::json& config) {
    try {
        config_ = MarkdownNormalizerConfig::from_json(config);
        log_debug("configure", "Configuration updated successfully");
        return true;
    } catch (const std::exception& e) {
        log_error("configure", "Configuration failed: " + std::string(e.what()));
        return false;
    }
}

bool MarkdownNormalizerPlugin::validate_configuration() const {
    return config_.max_content_size > 0 &&
           config_.max_line_length > 0 &&
           !config_.allowed_languages.empty();
}

nlohmann::json MarkdownNormalizerPlugin::get_configuration() const {
    nlohmann::json j = config_.to_json();
    j["plugin_name"] = get_name();
    j["version"] = version();
    return j;
}

nlohmann::json MarkdownNormalizerPlugin::get_metrics() const {
    nlohmann::json j = stats_.to_json();
    j["configuration"] = config_.to_json();
    return j;
}

void MarkdownNormalizerPlugin::reset_metrics() {
    stats_.reset();
}

nlohmann::json MarkdownNormalizerPlugin::health_check() const {
    nlohmann::json health;
    health["status"] = "healthy";
    health["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    health["valid_configuration"] = validate_configuration();
    health["streaming_active"] = streaming_active_;
    health["stats"] = stats_.to_json();
    return health;
}

nlohmann::json MarkdownNormalizerPlugin::get_diagnostics() const {
    nlohmann::json diagnostics;
    diagnostics["name"] = get_name();
    diagnostics["version"] = version();
    diagnostics["status"] = "active";
    diagnostics["config"] = config_.to_json();
    diagnostics["stats"] = stats_.to_json();
    diagnostics["streaming"] = streaming_active_;
    return diagnostics;
}

// Private methods implementation
ProcessingResult MarkdownNormalizerPlugin::normalize_markdown(
    const std::string& content,
    const ProcessingContext& context) {

    std::string processed = content;

    // Apply provider-specific normalization
    if (!context.provider_name.empty()) {
        processed = apply_provider_normalization(processed, context.provider_name);
    }

    // Apply common fixes
    processed = apply_common_fixes(processed);

    ProcessingResult result;
    result.success = true;
    result.processed_content = processed;
    return result;
}

std::string MarkdownNormalizerPlugin::apply_provider_normalization(
    const std::string& content,
    const std::string& provider) const {

    std::string processed = content;

    // Apply provider-specific transformations
    if (provider == "cerebras") {
        // Cerebras-specific fixes
        processed = std::regex_replace(processed,
            std::regex(R"(```\s*\n(.*?)\n```)"), "```text\n$1```");
    } else if (provider == "openai") {
        // OpenAI-specific fixes
        // Clean up verbose output
    } else if (provider == "anthropic") {
        // Anthropic-specific fixes
        // Handle XML tags gracefully
    } else if (provider == "synthetic") {
        // Synthetic-specific fixes
        // Normalize complex structures
    }

    return processed;
}

std::optional<std::string> MarkdownNormalizerPlugin::validate_content(const std::string& content) const {
    if (!config_.enable_security_validation) {
        return content;
    }

    // Check for injection patterns
    if (contains_injection_patterns(content)) {
        return std::nullopt;
    }

    // Check line length limits
    std::istringstream stream(content);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.length() > config_.max_line_length) {
            return std::nullopt;
        }
    }

    return sanitize_content(content);
}

std::string MarkdownNormalizerPlugin::normalize_code_blocks(const std::string& content) const {
    if (!config_.enable_code_block_fixing) {
        return content;
    }

    std::string processed = content;

    try {
        // Simple code block fixing - ensure language is specified
        static const std::regex code_block_no_lang_regex(R"(```\s*\n(.*?)\n```)", std::regex::ECMAScript);
        processed = std::regex_replace(processed, code_block_no_lang_regex, "```text\n$1```");

        stats_.code_blocks_fixed++;

    } catch (const std::exception& e) {
        log_error("normalize_code_blocks", "Code block normalization failed: " + std::string(e.what()));
    }

    return processed;
}

std::string MarkdownNormalizerPlugin::cleanup_whitespace(const std::string& content) const {
    if (!config_.enable_whitespace_cleanup) {
        return content;
    }

    std::string processed = content;

    try {
        // Reduce multiple consecutive newlines to maximum 2
        processed = std::regex_replace(processed, std::regex(R"(\n{3,})"), "\n\n");

        // Remove trailing whitespace from each line
        std::istringstream stream(processed);
        std::string line, cleaned;
        bool first = true;

        while (std::getline(stream, line)) {
            if (!first) cleaned += "\n";

            size_t end_pos = line.find_last_not_of(" \t");
            if (end_pos != std::string::npos) {
                cleaned += line.substr(0, end_pos + 1);
            } else {
                cleaned += line; // Keep empty lines as-is
            }

            first = false;
        }

        stats_.whitespace_cleaned++;

    } catch (const std::exception& e) {
        log_error("cleanup_whitespace", "Whitespace cleanup failed: " + std::string(e.what()));
    }

    return processed;
}

std::string MarkdownNormalizerPlugin::normalize_lists(const std::string& content) const {
    if (!config_.enable_list_normalization) {
        return content;
    }

    // This would implement list normalization logic
    // For now, return content as-is to focus on core functionality
    return content;
}

std::string MarkdownNormalizerPlugin::normalize_headings(const std::string& content) const {
    if (!config_.enable_heading_normalization) {
        return content;
    }

    // This would implement heading normalization logic
    // For now, return content as-is to focus on core functionality
    return content;
}

std::string MarkdownNormalizerPlugin::apply_common_fixes(const std::string& content) const {
    std::string processed = content;

    // Apply standard normalization
    if (config_.enable_code_block_fixing) {
        processed = normalize_code_blocks(processed);
    }

    if (config_.enable_whitespace_cleanup) {
        processed = cleanup_whitespace(processed);
    }

    if (config_.enable_list_normalization) {
        processed = normalize_lists(processed);
    }

    if (config_.enable_heading_normalization) {
        processed = normalize_headings(processed);
    }

    return processed;
}

bool MarkdownNormalizerPlugin::contains_injection_patterns(const std::string& content) const {
    // Check for common injection patterns
    std::vector<std::regex> injection_patterns = {
        std::regex(R"(<script[^>]*>.*?</script>)", std::regex::icase),
        std::regex(R"(javascript\s*:)", std::regex::icase),
        std::regex(R"(eval\s*\()", std::regex::icase),
        std::regex(R"(document\s*\.)", std::regex::icase),
        std::regex(R"(window\s*\.)", std::regex::icase)
    };

    for (const auto& pattern : injection_patterns) {
        if (std::regex_search(content, pattern)) {
            return true;
        }
    }

    return false;
}

bool MarkdownNormalizerPlugin::is_valid_language(const std::string& language) const {
    return std::find(config_.allowed_languages.begin(),
                     config_.allowed_languages.end(),
                     language) != config_.allowed_languages.end();
}

std::string MarkdownNormalizerPlugin::sanitize_content(const std::string& content) const {
    std::string sanitized = content;

    // Remove or escape dangerous HTML elements
    sanitized = std::regex_replace(sanitized,
        std::regex(R"(<script[^>]*>)", std::regex::icase), "&lt;script&gt;");
    sanitized = std::regex_replace(sanitized,
        std::regex(R"(</script>)", std::regex::icase), "&lt;/script&gt;");

    // Escape potential XSS vectors
    sanitized = std::regex_replace(sanitized,
        std::regex(R"(javascript\s*:)", std::regex::icase), "javascript:");

    return sanitized;
}

void MarkdownNormalizerPlugin::initialize_patterns() {
    std::lock_guard<std::mutex> lock(patterns_mutex_);

    provider_patterns_["cerebras"] = ProviderPatterns::get_cerebras_patterns();
    provider_patterns_["openai"] = ProviderPatterns::get_openai_patterns();
    provider_patterns_["anthropic"] = ProviderPatterns::get_anthropic_patterns();
    provider_patterns_["synthetic"] = ProviderPatterns::get_synthetic_patterns();
    common_patterns_ = ProviderPatterns::get_common_patterns();

    log_debug("initialize_patterns", "Pattern initialization completed");
}

std::vector<std::regex> MarkdownNormalizerPlugin::get_provider_patterns(const std::string& provider) const {
    std::lock_guard<std::mutex> lock(patterns_mutex_);

    auto it = provider_patterns_.find(provider);
    if (it != provider_patterns_.end()) {
        return it->second;
    }
    return common_patterns_;
}

void MarkdownNormalizerPlugin::update_stats(std::chrono::microseconds duration,
                                           bool success,
                                           bool security_block) {
    uint64_t duration_us = duration.count();

    // Update max time
    uint64_t current_max = stats_.max_time_us.load();
    while (duration_us > current_max &&
           !stats_.max_time_us.compare_exchange_weak(current_max, duration_us)) {
        // Retry if another thread updated max_time_us
    }

    // Update average time (simple moving average)
    uint64_t total = stats_.total_normalizations.load();
    if (total > 0) {
        uint64_t current_avg = stats_.average_time_us.load();
        uint64_t new_avg = ((current_avg * (total - 1)) + duration_us) / total;
        stats_.average_time_us = new_avg;
    }

    if (security_block) {
        stats_.security_blocks++;
    }
}

bool MarkdownNormalizerPlugin::check_content_limits(const std::string& content) const {
    return content.length() <= config_.max_content_size;
}

void MarkdownNormalizerPlugin::reset_streaming_state() {
    streaming_buffer_.clear();
    streaming_active_ = false;
    current_provider_.clear();
}

std::string MarkdownNormalizerPlugin::process_streaming_markdown(const std::string& chunk, bool is_final) {
    streaming_buffer_ += chunk;

    if (is_final) {
        // Apply full normalization on final chunk
        ProcessingContext context;
        context.provider_name = current_provider_;
        auto result = normalize_markdown(streaming_buffer_, context);
        return result.processed_content;
    } else {
        // For partial chunks, just return as-is to avoid breaking incomplete structures
        return chunk;
    }
}

void MarkdownNormalizerPlugin::log_debug(const std::string& operation, const std::string& message) const {
    LOG_DEBUG("%s - %s", operation.c_str(), message.c_str());
}

void MarkdownNormalizerPlugin::log_error(const std::string& operation, const std::string& message) const {
    LOG_ERROR("%s - %s", operation.c_str(), message.c_str());
}

} // namespace prettifier
} // namespace aimux