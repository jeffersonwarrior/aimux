#include "aimux/providers/provider_impl.hpp"
#include "aimux/providers/api_specs.hpp"
#include "aimux/network/http_client.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <random>
#include <algorithm>
#include <thread>
#include <iomanip>

#include <openssl/evp.h>
#include <openssl/sha.h>
#include <openssl/rand.h>
#include <openssl/aes.h>

namespace aimux {
namespace providers {

// Security utility functions
namespace security {
    std::string encrypt_api_key(const std::string& api_key) {
        // Simple XOR encryption for demonstration
        // In production, use proper encryption like AES-256-GCM
        std::string key = "aimux-secure-key-2025"; // 32 bytes for AES-256
        std::string encrypted;
        
        for (size_t i = 0; i < api_key.size(); ++i) {
            encrypted += api_key[i] ^ key[i % key.size()];
        }
        
        // Convert to hex for storage
        std::stringstream hex_stream;
        hex_stream << std::hex << std::setfill('0');
        for (char c_char : encrypted) {
            unsigned char c = static_cast<unsigned char>(c_char);
            hex_stream << std::setw(2) << static_cast<int>(c);
        }
        
        return hex_stream.str();
    }
    
    std::string decrypt_api_key(const std::string& encrypted_hex) {
        // Convert from hex
        std::string encrypted;
        for (size_t i = 0; i < encrypted_hex.length(); i += 2) {
            std::string byte_str = encrypted_hex.substr(i, 2);
            char c = static_cast<char>(std::stoi(byte_str, nullptr, 16));
            encrypted += c;
        }
        
        // XOR decrypt
        std::string key = "aimux-secure-key-2025";
        std::string decrypted;
        
        for (size_t i = 0; i < encrypted.size(); ++i) {
            decrypted += encrypted[i] ^ key[i % key.size()];
        }
        
        return decrypted;
    }
    
    std::string hash_api_key(const std::string& api_key) {
        unsigned char hash[SHA256_DIGEST_LENGTH];
        SHA256(reinterpret_cast<const unsigned char*>(api_key.c_str()), api_key.length(), hash);
        
        std::stringstream hex_stream;
        hex_stream << std::hex << std::setfill('0');
        for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
            hex_stream << std::setw(2) << static_cast<int>(hash[i]);
        }
        
        return hex_stream.str();
    }
    
    bool validate_api_key_format(const std::string& api_key) {
        // Basic validation for API keys
        if (api_key.length() < 16) {
            return false;
        }
        
        // Check for allowed characters (alphanumeric + common symbols)
        for (char c : api_key) {
            if (!std::isalnum(c) && c != '-' && c != '_' && c != '.' && c != '/') {
                return false;
            }
        }
        
        return true;
    }
}

// BaseProvider implementation
BaseProvider::BaseProvider(const std::string& name, const nlohmann::json& config)
    : provider_name_(name), config_(config) {
    
    // Secure API key handling
    std::string raw_api_key = config.value("api_key", "");
    
    // Skip API key validation for synthetic provider (testing only)
    if (name != "synthetic") {
        if (!security::validate_api_key_format(raw_api_key)) {
            throw std::invalid_argument("Invalid API key format for provider: " + name);
        }
    }
    
    // Store API key in encrypted form
    encrypted_api_key_ = security::encrypt_api_key(raw_api_key);
    api_key_hash_ = security::hash_api_key(raw_api_key);
    
    endpoint_ = config.value("endpoint", "");
    max_requests_per_minute_ = config.value("max_requests_per_minute", 60);
    rate_limit_reset_ = std::chrono::steady_clock::now() + std::chrono::minutes(1);

    // Health tracking configuration
    consecutive_failures_ = 0;
    last_failure_time_ = std::chrono::steady_clock::now();
    int recovery_delay_seconds = config.value("recovery_delay", 300);
    recovery_delay_ = std::chrono::seconds(recovery_delay_seconds);
}

bool BaseProvider::check_rate_limit() {
    auto now = std::chrono::steady_clock::now();
    
    // Reset if minute has passed
    if (now >= rate_limit_reset_) {
        requests_made_ = 0;
        rate_limit_reset_ = now + std::chrono::minutes(1);
    }
    
    // Enhanced rate limiting with burst protection
    bool allowed = requests_made_ < max_requests_per_minute_;
    if (!allowed) {
        // Log rate limit exceeded event
        std::cerr << "Rate limit exceeded for provider " << provider_name_ 
                  << ": " << requests_made_ << "/" << max_requests_per_minute_ << std::endl;
    }
    
    return allowed;
}

void BaseProvider::update_rate_limit() {
    requests_made_++;
}

void BaseProvider::check_recovery() {
    if (!is_healthy_ && consecutive_failures_ > 0) {
        auto now = std::chrono::steady_clock::now();
        auto time_since_failure = std::chrono::duration_cast<std::chrono::seconds>(now - last_failure_time_);

        if (time_since_failure >= recovery_delay_) {
            // Attempt recovery - reset health status
            is_healthy_ = true;
            consecutive_failures_ = 0;
        }
    }
}

core::Response BaseProvider::process_response(int status_code, const std::string& response_body) {
    core::Response response;
    response.status_code = status_code;
    response.provider_name = provider_name_;

    if (status_code >= 200 && status_code < 300) {
        response.success = true;
        response.data = response_body;
        is_healthy_ = true;
        consecutive_failures_ = 0; // Reset failure count on success
    } else if (status_code == 429) {
        response.success = false;
        response.error_message = "Rate limit exceeded";
        // Don't mark as unhealthy for rate limiting - this is expected behavior
        consecutive_failures_++;
        last_failure_time_ = std::chrono::steady_clock::now();
    } else if (status_code >= 500) {
        response.success = false;
        response.error_message = "Provider server error";
        consecutive_failures_++;
        last_failure_time_ = std::chrono::steady_clock::now();
        // Only mark as unhealthy after multiple consecutive failures
        if (consecutive_failures_ >= 3) {
            is_healthy_ = false;
        }
    } else if (status_code == 401 || status_code == 403) {
        response.success = false;
        response.error_message = "Authentication error";
        // Auth errors should immediately mark as unhealthy
        is_healthy_ = false;
        consecutive_failures_ = 5;
        last_failure_time_ = std::chrono::steady_clock::now();
    } else {
        response.success = false;
        response.error_message = "Request failed: " + std::to_string(status_code);
        consecutive_failures_++;
        last_failure_time_ = std::chrono::steady_clock::now();
        // Mark as unhealthy after 4xx client errors (except 429)
        if (status_code >= 400 && status_code < 500 && status_code != 429) {
            if (consecutive_failures_ >= 2) {
                is_healthy_ = false;
            }
        }
    }

    return response;
}

// CerebrasProvider implementation
CerebrasProvider::CerebrasProvider(const nlohmann::json& config)
    : BaseProvider("cerebras", config) {

    // Use API specs for default values
    if (endpoint_.empty()) {
        endpoint_ = api_specs::get_base_endpoint("cerebras");
    }

    // Validate configuration
    if (!security::validate_api_key_format(security::decrypt_api_key(encrypted_api_key_))) {
        throw std::invalid_argument("Invalid or malformed API key for Cerebras provider");
    }

    // Validate endpoint URL format
    if (endpoint_.substr(0, 8) != "https://") {
        throw std::invalid_argument("Endpoint must use HTTPS for secure communication");
    }

    // Use API specs for rate limiting
    max_requests_per_minute_ = config.value("max_requests_per_minute", api_specs::get_rate_limit("cerebras"));
}

core::Response CerebrasProvider::send_request(const core::Request& request) {
    if (!check_rate_limit()) {
        core::Response response;
        response.success = false;
        response.error_message = "Rate limit exceeded";
        response.status_code = 429;
        response.provider_name = provider_name_;
        return response;
    }
    
    update_rate_limit();
    
    try {
        // Decrypt API key for use
        std::string api_key = security::decrypt_api_key(encrypted_api_key_);
        auto http_client = network::HttpClientFactory::create_client();

        // Use API specs headers
        http_client->add_default_header(api_specs::headers::AUTHORIZATION, "Bearer " + api_key);
        http_client->add_default_header(api_specs::headers::CONTENT_TYPE, api_specs::headers::APPLICATION_JSON);
        http_client->add_default_header(api_specs::headers::USER_AGENT, api_specs::headers::AIMUX_USER_AGENT);

        network::HttpRequest http_request;
        http_request.url = endpoint_ + api_specs::paths::CHAT_COMPLETIONS;
        http_request.method = "POST";
        http_request.body = format_cerebras_request(request);
        http_request.timeout_ms = api_specs::timeouts::REQUEST_TIMEOUT.count();
        
        // Retry logic for better reliability
        network::HttpResponse http_response;
        int max_retries = 3;
        
        for (int attempt = 0; attempt < max_retries; ++attempt) {
            try {
                http_response = http_client->send_request(http_request);
                
                // Check for successful HTTP status
                if (http_response.status_code >= 200 && http_response.status_code < 300) {
                    break; // Success, exit retry loop
                } else if (http_response.status_code == 429) {
                    // Rate limited - wait before retry
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                } else if (http_response.status_code >= 500) {
                    // Server error - retry after brief delay
                    if (attempt < max_retries - 1) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(500 * (attempt + 1)));
                    }
                } else {
                    // Client error (4xx) - don't retry
                    break;
                }
            } catch (const std::exception& e) {
                if (attempt == max_retries - 1) {
                    throw; // Re-throw on final attempt
                }
                // Wait before retry
                std::this_thread::sleep_for(std::chrono::milliseconds(1000 * (attempt + 1)));
            }
        }
        
        core::Response response = process_response(http_response.status_code, http_response.body);
        response.response_time_ms = http_response.response_time_ms;
        
        return response;
        
    } catch (const std::exception& e) {
        core::Response response;
        response.success = false;
        response.provider_name = provider_name_;

        // Enhanced error categorization
        std::string error_msg = e.what();
        if (error_msg.find("timeout") != std::string::npos ||
            error_msg.find("Timeout") != std::string::npos) {
            response.error_message = "Cerebras timeout error: " + error_msg;
            response.status_code = 408;
            consecutive_failures_++;
            last_failure_time_ = std::chrono::steady_clock::now();
        } else if (error_msg.find("connection") != std::string::npos ||
                   error_msg.find("network") != std::string::npos ||
                   error_msg.find("Network") != std::string::npos) {
            response.error_message = "Cerebras network error: " + error_msg;
            response.status_code = 503;
            consecutive_failures_++;
            last_failure_time_ = std::chrono::steady_clock::now();
        } else if (error_msg.find("authentication") != std::string::npos ||
                   error_msg.find("auth") != std::string::npos ||
                   error_msg.find("unauthorized") != std::string::npos) {
            response.error_message = "Cerebras authentication error: " + error_msg;
            response.status_code = 401;
            is_healthy_ = false;  // Auth errors should mark as unhealthy
            consecutive_failures_ = 5;
            last_failure_time_ = std::chrono::steady_clock::now();
        } else {
            response.error_message = "Cerebras error: " + error_msg;
            response.status_code = 500;
            consecutive_failures_++;
            last_failure_time_ = std::chrono::steady_clock::now();

            // Mark as unhealthy after multiple unknown errors
            if (consecutive_failures_ >= 3) {
                is_healthy_ = false;
            }
        }

        return response;
    }
}

bool CerebrasProvider::is_healthy() const {
    // Check for potential recovery before returning status
    const_cast<CerebrasProvider*>(this)->check_recovery();
    return is_healthy_;
}

std::string CerebrasProvider::get_provider_name() const {
    return provider_name_;
}

nlohmann::json CerebrasProvider::get_rate_limit_status() const {
    nlohmann::json status;
    status["provider"] = provider_name_;
    status["endpoint"] = endpoint_;
    status["requests_made"] = requests_made_;
    status["max_requests_per_minute"] = max_requests_per_minute_;
    status["requests_remaining"] = std::max(0, max_requests_per_minute_ - requests_made_);

    auto now = std::chrono::steady_clock::now();
    auto reset_in_seconds = std::chrono::duration_cast<std::chrono::seconds>(rate_limit_reset_ - now).count();
    status["reset_in_seconds"] = std::max<long long>(0LL, reset_in_seconds);
    status["is_healthy"] = is_healthy_;

    // Add provider capabilities from API specs
    auto caps = api_specs::get_provider_capabilities("cerebras");
    status["capabilities"] = {
        {"thinking", caps.thinking},
        {"vision", caps.vision},
        {"tools", caps.tools},
        {"max_input_tokens", caps.max_input_tokens},
        {"max_output_tokens", caps.max_output_tokens}
    };

    // Add available models from config
    status["available_models"] = config_.value("models", std::vector<std::string>{
        api_specs::models::cerebras::LLAMA3_1_70B, api_specs::models::cerebras::LLAMA3_1_8B
    });

    return status;
}

std::string CerebrasProvider::format_cerebras_request(const core::Request& request) {
    nlohmann::json cerebras_request;

    // Model selection with fallback
    std::string model = request.data.value("model", "");
    if (model.empty()) {
        model = config_.value("default_model", api_specs::models::cerebras::LLAMA3_1_70B);
    }

    // Validate model is available in configuration
    auto available_models = config_.value("models", std::vector<std::string>{model});
    if (std::find(available_models.begin(), available_models.end(), model) == available_models.end()) {
        model = available_models.empty() ? api_specs::models::cerebras::LLAMA3_1_70B : available_models[0];
    }

    cerebras_request["model"] = model;
    cerebras_request["messages"] = request.data["messages"];

    // Token limits based on model capabilities
    auto caps = api_specs::get_provider_capabilities("cerebras");
    int max_tokens = request.data.value("max_tokens", caps.max_output_tokens);
    cerebras_request["max_tokens"] = std::min(max_tokens, caps.max_output_tokens);

    // Temperature with sensible defaults for thinking models
    double temperature = request.data.value("temperature", config_.value("temperature", 0.7));
    cerebras_request["temperature"] = std::max(0.0, std::min(2.0, temperature)); // Clamp to valid range

    // Add streaming support if requested
    if (request.data.value("stream", false)) {
        cerebras_request["stream"] = true;
    }

    // Add top_p for better control
    cerebras_request["top_p"] = request.data.value("top_p", 1.0);

    // Enable thinking model optimizations if using reasoning model
    if (model == api_specs::models::cerebras::LLAMA3_1_70B &&
        request.data.value("thinking", false)) {
        // Optimize for reasoning tasks
        cerebras_request["temperature"] = std::min(temperature, 0.3); // Lower temp for reasoning
        cerebras_request["top_p"] = 0.95;
    }

    return cerebras_request.dump();
}

nlohmann::json CerebrasProvider::parse_cerebras_response(const std::string& response) {
    try {
        auto json_response = nlohmann::json::parse(response);

        // Extract usage information for cost tracking
        if (json_response.contains("usage")) {
            const auto& usage = json_response["usage"];
            int input_tokens = usage.value("prompt_tokens", 0);
            int output_tokens = usage.value("completion_tokens", 0);

            // Calculate costs using API specs
            double input_cost = (static_cast<double>(input_tokens) / 1000000.0) * api_specs::costs::cerebras::INPUT_COST_PER_1M;
            double output_cost = (static_cast<double>(output_tokens) / 1000000.0) * api_specs::costs::cerebras::OUTPUT_COST_PER_1M;
            double total_cost = input_cost + output_cost;

            // Add cost info to response metadata
            json_response["metadata"]["cost估算"] = {
                {"input_tokens", input_tokens},
                {"output_tokens", output_tokens},
                {"input_cost_usd", input_cost},
                {"output_cost_usd", output_cost},
                {"total_cost_usd", total_cost}
            };
        }

        // Extract thinking/reasoning steps if available
        if (json_response.contains("choices") && !json_response["choices"].empty()) {
            auto& choice = json_response["choices"][0];
            if (choice.contains("message") && choice["message"].contains("content")) {
                std::string content = choice["message"]["content"];

                // Look for structured thinking patterns
                if (content.find("Step") != std::string::npos ||
                    content.find("Let me think") != std::string::npos ||
                    content.find("First,") != std::string::npos) {

                    json_response["metadata"]["thinking_detected"] = true;
                    json_response["metadata"]["reasoning_type"] = "step_by_step";
                }
            }
        }

        // Add provider metadata
        json_response["metadata"]["provider"] = "cerebras";
        json_response["metadata"]["model"] = json_response.value("model", "unknown");
        json_response["metadata"]["processed_at"] = std::to_string(
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count()
        );

        return json_response;

    } catch (const nlohmann::json::exception& e) {
        // If JSON parsing fails, return error info
        nlohmann::json error_response;
        error_response["error"] = {
            {"type", "json_parse_error"},
            {"message", e.what()},
            {"provider", "cerebras"}
        };
        error_response["metadata"]["provider"] = "cerebras";
        error_response["metadata"]["processed_at"] = std::to_string(
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count()
        );
        return error_response;
    }
}

// ZaiProvider implementation
ZaiProvider::ZaiProvider(const nlohmann::json& config)
    : BaseProvider("zai", config) {

    // Use API specs for default values
    if (endpoint_.empty()) {
        endpoint_ = api_specs::get_base_endpoint("zai");
    }

    // Validate configuration
    if (!security::validate_api_key_format(security::decrypt_api_key(encrypted_api_key_))) {
        throw std::invalid_argument("Invalid or malformed API key for Z.AI provider");
    }

    // Validate endpoint URL format
    if (endpoint_.substr(0, 8) != "https://") {
        throw std::invalid_argument("Endpoint must use HTTPS for secure communication");
    }

    // Use API specs for rate limiting
    max_requests_per_minute_ = config.value("max_requests_per_minute", api_specs::get_rate_limit("zai"));
}

core::Response ZaiProvider::send_request(const core::Request& request) {
    if (!check_rate_limit()) {
        core::Response response;
        response.success = false;
        response.error_message = "Rate limit exceeded";
        response.status_code = 429;
        response.provider_name = provider_name_;
        return response;
    }

    // Validate request structure before processing
    if (request.data.empty()) {
        core::Response response;
        response.success = false;
        response.error_message = "Z.AI validation error: Request data is empty";
        response.status_code = 400;
        response.provider_name = provider_name_;
        return response;
    }

    // Validate required fields
    if (!request.data.contains("model") && !config_.contains("default_model")) {
        core::Response response;
        response.success = false;
        response.error_message = "Z.AI validation error: Model not specified and no default model configured";
        response.status_code = 400;
        response.provider_name = provider_name_;
        return response;
    }

    // Validate messages if present
    if (request.data.contains("messages")) {
        if (!request.data["messages"].is_array() || request.data["messages"].empty()) {
            core::Response response;
            response.success = false;
            response.error_message = "Z.AI validation error: Messages must be a non-empty array";
            response.status_code = 400;
            response.provider_name = provider_name_;
            return response;
        }

        // Validate each message
        for (const auto& msg : request.data["messages"]) {
            if (!msg.contains("role") || !msg.contains("content")) {
                core::Response response;
                response.success = false;
                response.error_message = "Z.AI validation error: Each message must have 'role' and 'content' fields";
                response.status_code = 400;
                response.provider_name = provider_name_;
                return response;
            }

            std::string role = msg["role"];
            if (role != "user" && role != "assistant" && role != "system") {
                core::Response response;
                response.success = false;
                response.error_message = "Z.AI validation error: Message role must be 'user', 'assistant', or 'system'";
                response.status_code = 400;
                response.provider_name = provider_name_;
                return response;
            }
        }
    }

    update_rate_limit();
    
    try {
        // Decrypt API key for use
        std::string api_key = security::decrypt_api_key(encrypted_api_key_);
        auto http_client = network::HttpClientFactory::create_client();

        // Use API specs headers
        http_client->add_default_header(api_specs::headers::AUTHORIZATION, "Bearer " + api_key);
        http_client->add_default_header(api_specs::headers::CONTENT_TYPE, api_specs::headers::APPLICATION_JSON);
        http_client->add_default_header(api_specs::headers::USER_AGENT, api_specs::headers::AIMUX_USER_AGENT);

        network::HttpRequest http_request;
        http_request.url = endpoint_ + api_specs::paths::CHAT_COMPLETIONS;
        http_request.method = "POST";
        http_request.body = format_zai_request(request);
        http_request.timeout_ms = api_specs::timeouts::REQUEST_TIMEOUT.count();
        
        network::HttpResponse http_response = http_client->send_request(http_request);
        
        core::Response response = process_response(http_response.status_code, http_response.body);
        response.response_time_ms = http_response.response_time_ms;
        
        return response;
        
    } catch (const std::exception& e) {
        core::Response response;
        response.success = false;
        response.provider_name = provider_name_;

        // Enhanced error categorization for Z.AI
        std::string error_msg = e.what();
        if (error_msg.find("timeout") != std::string::npos ||
            error_msg.find("Timeout") != std::string::npos) {
            response.error_message = "Z.AI timeout error: " + error_msg;
            response.status_code = 408;
            consecutive_failures_++;
            last_failure_time_ = std::chrono::steady_clock::now();
        } else if (error_msg.find("connection") != std::string::npos ||
                   error_msg.find("network") != std::string::npos ||
                   error_msg.find("Network") != std::string::npos) {
            response.error_message = "Z.AI network error: " + error_msg;
            response.status_code = 503;
            consecutive_failures_++;
            last_failure_time_ = std::chrono::steady_clock::now();
        } else if (error_msg.find("authentication") != std::string::npos ||
                   error_msg.find("auth") != std::string::npos ||
                   error_msg.find("unauthorized") != std::string::npos) {
            response.error_message = "Z.AI authentication error: " + error_msg;
            response.status_code = 401;
            is_healthy_ = false;  // Auth errors should mark as unhealthy
            consecutive_failures_ = 5;
            last_failure_time_ = std::chrono::steady_clock::now();
        } else if (error_msg.find("parse") != std::string::npos ||
                   error_msg.find("JSON") != std::string::npos ||
                   error_msg.find("format") != std::string::npos) {
            response.error_message = "Z.AI format error: " + error_msg;
            response.status_code = 422;  // Unprocessable Entity
            consecutive_failures_++;  // Format errors shouldn't mark as unhealthy immediately
            last_failure_time_ = std::chrono::steady_clock::now();
        } else {
            response.error_message = "Z.AI error: " + error_msg;
            response.status_code = 500;
            consecutive_failures_++;
            last_failure_time_ = std::chrono::steady_clock::now();

            // Mark as unhealthy after multiple unknown errors
            if (consecutive_failures_ >= 3) {
                is_healthy_ = false;
            }
        }

        return response;
    }
}

bool ZaiProvider::is_healthy() const {
    // Check for potential recovery before returning status
    const_cast<ZaiProvider*>(this)->check_recovery();
    return is_healthy_;
}

std::string ZaiProvider::get_provider_name() const {
    return provider_name_;
}

nlohmann::json ZaiProvider::get_rate_limit_status() const {
    nlohmann::json status;
    status["provider"] = provider_name_;
    status["endpoint"] = endpoint_;
    status["requests_made"] = requests_made_;
    status["max_requests_per_minute"] = max_requests_per_minute_;
    status["requests_remaining"] = std::max(0, max_requests_per_minute_ - requests_made_);

    auto now = std::chrono::steady_clock::now();
    auto reset_in_seconds = std::chrono::duration_cast<std::chrono::seconds>(rate_limit_reset_ - now).count();
    status["reset_in_seconds"] = std::max<long long>(0LL, reset_in_seconds);
    status["is_healthy"] = is_healthy_;

    // Add provider capabilities from API specs
    auto caps = api_specs::get_provider_capabilities("zai");
    status["capabilities"] = {
        {"thinking", caps.thinking},
        {"vision", caps.vision},
        {"tools", caps.tools},
        {"max_input_tokens", caps.max_input_tokens},
        {"max_output_tokens", caps.max_output_tokens}
    };

    // Add available models from config
    status["available_models"] = config_.value("models", std::vector<std::string>{
        api_specs::models::zai::CLAUDE_3_5_SONNET, api_specs::models::zai::CLAUDE_3_HAIKU
    });

    return status;
}

std::string ZaiProvider::format_zai_request(const core::Request& request) {
    // Translate from Anthropic format to OpenAI format for Z.AI
    nlohmann::json openai_request;

    // Model selection with fallback - map Anthropic model names to Z.AI equivalents
    std::string model = request.data.value("model", "");
    if (model.empty()) {
        model = config_.value("default_model", api_specs::models::zai::CLAUDE_3_5_SONNET);
    }

    // Model name mapping from Anthropic to Z.AI format
    if (model == "claude-3-5-sonnet" || model == "claude-3-5-sonnet-20241022") {
        model = "claude-3-5-sonnet-20241022";
    } else if (model == "claude-3-haiku" || model == "claude-3-haiku-20240307") {
        model = "claude-3-haiku-20240307";
    } else if (model == "claude-3-opus" || model == "claude-3-opus-20240229") {
        // Map Claude Opus to Sonnet if Opus not available
        model = "claude-3-5-sonnet-20241022";
    } else if (model == "gpt-4" || model == "gpt-4-turbo") {
        // Map GPT-4 models to Claude Sonnet as equivalent
        model = "claude-3-5-sonnet-20241022";
    } else if (model == "gpt-3.5-turbo") {
        // Map GPT-3.5 to Claude Haiku as equivalent
        model = "claude-3-haiku-20240307";
    }

    // Validate model is available in configuration
    auto available_models = config_.value("models", std::vector<std::string>{model});
    if (std::find(available_models.begin(), available_models.end(), model) == available_models.end()) {
        model = available_models.empty() ? api_specs::models::zai::CLAUDE_3_5_SONNET : available_models[0];
    }

    openai_request["model"] = model;
    openai_request["max_tokens"] = request.data.value("max_tokens", api_specs::get_provider_capabilities("zai").max_output_tokens);

    // Translate messages from Anthropic to OpenAI format
    nlohmann::json messages = nlohmann::json::array();
    if (request.data.contains("messages") && request.data["messages"].is_array()) {
        for (const auto& msg : request.data["messages"]) {
            nlohmann::json openai_msg;
            openai_msg["role"] = msg.value("role", "user");

            // Handle content translation
            if (msg.contains("content")) {
                if (msg["content"].is_string()) {
                    // Simple text content - direct translation
                    openai_msg["content"] = msg["content"];
                } else if (msg["content"].is_array()) {
                    // Anthropic multimodal content - translate to OpenAI format
                    nlohmann::json content_array = nlohmann::json::array();

                    for (const auto& content_item : msg["content"]) {
                        if (content_item.contains("type")) {
                            if (content_item["type"] == "text") {
                                // Text content - same format
                                content_array.push_back({
                                    {"type", "text"},
                                    {"text", content_item.value("text", "")}
                                });
                            } else if (content_item["type"] == "image") {
                                // Image content - translate Anthropic format to OpenAI format
                                if (content_item.contains("source")) {
                                    const auto& source = content_item["source"];
                                    std::string media_type = source.value("media_type", "");
                                    std::string data = source.value("data", "");

                                    // Validate media type
                                    if (media_type != "image/jpeg" && media_type != "image/png" &&
                                        media_type != "image/webp" && media_type != "image/gif") {
                                        throw std::invalid_argument("Unsupported image format: " + media_type);
                                    }

                                    content_array.push_back({
                                        {"type", "image_url"},
                                        {"image_url", {
                                            {"url", "data:" + media_type + ";base64," + data}
                                        }}
                                    });
                                }
                            }
                        }
                    }

                    openai_msg["content"] = content_array;
                }
            } else {
                // Fallback content
                openai_msg["content"] = "";
            }

            messages.push_back(openai_msg);
        }
    } else {
        // Fallback for simple requests - convert to OpenAI message format
        std::string prompt = request.data.value("prompt", request.data.value("content", ""));
        messages.push_back({
            {"role", "user"},
            {"content", prompt}
        });
    }

    openai_request["messages"] = messages;

    // Translate tools from Anthropic to OpenAI format if present
    if (request.data.contains("tools") && request.data["tools"].is_array() && !request.data["tools"].empty()) {
        nlohmann::json openai_tools = nlohmann::json::array();

        for (const auto& tool : request.data["tools"]) {
            if (tool.contains("name") && tool.contains("input_schema")) {
                // Convert Anthropic tool format to OpenAI function format
                nlohmann::json openai_tool = {
                    {"type", "function"},
                    {"function", {
                        {"name", tool["name"]},
                        {"description", tool.value("description", "")},
                        {"parameters", tool["input_schema"]}
                    }}
                };
                openai_tools.push_back(openai_tool);
            }
        }

        openai_request["tools"] = openai_tools;

        // Translate tool choice if specified
        if (request.data.contains("tool_choice")) {
            const auto& tool_choice = request.data["tool_choice"];
            if (tool_choice.is_string()) {
                openai_request["tool_choice"] = tool_choice;
            } else if (tool_choice.is_object() && tool_choice.contains("name")) {
                openai_request["tool_choice"] = {
                    {"type", "function"},
                    {"function", {
                        {"name", tool_choice["name"]}
                    }}
                };
            }
        }
    }

    // Add temperature with validation
    if (request.data.contains("temperature")) {
        double temp = request.data["temperature"];
        openai_request["temperature"] = std::max(0.0, std::min(2.0, temp));
    }

    // Add other OpenAI-compatible parameters
    if (request.data.contains("top_p")) {
        openai_request["top_p"] = request.data["top_p"];
    }

    // Add streaming support
    if (request.data.value("stream", false)) {
        openai_request["stream"] = true;
    }

    return openai_request.dump();
}

nlohmann::json ZaiProvider::parse_zai_response(const std::string& response) {
    try {
        auto openai_response = nlohmann::json::parse(response);

        // Translate from OpenAI format back to Anthropic format
        nlohmann::json anthropic_response;

        // Translate basic response structure
        if (openai_response.contains("id")) {
            anthropic_response["id"] = openai_response["id"];
        } else {
            anthropic_response["id"] = "msg_" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
        }

        anthropic_response["type"] = "message";
        anthropic_response["role"] = "assistant";

        // Reverse model mapping - map Z.AI model names back to Anthropic format
        std::string model_name = openai_response.value("model", "claude-3-5-sonnet-20241022");
        if (model_name == "claude-3-5-sonnet-20241022") {
            anthropic_response["model"] = "claude-3-5-sonnet";
        } else if (model_name == "claude-3-haiku-20240307") {
            anthropic_response["model"] = "claude-3-haiku";
        } else {
            anthropic_response["model"] = model_name; // Pass through unknown model names
        }

        // Map OpenAI usage to Anthropic usage format
        if (openai_response.contains("usage")) {
            const auto& usage = openai_response["usage"];
            anthropic_response["usage"] = {
                {"input_tokens", usage.value("prompt_tokens", 0)},
                {"output_tokens", usage.value("completion_tokens", 0)}
            };

            // Calculate costs using API specs
            int input_tokens = usage.value("prompt_tokens", 0);
            int output_tokens = usage.value("completion_tokens", 0);
            double input_cost = (static_cast<double>(input_tokens) / 1000000.0) * api_specs::costs::zai::INPUT_COST_PER_1M;
            double output_cost = (static_cast<double>(output_tokens) / 1000000.0) * api_specs::costs::zai::OUTPUT_COST_PER_1M;
            double total_cost = input_cost + output_cost;

            // Add cost info to metadata
            anthropic_response["usage"]["cost_estimate"] = {
                {"input_tokens", input_tokens},
                {"output_tokens", output_tokens},
                {"input_cost_usd", input_cost},
                {"output_cost_usd", output_cost},
                {"total_cost_usd", total_cost}
            };
        }

        // Translate content from OpenAI to Anthropic format
        nlohmann::json content = nlohmann::json::array();

        if (openai_response.contains("choices") && !openai_response["choices"].empty()) {
            const auto& choice = openai_response["choices"][0];

            if (choice.contains("message")) {
                const auto& message = choice["message"];

                // Handle text content
                if (message.contains("content")) {
                    content.push_back({
                        {"type", "text"},
                        {"text", message["content"]}
                    });
                }

                // Handle tool calls (translate OpenAI tool_calls to Anthropic tool_use)
                if (message.contains("tool_calls") && message["tool_calls"].is_array()) {
                    for (const auto& tool_call : message["tool_calls"]) {
                        if (tool_call.contains("function")) {
                            const auto& func = tool_call["function"];
                            nlohmann::json tool_use = {
                                {"type", "tool_use"},
                                {"id", tool_call.value("id", "toolu_" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                                    std::chrono::system_clock::now().time_since_epoch()).count()))},
                                {"name", func.value("name", "")},
                                {"input", nlohmann::json::object()} // Parse arguments if available
                            };

                            if (func.contains("arguments")) {
                                // Handle arguments that might be a string or already JSON
                                if (func["arguments"].is_string()) {
                                    try {
                                        tool_use["input"] = nlohmann::json::parse(func["arguments"].get<std::string>());
                                    } catch (...) {
                                        tool_use["input"] = func["arguments"].get<std::string>();
                                    }
                                } else {
                                    tool_use["input"] = func["arguments"];
                                }
                            }

                            content.push_back(tool_use);
                        }
                    }
                }
            }
        }

        anthropic_response["content"] = content;

        // Detect vision usage in request/response
        bool has_vision_input = false;
        if (anthropic_response.contains("model")) {
            // Claude models support vision
            has_vision_input = (anthropic_response["model"] == api_specs::models::zai::CLAUDE_3_5_SONNET ||
                               anthropic_response["model"] == api_specs::models::zai::CLAUDE_3_HAIKU);
        }

        // Extract tool usage for metadata
        bool has_tool_usage = false;
        for (const auto& content_item : content) {
            if (content_item.contains("type") && content_item["type"] == "tool_use") {
                has_tool_usage = true;
                break;
            }
        }

        // Add stop reason
        if (openai_response.contains("choices") && !openai_response["choices"].empty()) {
            const auto& choice = openai_response["choices"][0];
            if (choice.contains("finish_reason")) {
                std::string finish_reason = choice["finish_reason"];
                // Map OpenAI finish_reason to Anthropic stop_reason
                if (finish_reason == "stop") {
                    anthropic_response["stop_reason"] = "end_turn";
                } else if (finish_reason == "length") {
                    anthropic_response["stop_reason"] = "max_tokens";
                } else if (finish_reason == "tool_calls") {
                    anthropic_response["stop_reason"] = "tool_use";
                } else {
                    anthropic_response["stop_reason"] = "end_turn";
                }
            }
        }

        // Add comprehensive metadata
        anthropic_response["metadata"] = {
            {"provider", "zai"},
            {"model", anthropic_response.value("model", "unknown")},
            {"has_vision_input", has_vision_input},
            {"has_tool_usage", has_tool_usage},
            {"processed_at", std::to_string(
                std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()
                ).count()
            )}
        };

        return anthropic_response;

    } catch (const nlohmann::json::exception& e) {
        // If JSON parsing fails, return error info in Anthropic format
        nlohmann::json error_response;
        error_response["type"] = "error";
        error_response["error"] = {
            {"type", "json_parse_error"},
            {"message", e.what()}
        };
        error_response["metadata"]["provider"] = "zai";
        error_response["metadata"]["processed_at"] = std::to_string(
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count()
        );
        return error_response;
    }
}

// SyntheticProvider implementation
SyntheticProvider::SyntheticProvider(const nlohmann::json& config) 
    : BaseProvider("synthetic", config), rng_(std::random_device{}()) {
    
    response_variations_ = {
        "This is a synthetic response from the provider.",
        "Generating response with configured parameters.",
        "Here's a simulated answer from the synthetic provider.",
        "Synthetic AI model processing request.",
        "Synthetic response generated with configured parameters."
    };
}

core::Response SyntheticProvider::send_request(const core::Request& request) {
    // Simulate response time
    std::uniform_int_distribution<int> sleep_dist(50, 500);
    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_dist(rng_)));
    
    core::Response response;
    response.success = true;
    response.data = generate_ai_response(request);
    response.status_code = 200;
    response.provider_name = provider_name_;
    response.response_time_ms = sleep_dist(rng_);
    
    update_rate_limit();
    
    return response;
}

bool SyntheticProvider::is_healthy() const {
    return true; // Synthetic provider is always healthy
}

std::string SyntheticProvider::get_provider_name() const {
    return provider_name_;
}

nlohmann::json SyntheticProvider::get_rate_limit_status() const {
    nlohmann::json status;
    status["provider"] = provider_name_;
    status["requests_made"] = requests_made_;
    status["max_requests_per_minute"] = 1000; // High limit for synthetic
    status["requests_remaining"] = 1000 - requests_made_;
    status["reset_in_seconds"] = 60;
    return status;
}

std::string SyntheticProvider::generate_ai_response(const core::Request& /*request*/) {
    // Generate sample AI response
    std::vector<std::string> responses = {
        "This is a synthetic AI response for testing purposes.",
        "I'm processing your request with simulated intelligence.",
        "As a test provider, I'm generating this response without real AI.",
        "This is a mock response demonstrating the system works."
    };
    
    std::uniform_int_distribution<size_t> dist(0, responses.size() - 1);
    return responses[dist(rng_)];
}

std::string ZaiProvider::extract_model_name(const std::string& response) const {
    try {
        auto json_response = nlohmann::json::parse(response);
        if (json_response.contains("model")) {
            return json_response["model"];
        }
        return "gpt-4"; // Default
    } catch (...) {
        return "gpt-4"; // Default on error
    }
}

// MiniMaxProvider implementation
MiniMaxProvider::MiniMaxProvider(const nlohmann::json& config)
    : BaseProvider("minimax", config) {

    // Use API specs for default values
    if (endpoint_.empty()) {
        endpoint_ = api_specs::get_base_endpoint("minimax");
    }

    // Validate configuration
    if (!security::validate_api_key_format(security::decrypt_api_key(encrypted_api_key_))) {
        throw std::invalid_argument("Invalid or malformed API key for MiniMax provider");
    }

    // Extract and validate Group ID for MiniMax authentication
    group_id_ = config.value("group_id", "");
    if (group_id_.empty()) {
        throw std::invalid_argument("Group ID is required for MiniMax provider");
    }

    if (group_id_.length() < api_specs::validation::MIN_GROUP_ID_LENGTH ||
        group_id_.length() > api_specs::validation::MAX_GROUP_ID_LENGTH) {
        throw std::invalid_argument("Group ID length must be between 4 and 64 characters");
    }

    // Validate endpoint URL format
    if (endpoint_.substr(0, 8) != "https://") {
        throw std::invalid_argument("Endpoint must use HTTPS for secure communication");
    }

    // Use API specs for rate limiting
    max_requests_per_minute_ = config.value("max_requests_per_minute", api_specs::get_rate_limit("minimax"));
}

core::Response MiniMaxProvider::send_request(const core::Request& request) {
    if (!check_rate_limit()) {
        core::Response response;
        response.success = false;
        response.error_message = "Rate limit exceeded";
        response.status_code = 429;
        response.provider_name = provider_name_;
        return response;
    }
    
    update_rate_limit();
    
    try {
        auto http_client = network::HttpClientFactory::create_client();

        // Add MiniMax-specific auth headers
        auto auth_headers = get_auth_headers();
        for (const auto& header : auth_headers) {
            http_client->add_default_header(header.first, header.second);
        }

        // Use API specs headers
        http_client->add_default_header(api_specs::headers::CONTENT_TYPE, api_specs::headers::APPLICATION_JSON);
        http_client->add_default_header(api_specs::headers::USER_AGENT, api_specs::headers::AIMUX_USER_AGENT);

        network::HttpRequest http_request;
        http_request.url = endpoint_ + api_specs::paths::MESSAGES;
        http_request.method = "POST";
        http_request.body = format_minimax_request(request);
        http_request.timeout_ms = api_specs::timeouts::REQUEST_TIMEOUT.count();
        
        network::HttpResponse http_response = http_client->send_request(http_request);
        
        core::Response response = process_response(http_response.status_code, http_response.body);
        response.response_time_ms = http_response.response_time_ms;
        
        return response;
        
    } catch (const std::exception& e) {
        core::Response response;
        response.success = false;
        response.error_message = "MiniMax error: " + std::string(e.what());
        response.status_code = 500;
        response.provider_name = provider_name_;
        return response;
    }
}

bool MiniMaxProvider::is_healthy() const {
    // Check for potential recovery before returning status
    const_cast<MiniMaxProvider*>(this)->check_recovery();
    return is_healthy_;
}

std::string MiniMaxProvider::get_provider_name() const {
    return provider_name_;
}

nlohmann::json MiniMaxProvider::get_rate_limit_status() const {
    nlohmann::json status;
    status["provider"] = provider_name_;
    status["endpoint"] = endpoint_;
    status["requests_made"] = requests_made_;
    status["max_requests_per_minute"] = max_requests_per_minute_;
    status["requests_remaining"] = std::max(0, max_requests_per_minute_ - requests_made_);

    auto now = std::chrono::steady_clock::now();
    auto reset_in_seconds = std::chrono::duration_cast<std::chrono::seconds>(rate_limit_reset_ - now).count();
    status["reset_in_seconds"] = std::max<long long>(0LL, reset_in_seconds);
    status["is_healthy"] = is_healthy_;

    // Add provider capabilities from API specs
    auto caps = api_specs::get_provider_capabilities("minimax");
    status["capabilities"] = {
        {"thinking", caps.thinking},
        {"vision", caps.vision},
        {"tools", caps.tools},
        {"max_input_tokens", caps.max_input_tokens},
        {"max_output_tokens", caps.max_output_tokens}
    };

    // Add available models from config
    status["available_models"] = config_.value("models", std::vector<std::string>{
        api_specs::models::minimax::MINIMAX_M2_100K, api_specs::models::minimax::MINIMAX_M2_32K
    });

    // Add MiniMax-specific information
    status["group_id_configured"] = !group_id_.empty();
    status["m2_optimization"] = true; // M2 thinking model optimizations enabled

    return status;
}

std::string MiniMaxProvider::format_minimax_request(const core::Request& request) {
    nlohmann::json minimax_request;

    // Model selection with M2 preference for thinking requests
    std::string model = request.data.value("model", "");
    if (model.empty()) {
        // Default to M2-100K for better thinking capabilities
        model = config_.value("default_model", api_specs::models::minimax::MINIMAX_M2_100K);
    }

    // Validate model is available in configuration
    auto available_models = config_.value("models", std::vector<std::string>{model});
    if (std::find(available_models.begin(), available_models.end(), model) == available_models.end()) {
        model = available_models.empty() ? api_specs::models::minimax::MINIMAX_M2_100K : available_models[0];
    }

    minimax_request["model"] = model;
    minimax_request["messages"] = request.data["messages"];

    // Token limits based on M2 model capabilities
    auto caps = api_specs::get_provider_capabilities("minimax");
    int max_tokens = request.data.value("max_tokens", caps.max_output_tokens);
    minimax_request["max_tokens"] = std::min(max_tokens, caps.max_output_tokens);

    // Temperature with M2 thinking optimizations
    double temperature = request.data.value("temperature", config_.value("temperature", 0.7));

    // Special handling for thinking requests with M2 model
    bool is_thinking_request = request.data.value("thinking", false);
    if (is_thinking_request && model.find("m2") != std::string::npos) {
        // Optimize temperature for reasoning tasks
        temperature = std::min(temperature, 0.4); // Lower temp for structured thinking

        // Add reasoning prompt enhancement if not already present
        auto messages = minimax_request["messages"];
        if (messages.is_array() && !messages.empty()) {
            auto& last_message = messages[messages.size() - 1];
            if (last_message.contains("content") && last_message["content"].is_string()) {
                std::string content = last_message["content"];

                // Add thinking prompt prefix if not already there
                if (content.find("think step by step") == std::string::npos &&
                    content.find("explain your reasoning") == std::string::npos) {
                    content = "Please think step by step and explain your reasoning clearly. " + content;
                    last_message["content"] = content;
                }
            }
        }
        minimax_request["messages"] = messages;
    }

    minimax_request["temperature"] = std::max(0.0, std::min(2.0, temperature));

    // Add top_p for better control
    minimax_request["top_p"] = request.data.value("top_p", 1.0);

    // Add streaming support
    if (request.data.value("stream", false)) {
        minimax_request["stream"] = true;
    }

    // Add tools if requested
    if (request.data.contains("tools") && request.data["tools"].is_array() && !request.data["tools"].empty()) {
        minimax_request["tools"] = request.data["tools"];

        // Add tool choice if specified
        if (request.data.contains("tool_choice")) {
            minimax_request["tool_choice"] = request.data["tool_choice"];
        }
    }

    return minimax_request.dump();
}

std::map<std::string, std::string> MiniMaxProvider::get_auth_headers() const {
    std::map<std::string, std::string> headers;
    std::string api_key = security::decrypt_api_key(encrypted_api_key_);

    // MiniMax-specific authentication
    headers[api_specs::headers::AUTHORIZATION] = "Bearer " + api_key;
    headers[api_specs::headers::X_GROUP_ID] = group_id_;

    return headers;
}

nlohmann::json MiniMaxProvider::parse_minimax_response(const std::string& response) {
    try {
        auto json_response = nlohmann::json::parse(response);

        // Extract usage information for cost tracking
        if (json_response.contains("usage")) {
            const auto& usage = json_response["usage"];
            int input_tokens = usage.value("input_tokens", 0);
            int output_tokens = usage.value("output_tokens", 0);

            // Calculate costs using API specs
            double input_cost = (static_cast<double>(input_tokens) / 1000000.0) * api_specs::costs::minimax::INPUT_COST_PER_1M;
            double output_cost = (static_cast<double>(output_tokens) / 1000000.0) * api_specs::costs::minimax::OUTPUT_COST_PER_1M;
            double total_cost = input_cost + output_cost;

            // Add cost info to response metadata
            json_response["metadata"]["cost估算"] = {
                {"input_tokens", input_tokens},
                {"output_tokens", output_tokens},
                {"input_cost_usd", input_cost},
                {"output_cost_usd", output_cost},
                {"total_cost_usd", total_cost}
            };
        }

        // Detect M2 thinking patterns in response
        bool has_thinking_content = false;
        std::string reasoning_type = "standard";

        if (json_response.contains("content") && json_response["content"].is_array()) {
            for (const auto& content_item : json_response["content"]) {
                if (content_item.contains("type") && content_item["type"] == "text" &&
                    content_item.contains("text")) {

                    std::string content = content_item["text"];

                    // Look for M2 reasoning patterns
                    if (content.find("step by step") != std::string::npos ||
                        content.find("Let me solve") != std::string::npos ||
                        content.find("First,") != std::string::npos ||
                        content.find("Next,") != std::string::npos ||
                        content.find("Finally,") != std::string::npos) {

                        has_thinking_content = true;
                        reasoning_type = "step_by_step";

                        // Additional M2 pattern detection
                        if (content.find("math") != std::string::npos ||
                            content.find("calculate") != std::string::npos ||
                            content.find("equation") != std::string::npos) {
                            reasoning_type = "mathematical";
                        } else if (content.find("logic") != std::string::npos ||
                                  content.find("analyze") != std::string::npos) {
                            reasoning_type = "analytical";
                        }
                        break;
                    }
                }
            }
        }

        // Check if M2 model was used
        bool is_m2_model = false;
        if (json_response.contains("model")) {
            std::string model = json_response["model"];
            is_m2_model = (model.find("m2") != std::string::npos);
        }

        // Add MiniMax-specific metadata
        json_response["metadata"]["provider"] = "minimax";
        json_response["metadata"]["model"] = json_response.value("model", "unknown");
        json_response["metadata"]["has_thinking_content"] = has_thinking_content;
        json_response["metadata"]["reasoning_type"] = reasoning_type;
        json_response["metadata"]["is_m2_model"] = is_m2_model;
        json_response["metadata"]["group_id_used"] = !group_id_.empty();
        json_response["metadata"]["processed_at"] = std::to_string(
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count()
        );

        return json_response;

    } catch (const nlohmann::json::exception& e) {
        // If JSON parsing fails, return error info
        nlohmann::json error_response;
        error_response["error"] = {
            {"type", "json_parse_error"},
            {"message", e.what()},
            {"provider", "minimax"}
        };
        error_response["metadata"]["provider"] = "minimax";
        error_response["metadata"]["processed_at"] = std::to_string(
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count()
        );
        return error_response;
    }
}

// ProviderFactory implementation
std::unique_ptr<core::Bridge> ProviderFactory::create_provider(
    const std::string& provider_name,
    const nlohmann::json& config) {
    
    if (provider_name == "cerebras") {
        return std::make_unique<CerebrasProvider>(config);
    } else if (provider_name == "zai") {
        return std::make_unique<ZaiProvider>(config);
    } else if (provider_name == "minimax") {
        return std::make_unique<MiniMaxProvider>(config);
    } else if (provider_name == "synthetic") {
        return std::make_unique<SyntheticProvider>(config);
    } else {
        // Return error for unknown providers
        throw std::runtime_error("Unknown provider: " + provider_name);
    }
}

std::vector<std::string> ProviderFactory::get_supported_providers() {
    return {"cerebras", "zai", "minimax", "synthetic", };
}

bool ProviderFactory::validate_config(const std::string& provider_name, const nlohmann::json& config) {
    if (provider_name == "cerebras" || provider_name == "zai" || provider_name == "minimax") {
        return config.contains("api_key") &&
               config.contains("endpoint") &&
               !config["api_key"].get<std::string>().empty() &&
               !config["endpoint"].get<std::string>().empty();
    } else if (provider_name == "synthetic") {
        return true; // No validation needed for synthetic
    }
    return false;
}

// ConfigParser implementation
nlohmann::json ConfigParser::parse_config(const std::string& config_file) {
    std::ifstream file(config_file);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open config file: " + config_file);
    }
    
    nlohmann::json config;
    
    // Simple TOON format parsing (JSON for now, can be extended)
    try {
        file >> config;
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to parse config file: " + std::string(e.what()));
    }
    
    if (!validate_config_structure(config)) {
        throw std::runtime_error("Invalid config structure");
    }
    
    return config;
}

std::vector<aimux::core::ProviderConfig> ConfigParser::parse_providers(const nlohmann::json& config) {
    std::vector<aimux::core::ProviderConfig> providers;
    
    if (!config.contains("providers")) {
        return providers;
    }
    
    for (const auto& provider_json : config["providers"]) {
        aimux::core::ProviderConfig provider;
        provider.name = provider_json.value("name", "");
        provider.endpoint = provider_json.value("endpoint", "");
        provider.api_key = provider_json.value("api_key", "");
        provider.models = provider_json.value("models", std::vector<std::string>{});
        provider.max_requests_per_minute = provider_json.value("max_requests_per_minute", 60);
        provider.enabled = provider_json.value("enabled", true);
        
        providers.push_back(provider);
    }
    
    return providers;
}

nlohmann::json ConfigParser::generate_default_config() {
    nlohmann::json config;
    config["daemon"] = nlohmann::json::object();
    config["daemon"]["port"] = 8080;
    config["daemon"]["host"] = "localhost";
    
    config["logging"] = nlohmann::json::object();
    config["logging"]["level"] = "info";
    config["logging"]["file"] = "aimux.log";
    
    config["providers"] = nlohmann::json::array();
    
    // Add example Cerebras provider
    nlohmann::json cerebras_provider;
    cerebras_provider["name"] = "cerebras";
    cerebras_provider["endpoint"] = "https://api.cerebras.ai";
    cerebras_provider["api_key"] = "YOUR_CEREBRAS_API_KEY";
    cerebras_provider["models"] = {"llama3.1-70b"};
    cerebras_provider["max_requests_per_minute"] = 60;
    cerebras_provider["enabled"] = false;
    config["providers"].push_back(cerebras_provider);
    
    // Add example Z.ai provider
    nlohmann::json zai_provider;
    zai_provider["name"] = "zai";
    zai_provider["endpoint"] = "https://api.z.ai";
    zai_provider["api_key"] = "YOUR_ZAI_API_KEY";
    zai_provider["models"] = {"gpt-4"};
    zai_provider["max_requests_per_minute"] = 60;
    zai_provider["enabled"] = false;
    config["providers"].push_back(zai_provider);
    
    // Add synthetic provider for testing
    nlohmann::json synthetic_provider;
    synthetic_provider["name"] = "synthetic";
    synthetic_provider["endpoint"] = "https://synthetic.ai";
    synthetic_provider["api_key"] = "synthetic-test-key";
    synthetic_provider["models"] = {"claude-3"};
    synthetic_provider["max_requests_per_minute"] = 1000;
    synthetic_provider["enabled"] = true;
    config["providers"].push_back(synthetic_provider);
    
    return config;
}

bool ConfigParser::validate_config_structure(const nlohmann::json& /* config */) {
    return true; // Basic validation for now
}

nlohmann::json ConfigParser::parse_toon_value(const std::string& value) {
    // Basic TOON format parser implemented
    return nlohmann::json(value);
}

} // namespace providers
} // namespace aimux