#include "aimux/core/bridge.hpp"
#include "aimux/core/error_handler.hpp"
// #include "aimux/providers/provider_impl.hpp"  // Temporarily disabled
#include <thread>
#include <chrono>
#include <algorithm>

namespace aimux {
namespace core {

// BridgeFactory implementation
std::unique_ptr<Bridge> BridgeFactory::create_bridge(
    const std::string& provider_name,
    const nlohmann::json& /* config */) {

    try {
        AIMUX_ERROR_CONTEXT("BridgeFactory", "create_bridge");
        AIMUX_INFO("Creating bridge for provider: " + provider_name);

        // Validate provider name
        if (provider_name.empty()) {
            AIMUX_THROW(ErrorCode::USER_MISSING_REQUIRED_PARAMETER,
                       "Provider name cannot be empty");
        }

        // Check if provider is supported
        auto supported = get_supported_providers();
        if (std::find(supported.begin(), supported.end(), provider_name) == supported.end()) {
            AIMUX_THROW(ErrorCode::PROVIDER_NOT_FOUND,
                       "Provider '" + provider_name + "' is not supported");
        }

        // Temporarily return a concrete bridge for all providers
        return std::make_unique<ConcreteBridge>(provider_name);

    } catch (const AimuxException&) {
        throw; // Re-throw aimux exceptions
    } catch (const std::exception& e) {
        AIMUX_THROW(ErrorCode::INTERNAL_LOGIC_ERROR,
                   "Failed to create bridge: " + std::string(e.what()));
    }
}

std::vector<std::string> BridgeFactory::get_supported_providers() {
    // Return list of supported providers
    return {"synthetic", "cerebras", "zai", "minimax"};
}

// ConcreteBridge implementation
ConcreteBridge::ConcreteBridge(const std::string& provider_name) 
    : provider_name_(provider_name) {}

Response ConcreteBridge::send_request(const Request& request) {
    try {
        AIMUX_ERROR_CONTEXT("ConcreteBridge", "send_request");
        AIMUX_DEBUG("Sending request to provider: " + provider_name_);

        Response response;
        response.provider_name = provider_name_;
        request_count_++;

        // Validate request
        if (request.model.empty()) {
            AIMUX_THROW(ErrorCode::USER_MISSING_REQUIRED_PARAMETER,
                       "Request model cannot be empty");
        }

        if (request.method.empty()) {
            AIMUX_THROW(ErrorCode::USER_MISSING_REQUIRED_PARAMETER,
                       "Request method cannot be empty");
        }

        // Simulate some processing time with error handling
        int response_time = 50 + (request_count_ % 100);
        std::this_thread::sleep_for(std::chrono::milliseconds(response_time));

        if (request_count_ % 10 == 0) {
            is_healthy_ = false;
            response.success = false;
            response.error_message = "Simulated provider failure";
            AIMUX_WARNING("Provider " + provider_name_ + " simulated failure");
        } else {
            response.success = true;
            response.data = "Response from " + provider_name_;
            AIMUX_DEBUG("Successfully received response from " + provider_name_);
        }

        response.response_time_ms = response_time;
        return response;

    } catch (const AimuxException&) {
        throw; // Re-throw aimux exceptions
    } catch (const std::exception& e) {
        AIMUX_THROW(ErrorCode::PROVIDER_RESPONSE_ERROR,
                   "Failed to send request to " + provider_name_ + ": " + std::string(e.what()));
    }
}

bool ConcreteBridge::is_healthy() const {
    return is_healthy_;
}

std::string ConcreteBridge::get_provider_name() const {
    return provider_name_;
}

nlohmann::json ConcreteBridge::get_rate_limit_status() const {
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