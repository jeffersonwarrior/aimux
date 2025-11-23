#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include "aimux/gateway/format_detector.hpp"

namespace aimux {
namespace gateway {

/**
 * @brief Transformation result with metadata
 */
struct TransformResult {
    bool success = false;
    nlohmann::json transformed_data;
    std::string error_message;

    // Metadata about transformation
    APIFormat source_format = APIFormat::UNKNOWN;
    APIFormat target_format = APIFormat::UNKNOWN;
    std::map<std::string, std::string> field_mappings;

    // Warnings about potential data loss or changes
    std::vector<std::string> warnings;

    operator bool() const { return success; }
};

/**
 * @brief Model mapping configuration for cross-format compatibility
 */
struct ModelMapping {
    std::string anthropic_model;
    std::string openai_model;
    bool equivalent_capabilities = true;
    std::string notes;
};

/**
 * @brief Configuration for API transformations
 */
struct TransformConfig {
    // Model name mappings
    std::vector<ModelMapping> model_mappings = {
        {"claude-3-5-sonnet-20241022", "gpt-4-turbo", true, "High-end reasoning models"},
        {"claude-3-5-haiku-20241022", "gpt-4o-mini", true, "Fast, efficient models"},
        {"claude-3-opus-20240229", "gpt-4-turbo", true, "High capability models"},
        {"claude-3-sonnet-20240229", "gpt-4-turbo", true, "Balanced performance"},
        {"claude-3-haiku-20240307", "gpt-3.5-turbo", true, "Fast models"}
    };

    // Default values for missing fields
    nlohmann::json anthropic_defaults = {
        {"max_tokens", 4096},
        {"temperature", 1.0},
        {"top_p", 1.0}
    };

    nlohmann::json openai_defaults = {
        {"max_tokens", 4096},
        {"temperature", 1.0},
        {"top_p", 1.0},
        {"frequency_penalty", 0.0},
        {"presence_penalty", 0.0}
    };

    // Transformation settings
    bool preserve_unknown_fields = true;
    bool warn_on_data_loss = true;
    bool auto_map_models = true;
    double default_temperature_for_unspecified = 1.0;
};

/**
 * @brief API transformer for bidirectional format conversion
 */
class ApiTransformer {
public:
    explicit ApiTransformer(const TransformConfig& config = TransformConfig{});

    /**
     * @brief Transform request from one format to another
     * @param source_data Original request data
     * @param source_format Source API format
     * @param target_format Target API format
     * @return Transform result with transformed data and metadata
     */
    TransformResult transform_request(
        const nlohmann::json& source_data,
        APIFormat source_format,
        APIFormat target_format
    );

    /**
     * @brief Transform response back to client format
     * @param source_response Response from provider in provider's format
     * @param client_original_format Client's original request format
     * @param provider_format Provider's native format
     * @return Transform result with formatted response
     */
    TransformResult transform_response(
        const nlohmann::json& source_response,
        APIFormat client_original_format,
        APIFormat provider_format
    );

    /**
     * @brief Quick format-aware transformation
     * @param data Data to transform
     * @param source_format Source format
     * @param target_format Target format
     * @param is_response True if this is a response transformation
     * @return Transform result
     */
    TransformResult transform(
        const nlohmann::json& data,
        APIFormat source_format,
        APIFormat target_format,
        bool is_response = false
    );

    /**
     * @brief Map model name between formats
     * @param model Original model name
     * @param from_format Source format of model
     * @param target_format Target format for model
     * @return Mapped model name (or original if no mapping found)
     */
    std::string map_model(
        const std::string& model,
        APIFormat from_format,
        APIFormat target_format
    );

    /**
     * @brief Get available model mappings
     * @return Vector of model mappings
     */
    const std::vector<ModelMapping>& get_model_mappings() const { return config_.model_mappings; }

    /**
     * @brief Update transformation configuration
     * @param config New configuration
     */
    void update_config(const TransformConfig& config) { config_ = config; }

    /**
     * @brief Get current configuration
     * @return Current transform config
     */
    const TransformConfig& get_config() const { return config_; }

private:
    TransformConfig config_;

    // Request transformations
    TransformResult anthropic_to_openai_request(const nlohmann::json& anthropic_req);
    TransformResult openai_to_anthropic_request(const nlohmann::json& openai_req);

    // Response transformations
    TransformResult anthropic_to_openai_response(const nlohmann::json& anthropic_resp);
    TransformResult openai_to_anthropic_response(const nlohmann::json& openai_resp);

    // Helper methods for request transformation
    nlohmann::json transform_messages_anthropic_to_openai(const nlohmann::json& messages);
    nlohmann::json transform_messages_openai_to_anthropic(const nlohmann::json& messages);
    nlohmann::json transform_common_params(const nlohmann::json& source_params, APIFormat target_format);

    // Helper methods for response transformation
    nlohmann::json transform_usage_info(const nlohmann::json& usage, APIFormat target_format);
    nlohmann::json transform_choices_content(const nlohmann::json& choices, APIFormat target_format);

    // Utility methods
    void apply_defaults(nlohmann::json& data, APIFormat format, bool is_request = true);
    void preserve_unknown_fields(
        nlohmann::json& target,
        const nlohmann::json& source,
        const std::vector<std::string>& known_fields
    );

    // Field validation and mapping
    std::vector<std::string> get_known_request_fields(APIFormat format);
    std::vector<std::string> get_known_response_fields(APIFormat format);

    // Message content handling
    nlohmann::json normalize_message_content(const nlohmann::json& content, APIFormat target_format);

    // Error handling
    TransformResult create_error_result(const std::string& error, APIFormat source = APIFormat::UNKNOWN, APIFormat target = APIFormat::UNKNOWN);
};

/**
 * @brief Factory for creating transformers with different configurations
 */
class TransformerFactory {
public:
    /**
     * @brief Create transformer for specific use case
     * @param use_case Configuration preset ("production", "development", "testing")
     * @return Unique pointer to transformer
     */
    static std::unique_ptr<ApiTransformer> create_transformer(const std::string& use_case = "production");

    /**
     * @brief Create transformer with custom configuration
     * @param config Custom transformation config
     * @return Unique pointer to transformer
     */
    static std::unique_ptr<ApiTransformer> create_transformer(const TransformConfig& config);

private:
    static TransformConfig get_preset_config(const std::string& use_case);
};

} // namespace gateway
} // namespace aimux