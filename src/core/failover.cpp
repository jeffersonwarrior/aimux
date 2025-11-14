#include "aimux/core/failover.hpp"
#include <algorithm>
#include <random>
#include <stdexcept>
#include <limits>

namespace aimux {
namespace core {

// FailoverManager implementation
FailoverManager::FailoverManager(const std::vector<std::string>& providers) {
    for (const auto& provider : providers) {
        ProviderStatus status;
        status.name = provider;
        provider_statuses_.push_back(status);
    }
}

std::string FailoverManager::get_next_provider(const std::string& failed_provider) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Mark the failed provider
    mark_failed(failed_provider);
    
    // Find next available provider
    for (auto& status : provider_statuses_) {
        if (status.name != failed_provider && 
            !status.is_failed && 
            is_cool_down_expired(status)) {
            return status.name;
        }
    }
    
    // Check if any failed providers have recovered
    for (auto& status : provider_statuses_) {
        if (status.name != failed_provider && 
            status.is_failed && 
            is_cool_down_expired(status)) {
            status.is_failed = false;
            return status.name;
        }
    }
    
    return ""; // No available providers
}

void FailoverManager::mark_failed(const std::string& provider, int cooldown_minutes) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (auto* status = find_provider(provider)) {
        status->is_failed = true;
        status->fail_time = std::chrono::steady_clock::now();
        status->cooldown_minutes = cooldown_minutes;
        status->failure_count++;
    }
}

void FailoverManager::mark_healthy(const std::string& provider) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (auto* status = find_provider(provider)) {
        status->is_failed = false;
        status->failure_count = std::max(0, status->failure_count - 1);
    }
}

bool FailoverManager::is_available(const std::string& provider) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (auto* status = find_provider(provider)) {
        return !status->is_failed || is_cool_down_expired(*status);
    }
    
    return false;
}

std::vector<std::string> FailoverManager::get_available_providers() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> available;
    for (const auto& status : provider_statuses_) {
        if (!status.is_failed || is_cool_down_expired(status)) {
            available.push_back(status.name);
        }
    }
    
    return available;
}

nlohmann::json FailoverManager::get_statistics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    nlohmann::json stats;
    stats["providers"] = nlohmann::json::array();
    
    for (const auto& status : provider_statuses_) {
        nlohmann::json provider_stats;
        provider_stats["name"] = status.name;
        provider_stats["is_failed"] = status.is_failed;
        provider_stats["failure_count"] = status.failure_count;
        
        if (status.is_failed) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(now - status.fail_time).count();
            int remaining_cooldown = status.cooldown_minutes - static_cast<int>(elapsed);
            provider_stats["cooldown_remaining_minutes"] = std::max(0, remaining_cooldown);
        }
        
        stats["providers"].push_back(provider_stats);
    }
    
    return stats;
}

void FailoverManager::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (auto& status : provider_statuses_) {
        status.is_failed = false;
        status.failure_count = 0;
    }
}

FailoverManager::ProviderStatus* FailoverManager::find_provider(const std::string& provider) {
    auto it = std::find_if(provider_statuses_.begin(), provider_statuses_.end(),
        [&provider](const ProviderStatus& status) { return status.name == provider; });
    return (it != provider_statuses_.end()) ? &(*it) : nullptr;
}

const FailoverManager::ProviderStatus* FailoverManager::find_provider(const std::string& provider) const {
    auto it = std::find_if(provider_statuses_.begin(), provider_statuses_.end(),
        [&provider](const ProviderStatus& status) { return status.name == provider; });
    return (it != provider_statuses_.end()) ? &(*it) : nullptr;
}

bool FailoverManager::is_cool_down_expired(const ProviderStatus& status) const {
    if (!status.is_failed) return true;
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(now - status.fail_time).count();
    return elapsed >= status.cooldown_minutes;
}

// LoadBalancer implementation
LoadBalancer::LoadBalancer(Strategy strategy) : strategy_(strategy) {}

std::string LoadBalancer::select_provider(const std::vector<std::string>& available_providers) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (available_providers.empty()) {
        return "";
    }
    
    // Initialize metrics for new providers
    for (const auto& provider : available_providers) {
        if (!find_metrics(provider)) {
            ProviderMetrics metrics;
            metrics.name = provider;
            provider_metrics_.push_back(metrics);
        }
    }
    
    switch (strategy_) {
        case Strategy::ROUND_ROBIN:
            return select_round_robin(available_providers);
        case Strategy::LEAST_CONNECTIONS:
            return select_least_connections(available_providers);
        case Strategy::FASTEST_RESPONSE:
            return select_fastest_response(available_providers);
        case Strategy::WEIGHTED_ROUND_ROBIN:
            return select_weighted_round_robin(available_providers);
        case Strategy::ADAPTIVE:
            return select_adaptive(available_providers);
        case Strategy::RANDOM:
            return select_random(available_providers);
        default:
            return available_providers[0];
    }
}

void LoadBalancer::update_response_time(const std::string& provider, double response_time_ms) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (auto* metrics = find_metrics(provider)) {
        update_avg_response_time(*metrics, response_time_ms);
    }
}

void LoadBalancer::update_connections(const std::string& provider, int connections) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (auto* metrics = find_metrics(provider)) {
        metrics->current_connections = connections;
    }
}

void LoadBalancer::set_strategy(Strategy strategy) {
    std::lock_guard<std::mutex> lock(mutex_);
    strategy_ = strategy;
}

nlohmann::json LoadBalancer::get_statistics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    nlohmann::json stats;
    stats["strategy"] = strategy_to_string(strategy_);
    stats["providers"] = nlohmann::json::array();
    stats["round_robin_index"] = round_robin_index_;
    
    for (const auto& metrics : provider_metrics_) {
        nlohmann::json provider_stats;
        provider_stats["name"] = metrics.name;
        provider_stats["avg_response_time_ms"] = metrics.avg_response_time_ms;
        provider_stats["current_connections"] = metrics.current_connections;
        provider_stats["total_requests"] = metrics.total_requests;
        stats["providers"].push_back(provider_stats);
    }
    
    return stats;
}

void LoadBalancer::update_avg_response_time(ProviderMetrics& metrics, double new_response_time) {
    metrics.total_requests++;
    metrics.response_time_sum += new_response_time;
    metrics.avg_response_time_ms = metrics.response_time_sum / metrics.total_requests;
}

LoadBalancer::ProviderMetrics* LoadBalancer::find_metrics(const std::string& provider) {
    auto it = std::find_if(provider_metrics_.begin(), provider_metrics_.end(),
        [&provider](const ProviderMetrics& metrics) { return metrics.name == provider; });
    return (it != provider_metrics_.end()) ? &(*it) : nullptr;
}

std::string LoadBalancer::select_round_robin(const std::vector<std::string>& available_providers) {
    if (round_robin_index_ >= available_providers.size()) {
        round_robin_index_ = 0;
    }
    return available_providers[round_robin_index_++];
}

std::string LoadBalancer::select_least_connections(const std::vector<std::string>& available_providers) {
    std::string best_provider = available_providers[0];
    int min_connections = std::numeric_limits<int>::max();
    
    for (const auto& provider : available_providers) {
        if (auto* metrics = find_metrics(provider)) {
            if (metrics->current_connections < min_connections) {
                min_connections = metrics->current_connections;
                best_provider = provider;
            }
        }
    }
    
    return best_provider;
}

std::string LoadBalancer::select_fastest_response(const std::vector<std::string>& available_providers) {
    std::string best_provider = available_providers[0];
    double best_time = std::numeric_limits<double>::max();
    
    for (const auto& provider : available_providers) {
        if (auto* metrics = find_metrics(provider)) {
            if (metrics->avg_response_time_ms < best_time && metrics->avg_response_time_ms > 0) {
                best_time = metrics->avg_response_time_ms;
                best_provider = provider;
            }
        } else {
            // New provider with no metrics yet
            return provider;
        }
    }
    
    return best_provider;
}

std::string LoadBalancer::select_adaptive(const std::vector<std::string>& available_providers) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (available_providers.empty()) {
        return "";
    }
    
    // Adaptive algorithm: combines multiple factors
    std::vector<std::tuple<std::string, double, int>> scored_providers;
    
    for (const auto& provider : available_providers) {
        if (auto* metrics = find_metrics(provider)) {
            double response_score = metrics->avg_response_time_ms > 0 ? 
                                100.0 / metrics->avg_response_time_ms : 100.0;
            int connection_score = std::max(0, 10 - metrics->current_connections);
            double overall_score = (response_score * 0.7) + (connection_score * 0.3);
            
            scored_providers.emplace_back(provider, overall_score, metrics->total_requests);
        }
    }
    
    // Sort by score (highest first), then by total_requests
    std::sort(scored_providers.begin(), scored_providers.end(),
        [](const auto& a, const auto& b) {
            if (std::get<1>(a) != std::get<1>(b)) {
                return std::get<1>(a) > std::get<1>(b);
            }
            return std::get<2>(a) < std::get<2>(b);
        });
    
    return std::get<0>(scored_providers[0]);
}

std::string LoadBalancer::select_weighted_round_robin(const std::vector<std::string>& available_providers) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (available_providers.empty()) {
        return "";
    }
    
    // For weighted round robin, we'll use response times as weights (faster = higher weight)
    std::vector<std::pair<std::string, double>> weighted_providers;
    double total_weight = 0.0;
    
    for (const auto& provider : available_providers) {
        if (auto* metrics = find_metrics(provider)) {
            // Inverse response time as weight (faster = higher weight)
            double weight = metrics->avg_response_time_ms > 0 ? 
                          1000.0 / metrics->avg_response_time_ms : 1000.0;
            weighted_providers.emplace_back(provider, weight);
            total_weight += weight;
        } else {
            // New provider gets default weight
            weighted_providers.emplace_back(provider, 100.0);
            total_weight += 100.0;
        }
    }
    
    // Select provider based on weights
    std::uniform_real_distribution<double> dist(0.0, total_weight);
    double random_weight = dist(rng_);
    
    double current_weight = 0.0;
    for (const auto& [provider, weight] : weighted_providers) {
        current_weight += weight;
        if (random_weight <= current_weight) {
            return provider;
        }
    }
    
    // Fallback to first provider
    return available_providers[0];
}

std::string LoadBalancer::select_random(const std::vector<std::string>& available_providers) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<std::size_t> dis(0, available_providers.size() - 1);
    return available_providers[dis(gen)];
}

std::string LoadBalancer::strategy_to_string(Strategy strategy) const {
    switch (strategy) {
        case Strategy::ROUND_ROBIN: return "round_robin";
        case Strategy::LEAST_CONNECTIONS: return "least_connections";
        case Strategy::FASTEST_RESPONSE: return "fastest_response";
        case Strategy::RANDOM: return "random";
        default: return "unknown";
    }
}

} // namespace core
} // namespace aimux