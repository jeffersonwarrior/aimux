#include "aimux/prettifier/prettifier_plugin.hpp"
#include <regex>
#include <sstream>

// TODO: Replace with proper logging when available
#define LOG_ERROR(msg, ...) do { printf("[ERROR] " msg "\n", ##__VA_ARGS__); } while(0)

namespace aimux {
namespace prettifier {

// ToolCall implementation

nlohmann::json ToolCall::to_json() const {
    nlohmann::json j;
    j["name"] = name;
    j["id"] = id;
    j["parameters"] = parameters;
    if (result) {
        j["result"] = *result;
    }
    j["status"] = status;
    j["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
        timestamp.time_since_epoch()).count();
    return j;
}

ToolCall ToolCall::from_json(const nlohmann::json& j) {
    ToolCall call;
    if (j.contains("name")) j.at("name").get_to(call.name);
    if (j.contains("id")) j.at("id").get_to(call.id);
    if (j.contains("parameters")) j.at("parameters").get_to(call.parameters);
    if (j.contains("result")) {
        call.result = j.at("result");
    }
    if (j.contains("status")) j.at("status").get_to(call.status);
    if (j.contains("timestamp")) {
        call.timestamp = std::chrono::system_clock::from_time_t(
            j.at("timestamp").get<std::time_t>());
    }
    return call;
}

// ProcessingContext implementation

nlohmann::json ProcessingContext::to_json() const {
    nlohmann::json j;
    j["provider_name"] = provider_name;
    j["model_name"] = model_name;
    j["original_format"] = original_format;
    j["requested_formats"] = requested_formats;
    j["streaming_mode"] = streaming_mode;
    if (provider_config) {
        j["provider_config"] = *provider_config;
    }
    j["processing_start"] = std::chrono::duration_cast<std::chrono::seconds>(
        processing_start.time_since_epoch()).count();
    return j;
}

// ProcessingResult implementation

nlohmann::json ProcessingResult::to_json() const {
    nlohmann::json j;
    j["success"] = success;
    j["processed_content"] = processed_content;
    j["output_format"] = output_format;

    nlohmann::json tools_json = nlohmann::json::array();
    for (const auto& tool : extracted_tool_calls) {
        tools_json.push_back(tool.to_json());
    }
    j["extracted_tool_calls"] = tools_json;

    if (reasoning) {
        j["reasoning"] = *reasoning;
    }
    j["processing_time_ms"] = processing_time.count();
    j["tokens_processed"] = tokens_processed;
    j["error_message"] = error_message;
    j["metadata"] = metadata;
    j["streaming_mode"] = streaming_mode;
    return j;
}

// PrettifierPlugin utility method implementations

std::vector<ToolCall> PrettifierPlugin::extract_tool_calls(const std::string& content) const {
    std::vector<ToolCall> tool_calls;

    // Common patterns for tool calls across different providers

    // Pattern 1: XML-style function calls (Claude format)
    static const std::regex xml_pattern(
        R"(<function_calls>\s*(\{[^<]*\})\s*</function_calls>)");

    // Pattern 2: JSON in code blocks (OpenAI format)
    static const std::regex json_code_pattern(
        R"(```(?:json)?\s*(\{[^`]*\})\s*```)");

    // Pattern 3: Direct JSON objects (non-recursive for C++ regex compatibility)
    static const std::regex direct_json_pattern(
        R"(\{[^{}]*\})");

    try {
        std::smatch match;
        std::string search_content = content;

        // Try XML pattern first
        while (std::regex_search(search_content, match, xml_pattern)) {
            std::string json_str = match[1].str();
            auto json_opt = validate_json(json_str);
            if (json_opt) {
                ToolCall call;
                call.status = "pending";
                call.timestamp = std::chrono::system_clock::now();

                if (json_opt->contains("name")) {
                    call.name = json_opt->at("name").get<std::string>();
                }
                if (json_opt->contains("arguments")) {
                    call.parameters = json_opt->at("arguments");
                }
                if (json_opt->contains("id")) {
                    call.id = json_opt->at("id").get<std::string>();
                }

                tool_calls.push_back(call);
            }

            search_content = match.suffix();
        }

        // Try JSON code block pattern
        search_content = content;
        while (std::regex_search(search_content, match, json_code_pattern)) {
            std::string json_str = match[1].str();
            auto json_opt = validate_json(json_str);
            if (json_opt) {
                ToolCall call;
                call.status = "pending";
                call.timestamp = std::chrono::system_clock::now();

                if (json_opt->contains("function")) {
                    if (json_opt->at("function").contains("name")) {
                        call.name = json_opt->at("function")["name"];
                    }
                    if (json_opt->at("function").contains("arguments")) {
                        call.parameters = json_opt->at("function")["arguments"];
                    }
                }
                if (json_opt->contains("id")) {
                    call.id = json_opt->at("id").get<std::string>();
                }

                tool_calls.push_back(call);
            }

            search_content = match.suffix();
        }

        // Try direct JSON pattern (be more selective to avoid false positives)
        // Temporarily disabled due to regex issues - will re-enable later
        /*
        static const std::regex tool_json_pattern(
            R"(\[\s*\{[^}]*"function"[^}]*\}\s*\])");

        if (std::regex_search(content, match, tool_json_pattern)) {
            std::string json_str = match[0].str();
            auto json_opt = validate_json(json_str);
            if (json_opt && json_opt->is_array()) {
                for (const auto& tool_json : *json_opt) {
                    ToolCall call;
                    call.status = "pending";
                    call.timestamp = std::chrono::system_clock::now();

                    if (tool_json.contains("function")) {
                        if (tool_json["function"].contains("name")) {
                            call.name = tool_json["function"]["name"];
                        }
                        if (tool_json["function"].contains("arguments")) {
                            call.parameters = tool_json["function"]["arguments"];
                        }
                    }
                    if (tool_json.contains("id")) {
                        call.id = tool_json["id"];
                    }

                    tool_calls.push_back(call);
                }
            }
        }
        */

    } catch (const std::exception& e) {
        LOG_ERROR("Tool call extraction error: %s", e.what());
    }

    return tool_calls;
}

std::optional<nlohmann::json> PrettifierPlugin::validate_json(const std::string& content) const {
    try {
        // Try to parse as JSON
        auto json = nlohmann::json::parse(content);
        return json;
    } catch (const nlohmann::json::parse_error& e) {
        // Try to repair common JSON issues
        try {
            std::string repaired = content;

            // Simple character-by-character approach for trailing comma removal
            for (size_t i = 0; i < repaired.length() - 1; ++i) {
                if (repaired[i] == ',' &&
                    (repaired[i+1] == '}' || repaired[i+1] == ']')) {
                    // Remove the comma
                    repaired.erase(i, 1);
                    break; // Only fix the first occurrence to be safe
                }
            }

            // Remove whitespace before closing braces/brackets that was after commas
            static const std::regex whitespace_before_closing(R"(\s*([}\]])\s*$)");
            repaired = std::regex_replace(repaired, whitespace_before_closing, "$1");

            // Try parsing again
            auto json = nlohmann::json::parse(repaired);
            return json;
        } catch (...) {
            // If repair fails, return empty
            return std::nullopt;
        }
    }
}

} // namespace prettifier
} // namespace aimux