#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>
#include <nlohmann/json.hpp>
#include "aimux/core/router.hpp"
#include "aimux/gateway/provider_health.hpp"

namespace aimux {
namespace gateway {

/**
 * @brief Request classification types
 */
enum class RequestType {
    STANDARD,       // Regular text generation
    THINKING,       // Reasoning/thinking prompts
    VISION,         // Image/vision processing
    MULTIMODAL,     // Mixed text and image
    TOOLS,          // Tool calling/function execution
    STREAMING,      // Streaming response
    LONG_CONTEXT    // Requests with very long context
};

/**
 * @brief Routing priority levels
 */
enum class RoutingPriority {
    COST = 0,       // Prefer cheapest provider
    PERFORMANCE = 1, // Prefer fastest provider
    RELIABILITY = 2, // Prefer most reliable provider
    BALANCED = 3,   // Balance all factors
    CUSTOM = 4      // Custom priority function
};

/**
 * @brief Request analysis result
 */
struct RequestAnalysis {
    RequestType type_{RequestType::STANDARD};
    ProviderCapability required_capabilities_{};
    int estimated_tokens_ = 1000;           // Estimated token usage
    double expected_response_time_ms_ = 1000.0;  // Expected response time
    bool requires_streaming_ = false;
    bool requires_tools_ = false;
    bool requires_json_mode_ = false;
    bool requires_function_calling_ = false;
    double cost_sensitivity_ = 0.5;        // 0.0 = cost insensitive, 1.0 = cost sensitive
    double latency_sensitivity_ = 0.5;      // 0.0 = latency insensitive, 1.0 = latency sensitive

    nlohmann::json to_json() const;
};

/**
 * @brief Routing decision with reason tracking
 */
struct RoutingDecision {
    std::string selected_provider_;
    std::string reasoning_;
    RoutingPriority priority_used_;
    double selection_score_;
    std::vector<std::string> alternative_providers_;

    nlohmann::json to_json() const;
};

/**
 * @brief Load balancer interface
 */
class LoadBalancer {
public:
    virtual ~LoadBalancer() = default;

    /**
     * @brief Select best provider from candidates using load balancing strategy
     * @param providers List of healthy providers
     * @param request_type Type of request for context-aware balancing
     * @return Selected provider name
     */
    virtual std::string select_provider(
        const std::vector<std::string>& providers,
        RequestType request_type = RequestType::STANDARD
    ) const = 0;

    /**
     * @brief Get load balancing strategy name
     */
    virtual std::string get_strategy_name() const = 0;
};

/**
 * @brief Round-robin load balancing
 */
class RoundRobinBalancer : public LoadBalancer {
public:
    std::string select_provider(
        const std::vector<std::string>& providers,
        RequestType request_type = RequestType::STANDARD
    ) const override;
    std::string get_strategy_name() const override { return "RoundRobin"; }

private:
    mutable std::atomic<size_t> counter_{0};
};

/**
 * @brief Weighted load balancing based on performance metrics
 */
class WeightedBalancer : public LoadBalancer {
public:
    explicit WeightedBalancer(ProviderHealthMonitor* health_monitor);

    std::string select_provider(
        const std::vector<std::string>& providers,
        RequestType request_type = RequestType::STANDARD
    ) const override;
    std::string get_strategy_name() const override { return "Weighted"; }

private:
    ProviderHealthMonitor* health_monitor_;

    double calculate_weight(const std::string& provider, RequestType request_type) const;
    bool provider_is_suitable(const std::string& provider, RequestType request_type) const;
};

/**
 * @brief Least connections load balancing
 */
class LeastConnectionsBalancer : public LoadBalancer {
public:
    explicit LeastConnectionsBalancer(ProviderHealthMonitor* health_monitor);

    std::string select_provider(
        const std::vector<std::string>& providers,
        RequestType request_type = RequestType::STANDARD
    ) const override;
    std::string get_strategy_name() const override { return "LeastConnections"; }

private:
    ProviderHealthMonitor* health_monitor_;
};

/**
 * @brief Custom priority-based routing function
 */
using CustomPriorityFunction = std::function<std::string(
    const std::vector<std::string>& providers,
    const RequestAnalysis& analysis,
    const std::unordered_map<std::string, ProviderHealth*>& health_states
)>;

/**
 * @brief Intelligent routing logic with multiple strategies
 */
class RoutingLogic {
public:
    explicit RoutingLogic(ProviderHealthMonitor* health_monitor);
    ~RoutingLogic() = default;

    // Core routing
    RoutingDecision route_request(
        const core::Request& request,
        RoutingPriority priority = RoutingPriority::BALANCED
    );

    // Provider selection strategies
    std::string select_by_cost(const std::vector<std::string>& providers);
    std::string select_by_performance(const std::vector<std::string>& providers);
    std::string select_by_reliability(const std::vector<std::string>& providers);
    std::string select_balanced(const std::vector<std::string>& providers, const RequestAnalysis& analysis);
    std::string select_custom(const std::vector<std::string>& providers,
                            const RequestAnalysis& analysis,
                            CustomPriorityFunction custom_func);

    // Load balancing
    void set_load_balancer(std::unique_ptr<LoadBalancer> balancer);
    std::string apply_load_balancing(
        const std::vector<std::string>& providers,
        RequestType request_type = RequestType::STANDARD
    );

    // Request analysis
    RequestAnalysis analyze_request(const core::Request& request);
    bool is_thinking_request(const std::string& content);
    bool has_vision_content(const nlohmann::json& content);
    bool requires_tools(const nlohmann::json& message);
    bool requires_streaming(const nlohmann::json& data);

    // Configuration
    void set_default_priority(RoutingPriority priority) { default_priority_ = priority; }
    void set_custom_priority_function(CustomPriorityFunction func) {
        custom_priority_function_ = std::move(func);
    }
    void set_thinking_keywords(const std::vector<std::string>& keywords);
    void set_vision_keywords(const std::vector<std::string>& keywords);

    // Provider filtering
    std::vector<std::string> filter_by_capability(
        const std::vector<std::string>& providers,
        ProviderCapability capability
    );
    std::vector<std::string> filter_by_request_type(
        const std::vector<std::string>& providers,
        RequestType request_type
    );
    std::vector<std::string> filter_by_capacity(
        const std::vector<std::string>& providers,
        int additional_requests = 1
    );

    // Metrics and monitoring
    void record_routing_decision(const RoutingDecision& decision);
    nlohmann::json get_routing_metrics() const;

private:
    ProviderHealthMonitor* health_monitor_;
    RoutingPriority default_priority_{RoutingPriority::BALANCED};
    std::unique_ptr<LoadBalancer> load_balancer_;
    CustomPriorityFunction custom_priority_function_;

    // Request analysis configuration
    std::vector<std::string> thinking_keywords_{
        "think", "reason", "analyze", "step by step", "break down",
        "explain", "consider", "evaluate", "compare", "conclude"
    };
    std::vector<std::string> vision_keywords_{
        "image", "photo", "picture", "visual", "diagram", "chart",
        "screenshot", "graph", "figure", "drawing", "illustration"
    };

    // Routing metrics
    mutable std::shared_mutex metrics_mutex_;
    std::unordered_map<std::string, int> provider_selection_counts_;
    std::unordered_map<RequestType, int> request_type_counts_;
    std::unordered_map<RoutingPriority, int> priority_usage_counts_;
    std::atomic<int> total_routings_{0};

    // Internal helper functions
    ProviderCapability get_required_capabilities(const RequestAnalysis& analysis);
    std::vector<std::string> get_capable_providers(ProviderCapability capabilities);
    double calculate_provider_score(const std::string& provider,
                                  ProviderCapability capabilities,
                                  RoutingPriority priority);
    std::string generate_reasoning(const RoutingDecision& decision,
                                 const RequestAnalysis& analysis,
                                 const std::vector<std::string>& candidates);
};

// Utility functions
std::string request_type_to_string(RequestType type);
RequestType string_to_request_type(const std::string& type_str);
std::string routing_priority_to_string(RoutingPriority priority);
RoutingPriority string_to_routing_priority(const std::string& priority_str);
std::string capabilities_to_string(ProviderCapability capability);

} // namespace gateway
} // namespace aimux