#include "aimux/gateway/format_detector.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <regex>

namespace aimux {
namespace gateway {

FormatDetector::FormatDetector(const FormatDetectionConfig& config)
    : config_(config) {}

DetectionResult FormatDetector::detect_format(const crow::request& req, const std::string& body) {
    // Extract headers
    std::map<std::string, std::string> headers;
    for (const auto& header : req.headers) {
        headers[to_lower(header.first)] = header.second;
    }

    // Get endpoint
    std::string endpoint = req.url;

    // Parse JSON body if possible
    nlohmann::json json_data;

    try {
        if (!body.empty() && (headers["content-type"].find("application/json") != std::string::npos ||
                            headers["content-type"].empty())) {
            json_data = nlohmann::json::parse(body);
        }
    } catch (const nlohmann::json::exception&) {
        // JSON parsing failed, continue with other detection methods
    }

    return detect_format(json_data, headers, endpoint);
}

DetectionResult FormatDetector::detect_format(
    const nlohmann::json& json_data,
    const std::map<std::string, std::string>& headers,
    const std::string& endpoint) {

    std::vector<DetectionResult> results;

    // Detect from endpoint (highest weight)
    if (!endpoint.empty()) {
        results.push_back(detect_from_endpoint(endpoint));
    }

    // Detect from headers (high weight)
    results.push_back(detect_from_headers(headers));

    // Detect from JSON body if available
    if (!json_data.is_null()) {
        // Detect from model name (medium-high weight)
        results.push_back(detect_from_model(json_data));

        // Detect from message structure (medium weight)
        results.push_back(detect_from_message_structure(json_data));

        // General body detection (lower weight)
        results.push_back(detect_from_body(json_data));
    }

    return combine_results(results);
}

APIFormat FormatDetector::detect_format_quick(
    const std::string& endpoint,
    const std::map<std::string, std::string>& headers) {

    // Quick endpoint detection
    if (!endpoint.empty()) {
        auto endpoint_result = detect_from_endpoint(endpoint);
        if (endpoint_result.is_reliable(0.8)) {
            return endpoint_result.format;
        }
    }

    // Quick header detection
    auto header_result = detect_from_headers(headers);
    if (header_result.is_reliable(0.8)) {
        return header_result.format;
    }

    return APIFormat::UNKNOWN;
}

DetectionResult FormatDetector::detect_from_endpoint(const std::string& endpoint) {
    DetectionResult result;
    std::string lower_endpoint = to_lower(endpoint);

    // Check Anthropic endpoints
    for (const auto& anthropic_endpoint : config_.anthropic_endpoints) {
        if (ends_with(lower_endpoint, anthropic_endpoint) ||
            lower_endpoint.find(anthropic_endpoint) != std::string::npos) {
            result.format = APIFormat::ANTHROPIC;
            result.confidence = 0.9;
            result.reasoning = "Endpoint matches Anthropic pattern: " + anthropic_endpoint;
            return result;
        }
    }

    // Check OpenAI endpoints
    for (const auto& openai_endpoint : config_.openai_endpoints) {
        if (ends_with(lower_endpoint, openai_endpoint) ||
            lower_endpoint.find(openai_endpoint) != std::string::npos) {
            result.format = APIFormat::OPENAI;
            result.confidence = 0.9;
            result.reasoning = "Endpoint matches OpenAI pattern: " + openai_endpoint;
            return result;
        }
    }

    result.reasoning = "No endpoint pattern matched";
    return result;
}

DetectionResult FormatDetector::detect_from_headers(const std::map<std::string, std::string>& headers) {
    DetectionResult result;

    // Check for Anthropic headers
    bool has_anthropic_headers = has_header_pattern(headers, config_.anthropic_headers);

    // Check for OpenAI headers
    bool has_openai_headers = has_header_pattern(headers, config_.openai_headers);

    if (has_anthropic_headers && !has_openai_headers) {
        result.format = APIFormat::ANTHROPIC;
        result.confidence = 0.8;
        result.reasoning = "Found Anthropic-specific headers";
    } else if (has_openai_headers && !has_anthropic_headers) {
        result.format = APIFormat::OPENAI;
        result.confidence = 0.8;
        result.reasoning = "Found OpenAI-specific headers";
    } else if (has_anthropic_headers && has_openai_headers) {
        // Both detected - need more context, lower confidence
        result.format = APIFormat::UNKNOWN;
        result.confidence = 0.3;
        result.reasoning = "Conflicting headers found - both Anthropic and OpenAI patterns detected";
    } else {
        result.reasoning = "No format-specific headers detected";
    }

    return result;
}

DetectionResult FormatDetector::detect_from_body(const nlohmann::json& json_data) {
    DetectionResult result;

    // Look for Anthropic-specific fields
    std::vector<std::string> anthropic_fields = {"max_tokens", "temperature", "top_p", "top_k"};
    std::vector<std::string> openai_fields = {"max_tokens", "temperature", "top_p", "frequency_penalty", "presence_penalty"};

    int anthropic_score = 0;
    int openai_score = 0;

    for (const auto& field : anthropic_fields) {
        if (json_data.contains(field)) anthropic_score++;
    }

    for (const auto& field : openai_fields) {
        if (json_data.contains(field)) openai_score++;
    }

    // Check for unique fields
    if (json_data.contains("top_k")) {
        anthropic_score += 2; // top_k is more unique to Anthropic
    }

    if (json_data.contains("frequency_penalty") || json_data.contains("presence_penalty")) {
        openai_score += 2; // These are OpenAI-specific
    }

    if (anthropic_score > openai_score) {
        result.format = APIFormat::ANTHROPIC;
        result.confidence = std::min(0.6, 0.1 + (anthropic_score * 0.1));
        result.reasoning = "Body contains Anthropic-specific fields";
    } else if (openai_score > anthropic_score) {
        result.format = APIFormat::OPENAI;
        result.confidence = std::min(0.6, 0.1 + (openai_score * 0.1));
        result.reasoning = "Body contains OpenAI-specific fields";
    } else {
        result.reasoning = "Body format analysis inconclusive";
    }

    return result;
}

DetectionResult FormatDetector::detect_from_model(const nlohmann::json& json_data) {
    DetectionResult result;

    // Check both "model" fields
    std::vector<std::string> model_fields = {"model"};
    for (const auto& field : model_fields) {
        if (json_data.contains(field) && json_data[field].is_string()) {
            std::string model = to_lower(json_data[field].get<std::string>());

            if (matches_model_pattern(model, config_.anthropic_model_patterns)) {
                result.format = APIFormat::ANTHROPIC;
                result.confidence = 0.7;
                result.reasoning = "Model matches Anthropic pattern: " + model;
                return result;
            }

            if (matches_model_pattern(model, config_.openai_model_patterns)) {
                result.format = APIFormat::OPENAI;
                result.confidence = 0.7;
                result.reasoning = "Model matches OpenAI pattern: " + model;
                return result;
            }
        }
    }

    result.reasoning = "No recognizable model pattern found";
    return result;
}

DetectionResult FormatDetector::detect_from_message_structure(const nlohmann::json& json_data) {
    DetectionResult result;

    if (has_anthropic_message_structure(json_data)) {
        result.format = APIFormat::ANTHROPIC;
        result.confidence = 0.6;
        result.reasoning = "Message structure matches Anthropic format";
    } else if (has_openai_message_structure(json_data)) {
        result.format = APIFormat::OPENAI;
        result.confidence = 0.6;
        result.reasoning = "Message structure matches OpenAI format";
    } else {
        result.reasoning = "Message structure not recognized";
    }

    return result;
}

bool FormatDetector::has_header_pattern(
    const std::map<std::string, std::string>& headers,
    const std::map<std::string, std::string>& patterns) {

    for (const auto& pattern : patterns) {
        auto it = headers.find(to_lower(pattern.first));
        if (it != headers.end()) {
            if (pattern.second.empty()) {
                // Just checking for header presence
                return true;
            } else {
                // Checking for header value pattern
                if (to_lower(it->second).find(to_lower(pattern.second)) != std::string::npos) {
                    return true;
                }
            }
        }
    }
    return false;
}

bool FormatDetector::matches_model_pattern(const std::string& model, const std::vector<std::string>& patterns) {
    for (const auto& pattern : patterns) {
        if (model.find(pattern) != std::string::npos) {
            return true;
        }
    }
    return false;
}

bool FormatDetector::has_anthropic_message_structure(const nlohmann::json& json_data) {
    // Anthropic format has "messages" array with "role" and "content"
    if (json_data.contains("messages") && json_data["messages"].is_array()) {
        bool has_valid_structure = false;
        for (const auto& message : json_data["messages"]) {
            if (message.is_object() &&
                message.contains("role") &&
                message.contains("content") &&
                (message["role"] == "user" || message["role"] == "assistant")) {
                has_valid_structure = true;
                break;
            }
        }

        // Also check for system field which is more common in Anthropic
        if (json_data.contains("system") && has_valid_structure) {
            return true;
        }
    }
    return false;
}

bool FormatDetector::has_openai_message_structure(const nlohmann::json& json_data) {
    // OpenAI format has "messages" array with "role" and "content"
    if (json_data.contains("messages") && json_data["messages"].is_array()) {
        bool has_valid_structure = false;
        for (const auto& message : json_data["messages"]) {
            if (message.is_object() &&
                message.contains("role") &&
                message.contains("content") &&
                (message["role"] == "user" ||
                 message["role"] == "assistant" ||
                 message["role"] == "system")) {
                has_valid_structure = true;
                break;
            }
        }

        // Check for OpenAI-specific fields
        if (has_valid_structure && (
            json_data.contains("functions") ||
            json_data.contains("tools") ||
            json_data.contains("response_format") ||
            json_data.contains("stream"))) {
            return true;
        }
    }
    return false;
}

DetectionResult FormatDetector::combine_results(const std::vector<DetectionResult>& results) {
    DetectionResult final_result;

    std::map<APIFormat, double> format_scores;
    std::map<APIFormat, std::vector<std::string>> reasons;

    // Weight different detection methods
    std::vector<double> weights = {0.4, 0.3, 0.15, 0.1, 0.05}; // endpoint, headers, model, structure, body

    for (size_t i = 0; i < results.size() && i < weights.size(); ++i) {
        const auto& result = results[i];
        if (result.format != APIFormat::UNKNOWN) {
            double weighted_score = result.confidence * weights[i];
            format_scores[result.format] += weighted_score;
            if (!result.reasoning.empty()) {
                reasons[result.format].push_back(result.reasoning);
            }
        }
    }

    // Find format with highest score
    APIFormat best_format = APIFormat::UNKNOWN;
    double best_score = 0.0;

    for (const auto& pair : format_scores) {
        if (pair.second > best_score) {
            best_score = pair.second;
            best_format = pair.first;
        }
    }

    final_result.format = best_format;
    final_result.confidence = best_score;

    // Combine reasoning
    if (!reasons[best_format].empty()) {
        std::stringstream ss;
        for (size_t i = 0; i < reasons[best_format].size(); ++i) {
            if (i > 0) ss << "; ";
            ss << reasons[best_format][i];
        }
        final_result.reasoning = ss.str();
    }

    return final_result;
}

std::string FormatDetector::to_lower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    return result;
}

bool FormatDetector::contains(const std::string& str, const std::string& substr) {
    return str.find(substr) != std::string::npos;
}

bool FormatDetector::ends_with(const std::string& str, const std::string& suffix) {
    if (suffix.length() > str.length()) return false;
    return std::equal(suffix.rbegin(), suffix.rend(), str.rbegin());
}

// Utility functions
std::string format_to_string(APIFormat format) {
    switch (format) {
        case APIFormat::ANTHROPIC: return "anthropic";
        case APIFormat::OPENAI: return "openai";
        case APIFormat::UNKNOWN: return "unknown";
        default: return "unknown";
    }
}

APIFormat string_to_format(const std::string& format_str) {
    std::string lower = format_str;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c){ return std::tolower(c); });

    if (lower == "anthropic" || lower == "claude") {
        return APIFormat::ANTHROPIC;
    } else if (lower == "openai" || lower == "gpt") {
        return APIFormat::OPENAI;
    } else {
        return APIFormat::UNKNOWN;
    }
}

} // namespace gateway
} // namespace aimux