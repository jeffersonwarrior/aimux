#pragma once

#include <string>
#include <unordered_map>
#include <chrono>

namespace aimux {
namespace providers {

/**
 * @brief API specifications and constants for all providers
 */
namespace api_specs {

// Provider endpoints
namespace endpoints {
    constexpr const char* CEREBRAS_BASE = "https://api.cerebras.ai/v1";
    constexpr const char* ZAI_BASE = "https://api.z.ai/api/paas/v4";
    constexpr const char* MINIMAX_BASE = "https://api.minimax.io/anthropic";
}

// Rate limits (requests per minute)
namespace rate_limits {
    constexpr int CEREBRAS_RPM = 100;  // TBD - confirm with docs
    constexpr int ZAI_RPM = 100;
    constexpr int MINIMAX_RPM = 60;
}

// Model identifiers
namespace models {
    // Cerebras models
    namespace cerebras {
        constexpr const char* LLAMA3_1_70B = "llama3.1-70b";
        constexpr const char* LLAMA3_1_8B = "llama3.1-8b";
    }

    // Z.AI models
    namespace zai {
        constexpr const char* CLAUDE_3_5_SONNET = "claude-3-5-sonnet-20241022";
        constexpr const char* CLAUDE_3_HAIKU = "claude-3-haiku-20240307";
    }

    // MiniMax models
    namespace minimax {
        constexpr const char* MINIMAX_M2_100K = "minimax-m2-100k";
        constexpr const char* MINIMAX_M2_32K = "minimax-m2-32k";
    }
}

// Capabilities per provider
namespace capabilities {
    struct ProviderCapabilities {
        bool thinking = false;
        bool vision = false;
        bool tools = false;
        int max_input_tokens = 4000;
        int max_output_tokens = 4000;
    };

    const ProviderCapabilities CEREBRAS_CAPS = {
        .thinking = true,
        .vision = false,
        .tools = true,
        .max_input_tokens = 8000,
        .max_output_tokens = 4000
    };

    const ProviderCapabilities ZAI_CAPS = {
        .thinking = false,
        .vision = true,
        .tools = true,
        .max_input_tokens = 100000,
        .max_output_tokens = 4096
    };

    const ProviderCapabilities MINIMAX_CAPS = {
        .thinking = true,
        .vision = false,
        .tools = true,
        .max_input_tokens = 100000,
        .max_output_tokens = 8192
    };
}

// HTTP headers
namespace headers {
    constexpr const char* AUTHORIZATION = "Authorization";
    constexpr const char* CONTENT_TYPE = "Content-Type";
    constexpr const char* X_GROUP_ID = "X-GroupId";
    constexpr const char* USER_AGENT = "User-Agent";
    constexpr const char* ACCEPT = "Accept";

    constexpr const char* APPLICATION_JSON = "application/json";
    constexpr const char* AIMUX_USER_AGENT = "aimux/2.0.0";
}

// API endpoints relative to base URLs
namespace paths {
    constexpr const char* MODELS = "/models";
    constexpr const char* CHAT_COMPLETIONS = "/chat/completions";
    constexpr const char* MESSAGES = "/messages";
}

// Error codes
namespace errors {
    enum class ErrorCode {
        UNKNOWN = 0,
        NETWORK_ERROR = 1,
        AUTHENTICATION_FAILED = 2,
        RATE_LIMIT_EXCEEDED = 3,
        INVALID_REQUEST = 4,
        MODEL_NOT_FOUND = 5,
        SERVER_ERROR = 6,
        TIMEOUT = 7,
        INVALID_RESPONSE = 8
    };

    const std::unordered_map<ErrorCode, const char*> ERROR_MESSAGES = {
        {ErrorCode::UNKNOWN, "Unknown error occurred"},
        {ErrorCode::NETWORK_ERROR, "Network connection failed"},
        {ErrorCode::AUTHENTICATION_FAILED, "Authentication failed - check API key"},
        {ErrorCode::RATE_LIMIT_EXCEEDED, "Rate limit exceeded - retry later"},
        {ErrorCode::INVALID_REQUEST, "Invalid request format"},
        {ErrorCode::MODEL_NOT_FOUND, "Requested model not found"},
        {ErrorCode::SERVER_ERROR, "Provider server error"},
        {ErrorCode::TIMEOUT, "Request timeout"},
        {ErrorCode::INVALID_RESPONSE, "Invalid response format from provider"}
    };
}

// Timeout configurations
namespace timeouts {
    constexpr std::chrono::milliseconds CONNECTION_TIMEOUT{30000};  // 30 seconds
    constexpr std::chrono::milliseconds REQUEST_TIMEOUT{120000};     // 2 minutes
    constexpr std::chrono::milliseconds RATE_LIMIT_RETRY{60000};     // 1 minute
}

// Provider configuration validation
namespace validation {
    constexpr int MIN_API_KEY_LENGTH = 16;
    constexpr int MAX_API_KEY_LENGTH = 256;
    constexpr int MIN_GROUP_ID_LENGTH = 4;
    constexpr int MAX_GROUP_ID_LENGTH = 64;

    // Valid API key format (alphanumeric + standard symbols)
    constexpr const char* API_KEY_PATTERN = "^[a-zA-Z0-9._/-]+$";
}

// Performance metrics
namespace metrics {
    struct ProviderMetrics {
        int total_requests = 0;
        int successful_requests = 0;
        int failed_requests = 0;
        double average_response_time_ms = 0.0;
        std::chrono::steady_clock::time_point last_request_time;
        std::chrono::steady_clock::time_point last_success_time;
        std::chrono::steady_clock::time_point last_failure_time;
        bool is_healthy = true;
        int consecutive_failures = 0;
    };
}

// Cost tracking (approximate costs per million tokens)
namespace costs {
    namespace cerebras {
        constexpr double INPUT_COST_PER_1M = 0.50;  // $0.50 per 1M input tokens
        constexpr double OUTPUT_COST_PER_1M = 1.50;  // $1.50 per 1M output tokens
    }

    namespace zai {
        constexpr double INPUT_COST_PER_1M = 3.00;   // $3.00 per 1M input tokens
        constexpr double OUTPUT_COST_PER_1M = 15.00;  // $15.00 per 1M output tokens
    }

    namespace minimax {
        constexpr double INPUT_COST_PER_1M = 0.20;   // $0.20 per 1M input tokens
        constexpr double OUTPUT_COST_PER_1M = 0.60;   // $0.60 per 1M output tokens
    }
}

/**
 * @brief Get capabilities for provider type
 */
inline capabilities::ProviderCapabilities get_provider_capabilities(const std::string& provider_type) {
    if (provider_type == "cerebras") {
        return capabilities::CEREBRAS_CAPS;
    } else if (provider_type == "zai") {
        return capabilities::ZAI_CAPS;
    } else if (provider_type == "minimax") {
        return capabilities::MINIMAX_CAPS;
    }

    // Default caps (synthetic/fallback)
    return capabilities::ProviderCapabilities{};
}

/**
 * @brief Get rate limit for provider type
 */
inline int get_rate_limit(const std::string& provider_type) {
    if (provider_type == "cerebras") return rate_limits::CEREBRAS_RPM;
    if (provider_type == "zai") return rate_limits::ZAI_RPM;
    if (provider_type == "minimax") return rate_limits::MINIMAX_RPM;
    return 60; // Default
}

/**
 * @brief Get base endpoint for provider type
 */
inline std::string get_base_endpoint(const std::string& provider_type) {
    if (provider_type == "cerebras") return endpoints::CEREBRAS_BASE;
    if (provider_type == "zai") return endpoints::ZAI_BASE;
    if (provider_type == "minimax") return endpoints::MINIMAX_BASE;
    return ""; // Invalid provider
}

} // namespace api_specs
} // namespace providers
} // namespace aimux