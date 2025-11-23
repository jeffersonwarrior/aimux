#include "aimux/core/router.hpp"
#include "aimux/core/failover.hpp"
#include "aimux/core/bridge.hpp"
#include <algorithm>
#include <stdexcept>
#include <chrono>

namespace aimux {
namespace core {

Router::Router(const std::vector<ProviderConfig>& providers) 
    : providers_(providers) {
    
    std::vector<std::string> provider_names;
    for (const auto& provider : providers_) {
        provider_names.push_back(provider.name);
    }
    
    failover_manager_ = std::make_unique<FailoverManager>(provider_names);
    load_balancer_ = std::make_unique<LoadBalancer>(LoadBalancer::Strategy::FASTEST_RESPONSE);
}

Response Router::route(const Request& request) {
    // Get available providers
    std::vector<std::string> available_providers;
    for (const auto& provider : providers_) {
        if (provider.enabled && failover_manager_->is_available(provider.name)) {
            available_providers.push_back(provider.name);
        }
    }
    
    if (available_providers.empty()) {
        Response response;
        response.success = false;
        response.error_message = "No available providers";
        response.status_code = 503;
        return response;
    }
    
    // Select best provider using load balancer
    std::string selected_provider = load_balancer_->select_provider(available_providers);
    
    // Find provider config
    auto provider_it = std::find_if(providers_.begin(), providers_.end(),
        [&selected_provider](const ProviderConfig& config) {
            return config.name == selected_provider;
        });
    
    if (provider_it == providers_.end()) {
        Response response;
        response.success = false;
        response.error_message = "Selected provider not found: " + selected_provider;
        response.status_code = 500;
        return response;
    }
    
    // Create bridge and send request
    try {
        auto bridge = BridgeFactory::create_bridge(provider_it->name, provider_it->to_json());
        auto start_time = std::chrono::high_resolution_clock::now();
        
        Response response = bridge->send_request(request);
        auto end_time = std::chrono::high_resolution_clock::now();
        
        // Update metrics
        response.response_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
        response.provider_name = selected_provider;
        load_balancer_->update_response_time(selected_provider, response.response_time_ms);
        
        if (response.success) {
            failover_manager_->mark_healthy(selected_provider);
        } else {
            failover_manager_->mark_failed(selected_provider);
        }
        
        return response;
    } catch (const std::exception& e) {
        failover_manager_->mark_failed(selected_provider);
        
        Response response;
        response.success = false;
        response.error_message = std::string("Provider error: ") + e.what();
        response.status_code = 502;
        response.provider_name = selected_provider;
        
        return response;
    }
}

std::string Router::get_health_status() const {
    nlohmann::json status;
    status["providers"] = nlohmann::json::array();
    
    for (const auto& provider : providers_) {
        nlohmann::json provider_status;
        provider_status["name"] = provider.name;
        provider_status["enabled"] = provider.enabled;
        provider_status["available"] = failover_manager_->is_available(provider.name);
        provider_status["endpoint"] = provider.endpoint;
        status["providers"].push_back(provider_status);
    }
    
    return status.dump(2);
}

std::string Router::get_metrics() const {
    nlohmann::json metrics;
    metrics["failover"] = failover_manager_->get_statistics();
    metrics["load_balancer"] = load_balancer_->get_statistics();
    return metrics.dump(2);
}

// ProviderConfig implementation
nlohmann::json ProviderConfig::to_json() const {
    nlohmann::json j;
    j["name"] = name;
    j["endpoint"] = endpoint;
    j["api_key"] = api_key;
    j["models"] = models;
    j["max_requests_per_minute"] = max_requests_per_minute;
    j["enabled"] = enabled;
    return j;
}

ProviderConfig ProviderConfig::from_json(const nlohmann::json& j) {
    ProviderConfig config;
    config.name = j.value("name", "");
    config.endpoint = j.value("endpoint", "");
    config.api_key = j.value("api_key", "");
    config.models = j.value("models", std::vector<std::string>{});
    config.max_requests_per_minute = j.value("max_requests_per_minute", 60);
    config.enabled = j.value("enabled", true);
    return config;
}

// Request implementation
nlohmann::json Request::to_json() const {
    nlohmann::json j;
    j["model"] = model;
    j["method"] = method;
    j["data"] = data;
    return j;
}

Request Request::from_json(const nlohmann::json& j) {
    Request request;
    request.model = j.value("model", "");
    request.method = j.value("method", "");
    request.data = j.value("data", nlohmann::json{});
    return request;
}

// Response implementation
nlohmann::json Response::to_json() const {
    nlohmann::json j;
    j["success"] = success;
    j["data"] = data;
    j["error_message"] = error_message;
    j["status_code"] = status_code;
    j["response_time_ms"] = response_time_ms;
    j["provider_name"] = provider_name;
    return j;
}

Response Response::from_json(const nlohmann::json& j) {
    Response response;
    response.success = j.value("success", false);
    response.data = j.value("data", "");
    response.error_message = j.value("error_message", "");
    response.status_code = j.value("status_code", 0);
    response.response_time_ms = j.value("response_time_ms", 0.0);
    response.provider_name = j.value("provider_name", "");
    return response;
}

} // namespace core
} // namespace aimux