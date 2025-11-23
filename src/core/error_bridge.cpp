#include "aimux/core/bridge.hpp"
#include <stdexcept>

namespace aimux {
namespace core {

ErrorBridge::ErrorBridge(const std::string& provider_name) 
    : provider_name_(provider_name), request_count_(0) {}

Response ErrorBridge::send_request(const Request& request) {
    Response response;
    response.success = false;
    response.status_code = 503;
    response.provider_name = provider_name_;
    response.response_time_ms = 0;
    response.error_message = "Provider '" + provider_name_ + "' is not available or properly configured. Please check your configuration and ensure the provider is supported.";
    request_count_++;
    
    return response;
}

bool ErrorBridge::is_healthy() const {
    return false; // Error bridge is never healthy
}

std::string ErrorBridge::get_provider_name() const {
    return provider_name_;
}

nlohmann::json ErrorBridge::get_rate_limit_status() const {
    nlohmann::json status;
    status["error"] = true;
    status["message"] = "Provider not available";
    status["configured"] = false;
    return status;
}

} // namespace core
} // namespace aimux
