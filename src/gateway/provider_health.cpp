#include "aimux/gateway/provider_health.hpp"
#include "aimux/logging/logger.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <random>

namespace aimux {
namespace gateway {

// PerformanceMetrics implementation
void PerformanceMetrics::update_response_time(double response_time_ms) {
    // Exponential moving average with alpha = 0.1
    const double alpha = 0.1;
    if (avg_response_time_ms_ == 0.0) {
        avg_response_time_ms_ = response_time_ms;
    } else {
        avg_response_time_ms_ = alpha * response_time_ms + (1.0 - alpha) * avg_response_time_ms_;
    }
    last_request_time_ = std::chrono::steady_clock::now();
}

void PerformanceMetrics::update_success(bool success) {
    if (success) {
        last_success_time_ = std::chrono::steady_clock::now();
        // Update success rate with EMA
        const double alpha = 0.05;
        success_rate_ = alpha * 1.0 + (1.0 - alpha) * success_rate_;
    } else {
        const double alpha = 0.05;
        success_rate_ = alpha * 0.0 + (1.0 - alpha) * success_rate_;
    }
}

void PerformanceMetrics::update_error() {
    last_error_time_ = std::chrono::steady_clock::now();
    error_rate_ = std::min(1.0, error_rate_ + 0.1);  // Simple error rate increment

    // Update success rate (inverse of error rate)
    success_rate_ = std::max(0.0, success_rate_ - 0.1);
}

void PerformanceMetrics::calculate_scores() {
    // Performance score based on response time and success rate
    // Normalize response time: 1000ms = perfect score, 5000ms = zero score
    double response_score = std::max(0.0, (5000.0 - avg_response_time_ms_) / 4000.0);

    // Combined performance score (weighted average)
    performance_score_ = 0.6 * success_rate_ + 0.4 * response_score;

    // Cost score: lower cost is better (normalize to 0-1 range)
    // Assuming reasonable cost bounds
    const double max_reasonable_cost = 20.0; // $20 per million tokens
    double total_cost_per_million = cost_per_input_token_ + cost_per_output_token_;
    cost_score_ = std::max(0.0, 1.0 - (total_cost_per_million / max_reasonable_cost));
}

nlohmann::json PerformanceMetrics::to_json() const {
    return nlohmann::json{
        {"avg_response_time_ms", avg_response_time_ms_},
        {"success_rate", success_rate_},
        {"requests_per_minute", requests_per_minute_},
        {"max_requests_per_minute", max_requests_per_minute_},
        {"error_rate", error_rate_},
        {"cost_per_input_token", cost_per_input_token_},
        {"cost_per_output_token", cost_per_output_token_},
        {"cost_score", cost_score_},
        {"performance_score", performance_score_}
    };
}

PerformanceMetrics PerformanceMetrics::from_json(const nlohmann::json& j) {
    PerformanceMetrics metrics;
    metrics.avg_response_time_ms_ = j.value("avg_response_time_ms", 0.0);
    metrics.success_rate_ = j.value("success_rate", 1.0);
    metrics.requests_per_minute_ = j.value("requests_per_minute", 0);
    metrics.max_requests_per_minute_ = j.value("max_requests_per_minute", 60);
    metrics.error_rate_ = j.value("error_rate", 0.0);
    metrics.cost_per_input_token_ = j.value("cost_per_input_token", 0.0);
    metrics.cost_per_output_token_ = j.value("cost_per_output_token", 0.0);
    metrics.cost_score_ = j.value("cost_score", 1.0);
    metrics.performance_score_ = j.value("performance_score", 1.0);
    return metrics;
}

// ProviderHealth implementation
ProviderHealth::ProviderHealth(const std::string& name) : provider_name_(name) {
    last_health_check_ = std::chrono::steady_clock::now();
    circuit_open_time_ = std::chrono::steady_clock::now();
}

// Copy constructor
ProviderHealth::ProviderHealth(const ProviderHealth& other)
    : provider_name_(other.provider_name_),
      status_(other.status_.load()),
      capability_flags_(other.capability_flags_.load()),
      metrics_(other.metrics_),
      consecutive_failures_(other.consecutive_failures_.load()),
      max_consecutive_failures_(other.max_consecutive_failures_.load()),
      failure_timeout_(other.failure_timeout_),
      circuit_open_time_(other.circuit_open_time_),
      last_error_time_(other.last_error_time_),
      health_check_interval_(other.health_check_interval_),
      last_health_check_(other.last_health_check_),
      health_check_in_progress_(other.health_check_in_progress_.load()),
      successful_probes_(other.successful_probes_.load()),
      required_probes_(other.required_probes_.load()),
      probe_interval_(other.probe_interval_) {
}

// Copy assignment operator
ProviderHealth& ProviderHealth::operator=(const ProviderHealth& other) {
    if (this != &other) {
        provider_name_ = other.provider_name_;
        status_.store(other.status_.load());
        capability_flags_.store(other.capability_flags_.load());
        metrics_ = other.metrics_;
        consecutive_failures_.store(other.consecutive_failures_.load());
        max_consecutive_failures_.store(other.max_consecutive_failures_.load());
        failure_timeout_ = other.failure_timeout_;
        circuit_open_time_ = other.circuit_open_time_;
        last_error_time_ = other.last_error_time_;
        health_check_interval_ = other.health_check_interval_;
        last_health_check_ = other.last_health_check_;
        health_check_in_progress_.store(other.health_check_in_progress_.load());
        successful_probes_.store(other.successful_probes_.load());
        required_probes_.store(other.required_probes_.load());
        probe_interval_ = other.probe_interval_;
    }
    return *this;
}

// Move constructor
ProviderHealth::ProviderHealth(ProviderHealth&& other) noexcept
    : provider_name_(std::move(other.provider_name_)),
      status_(other.status_.load()),
      capability_flags_(other.capability_flags_.load()),
      metrics_(std::move(other.metrics_)),
      consecutive_failures_(other.consecutive_failures_.load()),
      max_consecutive_failures_(other.max_consecutive_failures_.load()),
      failure_timeout_(other.failure_timeout_),
      circuit_open_time_(other.circuit_open_time_),
      last_error_time_(other.last_error_time_),
      health_check_interval_(other.health_check_interval_),
      last_health_check_(other.last_health_check_),
      health_check_in_progress_(other.health_check_in_progress_.load()),
      successful_probes_(other.successful_probes_.load()),
      required_probes_(other.required_probes_.load()),
      probe_interval_(other.probe_interval_) {
}

// Move assignment operator
ProviderHealth& ProviderHealth::operator=(ProviderHealth&& other) noexcept {
    if (this != &other) {
        provider_name_ = std::move(other.provider_name_);
        status_.store(other.status_.load());
        capability_flags_.store(other.capability_flags_.load());
        metrics_ = std::move(other.metrics_);
        consecutive_failures_.store(other.consecutive_failures_.load());
        max_consecutive_failures_.store(other.max_consecutive_failures_.load());
        failure_timeout_ = other.failure_timeout_;
        circuit_open_time_ = other.circuit_open_time_;
        last_error_time_ = other.last_error_time_;
        health_check_interval_ = other.health_check_interval_;
        last_health_check_ = other.last_health_check_;
        health_check_in_progress_.store(other.health_check_in_progress_.load());
        successful_probes_.store(other.successful_probes_.load());
        required_probes_.store(other.required_probes_.load());
        probe_interval_ = other.probe_interval_;
    }
    return *this;
}

void ProviderHealth::mark_success() {
    consecutive_failures_.store(0);

    if (status_.load() == HealthStatus::CIRCUIT_OPEN) {
        // In circuit open state, count successful probes for recovery
        successful_probes_.fetch_add(1);
        if (successful_probes_.load() >= required_probes_.load()) {
            close_circuit();
        }
    } else if (status_.load() == HealthStatus::UNHEALTHY) {
        status_.store(HealthStatus::HEALTHY);
    }

    metrics_.update_success(true);
}

void ProviderHealth::mark_failure() {
    int failures = consecutive_failures_.fetch_add(1) + 1;
    last_error_time_ = std::chrono::steady_clock::now();

    metrics_.update_error();

    if (failures >= max_consecutive_failures_.load()) {
        open_circuit();
    } else if (failures >= 2) {
        status_.store(HealthStatus::UNHEALTHY);
    }
}

void ProviderHealth::open_circuit() {
    status_.store(HealthStatus::CIRCUIT_OPEN);
    circuit_open_time_ = std::chrono::steady_clock::now();
    consecutive_failures_.store(max_consecutive_failures_.load());
    successful_probes_.store(0);
}

void ProviderHealth::attempt_recovery() {
    if (status_.load() == HealthStatus::CIRCUIT_OPEN) {
        // Perform a health probe
        health_check_in_progress_.store(true);
        // The actual health check will be performed by the monitoring thread
    }
}

void ProviderHealth::close_circuit() {
    status_.store(HealthStatus::HEALTHY);
    consecutive_failures_.store(0);
    successful_probes_.store(0);
}

void ProviderHealth::set_capability(ProviderCapability capability, bool enabled) {
    int flags = capability_flags_.load();
    if (enabled) {
        flags |= static_cast<int>(capability);
    } else {
        flags &= ~static_cast<int>(capability);
    }
    capability_flags_.store(flags);
}

bool ProviderHealth::has_capability(ProviderCapability capability) const {
    int flags = capability_flags_.load();
    return (flags & static_cast<int>(capability)) != 0;
}

bool ProviderHealth::is_healthy() const {
    HealthStatus current_status = status_.load();
    return current_status == HealthStatus::HEALTHY || current_status == HealthStatus::DEGRADED;
}

bool ProviderHealth::can_accept_requests() const {
    HealthStatus current_status = status_.load();
    bool circuit_closed = current_status != HealthStatus::CIRCUIT_OPEN;

    if (!circuit_closed) {
        // Check if circuit timeout has passed
        auto now = std::chrono::steady_clock::now();
        auto timeout_passed = (now - circuit_open_time_) >= std::chrono::seconds(300); // 5 minutes
        return timeout_passed;
    }

    return true;
}

std::chrono::seconds ProviderHealth::get_retry_delay() const {
    HealthStatus current_status = status_.load();
    if (current_status == HealthStatus::CIRCUIT_OPEN) {
        auto elapsed = std::chrono::steady_clock::now() - circuit_open_time_;
        auto remaining = std::chrono::seconds(300) - std::chrono::duration_cast<std::chrono::seconds>(elapsed);
        return std::max(std::chrono::seconds(1), remaining);
    }
    return std::chrono::seconds(0);
}

void ProviderHealth::update_metrics(const core::Response& response, double request_time_ms) {
    metrics_.update_response_time(request_time_ms);
    metrics_.update_success(response.success);
    metrics_.calculate_scores();
}

void ProviderHealth::reset_metrics() {
    metrics_ = PerformanceMetrics();
    consecutive_failures_.store(0);
    successful_probes_.store(0);
    status_.store(HealthStatus::HEALTHY);
}

nlohmann::json ProviderHealth::to_json() const {
    return nlohmann::json{
        {"provider_name", provider_name_},
        {"status", health_status_to_string(status_.load())},
        {"consecutive_failures", consecutive_failures_.load()},
        {"max_consecutive_failures", max_consecutive_failures_.load()},
        {"last_success_time", std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - metrics_.last_success_time_).count()},
        {"last_error_time", std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - metrics_.last_error_time_).count()},
        {"health_check_in_progress", health_check_in_progress_.load()},
        {"capability_flags", capability_flags_.load()},
        {"metrics", metrics_.to_json()}
    };
}

ProviderHealth ProviderHealth::from_json(const nlohmann::json& j) {
    ProviderHealth health(j.value("provider_name", ""));
    health.status_.store(string_to_health_status(j.value("status", "HEALTHY")));
    health.consecutive_failures_.store(j.value("consecutive_failures", 0));
    health.max_consecutive_failures_.store(j.value("max_consecutive_failures", 5));
    health.capability_flags_.store(j.value("capability_flags", 0));
    health.health_check_in_progress_.store(j.value("health_check_in_progress", false));
    health.metrics_ = PerformanceMetrics::from_json(j.value("metrics", nlohmann::json::object()));
    return health;
}

// ProviderHealthMonitor implementation
ProviderHealthMonitor::ProviderHealthMonitor() {
    // Load balancer will be set later
}

ProviderHealthMonitor::~ProviderHealthMonitor() {
    stop_monitoring();
}

void ProviderHealthMonitor::add_provider(const std::string& provider_name, const nlohmann::json& config) {
    std::unique_lock<std::shared_mutex> lock(providers_mutex_);

    auto health = std::make_unique<ProviderHealth>(provider_name);

    // Configure health check settings
    if (config.contains("health_check_interval")) {
        health->health_check_interval_ = std::chrono::seconds(config["health_check_interval"]);
    }
    if (config.contains("max_failures")) {
        health->max_consecutive_failures_ = config["max_failures"];
    }
    if (config.contains("failure_timeout")) {
        health->failure_timeout_ = std::chrono::seconds(config["failure_timeout"]);
    }

    // Configure capabilities
    if (config.contains("capabilities")) {
        for (const auto& cap : config["capabilities"]) {
            std::string cap_str = cap.get<std::string>();
            ProviderCapability capability = string_to_capability(cap_str);
            health->set_capability(capability, true);
        }
    }

    // Configure cost and performance
    if (config.contains("cost_per_input_token")) {
        health->metrics_.cost_per_input_token_ = config["cost_per_input_token"];
    }
    if (config.contains("cost_per_output_token")) {
        health->metrics_.cost_per_output_token_ = config["cost_per_output_token"];
    }

    providers_[provider_name] = std::move(health);

    aimux::info("Added provider to health monitoring: " + provider_name);
}

void ProviderHealthMonitor::remove_provider(const std::string& provider_name) {
    std::unique_lock<std::shared_mutex> lock(providers_mutex_);
    providers_.erase(provider_name);
    aimux::info("Removed provider from health monitoring: " + provider_name);
}

ProviderHealth* ProviderHealthMonitor::get_provider_health(const std::string& provider_name) {
    std::shared_lock<std::shared_mutex> lock(providers_mutex_);
    auto it = providers_.find(provider_name);
    return (it != providers_.end()) ? it->second.get() : nullptr;
}

const ProviderHealth* ProviderHealthMonitor::get_provider_health(const std::string& provider_name) const {
    std::shared_lock<std::shared_mutex> lock(providers_mutex_);
    auto it = providers_.find(provider_name);
    return (it != providers_.end()) ? it->second.get() : nullptr;
}

void ProviderHealthMonitor::start_monitoring() {
    if (monitoring_active_.load()) {
        aimux::warn("Health monitoring is already active");
        return;
    }

    monitoring_active_.store(true);
    monitoring_thread_ = std::thread(&ProviderHealthMonitor::monitoring_loop, this);

    aimux::info("Started provider health monitoring");
}

void ProviderHealthMonitor::stop_monitoring() {
    if (!monitoring_active_.load()) {
        return;
    }

    monitoring_active_.store(false);
    if (monitoring_thread_.joinable()) {
        monitoring_thread_.join();
    }

    aimux::info("Stopped provider health monitoring");
}

std::vector<std::string> ProviderHealthMonitor::get_healthy_providers() const {
    std::shared_lock<std::shared_mutex> lock(providers_mutex_);
    std::vector<std::string> healthy_providers;

    for (const auto& [name, health] : providers_) {
        if (health->is_healthy() && health->can_accept_requests()) {
            healthy_providers.push_back(name);
        }
    }

    return healthy_providers;
}

std::vector<std::string> ProviderHealthMonitor::get_providers_with_capability(ProviderCapability capability) const {
    std::shared_lock<std::shared_mutex> lock(providers_mutex_);
    std::vector<std::string> capable_providers;

    for (const auto& [name, health] : providers_) {
        if (health->has_capability(capability) && health->can_accept_requests()) {
            capable_providers.push_back(name);
        }
    }

    return capable_providers;
}

std::vector<std::string> ProviderHealthMonitor::get_unhealthy_providers() const {
    std::shared_lock<std::shared_mutex> lock(providers_mutex_);
    std::vector<std::string> unhealthy_providers;

    for (const auto& [name, health] : providers_) {
        if (!health->is_healthy() || !health->can_accept_requests()) {
            unhealthy_providers.push_back(name);
        }
    }

    return unhealthy_providers;
}

HealthStatus ProviderHealthMonitor::get_provider_status(const std::string& provider_name) const {
    const ProviderHealth* health = get_provider_health(provider_name);
    return health ? health->status_.load() : HealthStatus::UNHEALTHY;
}

void ProviderHealthMonitor::update_provider_metrics(const std::string& provider_name,
                                                   const core::Response& response,
                                                   double request_time_ms) {
    ProviderHealth* health = get_provider_health(provider_name);
    if (health) {
        health->update_metrics(response, request_time_ms);
    }
}

nlohmann::json ProviderHealthMonitor::get_all_provider_health() const {
    std::shared_lock<std::shared_mutex> lock(providers_mutex_);
    nlohmann::json health_json = nlohmann::json::object();

    for (const auto& [name, health] : providers_) {
        health_json[name] = health->to_json();
    }

    return health_json;
}

nlohmann::json ProviderHealthMonitor::get_provider_health_json(const std::string& provider_name) const {
    const ProviderHealth* health = get_provider_health(provider_name);
    return health ? health->to_json() : nlohmann::json::object();
}

void ProviderHealthMonitor::set_health_check_interval(const std::chrono::seconds& interval) {
    health_check_interval_ = interval;

    std::shared_lock<std::shared_mutex> lock(providers_mutex_);
    for (auto& [name, health] : providers_) {
        health->health_check_interval_ = interval;
    }
}

void ProviderHealthMonitor::set_circuit_breaker_threshold(const std::string& provider_name, int threshold) {
    ProviderHealth* health = get_provider_health(provider_name);
    if (health) {
        health->max_consecutive_failures_.store(threshold);
    }
}

void ProviderHealthMonitor::set_failure_timeout(const std::string& provider_name, const std::chrono::seconds& timeout) {
    ProviderHealth* health = get_provider_health(provider_name);
    if (health) {
        health->failure_timeout_ = timeout;
    }
}

void ProviderHealthMonitor::monitoring_loop() {
    auto last_check = std::chrono::steady_clock::now();

    while (monitoring_active_.load()) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = now - last_check;

        if (elapsed >= health_check_interval_) {
            perform_periodic_checks();
            check_circuit_breakers();
            last_check = now;
        }

        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}

void ProviderHealthMonitor::perform_periodic_checks() {
    std::shared_lock<std::shared_mutex> lock(providers_mutex_);

    for (const auto& [name, health] : providers_) {
        if (!health->health_check_in_progress_.load()) {
            auto now = std::chrono::steady_clock::now();
            auto time_since_check = now - health->last_health_check_;

            if (time_since_check >= health->health_check_interval_) {
                perform_health_check(name);
            }
        }
    }
}

void ProviderHealthMonitor::perform_health_check(const std::string& provider_name) {
    ProviderHealth* health = get_provider_health(provider_name);
    if (!health || health->health_check_in_progress_.load()) {
        return;
    }

    health->health_check_in_progress_.store(true);
    health->last_health_check_ = std::chrono::steady_clock::now();

    // In a real implementation, this would make an actual health check request
    // For now, we'll simulate based on current status
    try {
        // Simple health check - in real implementation, make request to provider
        bool is_healthy = (std::rand() % 100) > 10; // 90% success rate for simulation

        if (is_healthy) {
            health->mark_success();
            aimux::debug("Health check passed for provider: " + provider_name);
        } else {
            health->mark_failure();
            aimux::warn("Health check failed for provider: " + provider_name);
        }
    } catch (const std::exception& e) {
        health->mark_failure();
        aimux::error("Health check exception for provider " + provider_name + ": " + e.what());
    }

    health->health_check_in_progress_.store(false);
}

void ProviderHealthMonitor::check_circuit_breakers() {
    std::shared_lock<std::shared_mutex> lock(providers_mutex_);

    for (const auto& [name, health] : providers_) {
        if (health->status_.load() == HealthStatus::CIRCUIT_OPEN) {
            auto now = std::chrono::steady_clock::now();
            auto time_circuit_open = now - health->circuit_open_time_;

            if (time_circuit_open >= health->failure_timeout_) {
                // Attempt recovery after timeout
                health->attempt_recovery();
            }
        }
    }
}

// Utility function implementations are now inline in header

} // namespace gateway
} // namespace aimux