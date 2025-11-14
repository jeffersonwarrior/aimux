#pragma once

#include <chrono>
#include <atomic>
#include <shared_mutex>
#include <unordered_map>
#include <string>
#include <thread>
#include <nlohmann/json.hpp>
#include "aimux/core/router.hpp"

namespace aimux {
namespace gateway {

/**
 * @brief Health status levels for providers
 */
enum class HealthStatus {
    HEALTHY,        // Provider is fully functional
    DEGRADED,       // Provider is slow but working
    UNHEALTHY,      // Provider is failing
    CIRCUIT_OPEN    // Provider is temporarily disabled
};

/**
 * @brief Capability flags for providers
 */
enum class ProviderCapability {
    THINKING = 1 << 0,   // Supports reasoning/thinking models
    VISION = 1 << 1,     // Supports image/vision processing
    TOOLS = 1 << 2,      // Supports tool calling
    STREAMING = 1 << 3,  // Supports streaming responses
    JSON_MODE = 1 << 4,  // Supports JSON response mode
    FUNCTION_CALLING = 1 << 5  // Supports function calling
};

/**
 * @brief Performance metrics for a provider
 */
struct PerformanceMetrics {
    double avg_response_time_ms_ = 0.0;     // Average response time
    double success_rate_ = 1.0;            // Success rate (0.0-1.0)
    int requests_per_minute_ = 0;          // Current request rate
    int max_requests_per_minute_ = 60;     // Rate limit
    double error_rate_ = 0.0;              // Error rate (0.0-1.0)
    std::chrono::steady_clock::time_point last_request_time_;
    std::chrono::steady_clock::time_point last_success_time_;
    std::chrono::steady_clock::time_point last_error_time_;

    // Cost configuration
    double cost_per_input_token_ = 0.0;    // Cost per 1M input tokens
    double cost_per_output_token_ = 0.0;   // Cost per 1M output tokens
    double cost_score_ = 1.0;              // Normalized cost score (lower is better)

    // Performance score (0.0-1.0, higher is better)
    double performance_score_ = 1.0;

    void update_response_time(double response_time_ms);
    void update_success(bool success);
    void update_error();
    void calculate_scores();

    nlohmann::json to_json() const;
    static PerformanceMetrics from_json(const nlohmann::json& j);
};

/**
 * @brief Provider health monitoring information
 */
struct ProviderHealth {
    std::string provider_name_;
    std::atomic<HealthStatus> status_{HealthStatus::HEALTHY};
    std::atomic<int> capability_flags_{0};

    PerformanceMetrics metrics_;

    // Circuit breaker configuration
    std::atomic<int> consecutive_failures_{0};
    std::atomic<int> max_consecutive_failures_{5};
    std::chrono::seconds failure_timeout_{300};  // 5 minutes
    std::chrono::steady_clock::time_point circuit_open_time_;
    std::chrono::steady_clock::time_point last_error_time_;

    // Health check configuration
    std::chrono::seconds health_check_interval_{60};  // 1 minute
    std::chrono::steady_clock::time_point last_health_check_;
    std::atomic<bool> health_check_in_progress_{false};

    // Recovery configuration
    std::atomic<int> successful_probes_{0};
    std::atomic<int> required_probes_{3};           // Success probes needed for recovery
    std::chrono::seconds probe_interval_{30};       // 30 seconds between probes

    ProviderHealth(const std::string& name);

    // Copy constructor and assignment operator
    ProviderHealth(const ProviderHealth& other);
    ProviderHealth& operator=(const ProviderHealth& other);

    // Move constructor and assignment operator
    ProviderHealth(ProviderHealth&& other) noexcept;
    ProviderHealth& operator=(ProviderHealth&& other) noexcept;

    // Health status management
    void mark_success();
    void mark_failure();
    void open_circuit();
    void attempt_recovery();
    void close_circuit();

    // Capability management
    void set_capability(ProviderCapability capability, bool enabled);
    bool has_capability(ProviderCapability capability) const;

    // Health checking
    bool is_healthy() const;
    bool can_accept_requests() const;
    std::chrono::seconds get_retry_delay() const;

    // Performance tracking
    void update_metrics(const core::Response& response, double request_time_ms);
    void reset_metrics();

    nlohmann::json to_json() const;
    static ProviderHealth from_json(const nlohmann::json& j);
};

/**
 * @brief Health monitoring system for providers
 */
class ProviderHealthMonitor {
public:
    using HealthChangeCallback = std::function<void(const std::string&, HealthStatus, HealthStatus)>;

    ProviderHealthMonitor();
    ~ProviderHealthMonitor();

    // Provider management
    void add_provider(const std::string& provider_name, const nlohmann::json& config);
    void remove_provider(const std::string& provider_name);
    ProviderHealth* get_provider_health(const std::string& provider_name);
    const ProviderHealth* get_provider_health(const std::string& provider_name) const;

    // Health monitoring
    void start_monitoring();
    void stop_monitoring();
    bool is_monitoring() const { return monitoring_active_.load(); }

    // Health queries
    std::vector<std::string> get_healthy_providers() const;
    std::vector<std::string> get_providers_with_capability(ProviderCapability capability) const;
    std::vector<std::string> get_unhealthy_providers() const;
    HealthStatus get_provider_status(const std::string& provider_name) const;

    // Metrics collection
    void update_provider_metrics(const std::string& provider_name,
                                const core::Response& response,
                                double request_time_ms);
    nlohmann::json get_all_provider_health() const;
    nlohmann::json get_provider_health_json(const std::string& provider_name) const;

    // Configuration
    void set_health_check_interval(const std::chrono::seconds& interval);
    void set_circuit_breaker_threshold(const std::string& provider_name, int threshold);
    void set_failure_timeout(const std::string& provider_name, const std::chrono::seconds& timeout);

    // Callbacks
    void set_health_change_callback(HealthChangeCallback callback) {
        health_change_callback_ = std::move(callback);
    }

private:
    mutable std::shared_mutex providers_mutex_;
    std::unordered_map<std::string, std::unique_ptr<ProviderHealth>> providers_;

    std::atomic<bool> monitoring_active_{false};
    std::thread monitoring_thread_;
    std::chrono::seconds health_check_interval_{60};

    HealthChangeCallback health_change_callback_;

    // Internal monitoring
    void monitoring_loop();
    void perform_health_check(const std::string& provider_name);
    void perform_periodic_checks();
    void check_circuit_breakers();
};

// Utility functions
inline std::string health_status_to_string(HealthStatus status) {
    switch (status) {
        case HealthStatus::HEALTHY: return "healthy";
        case HealthStatus::DEGRADED: return "degraded";
        case HealthStatus::UNHEALTHY: return "unhealthy";
        case HealthStatus::CIRCUIT_OPEN: return "circuit_open";
        default: return "unknown";
    }
}

inline HealthStatus string_to_health_status(const std::string& status_str) {
    if (status_str == "healthy") return HealthStatus::HEALTHY;
    if (status_str == "degraded") return HealthStatus::DEGRADED;
    if (status_str == "unhealthy") return HealthStatus::UNHEALTHY;
    if (status_str == "circuit_open") return HealthStatus::CIRCUIT_OPEN;
    return HealthStatus::UNHEALTHY;
}

inline std::string capability_to_string(ProviderCapability capability) {
    switch (capability) {
        case ProviderCapability::THINKING: return "thinking";
        case ProviderCapability::VISION: return "vision";
        case ProviderCapability::TOOLS: return "tools";
        case ProviderCapability::STREAMING: return "streaming";
        case ProviderCapability::JSON_MODE: return "json_mode";
        case ProviderCapability::FUNCTION_CALLING: return "function_calling";
        default: return "unknown";
    }
}

inline ProviderCapability string_to_capability(const std::string& capability_str) {
    if (capability_str == "thinking") return ProviderCapability::THINKING;
    if (capability_str == "vision") return ProviderCapability::VISION;
    if (capability_str == "tools") return ProviderCapability::TOOLS;
    if (capability_str == "streaming") return ProviderCapability::STREAMING;
    if (capability_str == "json_mode") return ProviderCapability::JSON_MODE;
    if (capability_str == "function_calling") return ProviderCapability::FUNCTION_CALLING;
    return static_cast<ProviderCapability>(0);
}

constexpr ProviderCapability operator|(ProviderCapability a, ProviderCapability b) {
    return static_cast<ProviderCapability>(static_cast<int>(a) | static_cast<int>(b));
}

constexpr ProviderCapability operator&(ProviderCapability a, ProviderCapability b) {
    return static_cast<ProviderCapability>(static_cast<int>(a) & static_cast<int>(b));
}

} // namespace gateway
} // namespace aimux