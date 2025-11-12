#pragma once

#include <string>
#include <vector>
#include <memory>
#include "aimux/core/failover.hpp"
#include <nlohmann/json.hpp>

namespace aimux {
namespace core {

// Forward declarations
struct ProviderConfig;
struct Request;
struct Response;

/**
 * @brief Core router class that manages provider selection and failover
 */
class Router {
public:
    explicit Router(const std::vector<ProviderConfig>& providers);
    
    /**
     * @brief Route a request to the best available provider
     * @param request The request to route
     * @return Response from the provider
     */
    Response route(const Request& request);
    
    /**
     * @brief Get health status of all providers
     * @return JSON string with provider health information
     */
    std::string get_health_status() const;
    
    /**
     * @brief Get performance metrics
     * @return JSON string with performance data
     */
    std::string get_metrics() const;

private:
    std::vector<ProviderConfig> providers_;
    std::unique_ptr<FailoverManager> failover_manager_;
    std::unique_ptr<LoadBalancer> load_balancer_;
};

/**
 * @brief Configuration for a provider
 */
struct ProviderConfig {
    std::string name;
    std::string endpoint;
    std::string api_key;
    std::vector<std::string> models;
    int max_requests_per_minute = 60;
    bool enabled = true;
    
    nlohmann::json to_json() const;
    static ProviderConfig from_json(const nlohmann::json& j);
};

/**
 * @brief API request structure
 */
struct Request {
    std::string model;
    std::string method;
    nlohmann::json data;
    
    nlohmann::json to_json() const;
    static Request from_json(const nlohmann::json& j);
};

/**
 * @brief API response structure  
 */
struct Response {
    bool success = false;
    std::string data;
    std::string error_message;
    int status_code = 0;
    double response_time_ms = 0.0;
    std::string provider_name;
    
    nlohmann::json to_json() const;
    static Response from_json(const nlohmann::json& j);
};

} // namespace core
} // namespace aimux