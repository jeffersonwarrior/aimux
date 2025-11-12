#include "aimux/core/bridge.hpp"
#include <thread>
#include <chrono>

namespace aimux {
namespace core {

// BridgeFactory implementation
std::unique_ptr<Bridge> BridgeFactory::create_bridge(
    const std::string& provider_name,
    const nlohmann::json& config) {
    
    // For now, return mock bridge for testing
    // TODO: Implement actual provider bridges
    return std::make_unique<MockBridge>(provider_name);
}

std::vector<std::string> BridgeFactory::get_supported_providers() {
    return {"cerebras", "zai", "synthetic", "mock"};
}

// MockBridge implementation
MockBridge::MockBridge(const std::string& provider_name) 
    : provider_name_(provider_name) {}

Response MockBridge::send_request(const Request& request) {
    Response response;
    response.provider_name = provider_name_;
    request_count_++;
    
    // Simulate some processing time
    std::this_thread::sleep_for(std::chrono::milliseconds(50 + (request_count_ % 100)));
    
    // Mock success for most requests, some failures for testing
    if (request_count_ % 10 == 0) {
        // Simulate occasional failure
        response.success = false;
        response.error_message = "Mock provider error: rate limit exceeded";
        response.status_code = 429;
    } else {
        response.success = true;
        response.data = "{\"response\": \"Mock response from " + provider_name_ + "\", \"request_id\": " + std::to_string(request_count_) + "}";
        response.status_code = 200;
    }
    
    return response;
}

bool MockBridge::is_healthy() const {
    return is_healthy_;
}

std::string MockBridge::get_provider_name() const {
    return provider_name_;
}

nlohmann::json MockBridge::get_rate_limit_status() const {
    nlohmann::json status;
    status["provider"] = provider_name_;
    status["requests_made"] = request_count_;
    status["requests_remaining"] = 1000 - request_count_;
    status["reset_time"] = "2025-01-12T00:00:00Z";
    status["is_healthy"] = is_healthy_;
    return status;
}

} // namespace core
} // namespace aimux