#pragma once

#include <string>
#include <map>
#include <nlohmann/json.hpp>
#include <crow.h>

namespace aimux {
namespace gateway {

/**
 * @brief Enum representing supported API formats
 */
enum class APIFormat {
    ANTHROPIC,  // Claude API format
    OPENAI,     // OpenAI ChatGPT API format
    UNKNOWN     // Could not determine format
};

/**
 * @brief Configuration for format detection
 */
struct FormatDetectionConfig {
    // Header-based detection keys
    std::map<std::string, std::string> anthropic_headers = {
        {"anthropic-version", "2023-06-01"},
        {"x-api-key", ""}  // Presence indicates Anthropic
    };

    std::map<std::string, std::string> openai_headers = {
        {"authorization", "Bearer "},  // Bearer token indicates OpenAI
        {"openai-organization", ""}    // OpenAI specific header
    };

    // Content-type detection
    std::string anthropic_content_type = "application/json";
    std::string openai_content_type = "application/json";

    // Model name patterns for detection
    std::vector<std::string> anthropic_model_patterns = {
        "claude-3", "claude-2", "claude-instant"
    };

    std::vector<std::string> openai_model_patterns = {
        "gpt-4", "gpt-3.5", "text-davinci", "gpt-3"
    };

    // Endpoint patterns
    std::vector<std::string> anthropic_endpoints = {
        "/v1/messages", "/v1/complete"
    };

    std::vector<std::string> openai_endpoints = {
        "/v1/chat/completions", "/v1/completions", "/v1/engines"
    };
};

/**
 * @brief Format detection result with confidence score
 */
struct DetectionResult {
    APIFormat format = APIFormat::UNKNOWN;
    double confidence = 0.0;  // 0.0 to 1.0
    std::string reasoning;     // Why this format was detected

    bool is_reliable(double threshold = 0.7) const {
        return format != APIFormat::UNKNOWN && confidence >= threshold;
    }
};

/**
 * @brief API format detector that analyzes requests to determine format
 */
class FormatDetector {
public:
    explicit FormatDetector(const FormatDetectionConfig& config = FormatDetectionConfig{});

    /**
     * @brief Detect API format from HTTP request
     * @param req Crow HTTP request
     * @param body Raw request body
     * @return Detection result with confidence
     */
    DetectionResult detect_format(const crow::request& req, const std::string& body);

    /**
     * @brief Detect format from JSON payload
     * @param json_data Request body as JSON
     * @param headers HTTP headers
     * @param endpoint Request endpoint/path
     * @return Detection result with confidence
     */
    DetectionResult detect_format(
        const nlohmann::json& json_data,
        const std::map<std::string, std::string>& headers,
        const std::string& endpoint = ""
    );

    /**
     * @brief Quick format detection based primarily on endpoint and key headers
     * @param endpoint Request endpoint
     * @param headers HTTP headers
     * @return Likely API format
     */
    APIFormat detect_format_quick(
        const std::string& endpoint,
        const std::map<std::string, std::string>& headers
    );

    /**
     * @brief Update detection configuration
     * @param config New configuration
     */
    void update_config(const FormatDetectionConfig& config) { config_ = config; }

    /**
     * @brief Get current configuration
     * @return Current detection config
     */
    const FormatDetectionConfig& get_config() const { return config_; }

private:
    FormatDetectionConfig config_;

    // Detection methods
    DetectionResult detect_from_endpoint(const std::string& endpoint);
    DetectionResult detect_from_headers(const std::map<std::string, std::string>& headers);
    DetectionResult detect_from_body(const nlohmann::json& json_data);
    DetectionResult detect_from_model(const nlohmann::json& json_data);
    DetectionResult detect_from_message_structure(const nlohmann::json& json_data);

    // Helper methods
    bool has_header_pattern(
        const std::map<std::string, std::string>& headers,
        const std::map<std::string, std::string>& patterns
    );

    bool matches_model_pattern(const std::string& model, const std::vector<std::string>& patterns);

    bool has_anthropic_message_structure(const nlohmann::json& json_data);
    bool has_openai_message_structure(const nlohmann::json& json_data);

    DetectionResult combine_results(const std::vector<DetectionResult>& results);

    // Utility methods
    std::string to_lower(const std::string& str);
    bool contains(const std::string& str, const std::string& substr);
    bool ends_with(const std::string& str, const std::string& suffix);
};

/**
 * @brief Convert API format enum to string
 */
std::string format_to_string(APIFormat format);

/**
 * @brief Convert string to API format enum
 */
APIFormat string_to_format(const std::string& format_str);

} // namespace gateway
} // namespace aimux