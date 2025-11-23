#include "aimux/gateway/gateway_manager.hpp"
#include "aimux/logging/logger.hpp"
#include "aimux/network/http_client.hpp"
#include "aimux/providers/provider_impl.hpp"
#include <algorithm>
#include <random>
#include <sstream>
#include <fstream>
#include <regex>
#include <iomanip>

namespace aimux {
namespace gateway {

// ============================================================================
// GatewayProviderConfig Implementation
// ============================================================================

nlohmann::json GatewayProviderConfig::to_json() const {
    nlohmann::json j;
    j["name"] = name_;
    j["api_key"] = api_key_;  // In production, this should be encrypted
    j["base_url"] = base_url_;
    j["models"] = models_;
    j["capability_flags"] = capability_flags_;
    j["supports_thinking"] = supports_thinking_;
    j["supports_vision"] = supports_vision_;
    j["supports_tools"] = supports_tools_;
    j["supports_streaming"] = supports_streaming_;
    j["avg_response_time_ms"] = avg_response_time_ms_;
    j["success_rate"] = success_rate_;
    j["max_concurrent_requests"] = max_concurrent_requests_;
    j["cost_per_output_token"] = cost_per_output_token_;
    j["health_check_interval"] = health_check_interval_.count();
    j["max_failures"] = max_failures_;
    j["recovery_delay"] = recovery_delay_.count();
    j["priority_score"] = priority_score_;
    j["enabled"] = enabled_;
    return j;
}

GatewayProviderConfig GatewayProviderConfig::from_json(const nlohmann::json& j) {
    GatewayProviderConfig config;
    config.name_ = j.value("name", "");
    config.api_key_ = j.value("api_key", "");
    config.base_url_ = j.value("base_url", "");
    config.models_ = j.value("models", std::vector<std::string>{});
    config.capability_flags_ = j.value("capability_flags", 0);
    config.supports_thinking_ = j.value("supports_thinking", false);
    config.supports_vision_ = j.value("supports_vision", false);
    config.supports_tools_ = j.value("supports_tools", false);
    config.supports_streaming_ = j.value("supports_streaming", false);
    config.avg_response_time_ms_ = j.value("avg_response_time_ms", 1000.0);
    config.success_rate_ = j.value("success_rate", 1.0);
    config.max_concurrent_requests_ = j.value("max_concurrent_requests", 10);
    config.cost_per_output_token_ = j.value("cost_per_output_token", 0.0);
    config.health_check_interval_ = std::chrono::seconds(j.value("health_check_interval", 60));
    config.max_failures_ = j.value("max_failures", 5);
    config.recovery_delay_ = std::chrono::seconds(j.value("recovery_delay", 300));
    config.priority_score_ = j.value("priority_score", 100);
    config.enabled_ = j.value("enabled", true);
    return config;
}

// ============================================================================
// RequestMetrics Implementation
// ============================================================================

nlohmann::json RequestMetrics::to_json() const {
    nlohmann::json j;
    j["provider_name"] = provider_name_;
    j["start_time"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        start_time_.time_since_epoch()).count();
    j["end_time"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time_.time_since_epoch()).count();
    j["duration_ms"] = duration_ms_;
    j["success"] = success_;
    j["http_status_code"] = http_status_code_;
    j["error_message"] = error_message_;
    j["request_tokens"] = request_tokens_;
    j["response_tokens"] = response_tokens_;
    j["cost_usd"] = cost_usd_;
    j["request_type"] = request_type_to_string(request_type_);
    j["routing_reasoning"] = routing_reasoning_;
    return j;
}

RequestMetrics RequestMetrics::create_metrics(const std::string& provider,
                                             const core::Request& request,
                                             RequestType type,
                                             const std::string& reasoning) {
    RequestMetrics metrics;
    metrics.provider_name_ = provider;
    metrics.start_time_ = std::chrono::steady_clock::now();
    metrics.request_type_ = type;
    metrics.routing_reasoning_ = reasoning;

    // Extract token estimates from request data
    if (request.data.contains("messages") && request.data["messages"].is_array()) {
        for (const auto& message : request.data["messages"]) {
            if (message.contains("content")) {
                std::string content = message["content"];
                metrics.request_tokens_ += static_cast<int>(content.length() / 4); // Rough estimate
            }
        }
    }

    return metrics;
}

void RequestMetrics::record_response(const core::Response& response) {
    end_time_ = std::chrono::steady_clock::now();
    duration_ms_ = std::chrono::duration<double, std::milli>(end_time_ - start_time_).count();
    success_ = response.success;
    http_status_code_ = response.status_code;
    error_message_ = response.error_message;

    // Extract response tokens
    if (!response.data.empty()) {
        response_tokens_ = response.data.length() / 4; // Rough estimate
    }
}

// ============================================================================
// GatewayManager Implementation
// ============================================================================

GatewayManager::GatewayManager()
    : health_monitor_(std::make_unique<ProviderHealthMonitor>()),
      routing_logic_(std::make_unique<RoutingLogic>(health_monitor_.get())) {

    // Initialize with default providers if any
    aimux::info("GatewayManager: Initializing unified gateway manager");
}

GatewayManager::~GatewayManager() {
    shutdown();
    aimux::info("GatewayManager: Shut down successfully");
}

void GatewayManager::initialize() {
    if (initialized_.load()) {
        aimux::warn("GatewayManager: Already initialized");
        return;
    }

    std::lock_guard<std::shared_mutex> lock(adapters_mutex_);

    // Load configuration if available
    try {
        std::ifstream config_file("config.json");
        if (config_file.good()) {
            nlohmann::json config = nlohmann::json::parse(config_file);
            load_configuration(config);
            aimux::info("GatewayManager: Loaded configuration from file");
        }
    } catch (const std::exception& e) {
        aimux::warn("Could not load configuration file: " + std::string(e.what()));
    }

    // Start health monitoring
    start_health_monitoring();

    initialized_.store(true);
    aimux::info("GatewayManager: Initialization complete");
}

void GatewayManager::shutdown() {
    if (!initialized_.load()) {
        return;
    }

    // Stop health monitoring
    stop_health_monitoring();

    // Clean up adapters
    std::unique_lock<std::shared_mutex> lock(adapters_mutex_);
    adapters_.clear();
    provider_configs_.clear();
    lock.unlock();

    initialized_.store(false);
    aimux::info("GatewayManager: Shutdown complete");
}

// ============================================================================
// Provider Management
// ============================================================================

void GatewayManager::add_provider(const std::string& provider_name, const nlohmann::json& config) {
    if (!validate_provider_name(provider_name)) {
        throw std::invalid_argument("Invalid provider name: " + provider_name);
    }

    if (!validate_provider_config(config)) {
        throw std::invalid_argument("Invalid provider configuration for: " + provider_name);
    }

    // Create provider configuration
    GatewayProviderConfig provider_config = GatewayProviderConfig::from_json(config);
    provider_config.name_ = provider_name;

    // Create provider instance using factory
    auto bridge = providers::ProviderFactory::create_provider(provider_name, config);
    if (!bridge) {
        throw std::runtime_error("Failed to create provider: " + provider_name);
    }

    // Add to collections
    std::unique_lock<std::shared_mutex> lock(adapters_mutex_);
    adapters_[provider_name] = std::move(bridge);
    provider_configs_[provider_name] = std::move(provider_config);
    lock.unlock();

    // Add to health monitor
    health_monitor_->add_provider(provider_name, config);

    notify_provider_change(provider_name, true);
    aimux::info("GatewayManager: Added provider: " + provider_name);
}

void GatewayManager::remove_provider(const std::string& provider_name) {
    std::unique_lock<std::shared_mutex> lock(adapters_mutex_);

    auto adapter_it = adapters_.find(provider_name);
    if (adapter_it != adapters_.end()) {
        adapters_.erase(adapter_it);
    }

    auto config_it = provider_configs_.find(provider_name);
    if (config_it != provider_configs_.end()) {
        provider_configs_.erase(config_it);
    }

    lock.unlock();

    // Remove from health monitor
    health_monitor_->remove_provider(provider_name);

    notify_provider_change(provider_name, false);
    aimux::info("GatewayManager: Removed provider: " + provider_name);
}

void GatewayManager::update_provider_config(const std::string& provider_name, const nlohmann::json& config) {
    if (!validate_provider_config(config)) {
        throw std::invalid_argument("Invalid provider configuration for: " + provider_name);
    }

    std::unique_lock<std::shared_mutex> lock(adapters_mutex_);

    auto config_it = provider_configs_.find(provider_name);
    if (config_it == provider_configs_.end()) {
        throw std::runtime_error("Provider not found: " + provider_name);
    }

    config_it->second = GatewayProviderConfig::from_json(config);
    config_it->second.name_ = provider_name;

    lock.unlock();

    // Update health monitor
    health_monitor_->add_provider(provider_name, config);

    aimux::info("GatewayManager: Updated configuration for provider: " + provider_name);
}

bool GatewayManager::provider_exists(const std::string& provider_name) const {
    std::shared_lock<std::shared_mutex> lock(adapters_mutex_);
    return adapters_.find(provider_name) != adapters_.end();
}

void GatewayManager::add_provider_adapter(std::unique_ptr<core::Bridge> bridge) {
    if (!bridge) {
        throw std::invalid_argument("Bridge cannot be null");
    }

    std::string provider_name = bridge->get_provider_name();
    std::unique_lock<std::shared_mutex> lock(adapters_mutex_);
    adapters_[provider_name] = std::move(bridge);
    lock.unlock();

    notify_provider_change(provider_name, true);
    aimux::info("GatewayManager: Added adapter for provider: " + provider_name);
}

void GatewayManager::remove_provider_adapter(const std::string& provider_name) {
    std::unique_lock<std::shared_mutex> lock(adapters_mutex_);
    adapters_.erase(provider_name);
    lock.unlock();

    notify_provider_change(provider_name, false);
    aimux::info("GatewayManager: Removed adapter for provider: " + provider_name);
}

core::Bridge* GatewayManager::get_provider_adapter(const std::string& provider_name) {
    std::shared_lock<std::shared_mutex> lock(adapters_mutex_);
    auto it = adapters_.find(provider_name);
    return (it != adapters_.end()) ? it->second.get() : nullptr;
}

const core::Bridge* GatewayManager::get_provider_adapter(const std::string& provider_name) const {
    std::shared_lock<std::shared_mutex> lock(adapters_mutex_);
    auto it = adapters_.find(provider_name);
    return (it != adapters_.end()) ? it->second.get() : nullptr;
}

// ============================================================================
// Core Request Routing
// ============================================================================

core::Response GatewayManager::route_request(const core::Request& request) {
    if (!initialized_.load()) {
        return create_error_response("NOT_INITIALIZED",
                                  "GatewayManager not initialized",
                                  503);
    }

    // Analyze request to determine routing strategy
    RequestAnalysis analysis = routing_logic_->analyze_request(request);

    // Get routing decision
    RoutingDecision decision = routing_logic_->route_request(request);

    // Create metrics
    RequestMetrics metrics = RequestMetrics::create_metrics(
        decision.selected_provider_, request, analysis.type_, decision.reasoning_);

    core::Response response;

    try {
        // Route to selected provider
        response = route_request_to_provider(request, decision.selected_provider_);

        // Update metrics
        metrics.record_response(response);

        // Update provider metrics
        update_provider_metrics(decision.selected_provider_, metrics);

        // Handle failure cases
        if (!response.success) {
            health_monitor_->update_provider_metrics(decision.selected_provider_,
                                                    response, metrics.duration_ms_);

            // Try failover providers if available
            for (const auto& alt_provider : decision.alternative_providers_) {
                if (provider_is_available(alt_provider)) {
                    aimux::warn("Attempting failover from " + decision.selected_provider_ +
                               " to " + alt_provider);

                    metrics.provider_name_ = alt_provider;
                    metrics.routing_reasoning_ += " [FAILOVER]";

                    response = route_request_to_provider(request, alt_provider);
                    metrics.record_response(response);

                    if (response.success) {
                        update_provider_metrics(alt_provider, metrics);
                        break;
                    }
                }
            }
        }

    } catch (const std::exception& e) {
        aimux::error("Exception during request routing: " + std::string(e.what()));
        response = create_error_response("ROUTING_EXCEPTION", e.what(), 500);
        metrics.record_response(response);
    }

    // Record metrics
    record_routing_metrics(metrics);

    // Trigger callback if set
    if (route_callback_) {
        route_callback_(metrics);
    }

    log_debug("Request routed to " + metrics.provider_name_ +
             " in " + std::to_string(metrics.duration_ms_) + "ms");

    return response;
}

core::Response GatewayManager::route_request_to_provider(const core::Request& request,
                                                        const std::string& provider_name) {
    core::Bridge* adapter = get_provider_adapter(provider_name);
    if (!adapter) {
        return create_error_response("PROVIDER_NOT_FOUND",
                                  "Provider not found: " + provider_name,
                                  404);
    }

    if (!adapter->is_healthy()) {
        return create_error_response("PROVIDER_UNHEALTHY",
                                  "Provider unhealthy: " + provider_name,
                                  503);
    }

    try {
        return adapter->send_request(request);
    } catch (const std::exception& e) {
        aimux::error("Provider " + provider_name + " request failed: " + e.what());
        return create_error_response("PROVIDER_REQUEST_FAILED", e.what(), 502);
    }
}

// ============================================================================
// Configuration Management
// ============================================================================

void GatewayManager::set_default_provider(const std::string& provider_name) {
    if (!provider_exists(provider_name)) {
        throw std::runtime_error("Provider not found: " + provider_name);
    }
    default_provider_ = provider_name;
    aimux::info("GatewayManager: Set default provider to: " + provider_name);
}

void GatewayManager::set_thinking_provider(const std::string& provider_name) {
    if (!provider_exists(provider_name)) {
        throw std::runtime_error("Provider not found: " + provider_name);
    }
    thinking_provider_ = provider_name;
    aimux::info("GatewayManager: Set thinking provider to: " + provider_name);
}

void GatewayManager::set_vision_provider(const std::string& provider_name) {
    if (!provider_exists(provider_name)) {
        throw std::runtime_error("Provider not found: " + provider_name);
    }
    vision_provider_ = provider_name;
    aimux::info("GatewayManager: Set vision provider to: " + provider_name);
}

void GatewayManager::set_tools_provider(const std::string& provider_name) {
    if (!provider_exists(provider_name)) {
        throw std::runtime_error("Provider not found: " + provider_name);
    }
    tools_provider_ = provider_name;
    aimux::info("GatewayManager: Set tools provider to: " + provider_name);
}

// ============================================================================
// Health Monitoring
// ============================================================================

void GatewayManager::start_health_monitoring() {
    if (health_monitoring_active_.load()) {
        return;
    }

    health_monitor_->start_monitoring();
    health_monitoring_active_.store(true);
    aimux::info("GatewayManager: Started health monitoring");
}

void GatewayManager::stop_health_monitoring() {
    if (!health_monitoring_active_.load()) {
        return;
    }

    health_monitor_->stop_monitoring();
    health_monitoring_active_.store(false);
    aimux::info("GatewayManager: Stopped health monitoring");
}

std::vector<std::string> GatewayManager::get_healthy_providers() const {
    return health_monitor_->get_healthy_providers();
}

std::vector<std::string> GatewayManager::get_unhealthy_providers() const {
    return health_monitor_->get_unhealthy_providers();
}

ProviderHealth* GatewayManager::get_provider_health(const std::string& provider_name) {
    return health_monitor_->get_provider_health(provider_name);
}

HealthStatus GatewayManager::get_provider_status(const std::string& provider_name) const {
    return health_monitor_->get_provider_status(provider_name);
}

// ============================================================================
// Load Balancing
// ============================================================================

std::string GatewayManager::select_balanced_provider(const std::vector<std::string>& candidates,
                                                   RequestType request_type) {
    if (candidates.empty()) {
        return "";
    }

    if (candidates.size() == 1) {
        return candidates[0];
    }

    return routing_logic_->apply_load_balancing(candidates, request_type);
}

void GatewayManager::update_provider_metrics(const std::string& provider_name,
                                            const RequestMetrics& metrics) {
    // Create a dummy response for health monitoring
    core::Response response;
    response.success = metrics.success_;
    response.status_code = metrics.http_status_code_;
    response.response_time_ms = metrics.duration_ms_;

    health_monitor_->update_provider_metrics(provider_name, response, metrics.duration_ms_);
}

// ============================================================================
// Request Analysis
// ============================================================================

RequestAnalysis GatewayManager::analyze_request(const core::Request& request) const {
    return routing_logic_->analyze_request(request);
}

bool GatewayManager::is_thinking_request(const nlohmann::json& messages) {
    return routing_logic_->is_thinking_request(messages.dump());
}

bool GatewayManager::has_vision_content(const nlohmann::json& messages) {
    return routing_logic_->has_vision_content(messages);
}

bool GatewayManager::requires_tools(const nlohmann::json& messages) {
    return routing_logic_->requires_tools(messages);
}

// ============================================================================
// Configuration Persistence
// ============================================================================

nlohmann::json GatewayManager::get_configuration() const {
    nlohmann::json config;
    config["default_provider"] = default_provider_;
    config["thinking_provider"] = thinking_provider_;
    config["vision_provider"] = vision_provider_;
    config["tools_provider"] = tools_provider_;
    config["providers"] = get_provider_configs();
    config["routing"] = get_routing_config();
    return config;
}

void GatewayManager::load_configuration(const nlohmann::json& config) {
    default_provider_ = config.value("default_provider", "");
    thinking_provider_ = config.value("thinking_provider", "");
    vision_provider_ = config.value("vision_provider", "");
    tools_provider_ = config.value("tools_provider", "");

    if (config.contains("providers") && config["providers"].is_object()) {
        for (const auto& [name, provider_config] : config["providers"].items()) {
            try {
                add_provider(name, provider_config);
            } catch (const std::exception& e) {
                aimux::error("Failed to load provider " + name + ": " + e.what());
            }
        }
    }
}

nlohmann::json GatewayManager::get_provider_configs() const {
    std::shared_lock<std::shared_mutex> lock(adapters_mutex_);
    nlohmann::json configs;
    for (const auto& [name, config] : provider_configs_) {
        configs[name] = config.to_json();
    }
    return configs;
}

nlohmann::json GatewayManager::get_routing_config() const {
    nlohmann::json config;
    config["priority"] = routing_priority_to_string(RoutingPriority::BALANCED);
    config["load_balancer"] = "weighted"; // Default
    return config;
}

// ============================================================================
// Metrics and Monitoring
// ============================================================================

nlohmann::json GatewayManager::get_metrics() const {
    nlohmann::json metrics;

    // Gateway manager metrics
    metrics["total_providers"] = adapters_.size();
    metrics["healthy_providers"] = get_healthy_providers().size();
    metrics["unhealthy_providers"] = get_unhealthy_providers().size();
    metrics["health_monitoring_active"] = health_monitoring_active_.load();
    metrics["metrics_collection_enabled"] = metrics_collection_enabled_.load();

    // Request metrics summary
    std::shared_lock<std::shared_mutex> metrics_lock(metrics_mutex_);
    int total_requests = request_metrics_.size();
    int successful_requests = 0;
    double total_duration = 0.0;

    for (const auto& metric : request_metrics_) {
        if (metric.success_) {
            successful_requests++;
        }
        total_duration += metric.duration_ms_;
    }

    metrics["total_requests"] = total_requests;
    metrics["successful_requests"] = successful_requests;
    metrics["success_rate"] = total_requests > 0 ?
        static_cast<double>(successful_requests) / total_requests : 0.0;
    metrics["avg_response_time_ms"] = total_requests > 0 ?
        total_duration / total_requests : 0.0;

    // Provider health metrics
    metrics["provider_health"] = health_monitor_->get_all_provider_health();

    return metrics;
}

std::vector<RequestMetrics> GatewayManager::get_recent_metrics(int count) const {
    std::shared_lock<std::shared_mutex> lock(metrics_mutex_);

    if (count <= 0 || request_metrics_.empty()) {
        return {};
    }

    int start_idx = std::max(0, static_cast<int>(request_metrics_.size()) - count);
    std::vector<RequestMetrics> recent;

    for (auto it = request_metrics_.begin() + start_idx; it != request_metrics_.end(); ++it) {
        recent.push_back(*it);
    }

    return recent;
}

void GatewayManager::enable_metrics_collection(bool enabled) {
    metrics_collection_enabled_.store(enabled);
    aimux::info("GatewayManager: Metrics collection " + std::string(enabled ? "enabled" : "disabled"));
}

void GatewayManager::clear_metrics() {
    std::unique_lock<std::shared_mutex> lock(metrics_mutex_);
    request_metrics_.clear();
    aimux::info("GatewayManager: Cleared all metrics");
}

// ============================================================================
// Provider Capabilities
// ============================================================================

ProviderCapability GatewayManager::get_provider_capabilities(const std::string& provider_name) const {
    ProviderHealth* health = health_monitor_->get_provider_health(provider_name);
    if (!health) {
        return static_cast<ProviderCapability>(0);
    }

    ProviderCapability capabilities = static_cast<ProviderCapability>(0);

    std::shared_lock<std::shared_mutex> lock(adapters_mutex_);
    auto config_it = provider_configs_.find(provider_name);
    if (config_it != provider_configs_.end()) {
        const auto& config = config_it->second;
        if (config.supports_thinking_) {
            capabilities = capabilities | ProviderCapability::THINKING;
        }
        if (config.supports_vision_) {
            capabilities = capabilities | ProviderCapability::VISION;
        }
        if (config.supports_tools_) {
            capabilities = capabilities | ProviderCapability::TOOLS;
        }
        if (config.supports_streaming_) {
            capabilities = capabilities | ProviderCapability::STREAMING;
        }
    }

    return capabilities;
}

std::vector<std::string> GatewayManager::get_providers_with_capability(ProviderCapability capability) const {
    std::vector<std::string> providers;

    std::shared_lock<std::shared_mutex> lock(adapters_mutex_);
    for (const auto& [name, config] : provider_configs_) {
        ProviderCapability provider_caps = get_provider_capabilities(name);
        if ((provider_caps & capability) != static_cast<ProviderCapability>(0)) {
            providers.push_back(name);
        }
    }

    return providers;
}

bool GatewayManager::validate_provider_capabilities(const std::string& provider_name,
                                                  const RequestAnalysis& analysis) {
    ProviderCapability provider_caps = get_provider_capabilities(provider_name);
    ProviderCapability required_caps = analysis.required_capabilities_;

    return (provider_caps & required_caps) == required_caps;
}

// ============================================================================
// Failover and Reliability
// ============================================================================

void GatewayManager::enable_circuit_breaker(bool enabled, int max_failures) {
    // This would configure the circuit breaker for all providers
    aimux::info("GatewayManager: Circuit breaker " + std::string(enabled ? "enabled" : "disabled") +
                " with max_failures=" + std::to_string(max_failures));
}

void GatewayManager::enable_auto_recovery(bool enabled, int probe_count) {
    // This would configure auto-recovery for all providers
    aimux::info("GatewayManager: Auto recovery " + std::string(enabled ? "enabled" : "disabled") +
                " with probe_count=" + std::to_string(probe_count));
}

void GatewayManager::manually_mark_provider_healthy(const std::string& provider_name) {
    ProviderHealth* health = health_monitor_->get_provider_health(provider_name);
    if (health) {
        health->mark_success();
        aimux::info("GatewayManager: Manually marked provider healthy: " + provider_name);
    }
}

void GatewayManager::manually_mark_provider_unhealthy(const std::string& provider_name) {
    ProviderHealth* health = health_monitor_->get_provider_health(provider_name);
    if (health) {
        health->mark_failure();
        aimux::info("GatewayManager: Manually marked provider unhealthy: " + provider_name);
    }
}

// ============================================================================
// Validation and Testing
// ============================================================================

bool GatewayManager::validate_provider_config(const nlohmann::json& config) const {
    try {
        // Basic structure validation
        if (!config.contains("name") || !config["name"].is_string()) {
            return false;
        }
        if (!config.contains("base_url") || !config["base_url"].is_string()) {
            return false;
        }

        std::string name = config["name"];
        std::string base_url = config["base_url"];

        return validate_provider_name(name) && validate_base_url(base_url);
    } catch (const std::exception&) {
        return false;
    }
}

bool GatewayManager::test_provider_connectivity(const std::string& provider_name) const {
    const core::Bridge* adapter = get_provider_adapter(provider_name);
    if (!adapter) {
        return false;
    }

    return adapter->is_healthy();
}

std::vector<std::string> GatewayManager::get_configuration_errors() const {
    std::vector<std::string> errors;

    // Check for required providers
    if (default_provider_.empty()) {
        errors.push_back("No default provider configured");
    }
    if (thinking_provider_.empty()) {
        errors.push_back("No thinking provider configured");
    }

    // Check provider connectivity
    std::shared_lock<std::shared_mutex> lock(adapters_mutex_);
    for (const auto& [name, adapter] : adapters_) {
        if (!adapter->is_healthy()) {
            errors.push_back("Provider unhealthy: " + name);
        }
    }

    return errors;
}

// ============================================================================
// Debug and Diagnostics
// ============================================================================

nlohmann::json GatewayManager::debug_routing_decision(const core::Request& request) const {
    nlohmann::json debug_info;

    // Analyze request
    RequestAnalysis analysis = analyze_request(request);
    debug_info["request_analysis"] = analysis.to_json();

    // Get healthy providers
    std::vector<std::string> healthy = get_healthy_providers();
    debug_info["healthy_providers"] = healthy;

    // Provider capabilities
    nlohmann::json capabilities;
    for (const auto& provider : healthy) {
        ProviderCapability caps = get_provider_capabilities(provider);
        capabilities[provider] = capability_to_string(caps);
    }
    debug_info["provider_capabilities"] = capabilities;

    // Routing decision
    RoutingDecision decision = routing_logic_->route_request(request);
    debug_info["routing_decision"] = decision.to_json();

    return debug_info;
}

void GatewayManager::set_debug_mode(bool enabled) {
    debug_mode_.store(enabled);
    aimux::info("GatewayManager: Debug mode " + std::string(enabled ? "enabled" : "disabled"));
}

void GatewayManager::set_log_level(const std::string& level) {
    // This would configure the logging system
    aimux::info("GatewayManager: Log level set to: " + level);
}

// ============================================================================
// Internal Helper Methods
// ============================================================================

void GatewayManager::validate_provider_config_internal(const GatewayProviderConfig& config) {
    validate_provider_name(config.name_);
    validate_base_url(config.base_url_);
    validate_api_key(config.api_key_);
    validate_capability_flags(config.capability_flags_);
}

void GatewayManager::notify_provider_change(const std::string& provider_name, bool added) {
    if (provider_change_callback_) {
        provider_change_callback_(provider_name, added);
    }
}

void GatewayManager::record_routing_metrics(const RequestMetrics& metrics) {
    if (!metrics_collection_enabled_.load()) {
        return;
    }

    std::unique_lock<std::shared_mutex> lock(metrics_mutex_);
    request_metrics_.push_back(metrics);

    // Maintain history limit
    while (request_metrics_.size() > MAX_METRICS_HISTORY) {
        request_metrics_.pop_front();
    }
}

std::string GatewayManager::select_failover_provider(const std::string& failed_provider,
                                                    const core::Request& request) {
    std::vector<std::string> healthy = get_healthy_providers();

    // Remove failed provider from candidates
    healthy.erase(
        std::remove(healthy.begin(), healthy.end(), failed_provider),
        healthy.end()
    );

    if (healthy.empty()) {
        return "";
    }

    // Select based on request analysis
    RequestAnalysis analysis = analyze_request(request);
    return select_balanced_provider(healthy, analysis.type_);
}

bool GatewayManager::provider_is_available(const std::string& provider_name) const {
    if (!provider_exists(provider_name)) {
        return false;
    }

    HealthStatus status = get_provider_status(provider_name);
    return status == HealthStatus::HEALTHY || status == HealthStatus::DEGRADED;
}

std::vector<std::string> GatewayManager::get_prioritized_providers() const {
    // Return providers sorted by priority score
    std::shared_lock<std::shared_mutex> lock(adapters_mutex_);

    std::vector<std::pair<std::string, int>> provider_priorities;
    for (const auto& [name, config] : provider_configs_) {
        provider_priorities.emplace_back(name, config.priority_score_);
    }

    std::sort(provider_priorities.begin(), provider_priorities.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    std::vector<std::string> result;
    for (const auto& [name, priority] : provider_priorities) {
        result.push_back(name);
    }

    return result;
}

// ============================================================================
// Configuration Validation Methods
// ============================================================================

bool GatewayManager::validate_provider_name(const std::string& name) const {
    if (name.empty() || name.length() > 64) {
        return false;
    }

    // Check for valid characters (alphanumeric, hyphens, underscores)
    static const std::regex valid_name("^[a-zA-Z0-9_-]+$");
    return std::regex_match(name, valid_name);
}

bool GatewayManager::validate_api_key(const std::string& api_key) const {
    if (api_key.empty()) {
        return false;
    }

    // Basic validation - should at least look like an API key
    bool has_alphanumeric = false;

    for (char c : api_key) {
        if (std::isalnum(c)) {
            has_alphanumeric = true;
        }
    }

    return has_alphanumeric && api_key.length() >= 16;
}

bool GatewayManager::validate_base_url(const std::string& url) const {
    if (url.empty()) {
        return false;
    }

    // Basic URL validation
    static const std::regex valid_url(
        R"(^https?://[a-zA-Z0-9.-]+(:[0-9]+)?(/.*)?$)"
    );
    return std::regex_match(url, valid_url);
}

bool GatewayManager::validate_capability_flags(int flags) const {
    // Check if flags are within reasonable range
    return flags >= 0 && flags <= (1 << 6) - 1; // Up to 6 capability bits
}

// ============================================================================
// Error Handling
// ============================================================================

core::Response GatewayManager::create_error_response(const std::string& error_code,
                                                    const std::string& error_message,
                                                    int http_status) const {
    core::Response response;
    response.success = false;
    response.status_code = http_status;
    response.error_message = error_code + ": " + error_message;
    response.provider_name = "GatewayManager";

    // Create error JSON
    nlohmann::json error_json;
    error_json["error"]["code"] = error_code;
    error_json["error"]["message"] = error_message;
    error_json["error"]["type"] = "gateway_error";
    error_json["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    response.data = error_json.dump();
    return response;
}

void GatewayManager::log_error(const std::string& error_type, const std::string& message) const {
    aimux::error("GatewayManager [" + error_type + "]: " + message);
}

void GatewayManager::log_debug(const std::string& message) const {
    if (debug_mode_.load()) {
        aimux::debug("GatewayManager: " + message);
    }
}

} // namespace gateway
} // namespace aimux