#include "aimux/prettifier/tool_call_extractor.hpp"
#include <algorithm>
#include <sstream>
#include <iostream>
#include <unordered_set>
#include <cctype>
#include <random>

// TODO: Replace with proper logging when available
#define LOG_DEBUG(msg, ...) do { printf("[DEBUG] ToolCallExtractor: " msg "\n", ##__VA_ARGS__); } while(0)
#define LOG_ERROR(msg, ...) do { printf("[ERROR] ToolCallExtractor: " msg "\n", ##__VA_ARGS__); } while(0)
#define LOG_INFO(msg, ...) do { printf("[INFO] ToolCallExtractor: " msg "\n", ##__VA_ARGS__); } while(0)

namespace aimux {
namespace prettifier {

// ProviderToolPatterns implementation
std::vector<std::regex> ProviderToolPatterns::get_cerebras_patterns() {
    return {
        // Cerebras JSON tool calls format (simplified)
        std::regex(R"(\{\s*"tool_calls"\s*:\s*\[.*?\]\s*\})", std::regex::ECMAScript),

        // Simple tool call format (simplified)
        std::regex(R"(\{\s*"name"\s*:\s*"[^"]*"\s*,\s*"arguments"\s*:\s*"[^"]*"\s*\})", std::regex::ECMAScript),

        // Tool calls with id (simplified)
        std::regex(R"(\{\s*"id"\s*:\s*"[^"]*"[^}]*\})", std::regex::ECMAScript)
    };
}

std::vector<std::regex> ProviderToolPatterns::get_openai_patterns() {
    return {
        // OpenAI function calling format (simplified)
        std::regex(R"(\{\s*"function"\s*:\s*\{[^}]*\}\s*\})", std::regex::ECMAScript),

        // Multiple tool calls (simplified)
        std::regex(R"(\{\s*"tool_calls"\s*:\s*\[.*?\]\s*\})", std::regex::ECMAScript),

        // Individual tool call with id (simplified)
        std::regex(R"(\{\s*"id".*?"function".*?\})", std::regex::ECMAScript)
    };
}

std::vector<std::regex> ProviderToolPatterns::get_anthropic_patterns() {
    return {
        // Anthropic XML tool calls (simplified)
        std::regex(R"(<tool_use>.*?</tool_use>)", std::regex::ECMAScript),

        // Claude tool use format (simplified)
        std::regex(R"(<invoke.*?>.*?</invoke>)", std::regex::ECMAScript),

        // Function call with parameters (simplified)
        std::regex(R"(<function_call>.*?</function_call>)", std::regex::ECMAScript)
    };
}

std::vector<std::regex> ProviderToolPatterns::get_synthetic_patterns() {
    return {
        // Synthetic mixed format (simplified)
        std::regex(R"(\{\s*"tool".*?\})", std::regex::ECMAScript),

        // Command style (simplified)
        std::regex(R"(`[^`]+`\s*:\s*`[^`]*`)", std::regex::ECMAScript),

        // Step format (simplified)
        std::regex(R"(^\s*(?:Tool|Execute|Run):\s*[^=]+)", std::regex::ECMAScript)
    };
}

std::vector<std::regex> ProviderToolPatterns::get_common_patterns() {
    return {
        // Generic JSON tool pattern (simplified)
        std::regex(R"(\{\s*".*?"\s*:\s*".*?"\s*\})", std::regex::ECMAScript),

        // Bracket notation (simplified)
        std::regex(R"(\[[^\[\]]+\]\s*\([^)]*\))", std::regex::ECMAScript),

        // Simple name:args format
        std::regex(R"(^\s*[a-zA-Z_][a-zA-Z0-9_]*\s*:\s*.+$)", std::regex::ECMAScript)
    };
}

// ToolCallExtractorPlugin implementation
ToolCallExtractorPlugin::ToolCallExtractorPlugin()
    : config_(ToolCallExtractorConfig{}) {
    initialize_patterns();
    LOG_INFO("ToolCallExtractorPlugin created with default configuration");
}

ToolCallExtractorPlugin::ToolCallExtractorPlugin(const ToolCallExtractorConfig& config)
    : config_(config) {
    initialize_patterns();
    LOG_INFO("ToolCallExtractorPlugin created with custom configuration");
}

ProcessingResult ToolCallExtractorPlugin::preprocess_request(const core::Request& request) {
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

ProcessingResult ToolCallExtractorPlugin::postprocess_response(
    const core::Response& response,
    const ProcessingContext& context) {

    auto start_time = std::chrono::high_resolution_clock::now();
    stats_.total_extractions++;

    try {
        // Check content size limits first
        if (!check_content_limits(response.data)) {
            auto error_result = create_error_result("Content size exceeds maximum limit");
            stats_.security_blocks++;
            update_stats(std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::high_resolution_clock::now() - start_time), false, 0);
            return error_result;
        }

        // Security validation
        if (config_.enable_security_validation) {
            if (contains_malicious_patterns(response.data)) {
                auto error_result = create_error_result("Content contains malicious patterns");
                stats_.security_blocks++;
                update_stats(std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::high_resolution_clock::now() - start_time), false, 0);
                return error_result;
            }
        }

        // Perform tool call extraction
        std::vector<ToolCall> tool_calls = extract_tool_calls(response.data, context.provider_name);

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

        // Serialize to TOON format
        std::string serialized_content = serialize_tool_calls_to_toon(tool_calls);

        ProcessingResult result;
        result.success = true;
        result.processed_content = serialized_content;
        result.output_format = "toon";
        result.processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
        result.metadata["tool_calls_extracted"] = tool_calls.size();
        result.metadata["provider"] = context.provider_name;

        stats_.successful_extractions++;
        stats_.tools_extracted += tool_calls.size();
        update_stats(duration, true, tool_calls.size());

        LOG_DEBUG("Tool call extraction completed in %ldus, extracted %zu tools", duration.count(), tool_calls.size());
        return result;

    } catch (const std::exception& e) {
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now() - start_time);
        update_stats(duration, false, 0);

        LOG_ERROR("Tool call extraction failed: %s", e.what());
        return create_error_result("Tool call extraction failed: " + std::string(e.what()));
    }
}

std::string ToolCallExtractorPlugin::get_name() const {
    return "tool-call-extractor-v1.0.0";
}

std::string ToolCallExtractorPlugin::version() const {
    return "1.0.0";
}

std::string ToolCallExtractorPlugin::description() const {
    return "Extracts tool calls from AI responses with security validation and error recovery. "
           "Supports Cerebras, OpenAI, Anthropic, and Synthetic formats. "
           "Handles JSON, XML, and text-based tool calls with provider-specific patterns.";
}

std::vector<std::string> ToolCallExtractorPlugin::supported_formats() const {
    return {"text", "json", "xml", "mixed"};
}

std::vector<std::string> ToolCallExtractorPlugin::output_formats() const {
    return {"toon", "json"};
}

std::vector<std::string> ToolCallExtractorPlugin::supported_providers() const {
    return {"cerebras", "openai", "anthropic", "synthetic"};
}

std::vector<std::string> ToolCallExtractorPlugin::capabilities() const {
    return {"tool-extraction", "security-validation", "json-parsing",
            "xml-parsing", "error-recovery", "provider-specific"};
}

bool ToolCallExtractorPlugin::begin_streaming(const ProcessingContext& context) {
    reset_streaming_state();
    streaming_active_ = true;
    current_provider_ = context.provider_name;
    log_debug("streaming", "Started streaming for provider: " + context.provider_name);
    return true;
}

ProcessingResult ToolCallExtractorPlugin::process_streaming_chunk(
    const std::string& chunk,
    bool is_final,
    const ProcessingContext& context) {

    if (!streaming_active_) {
        return create_error_result("Streaming not initialized");
    }

    try {
        std::string processed_chunk = process_streaming_extraction(chunk, is_final);

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

ProcessingResult ToolCallExtractorPlugin::end_streaming(const ProcessingContext& context) {
    ProcessingResult result;
    result.success = true;
    result.processed_content = streaming_buffer_;
    result.streaming_mode = false;

    reset_streaming_state();
    log_debug("streaming", "Ended streaming");

    return result;
}

bool ToolCallExtractorPlugin::configure(const nlohmann::json& config) {
    try {
        config_ = ToolCallExtractorConfig::from_json(config);
        log_debug("configure", "Configuration updated successfully");
        return true;
    } catch (const std::exception& e) {
        log_error("configure", "Configuration failed: " + std::string(e.what()));
        return false;
    }
}

bool ToolCallExtractorPlugin::validate_configuration() const {
    return config_.max_content_size > 0 &&
           config_.max_tool_calls > 0 &&
           !config_.allowed_tool_names.empty();
}

nlohmann::json ToolCallExtractorPlugin::get_configuration() const {
    nlohmann::json j = config_.to_json();
    j["plugin_name"] = get_name();
    j["version"] = version();
    return j;
}

nlohmann::json ToolCallExtractorPlugin::get_metrics() const {
    nlohmann::json j = stats_.to_json();
    j["configuration"] = config_.to_json();
    return j;
}

void ToolCallExtractorPlugin::reset_metrics() {
    stats_.reset();
}

nlohmann::json ToolCallExtractorPlugin::health_check() const {
    nlohmann::json health;
    health["status"] = "healthy";
    health["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    health["valid_configuration"] = validate_configuration();
    health["streaming_active"] = streaming_active_;
    health["stats"] = stats_.to_json();
    return health;
}

nlohmann::json ToolCallExtractorPlugin::get_diagnostics() const {
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
std::vector<ToolCall> ToolCallExtractorPlugin::extract_tool_calls(
    const std::string& content,
    const std::string& provider) const {

    std::vector<ToolCall> all_tool_calls;

    // Extract provider-specific tool calls
    auto provider_calls = extract_provider_tool_calls(content, provider);
    all_tool_calls.insert(all_tool_calls.end(), provider_calls.begin(), provider_calls.end());

    // Apply common patterns as fallback
    if (all_tool_calls.empty()) {
        auto patterns = get_provider_patterns("common");
        for (const auto& pattern : patterns) {
            std::sregex_iterator iter(content.begin(), content.end(), pattern);
            std::sregex_iterator end;

            for (; iter != end && all_tool_calls.size() < config_.max_tool_calls; ++iter) {
                std::smatch match = *iter;
                if (match.size() >= 2) {
                    ToolCall tool;
                    tool.name = "extracted_tool";
                    tool.id = generate_call_id();
                    tool.parameters = nlohmann::json{{"raw_content", match[0].str()}};
                    tool.status = "pending";

                    if (validate_tool_call(tool)) {
                        all_tool_calls.push_back(tool);
                    }
                }
            }
        }
    }

    // Limit number of tool calls
    if (all_tool_calls.size() > config_.max_tool_calls) {
        all_tool_calls.resize(config_.max_tool_calls);
    }

    return all_tool_calls;
}

std::vector<ToolCall> ToolCallExtractorPlugin::extract_provider_tool_calls(
    const std::string& content,
    const std::string& provider) const {

    std::vector<ToolCall> tool_calls;

    if (provider == "cerebras" || provider == "openai") {
        tool_calls = extract_json_tool_calls(content);
    } else if (provider == "anthropic") {
        tool_calls = extract_xml_tool_calls(content);
    } else if (provider == "synthetic") {
        // Try JSON first, then XML, then text patterns
        auto json_calls = extract_json_tool_calls(content);
        tool_calls.insert(tool_calls.end(), json_calls.begin(), json_calls.end());

        if (tool_calls.empty()) {
            auto xml_calls = extract_xml_tool_calls(content);
            tool_calls.insert(tool_calls.end(), xml_calls.begin(), xml_calls.end());
        }
    }

    return tool_calls;
}

std::vector<ToolCall> ToolCallExtractorPlugin::extract_json_tool_calls(const std::string& content) const {
    std::vector<ToolCall> tool_calls;

    try {
        // Try to parse as complete JSON
        auto json_obj = nlohmann::json::parse(content);

        // Handle different JSON structures
        if (json_obj.contains("tool_calls") && json_obj["tool_calls"].is_array()) {
            for (const auto& call : json_obj["tool_calls"]) {
                if (auto tool = parse_tool_call_from_json(call)) {
                    tool_calls.push_back(*tool);
                }
            }
        }

        // Handle function array
        if (json_obj.contains("functions") && json_obj["functions"].is_array()) {
            for (const auto& func : json_obj["functions"]) {
                if (auto tool = parse_tool_call_from_json(func)) {
                    tool_calls.push_back(*tool);
                }
            }
        }

    } catch (const std::exception& e) {
        // Try regex-based extraction as fallback
        if (config_.enable_error_recovery) {
            tool_calls = recover_from_json_error(content);
        }
        stats_.json_parse_failures++;
        log_debug("extract_json_tool_calls", "JSON parse failed: " + std::string(e.what()));
    }

    return tool_calls;
}

std::vector<ToolCall> ToolCallExtractorPlugin::extract_xml_tool_calls(const std::string& content) const {
    std::vector<ToolCall> tool_calls;

    try {
        // Simple XML parsing using regex for now
        std::regex tool_regex(R"(<tool_use>.*?</tool_use>)");
        std::sregex_iterator iter(content.begin(), content.end(), tool_regex);
        std::sregex_iterator end;

        for (; iter != end && tool_calls.size() < config_.max_tool_calls; ++iter) {
            std::smatch match = *iter;
            if (match.size() >= 1) {
                ToolCall tool;
                tool.name = "xml_tool_call";
                tool.id = generate_call_id();
                tool.parameters = nlohmann::json{{"raw_content", match[0].str()}};
                tool.status = "pending";

                if (validate_tool_call(tool)) {
                    tool_calls.push_back(tool);
                }
            }
        }

    } catch (const std::exception& e) {
        // Try recovery
        if (config_.enable_error_recovery) {
            tool_calls = recover_from_xml_error(content);
        }
        stats_.xml_parse_failures++;
        log_debug("extract_xml_tool_calls", "XML parse failed: " + std::string(e.what()));
    }

    return tool_calls;
}

std::optional<ToolCall> ToolCallExtractorPlugin::parse_tool_call_from_json(const nlohmann::json& json_obj) const {
    try {
        ToolCall tool;
        tool.status = "pending";
        tool.timestamp = std::chrono::system_clock::now();

        // Handle different JSON structures
        if (json_obj.contains("function")) {
            // OpenAI format
            const auto& func = json_obj["function"];
            tool.name = func.value("name", "");
            tool.parameters = nlohmann::json{{"arguments", func.value("arguments", "")}};
            tool.id = json_obj.value("id", generate_call_id());
        } else if (json_obj.contains("name")) {
            // Simple format
            tool.name = json_obj["name"];
            tool.parameters = nlohmann::json{{"arguments", json_obj.value("arguments", json_obj.value("input", ""))}};
            tool.id = json_obj.value("id", generate_call_id());
        } else if (json_obj.contains("tool")) {
            // Tool format
            tool.name = json_obj["tool"];
            tool.parameters = nlohmann::json{{"args", json_obj.value("args", "")}};
            tool.id = json_obj.value("id", generate_call_id());
        } else {
            return std::nullopt;
        }

        if (validate_tool_call(tool)) {
            return tool;
        }

    } catch (const std::exception& e) {
        log_debug("parse_tool_call_from_json", "Parse failed: " + std::string(e.what()));
    }

    return std::nullopt;
}

std::optional<ToolCall> ToolCallExtractorPlugin::parse_tool_call_from_xml(const std::string& xml_element) const {
    // Simple XML parsing - for now, just create a basic tool call
    ToolCall tool;
    tool.name = "xml_tool_call";
    tool.id = generate_call_id();
    tool.parameters = nlohmann::json{{"raw_content", xml_element}};
    tool.status = "pending";
    tool.timestamp = std::chrono::system_clock::now();

    if (validate_tool_call(tool)) {
        return tool;
    }

    return std::nullopt;
}

bool ToolCallExtractorPlugin::validate_tool_call(const ToolCall& tool_call) const {
    // Check tool name
    if (!is_valid_tool_name(tool_call.name)) {
        return false;
    }

    // Check content size
    if (tool_call.name.length() > 100) {
        return false;
    }

    // Check for malicious content
    if (config_.enable_security_validation) {
        std::string content_to_check = tool_call.name + tool_call.parameters.dump();
        if (contains_malicious_patterns(content_to_check)) {
            return false;
        }
    }

    return true;
}

bool ToolCallExtractorPlugin::contains_malicious_patterns(const std::string& content) const {
    std::vector<std::regex> malicious_patterns = {
        std::regex(R"(<script[^>]*>)", std::regex::icase),
        std::regex(R"(javascript\s*:)", std::regex::icase),
        std::regex(R"(eval\s*\()", std::regex::icase),
        std::regex(R"(system\s*\()", std::regex::icase),
        std::regex(R"(exec\s*\()", std::regex::icase)
    };

    for (const auto& pattern : malicious_patterns) {
        if (std::regex_search(content, pattern)) {
            return true;
        }
    }

    return false;
}

bool ToolCallExtractorPlugin::is_valid_tool_name(const std::string& tool_name) const {
    if (tool_name.empty() || tool_name.length() > 100) {
        return false;
    }

    // Check against allowlist
    for (const auto& allowed_name : config_.allowed_tool_names) {
        if (tool_name == allowed_name || tool_name.find(allowed_name) != std::string::npos) {
            return true;
        }
    }

    // Allow any alphanumeric name if not on strict allowlist
    return std::all_of(tool_name.begin(), tool_name.end(),
                      [](char c) { return std::isalnum(c) || c == '_' || c == '-'; });
}

std::string ToolCallExtractorPlugin::sanitize_tool_arguments(const std::string& arguments) const {
    std::string sanitized = arguments;

    // Remove or escape dangerous elements
    sanitized = std::regex_replace(sanitized,
        std::regex(R"(<script[^>]*>)", std::regex::icase), "&lt;script&gt;");
    sanitized = std::regex_replace(sanitized,
        std::regex(R"(</script>)", std::regex::icase), "&lt;/script&gt;");

    return sanitized;
}

std::vector<ToolCall> ToolCallExtractorPlugin::recover_from_json_error(const std::string& content) const {
    std::vector<ToolCall> tool_calls;

    // Simple pattern matching for common tool call formats
    std::regex simple_regex(R"(\{\s*"name"\s*:\s*"[^"]*"\s*,\s*"arguments"\s*:\s*"[^"]*"\s*\})");
    std::sregex_iterator iter(content.begin(), content.end(), simple_regex);
    std::sregex_iterator end;

    for (; iter != end && tool_calls.size() < config_.max_tool_calls; ++iter) {
        std::smatch match = *iter;
        if (match.size() >= 1) {
            ToolCall tool;
            tool.name = "recovered_tool";
            tool.id = generate_call_id();
            tool.parameters = nlohmann::json{{"raw_content", match[0].str()}};
            tool.status = "pending";

            if (validate_tool_call(tool)) {
                tool_calls.push_back(tool);
            }
        }
    }

    return tool_calls;
}

std::vector<ToolCall> ToolCallExtractorPlugin::recover_from_xml_error(const std::string& content) const {
    std::vector<ToolCall> tool_calls;

    // Simple XML-like pattern matching
    std::regex xml_regex(R"(<invoke.*?>.*?</invoke>)");
    std::sregex_iterator iter(content.begin(), content.end(), xml_regex);
    std::sregex_iterator end;

    for (; iter != end && tool_calls.size() < config_.max_tool_calls; ++iter) {
        std::smatch match = *iter;
        if (match.size() >= 1) {
            ToolCall tool;
            tool.name = "recovered_xml_tool";
            tool.id = generate_call_id();
            tool.parameters = nlohmann::json{{"raw_content", match[0].str()}};
            tool.status = "pending";

            if (validate_tool_call(tool)) {
                tool_calls.push_back(tool);
            }
        }
    }

    return tool_calls;
}

void ToolCallExtractorPlugin::initialize_patterns() {
    std::lock_guard<std::mutex> lock(patterns_mutex_);

    provider_patterns_["cerebras"] = ProviderToolPatterns::get_cerebras_patterns();
    provider_patterns_["openai"] = ProviderToolPatterns::get_openai_patterns();
    provider_patterns_["anthropic"] = ProviderToolPatterns::get_anthropic_patterns();
    provider_patterns_["synthetic"] = ProviderToolPatterns::get_synthetic_patterns();
    provider_patterns_["common"] = ProviderToolPatterns::get_common_patterns();
    common_patterns_ = ProviderToolPatterns::get_common_patterns();

    log_debug("initialize_patterns", "Pattern initialization completed");
}

std::vector<std::regex> ToolCallExtractorPlugin::get_provider_patterns(const std::string& provider) const {
    std::lock_guard<std::mutex> lock(patterns_mutex_);

    auto it = provider_patterns_.find(provider);
    if (it != provider_patterns_.end()) {
        return it->second;
    }
    return common_patterns_;
}

void ToolCallExtractorPlugin::update_stats(std::chrono::microseconds duration, bool success, size_t tools_extracted) {
    uint64_t duration_us = duration.count();

    // Update max time
    uint64_t current_max = stats_.max_time_us.load();
    while (duration_us > current_max &&
           !stats_.max_time_us.compare_exchange_weak(current_max, duration_us)) {
        // Retry if another thread updated max_time_us
    }

    // Update average time (simple moving average)
    uint64_t total = stats_.total_extractions.load();
    if (total > 0) {
        uint64_t current_avg = stats_.average_time_us.load();
        uint64_t new_avg = ((current_avg * (total - 1)) + duration_us) / total;
        stats_.average_time_us = new_avg;
    }

    stats_.tools_extracted += tools_extracted;
}

bool ToolCallExtractorPlugin::check_content_limits(const std::string& content) const {
    return content.length() <= config_.max_content_size;
}

void ToolCallExtractorPlugin::reset_streaming_state() {
    streaming_buffer_.clear();
    streaming_active_ = false;
    current_provider_.clear();
}

std::string ToolCallExtractorPlugin::process_streaming_extraction(const std::string& chunk, bool is_final) {
    streaming_buffer_ += chunk;

    if (is_final) {
        // Extract tool calls from complete buffer
        std::vector<ToolCall> tool_calls = extract_tool_calls(streaming_buffer_, current_provider_);
        return serialize_tool_calls_to_toon(tool_calls);
    } else {
        // For partial chunks, just return as-is
        return chunk;
    }
}

std::string ToolCallExtractorPlugin::generate_call_id() const {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(100000, 999999);

    return "tc_" + std::to_string(dis(gen));
}

std::string ToolCallExtractorPlugin::serialize_tool_calls_to_toon(const std::vector<ToolCall>& tool_calls) const {
    if (tool_calls.empty()) {
        return "// No tool calls extracted\n";
    }

    std::stringstream ss;
    ss << "// Tool Calls Extraction Results\n";
    ss << "// Count: " << tool_calls.size() << "\n\n";

    for (size_t i = 0; i < tool_calls.size(); ++i) {
        const auto& call = tool_calls[i];
        ss << "[tool_call_" << (i + 1) << "]\n";
        ss << "id: " << call.id << "\n";
        ss << "name: " << call.name << "\n";
        ss << "status: " << call.status << "\n";
        ss << "parameters: " << call.parameters.dump() << "\n";
        if (i < tool_calls.size() - 1) {
            ss << "\n";
        }
    }

    return ss.str();
}

void ToolCallExtractorPlugin::log_debug(const std::string& operation, const std::string& message) const {
    LOG_DEBUG("%s - %s", operation.c_str(), message.c_str());
}

void ToolCallExtractorPlugin::log_error(const std::string& operation, const std::string& message) const {
    LOG_ERROR("%s - %s", operation.c_str(), message.c_str());
}

} // namespace prettifier
} // namespace aimux