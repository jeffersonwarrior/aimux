#include "aimux/gateway/api_transformer.hpp"
#include <algorithm>
#include <sstream>
#include <stdexcept>

namespace aimux {
namespace gateway {

ApiTransformer::ApiTransformer(const TransformConfig& config)
    : config_(config) {}

TransformResult ApiTransformer::transform_request(
    const nlohmann::json& source_data,
    APIFormat source_format,
    APIFormat target_format) {

    if (source_format == target_format) {
        TransformResult result;
        result.success = true;
        result.transformed_data = source_data;
        result.source_format = source_format;
        result.target_format = target_format;
        return result;
    }

    try {
        if (source_format == APIFormat::ANTHROPIC && target_format == APIFormat::OPENAI) {
            return anthropic_to_openai_request(source_data);
        } else if (source_format == APIFormat::OPENAI && target_format == APIFormat::ANTHROPIC) {
            return openai_to_anthropic_request(source_data);
        } else {
            return create_error_result("Unsupported format transformation", source_format, target_format);
        }
    } catch (const std::exception& e) {
        return create_error_result("Transformation failed: " + std::string(e.what()), source_format, target_format);
    }
}

TransformResult ApiTransformer::transform_response(
    const nlohmann::json& source_response,
    APIFormat client_original_format,
    APIFormat provider_format) {

    // Transform from provider format back to client's original format
    return transform(source_response, provider_format, client_original_format, true);
}

TransformResult ApiTransformer::transform(
    const nlohmann::json& data,
    APIFormat source_format,
    APIFormat target_format,
    bool is_response) {

    if (source_format == target_format) {
        TransformResult result;
        result.success = true;
        result.transformed_data = data;
        result.source_format = source_format;
        result.target_format = target_format;
        return result;
    }

    try {
        if (is_response) {
            if (source_format == APIFormat::ANTHROPIC && target_format == APIFormat::OPENAI) {
                return anthropic_to_openai_response(data);
            } else if (source_format == APIFormat::OPENAI && target_format == APIFormat::ANTHROPIC) {
                return openai_to_anthropic_response(data);
            }
        } else {
            return transform_request(data, source_format, target_format);
        }

        return create_error_result("Unsupported transformation", source_format, target_format);
    } catch (const std::exception& e) {
        return create_error_result("Transformation failed: " + std::string(e.what()), source_format, target_format);
    }
}

std::string ApiTransformer::map_model(
    const std::string& model,
    APIFormat from_format,
    APIFormat target_format) {

    if (from_format == target_format || !config_.auto_map_models) {
        return model;
    }

    for (const auto& mapping : config_.model_mappings) {
        if (from_format == APIFormat::ANTHROPIC && target_format == APIFormat::OPENAI) {
            if (model == mapping.anthropic_model) {
                return mapping.openai_model;
            }
        } else if (from_format == APIFormat::OPENAI && target_format == APIFormat::ANTHROPIC) {
            if (model == mapping.openai_model) {
                return mapping.anthropic_model;
            }
        }
    }

    // No mapping found, return original model
    return model;
}

TransformResult ApiTransformer::anthropic_to_openai_request(const nlohmann::json& anthropic_req) {
    TransformResult result;
    result.source_format = APIFormat::ANTHROPIC;
    result.target_format = APIFormat::OPENAI;

    try {
        nlohmann::json openai_req;

        // Map model
        if (anthropic_req.contains("model")) {
            std::string anthropic_model = anthropic_req["model"];
            openai_req["model"] = map_model(anthropic_model, APIFormat::ANTHROPIC, APIFormat::OPENAI);

            if (openai_req["model"] == anthropic_model) {
                result.warnings.push_back("No direct model mapping found for: " + anthropic_model);
            }
            result.field_mappings["model"] = anthropic_model + " -> " + openai_req["model"].get<std::string>();
        }

        // Transform messages
        if (anthropic_req.contains("messages")) {
            openai_req["messages"] = transform_messages_anthropic_to_openai(anthropic_req["messages"]);
        }

        // Handle system message (Anthropic has separate system field)
        if (anthropic_req.contains("system")) {
            nlohmann::json system_msg;
            system_msg["role"] = "system";
            system_msg["content"] = anthropic_req["system"];

            if (!openai_req.contains("messages")) {
                openai_req["messages"] = nlohmann::json::array();
            }

            openai_req["messages"].insert(openai_req["messages"].begin(), system_msg);
            result.field_mappings["system"] = "system -> messages[0].content";
        }

        // Copy common parameters with adaptation
        std::vector<std::string> common_params = {"max_tokens", "temperature", "top_p"};
        for (const auto& param : common_params) {
            if (anthropic_req.contains(param)) {
                openai_req[param] = anthropic_req[param];
                result.field_mappings[param] = param;
            }
        }

        // Handle Anthropic-specific fields
        if (anthropic_req.contains("top_k")) {
            // Anthropic's top_k doesn't have direct OpenAI equivalent
            if (config_.warn_on_data_loss) {
                result.warnings.push_back("top_k parameter has no direct OpenAI equivalent and will be ignored");
            }
        }

        // Set OpenAI defaults
        apply_defaults(openai_req, APIFormat::OPENAI, true);

        // Handle streaming
        if (anthropic_req.contains("stream")) {
            openai_req["stream"] = anthropic_req["stream"];
        }

        // Preserve unknown fields if requested
        if (config_.preserve_unknown_fields) {
            preserve_unknown_fields(openai_req, anthropic_req, get_known_request_fields(APIFormat::OPENAI));
        }

        result.success = true;
        result.transformed_data = openai_req;

    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = std::string("Anthropic to OpenAI request transformation failed: ") + e.what();
    }

    return result;
}

TransformResult ApiTransformer::openai_to_anthropic_request(const nlohmann::json& openai_req) {
    TransformResult result;
    result.source_format = APIFormat::OPENAI;
    result.target_format = APIFormat::ANTHROPIC;

    try {
        nlohmann::json anthropic_req;

        // Map model
        if (openai_req.contains("model")) {
            std::string openai_model = openai_req["model"];
            anthropic_req["model"] = map_model(openai_model, APIFormat::OPENAI, APIFormat::ANTHROPIC);

            if (anthropic_req["model"] == openai_model) {
                result.warnings.push_back("No direct model mapping found for: " + openai_model);
            }
            result.field_mappings["model"] = openai_model + " -> " + anthropic_req["model"].get<std::string>();
        }

        // Transform messages
        if (openai_req.contains("messages")) {
            auto transformed_messages = transform_messages_openai_to_anthropic(openai_req["messages"]);

            // Extract system message separately
            nlohmann::json filtered_messages = nlohmann::json::array();
            for (const auto& msg : transformed_messages) {
                if (msg.contains("role") && msg["role"] == "system") {
                    anthropic_req["system"] = msg["content"];
                    result.field_mappings["system"] = "messages[system].content -> system";
                } else {
                    filtered_messages.push_back(msg);
                }
            }

            anthropic_req["messages"] = filtered_messages;
        }

        // Copy common parameters
        std::vector<std::string> common_params = {"max_tokens", "temperature", "top_p"};
        for (const auto& param : common_params) {
            if (openai_req.contains(param)) {
                anthropic_req[param] = openai_req[param];
                result.field_mappings[param] = param;
            }
        }

        // Handle OpenAI-specific fields
        if (openai_req.contains("frequency_penalty") || openai_req.contains("presence_penalty")) {
            if (config_.warn_on_data_loss) {
                result.warnings.push_back("frequency_penalty and presence_penalty have no Anthropic equivalents");
            }
        }

        // Set Anthropic defaults
        apply_defaults(anthropic_req, APIFormat::ANTHROPIC, true);

        // Handle streaming
        if (openai_req.contains("stream")) {
            anthropic_req["stream"] = openai_req["stream"];
        }

        // Preserve unknown fields if requested
        if (config_.preserve_unknown_fields) {
            preserve_unknown_fields(anthropic_req, openai_req, get_known_request_fields(APIFormat::ANTHROPIC));
        }

        result.success = true;
        result.transformed_data = anthropic_req;

    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = std::string("OpenAI to Anthropic request transformation failed: ") + e.what();
    }

    return result;
}

TransformResult ApiTransformer::anthropic_to_openai_response(const nlohmann::json& anthropic_resp) {
    TransformResult result;
    result.source_format = APIFormat::ANTHROPIC;
    result.target_format = APIFormat::OPENAI;

    try {
        nlohmann::json openai_resp;

        // Handle error responses
        if (anthropic_resp.contains("error")) {
            openai_resp["error"] = anthropic_resp["error"];
            result.success = true;
            result.transformed_data = openai_resp;
            return result;
        }

        // Transform content
        if (anthropic_resp.contains("content") && anthropic_resp["content"].is_array() &&
            !anthropic_resp["content"].empty()) {

            auto& content_block = anthropic_resp["content"][0];
            if (content_block.contains("text")) {
                openai_resp["choices"] = nlohmann::json::array();
                nlohmann::json choice;
                choice["index"] = 0;
                choice["message"]["role"] = "assistant";
                choice["message"]["content"] = content_block["text"];
                choice["finish_reason"] = anthropic_resp.value("stop_reason", "stop");

                openai_resp["choices"].push_back(choice);
                result.field_mappings["content"] = "content[0].text -> choices[0].message.content";
            }
        }

        // Transform usage information
        if (anthropic_resp.contains("usage")) {
            openai_resp["usage"] = transform_usage_info(anthropic_resp["usage"], APIFormat::OPENAI);
        }

        // Copy id and created timestamps
        if (anthropic_resp.contains("id")) {
            openai_resp["id"] = anthropic_resp["id"];
        }
        if (anthropic_resp.contains("created")) {
            openai_resp["created"] = anthropic_resp["created"];
        } else {
            openai_resp["created"] = std::time(nullptr);
        }

        // Set model if available
        if (anthropic_resp.contains("model")) {
            openai_resp["model"] = anthropic_resp["model"];
        }

        // Set object type
        openai_resp["object"] = "chat.completion";

        result.success = true;
        result.transformed_data = openai_resp;

    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = std::string("Anthropic to OpenAI response transformation failed: ") + e.what();
    }

    return result;
}

TransformResult ApiTransformer::openai_to_anthropic_response(const nlohmann::json& openai_resp) {
    TransformResult result;
    result.source_format = APIFormat::OPENAI;
    result.target_format = APIFormat::ANTHROPIC;

    try {
        nlohmann::json anthropic_resp;

        // Handle error responses
        if (openai_resp.contains("error")) {
            anthropic_resp["error"] = openai_resp["error"];
            result.success = true;
            result.transformed_data = anthropic_resp;
            return result;
        }

        // Transform choices to content
        if (openai_resp.contains("choices") && !openai_resp["choices"].empty()) {
            auto& choice = openai_resp["choices"][0];

            if (choice.contains("message") && choice["message"].contains("content")) {
                anthropic_resp["content"] = nlohmann::json::array();
                nlohmann::json content_block;
                content_block["type"] = "text";
                content_block["text"] = choice["message"]["content"];
                anthropic_resp["content"].push_back(content_block);
                result.field_mappings["choices[0].message.content"] = "content[0].text";
            }

            // Map finish reason
            if (choice.contains("finish_reason")) {
                std::string finish_reason = choice["finish_reason"];
                if (finish_reason == "stop") {
                    anthropic_resp["stop_reason"] = "end_turn";
                } else if (finish_reason == "length") {
                    anthropic_resp["stop_reason"] = "max_tokens";
                } else {
                    anthropic_resp["stop_reason"] = finish_reason;
                }
                result.field_mappings["finish_reason"] = finish_reason + " -> stop_reason";
            }
        }

        // Transform usage information
        if (openai_resp.contains("usage")) {
            anthropic_resp["usage"] = transform_usage_info(openai_resp["usage"], APIFormat::ANTHROPIC);
        }

        // Copy id and created timestamps
        if (openai_resp.contains("id")) {
            anthropic_resp["id"] = openai_resp["id"];
        }
        if (openai_resp.contains("created")) {
            anthropic_resp["created"] = openai_resp["created"];
        } else {
            anthropic_resp["created"] = std::time(nullptr);
        }

        // Set model if available
        if (openai_resp.contains("model")) {
            anthropic_resp["model"] = openai_resp["model"];
        }

        // Set object type
        anthropic_resp["type"] = "message";

        result.success = true;
        result.transformed_data = anthropic_resp;

    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = std::string("OpenAI to Anthropic response transformation failed: ") + e.what();
    }

    return result;
}

nlohmann::json ApiTransformer::transform_messages_anthropic_to_openai(const nlohmann::json& anthropic_messages) {
    nlohmann::json openai_messages = nlohmann::json::array();

    for (const auto& msg : anthropic_messages) {
        nlohmann::json openai_msg;

        if (msg.contains("role")) {
            openai_msg["role"] = msg["role"];
        }

        if (msg.contains("content")) {
            openai_msg["content"] = normalize_message_content(msg["content"], APIFormat::OPENAI);
        }

        openai_messages.push_back(openai_msg);
    }

    return openai_messages;
}

nlohmann::json ApiTransformer::transform_messages_openai_to_anthropic(const nlohmann::json& openai_messages) {
    nlohmann::json anthropic_messages = nlohmann::json::array();

    for (const auto& msg : openai_messages) {
        nlohmann::json anthropic_msg;

        if (msg.contains("role")) {
            anthropic_msg["role"] = msg["role"];
        }

        if (msg.contains("content")) {
            anthropic_msg["content"] = normalize_message_content(msg["content"], APIFormat::ANTHROPIC);
        }

        anthropic_messages.push_back(anthropic_msg);
    }

    return anthropic_messages;
}

nlohmann::json ApiTransformer::transform_usage_info(const nlohmann::json& usage, APIFormat target_format) {
    nlohmann::json transformed_usage = usage;

    if (target_format == APIFormat::ANTHROPIC) {
        // OpenAI format has prompt_tokens, completion_tokens, total_tokens
        // Anthropic format has input_tokens, output_tokens
        if (usage.contains("prompt_tokens")) {
            transformed_usage["input_tokens"] = usage["prompt_tokens"];
        }
        if (usage.contains("completion_tokens")) {
            transformed_usage["output_tokens"] = usage["completion_tokens"];
        }
    } else if (target_format == APIFormat::OPENAI) {
        // Anthropic format has input_tokens, output_tokens
        // OpenAI format has prompt_tokens, completion_tokens, total_tokens
        if (usage.contains("input_tokens")) {
            transformed_usage["prompt_tokens"] = usage["input_tokens"];
        }
        if (usage.contains("output_tokens")) {
            transformed_usage["completion_tokens"] = usage["output_tokens"];
        }
        if (!usage.contains("total_tokens") && usage.contains("input_tokens") && usage.contains("output_tokens")) {
            transformed_usage["total_tokens"] = usage["input_tokens"].get<int>() + usage["output_tokens"].get<int>();
        }
    }

    return transformed_usage;
}

nlohmann::json ApiTransformer::normalize_message_content(const nlohmann::json& content, APIFormat /* target_format */) {
    if (content.is_string()) {
        return content;
    }

    if (content.is_array()) {
        for (const auto& block : content) {
            if (block.contains("text")) {
                return block["text"];
            }
        }
    }

    // Fallback: convert to string
    return content.dump();
}

void ApiTransformer::apply_defaults(nlohmann::json& data, APIFormat format, bool is_request) {
    if (!is_request) return;

    const auto& defaults = (format == APIFormat::ANTHROPIC) ? config_.anthropic_defaults : config_.openai_defaults;

    for (const auto& [key, value] : defaults.items()) {
        if (!data.contains(key)) {
            data[key] = value;
        }
    }
}

void ApiTransformer::preserve_unknown_fields(
    nlohmann::json& target,
    const nlohmann::json& source,
    const std::vector<std::string>& known_fields) {

    for (const auto& [key, value] : source.items()) {
        if (std::find(known_fields.begin(), known_fields.end(), key) == known_fields.end()) {
            target[key] = value;
        }
    }
}

std::vector<std::string> ApiTransformer::get_known_request_fields(APIFormat format) {
    if (format == APIFormat::ANTHROPIC) {
        return {"model", "messages", "system", "max_tokens", "temperature", "top_p", "top_k", "stream"};
    } else if (format == APIFormat::OPENAI) {
        return {"model", "messages", "max_tokens", "temperature", "top_p", "frequency_penalty",
                "presence_penalty", "stream", "functions", "tools", "response_format"};
    }
    return {};
}

std::vector<std::string> ApiTransformer::get_known_response_fields(APIFormat format) {
    if (format == APIFormat::ANTHROPIC) {
        return {"id", "type", "role", "content", "model", "stop_reason", "stop_sequence", "usage"};
    } else if (format == APIFormat::OPENAI) {
        return {"id", "object", "created", "model", "choices", "usage"};
    }
    return {};
}

TransformResult ApiTransformer::create_error_result(const std::string& error, APIFormat source, APIFormat target) {
    TransformResult result;
    result.success = false;
    result.error_message = error;
    result.source_format = source;
    result.target_format = target;
    return result;
}

// TransformerFactory implementation
std::unique_ptr<ApiTransformer> TransformerFactory::create_transformer(const std::string& use_case) {
    return create_transformer(get_preset_config(use_case));
}

std::unique_ptr<ApiTransformer> TransformerFactory::create_transformer(const TransformConfig& config) {
    return std::make_unique<ApiTransformer>(config);
}

TransformConfig TransformerFactory::get_preset_config(const std::string& use_case) {
    TransformConfig config;

    if (use_case == "development") {
        config.warn_on_data_loss = true;
        config.preserve_unknown_fields = true;
    } else if (use_case == "testing") {
        config.warn_on_data_loss = false;
        config.auto_map_models = true;
    } else if (use_case == "production") {
        config.warn_on_data_loss = true;
        config.preserve_unknown_fields = false;
        config.auto_map_models = true;
    }

    return config;
}

} // namespace gateway
} // namespace aimux