#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <mutex>
#include <nlohmann/json.hpp>

namespace aimux {
namespace core {

/**
 * @brief Failover manager for handling provider failures and automatic switching
 */
class FailoverManager {
public:
    explicit FailoverManager(const std::vector<std::string>& providers);
    
    /**
     * @brief Get the next available provider for failover
     * @param failed_provider Provider that failed
     * @return Next provider name or empty string if none available
     */
    std::string get_next_provider(const std::string& failed_provider);
    
    /**
     * @brief Mark a provider as failed
     * @param provider Provider name
     * @param cooldown_minutes Cool down period in minutes
     */
    void mark_failed(const std::string& provider, int cooldown_minutes = 5);
    
    /**
     * @brief Mark a provider as healthy again
     * @param provider Provider name
     */
    void mark_healthy(const std::string& provider);
    
    /**
     * @brief Check if a provider is currently available
     * @param provider Provider name
     * @return true if available
     */
    bool is_available(const std::string& provider) const;
    
    /**
     * @brief Get list of all available providers
     * @return Vector of available provider names
     */
    std::vector<std::string> get_available_providers() const;
    
    /**
     * @brief Get failover statistics
     * @return JSON with failover metrics
     */
    nlohmann::json get_statistics() const;
    
    /**
     * @brief Reset all failover state
     */
    void reset();

private:
    struct ProviderStatus {
        std::string name;
        bool is_failed = false;
        std::chrono::steady_clock::time_point fail_time;
        int cooldown_minutes = 5;
        int failure_count = 0;
    };
    
    std::vector<ProviderStatus> provider_statuses_;
    mutable std::mutex mutex_; // Thread safety
    
    /**
     * @brief Find provider status by name
     * @param provider Provider name
     * @return Pointer to status or nullptr
     */
    ProviderStatus* find_provider(const std::string& provider);
    const ProviderStatus* find_provider(const std::string& provider) const;
    
    /**
     * @brief Check if cool down has expired for a failed provider
     * @param status Provider status
     * @return true if cool down expired
     */
    bool is_cool_down_expired(const ProviderStatus& status) const;
};

/**
 * @brief Load balancer for distributing requests across providers
 */
class LoadBalancer {
public:
    enum class Strategy {
        ROUND_ROBIN,
        LEAST_CONNECTIONS,
        FASTEST_RESPONSE,
        RANDOM
    };
    
    explicit LoadBalancer(Strategy strategy = Strategy::ROUND_ROBIN);
    
    /**
     * @brief Select the best provider for a request
     * @param available_providers List of available providers
     * @return Selected provider name
     */
    std::string select_provider(const std::vector<std::string>& available_providers);
    
    /**
     * @brief Update response time metric for a provider
     * @param provider Provider name
     * @param response_time_ms Response time in milliseconds
     */
    void update_response_time(const std::string& provider, double response_time_ms);
    
    /**
     * @brief Update connection count for a provider
     * @param provider Provider name
     * @param connections Current number of connections
     */
    void update_connections(const std::string& provider, int connections);
    
    /**
     * @brief Set load balancing strategy
     * @param strategy New strategy
     */
    void set_strategy(Strategy strategy);
    
    /**
     * @brief Get load balancer statistics
     * @return JSON with load balancing metrics
     */
    nlohmann::json get_statistics() const;

private:
    struct ProviderMetrics {
        std::string name;
        double avg_response_time_ms = 0.0;
        int current_connections = 0;
        int total_requests = 0;
        double response_time_sum = 0.0;
    };
    
    Strategy strategy_;
    std::vector<ProviderMetrics> provider_metrics_;
    size_t round_robin_index_ = 0;
    mutable std::mutex mutex_;
    
    /**
     * @brief Update average response time
     * @param metrics Provider metrics
     * @param new_response_time New response time
     */
    void update_avg_response_time(ProviderMetrics& metrics, double new_response_time);
    
    /**
     * @brief Find provider metrics by name
     * @param provider Provider name
     * @return Pointer to metrics or nullptr
     */
    ProviderMetrics* find_metrics(const std::string& provider);
    
    /**
     * @brief Helper methods for different strategies
     */
    std::string select_round_robin(const std::vector<std::string>& available_providers);
    std::string select_least_connections(const std::vector<std::string>& available_providers);
    std::string select_fastest_response(const std::vector<std::string>& available_providers);
    std::string select_random(const std::vector<std::string>& available_providers);
    
    /**
     * @brief Convert strategy enum to string
     * @param strategy Strategy enum
     * @return String representation
     */
    std::string strategy_to_string(Strategy strategy) const;
};

} // namespace core
} // namespace aimux