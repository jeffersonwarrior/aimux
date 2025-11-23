#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <thread>
#include <atomic>
#include <shared_mutex>
#include <functional>
#include <deque>
#include <nlohmann/json.hpp>
#include "aimux/core/bridge.hpp"
#include "aimux/core/router.hpp"
#include "aimux/gateway/provider_health.hpp"
#include "aimux/gateway/routing_logic.hpp"
#include "aimux/prettifier/prettifier_plugin.hpp"
#include "aimux/prettifier/cerebras_formatter.hpp"
#include "aimux/prettifier/openai_formatter.hpp"
#include "aimux/prettifier/anthropic_formatter.hpp"
#include "aimux/prettifier/synthetic_formatter.hpp"

namespace aimux {
namespace gateway {

/**
 * @brief Configuration for gateway provider routing
 */
struct GatewayProviderConfig {
    std::string name_;
    std::string api_key_;
    std::string base_url_;
    std::vector<std::string> models_;

    // Capabilities
    int capability_flags_ = 0;
    bool supports_thinking_ = false;
    bool supports_vision_ = false;
    bool supports_tools_ = false;
    bool supports_streaming_ = false;

    // Performance characteristics
    double avg_response_time_ms_ = 1000.0;
    double success_rate_ = 1.0;
    int max_concurrent_requests_ = 10;
    double cost_per_output_token_ = 0.0;

    // Health check settings
    std::chrono::seconds health_check_interval_{60};
    int max_failures_ = 5;
    std::chrono::seconds recovery_delay_{300};

    // Provider priority settings
    int priority_score_ = 100;  // Higher = more preferred
    bool enabled_ = true;

    nlohmann::json to_json() const;
    static GatewayProviderConfig from_json(const nlohmann::json& j);
};

/**
 * @brief Request metrics for tracking and optimization
 */
struct RequestMetrics {
    std::string provider_name_;
    std::chrono::steady_clock::time_point start_time_;
    std::chrono::steady_clock::time_point end_time_;
    double duration_ms_ = 0.0;
    bool success_ = false;
    int http_status_code_ = 0;
    std::string error_message_;
    int request_tokens_ = 0;
    int response_tokens_ = 0;
    double cost_usd_ = 0.0;
    RequestType request_type_;
    std::string routing_reasoning_;

    nlohmann::json to_json() const;
    static RequestMetrics create_metrics(const std::string& provider,
                                       const core::Request& request,
                                       RequestType type,
                                       const std::string& reasoning);
    void record_response(const core::Response& response);
};

/**
 * @brief Core GatewayManager implementing V3 unified gateway architecture
 *
 * This class provides the intelligent routing logic and provider management
 * for the V3 unified gateway, replacing the dual-port approach with a single
 * intelligent gateway that handles all routing decisions.
 */
class GatewayManager {
public:
    using RouteCallback = std::function<void(const RequestMetrics&)>;
    using ProviderChangeCallback = std::function<void(const std::string&, bool)>;

    explicit GatewayManager();
    ~GatewayManager();

    // Core management
    void initialize();
    void shutdown();
    bool is_initialized() const { return initialized_.load(); }

    // Provider management
    void add_provider(const std::string& provider_name, const nlohmann::json& config);
    void remove_provider(const std::string& provider_name);
    void update_provider_config(const std::string& provider_name, const nlohmann::json& config);
    bool provider_exists(const std::string& provider_name) const;

    // Provider adapters (using existing Bridge interface)
    void add_provider_adapter(std::unique_ptr<core::Bridge> bridge);
    void remove_provider_adapter(const std::string& provider_name);
    core::Bridge* get_provider_adapter(const std::string& provider_name);
    const core::Bridge* get_provider_adapter(const std::string& provider_name) const;

    // Primary routing method
    core::Response route_request(const core::Request& request);
    core::Response route_request_to_provider(const core::Request& request,
                                           const std::string& provider_name);

    // Configuration management
    void set_default_provider(const std::string& provider_name);
    void set_thinking_provider(const std::string& provider_name);
    void set_vision_provider(const std::string& provider_name);
    void set_tools_provider(const std::string& provider_name);

    std::string get_default_provider() const { return default_provider_; }
    std::string get_thinking_provider() const { return thinking_provider_; }
    std::string get_vision_provider() const { return vision_provider_; }
    std::string get_tools_provider() const { return tools_provider_; }

    // Routing configuration
    void set_routing_priority(RoutingPriority priority);
    void set_custom_routing_function(CustomPriorityFunction func);
    void enable_load_balancer(std::unique_ptr<LoadBalancer> balancer);
    void disable_load_balancer();

    // Health monitoring
    void start_health_monitoring();
    void stop_health_monitoring();
    bool is_health_monitoring_enabled() const { return health_monitoring_active_.load(); }

    std::vector<std::string> get_healthy_providers() const;
    std::vector<std::string> get_unhealthy_providers() const;
    ProviderHealth* get_provider_health(const std::string& provider_name);
    HealthStatus get_provider_status(const std::string& provider_name) const;

    // Load balancing
    std::string select_balanced_provider(const std::vector<std::string>& candidates,
                                       RequestType request_type = RequestType::STANDARD);
    void update_provider_metrics(const std::string& provider_name,
                               const RequestMetrics& metrics);

    // Request analysis and classification
    RequestAnalysis analyze_request(const core::Request& request) const;
    bool is_thinking_request(const nlohmann::json& messages);
    bool has_vision_content(const nlohmann::json& messages);
    bool requires_tools(const nlohmann::json& messages);

    // Configuration persistence
    nlohmann::json get_configuration() const;
    void load_configuration(const nlohmann::json& config);
    nlohmann::json get_provider_configs() const;
    nlohmann::json get_routing_config() const;

    // Metrics and monitoring
    nlohmann::json get_metrics() const;
    std::vector<RequestMetrics> get_recent_metrics(int count = 100) const;
    void enable_metrics_collection(bool enabled);
    void clear_metrics();

    // Provider capabilities
    ProviderCapability get_provider_capabilities(const std::string& provider_name) const;
    std::vector<std::string> get_providers_with_capability(ProviderCapability capability) const;
    bool validate_provider_capabilities(const std::string& provider_name,
                                      const RequestAnalysis& analysis);

    // Failover and reliability
    void enable_circuit_breaker(bool enabled, int max_failures = 5);
    void enable_auto_recovery(bool enabled, int probe_count = 3);
    void manually_mark_provider_healthy(const std::string& provider_name);
    void manually_mark_provider_unhealthy(const std::string& provider_name);

    // Callbacks
    void set_route_callback(RouteCallback callback) { route_callback_ = std::move(callback); }
    void set_provider_change_callback(ProviderChangeCallback callback) {
        provider_change_callback_ = std::move(callback);
    }

    // Validation and testing
    bool validate_provider_config(const nlohmann::json& config) const;
    bool test_provider_connectivity(const std::string& provider_name) const;
    std::vector<std::string> get_configuration_errors() const;

    // Debug and diagnostics
    nlohmann::json debug_routing_decision(const core::Request& request) const;
    void set_debug_mode(bool enabled);
    void set_log_level(const std::string& level);

private:
    // Provider configuration
    mutable std::shared_mutex adapters_mutex_;
    std::unordered_map<std::string, std::unique_ptr<core::Bridge>> adapters_;
    std::unordered_map<std::string, GatewayProviderConfig> provider_configs_;

    // Routing preferences
    std::string default_provider_;
    std::string thinking_provider_;
    std::string vision_provider_;
    std::string tools_provider_;

    // Health monitoring
    std::unique_ptr<ProviderHealthMonitor> health_monitor_;
    std::atomic<bool> health_monitoring_active_{false};
    std::thread health_monitoring_thread_;

    // Routing logic
    std::unique_ptr<RoutingLogic> routing_logic_;

    // Prettifier support (v2.1)
    std::unordered_map<std::string, std::shared_ptr<prettifier::PrettifierPlugin>> prettifier_formatters_;
    std::atomic<bool> prettifier_enabled_{true};

    // State management
    std::atomic<bool> initialized_{false};
    std::atomic<bool> debug_mode_{false};
    std::atomic<bool> metrics_collection_enabled_{true};

    // Metrics storage
    mutable std::shared_mutex metrics_mutex_;
    std::deque<RequestMetrics> request_metrics_;
    static constexpr size_t MAX_METRICS_HISTORY = 10000;

    // Callbacks
    RouteCallback route_callback_;
    ProviderChangeCallback provider_change_callback_;

    // Internal helper methods
    void validate_provider_config_internal(const GatewayProviderConfig& config);
    void notify_provider_change(const std::string& provider_name, bool added);
    void record_routing_metrics(const RequestMetrics& metrics);
    std::string select_failover_provider(const std::string& failed_provider,
                                       const core::Request& request);
    bool provider_is_available(const std::string& provider_name) const;
    std::vector<std::string> get_prioritized_providers() const;

    // Configuration validation
    bool validate_provider_name(const std::string& name) const;
    bool validate_api_key(const std::string& api_key) const;
    bool validate_base_url(const std::string& url) const;
    bool validate_capability_flags(int flags) const;

    // Prettifier helpers
    void initialize_prettifier_formatters();
    prettifier::PrettifierPlugin* get_prettifier_for_provider(const std::string& provider_name);
    core::Response apply_prettifier(const core::Response& response, const std::string& provider_name,
                                    const core::Request& request);

    // Error handling
    core::Response create_error_response(const std::string& error_code,
                                       const std::string& error_message,
                                       int http_status = 500) const;
    void log_error(const std::string& error_type, const std::string& message) const;
    void log_debug(const std::string& message) const;
};

} // namespace gateway
} // namespace aimux