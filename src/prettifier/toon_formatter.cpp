#include "aimux/prettifier/toon_formatter.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <regex>
#include <chrono>
#include <ctime>
#include <set>

// TODO: Replace with proper logging when available
#define LOG_DEBUG(msg, ...) do { printf("[DEBUG] " msg "\n", ##__VA_ARGS__); } while(0)
#define LOG_ERROR(msg, ...) do { printf("[ERROR] " msg "\n", ##__VA_ARGS__); } while(0)

namespace aimux {
namespace prettifier {

// ToonParseResult implementation
nlohmann::json ToonParseResult::to_json() const {
    nlohmann::json j;
    j["success"] = success;
    j["data"] = data;
    j["metadata"] = metadata;
    j["content"] = content;

    nlohmann::json tools_json = nlohmann::json::array();
    for (const auto& tool : tools) {
        tools_json.push_back(tool.to_json());
    }
    j["tools"] = tools_json;

    j["thinking"] = thinking;
    j["error_message"] = error_message;
    j["parse_time_ms"] = parse_time.count();
    return j;
}

// ToonFormatter implementation
ToonFormatter::ToonFormatter() : config_(Config{}) {}

ToonFormatter::ToonFormatter(const Config& config) : config_(config) {}

std::string ToonFormatter::serialize_response(
    const core::Response& response,
    const ProcessingContext& context,
    const std::vector<ToolCall>& tool_calls,
    const std::string& thinking) {

    auto start_time = std::chrono::high_resolution_clock::now();

    std::ostringstream toon_stream;

    // META section
    if (config_.include_metadata) {
        nlohmann::json metadata = nlohmann::json::object();
        metadata["provider"] = context.provider_name;
        metadata["model"] = context.model_name;
        metadata["original_format"] = context.original_format;
        // Note: tokens_used field not available in current Response structure
        // metadata["tokens_used"] = response.tokens_used;
        metadata["response_time_ms"] = response.response_time_ms;
        metadata["success"] = response.success;
        metadata["status_code"] = response.status_code;
        metadata["timestamp"] = generate_timestamp();
        metadata["streaming_mode"] = context.streaming_mode;

        toon_stream << create_meta_section(metadata);
    }

    // CONTENT section
    std::string content_type = "text";
    if (context.original_format == "json") content_type = "json";
    else if (context.original_format == "markdown") content_type = "markdown";

    toon_stream << create_content_section(response.data, content_type, response.success ? "complete" : "error");

    // TOOLS section
    if (config_.include_tools && !tool_calls.empty()) {
        toon_stream << create_tools_section(tool_calls);
    }

    // THINKING section
    if (config_.include_thinking && !thinking.empty()) {
        toon_stream << create_thinking_section(thinking);
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

    LOG_DEBUG("Serialized response in %ldus, size: %zu bytes",
             duration.count(), toon_stream.str().length());

    return toon_stream.str();
}

std::string ToonFormatter::serialize_data(
    const nlohmann::json& data,
    const std::map<std::string, std::string>& metadata) {

    std::ostringstream toon_stream;

    // META section from provided metadata
    if (!metadata.empty() && config_.include_metadata) {
        nlohmann::json meta_json = nlohmann::json::object();
        for (const auto& [key, value] : metadata) {
            meta_json[key] = value;
        }
        meta_json["timestamp"] = generate_timestamp();
        toon_stream << create_meta_section(meta_json);
    }

    // CONTENT section with JSON data
    std::string json_str = data.dump();
    toon_stream << create_content_section(json_str, "json", "complete");

    return toon_stream.str();
}

std::optional<nlohmann::json> ToonFormatter::deserialize_toon(const std::string& toon_content) {
    auto start_time = std::chrono::high_resolution_clock::now();

    try {
        std::string error_msg;
        if (!validate_toon(toon_content, error_msg)) {
            LOG_ERROR("Validation failed: %s", error_msg.c_str());
            return std::nullopt;
        }

        nlohmann::json result = nlohmann::json::object();
        auto sections = parse_sections(toon_content);

        for (const auto& [name, content] : sections) {
            if (name == "META") {
                result["metadata"] = parse_meta_section(content);
            } else if (name == "CONTENT") {
                result["content"] = parse_content_section(content);
            } else if (name == "TOOLS") {
                result["tools"] = parse_tools_section(content);
            } else if (name == "THINKING") {
                result["thinking"] = parse_thinking_section(content);
            }
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

        LOG_DEBUG("Deserialized TOON in %ldus", duration.count());

        return result;

    } catch (const std::exception& e) {
        LOG_ERROR("Deserialization error: %s", e.what());
        return std::nullopt;
    }
}

std::optional<std::string> ToonFormatter::extract_section(
    const std::string& toon_content,
    const std::string& section_name) {

    auto sections = parse_sections(toon_content);
    auto it = std::find_if(sections.begin(), sections.end(),
        [&section_name](const auto& pair) { return pair.first == section_name; });

    if (it != sections.end()) {
        return it->second;
    }

    return std::nullopt;
}

bool ToonFormatter::validate_toon(const std::string& toon_content, std::string& error_message) {
    try {
        // Check for basic structure
        static const std::regex section_regex(R"(^# ([A-Z]+)\s*$)", std::regex::multiline);

        std::sregex_iterator iter(toon_content.begin(), toon_content.end(), section_regex);
        std::sregex_iterator end;

        if (iter == end) {
            error_message = "No valid TOON sections found";
            return false;
        }

        // Validate section names
        std::set<std::string> found_sections;
        for (auto it = iter; it != end; ++it) {
            std::string section_name = (*it)[1].str();
            if (!is_valid_section_name(section_name)) {
                error_message = "Invalid section name: " + section_name;
                return false;
            }
            found_sections.insert(section_name);
        }

        // Check for required sections
        if (found_sections.find("CONTENT") == found_sections.end()) {
            error_message = "Missing required CONTENT section";
            return false;
        }

        return true;

    } catch (const std::exception& e) {
        error_message = std::string("Validation error: ") + e.what();
        return false;
    }
}

nlohmann::json ToonFormatter::analyze_toon(const std::string& toon_content) {
    nlohmann::json analysis = nlohmann::json::object();

    analysis["total_size_bytes"] = toon_content.length();
    analysis["line_count"] = std::count(toon_content.begin(), toon_content.end(), '\n') + 1;
    analysis["estimated_overhead"] = estimate_size(toon_content);

    auto sections = parse_sections(toon_content);
    analysis["sections"] = nlohmann::json::array();
    analysis["section_count"] = sections.size();

    for (const auto& [name, content] : sections) {
        nlohmann::json section_info;
        section_info["name"] = name;
        section_info["size_bytes"] = content.length();
        section_info["line_count"] = std::count(content.begin(), content.end(), '\n') + 1;
        analysis["sections"].push_back(section_info);
    }

    return analysis;
}

std::string ToonFormatter::json_to_toon(const nlohmann::json& json_data, int indent) {
    std::ostringstream stream;

    if (json_data.is_object()) {
        for (const auto& [key, value] : json_data.items()) {
            std::string indent_str(static_cast<size_t>(indent) * 2, ' ');
            stream << indent_str << key << ": " << format_json_value(value) << "\n";
        }
    } else if (json_data.is_array()) {
        for (size_t i = 0; i < json_data.size(); ++i) {
            std::string indent_str(static_cast<size_t>(indent) * 2, ' ');
            stream << indent_str << "[" << i << "]: " << format_json_value(json_data[i]) << "\n";
        }
    } else {
        stream << format_json_value(json_data) << "\n";
    }

    return stream.str();
}

std::string ToonFormatter::escape_toon_content(const std::string& input) {
    std::string escaped = input;

    // Escape section headers that could interfere
    // Match headers at beginning of line or after newline
    static const std::regex header_regex(R"((^|\n)# )");
    escaped = std::regex_replace(escaped, header_regex, "$1\\# ");

    return escaped;
}

std::string ToonFormatter::unescape_toon_content(const std::string& input) {
    std::string unescaped = input;

    // Unescape escaped headers
    static const std::regex escaped_header_regex(R"((^|\n)\\# )");
    unescaped = std::regex_replace(unescaped, escaped_header_regex, "$1# ");

    return unescaped;
}

std::string ToonFormatter::create_meta_section(const nlohmann::json& metadata) {
    std::ostringstream stream;
    stream << "# META\n";

    for (const auto& [key, value] : metadata.items()) {
        stream << key << ": " << format_json_value(value) << "\n";
    }
    stream << "\n";

    return stream.str();
}

std::string ToonFormatter::create_content_section(
    const std::string& content,
    const std::string& type,
    const std::string& format) {

    std::ostringstream stream;
    stream << "# CONTENT\n";

    std::string escaped_content = escape_toon_content(content);
    stream << "[TYPE: " << type << "]\n";

    if (!format.empty()) {
        stream << "[FORMAT: " << format << "]\n";
    }

    if (escaped_content.length() < 1000 || !config_.enable_compression) {
        stream << "[CONTENT: " << escaped_content << "]\n";
    } else {
        // For large content, write as separate block
        stream << "[CONTENT_SIZE: " << escaped_content.length() << " bytes]\n";
        stream << escaped_content << "\n";
    }
    stream << "\n";

    return stream.str();
}

std::string ToonFormatter::create_tools_section(const std::vector<ToolCall>& tool_calls) {
    std::ostringstream stream;
    stream << "# TOOLS\n";

    for (const auto& tool : tool_calls) {
        stream << "[CALL: " << tool.name << "]\n";
        stream << "[PARAM: " << tool.parameters.dump() << "]\n";
        stream << "[STATUS: " << tool.status << "]\n";
        if (tool.result) {
            stream << "[RESULT: " << tool.result->dump() << "]\n";
        }
        stream << "\n";
    }

    return stream.str();
}

std::string ToonFormatter::create_thinking_section(const std::string& reasoning) {
    std::ostringstream stream;
    stream << "# THINKING\n";
    stream << "[REASONING: " << escape_toon_content(reasoning) << "]\n\n";
    return stream.str();
}

void ToonFormatter::update_config(const Config& new_config) {
    config_ = new_config;
}

// Private helper methods
std::vector<std::pair<std::string, std::string>> ToonFormatter::parse_sections(
    const std::string& toon_content) {

    std::vector<std::pair<std::string, std::string>> sections;

    static const std::regex section_regex(R"(# ([A-Z]+)\s*$)", std::regex::multiline);

    std::sregex_token_iterator iter(toon_content.begin(), toon_content.end(),
                                   section_regex, {-1, 1});
    std::sregex_token_iterator end;

    std::string current_section;
    std::string current_content;

    while (iter != end) {
        // First token is the content before the match
        if (iter != end) {
            current_content += *iter++;
        }

        // Second token is the captured group (section name)
        if (iter != end) {
            if (!current_section.empty()) {
                sections.emplace_back(current_section, current_content);
            }
            current_section = *iter++;
            current_content = "";
        }
    }

    // Add the last section
    if (!current_section.empty() && !current_content.empty()) {
        sections.emplace_back(current_section, current_content);
    }

    return sections;
}

nlohmann::json ToonFormatter::parse_meta_section(const std::string& content) {
    nlohmann::json metadata = nlohmann::json::object();

    std::istringstream stream(content);
    std::string line;

    while (std::getline(stream, line)) {
        line.erase(line.find_last_not_of(" \t\r\n") + 1); // Trim whitespace

        if (line.empty()) continue;

        size_t colon_pos = line.find(':');
        if (colon_pos != std::string::npos) {
            std::string key = line.substr(0, colon_pos);
            std::string value = line.substr(colon_pos + 1);
            value.erase(0, value.find_first_not_of(" \t")); // Trim leading whitespace

            metadata[key] = value;
        }
    }

    return metadata;
}

nlohmann::json ToonFormatter::parse_content_section(const std::string& content) {
    nlohmann::json parsed = nlohmann::json::object();

    try {
        static const std::regex type_regex(R"(^\[TYPE:\s*([^\]]+)\])");
        static const std::regex format_regex(R"(^\[FORMAT:\s*([^\]]+)\])");
        static const std::regex content_regex(R"(^\[CONTENT:\s*(.*)\])");
        static const std::regex content_size_regex(R"(^\[CONTENT_SIZE:\s*([^\]]+)\])");

        bool in_content_block = false;
        std::string accumulated_content;

        std::istringstream stream(content);
        std::string line;

        while (std::getline(stream, line)) {
            line.erase(line.find_last_not_of(" \t\r\n") + 1);

            if (line.empty()) continue;

            std::smatch match;
            if (std::regex_match(line, match, type_regex)) {
                parsed["type"] = match[1].str();
            } else if (std::regex_match(line, match, format_regex)) {
                parsed["format"] = match[1].str();
            } else if (std::regex_match(line, match, content_size_regex)) {
                in_content_block = true;
                parsed["content_size"] = match[1].str();
            } else if (std::regex_match(line, match, content_regex)) {
                parsed["content"] = match[1].str();
            } else if (in_content_block) {
                if (!accumulated_content.empty()) accumulated_content += "\n";
                accumulated_content += line;
                parsed["content"] = accumulated_content;
            }
        }

    } catch (const std::exception& e) {
        LOG_ERROR("Error parsing content section: %s", e.what());
        parsed["error"] = e.what();
    }

    return parsed;
}

nlohmann::json ToonFormatter::parse_tools_section(const std::string& content) {
    nlohmann::json tools = nlohmann::json::array();

    static const std::regex call_regex(R"(^\[CALL:\s*([^\]]+)\])");
    static const std::regex params_regex(R"(^\[PARAM:\s*([^\]]+)\])");
    static const std::regex status_regex(R"(^\[STATUS:\s*([^\]]+)\])");
    static const std::regex result_regex(R"(^\[RESULT:\s*([^\]]+)\])");

    std::istringstream stream(content);
    std::string line;

    nlohmann::json current_tool;

    while (std::getline(stream, line)) {
        line.erase(line.find_last_not_of(" \t\r\n") + 1);
        if (line.empty()) continue;

        std::smatch match;
        if (std::regex_match(line, match, call_regex)) {
            if (!current_tool.empty()) {
                tools.push_back(current_tool);
                current_tool = nlohmann::json::object();
            }
            current_tool["name"] = match[1].str();
        } else if (std::regex_match(line, match, params_regex)) {
            try {
                current_tool["parameters"] = nlohmann::json::parse(match[1].str());
            } catch (const std::exception&) {
                current_tool["parameters_raw"] = match[1].str();
            }
        } else if (std::regex_match(line, match, status_regex)) {
            current_tool["status"] = match[1].str();
        } else if (std::regex_match(line, match, result_regex)) {
            try {
                current_tool["result"] = nlohmann::json::parse(match[1].str());
            } catch (const std::exception&) {
                current_tool["result_raw"] = match[1].str();
            }
        }
    }

    // Add the last tool if exists
    if (!current_tool.empty()) {
        tools.push_back(current_tool);
    }

    return tools;
}

std::string ToonFormatter::parse_thinking_section(const std::string& content) {
    static const std::regex reasoning_regex(R"(^\[REASONING:\s*(.*)\])");

    std::istringstream stream(content);
    std::string line;

    while (std::getline(stream, line)) {
        line.erase(line.find_last_not_of(" \t\r\n") + 1);

        std::smatch match;
        if (std::regex_match(line, match, reasoning_regex)) {
            return unescape_toon_content(match[1].str());
        }
    }

    return "";
}

bool ToonFormatter::is_valid_section_name(const std::string& name) {
    static const std::set<std::string> valid_sections = {
        "META", "CONTENT", "TOOLS", "THINKING"
    };

    return valid_sections.find(name) != valid_sections.end();
}

bool ToonFormatter::is_valid_meta_line(const std::string& line) {
    size_t colon_pos = line.find(':');
    return colon_pos != std::string::npos && colon_pos > 0 && colon_pos < line.length() - 1;
}

bool ToonFormatter::is_valid_content_tag(const std::string& tag) {
    static const std::set<std::string> valid_tags = {
        "TYPE", "FORMAT", "CONTENT", "CONTENT_SIZE"
    };
    return valid_tags.find(tag) != valid_tags.end();
}

std::string ToonFormatter::generate_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

std::string ToonFormatter::format_json_value(const nlohmann::json& value) {
    if (value.is_string()) {
        return value.get<std::string>();
    } else if (value.is_number() || value.is_boolean()) {
        return value.dump();
    } else if (value.is_null()) {
        return "null";
    } else {
        return value.dump();
    }
}

std::string ToonFormatter::indent_string(const std::string& input, const std::string& indent) {
    std::ostringstream result;
    std::istringstream stream(input);
    std::string line;

    bool first = true;
    while (std::getline(stream, line)) {
        if (!first) result << "\n";
        result << indent << line;
        first = false;
    }

    return result.str();
}

size_t ToonFormatter::estimate_size(const std::string& content) {
    return content.length(); // Simple estimation
}

void ToonFormatter::log_error(const std::string& operation, const std::string& message) {
    LOG_ERROR("ToonFormatter:%s - %s", operation.c_str(), message.c_str());
}

void ToonFormatter::log_debug(const std::string& operation, const std::string& message) {
    LOG_DEBUG("ToonFormatter:%s - %s", operation.c_str(), message.c_str());
}

} // namespace prettifier
} // namespace aimux