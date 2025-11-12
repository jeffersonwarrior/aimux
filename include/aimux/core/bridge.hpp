#pragma once

#include <string>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>
#include "aimux/core/router.hpp"

namespace aimux {
namespace core {

// Forward declarations
struct Request;
struct Response;

/**
 * @brief Bridge interface for connecting with different providers
 */
class Bridge {
public:
    virtual ~Bridge() = default;
    
    /**
     * @brief Send a request to the provider
     * @param request The request to send
     * @return Response from the provider
     */
    virtual Response send_request(const Request& request) = 0;
    
    /**
     * @brief Check if the provider is healthy
     * @return true if provider is responding
     */
    virtual bool is_healthy() const = 0;
    
    /**
     * @brief Get provider name
     * @return Provider identifier
     */
    virtual std::string get_provider_name() const = 0;
    
    /**
     * @brief Get current rate limit status
     * @return JSON with rate limit information
     */
    virtual nlohmann::json get_rate_limit_status() const = 0;
};

/**
 * @brief Factory for creating bridge instances
 */
class BridgeFactory {
public:
    /**
     * @brief Create a bridge for the specified provider
     * @param provider_name Name of provider
     * @param config Provider configuration
     * @return Unique pointer to bridge instance
     */
    static std::unique_ptr<Bridge> create_bridge(
        const std::string& provider_name,
        const nlohmann::json& config
    );
    
    /**
     * @brief Get list of supported provider types
     * @return Vector of provider names
     */
    static std::vector<std::string> get_supported_providers();
};

/**
 * @brief Mock bridge implementation for testing
 */
class MockBridge : public Bridge {
public:
    explicit MockBridge(const std::string& provider_name);
    
    Response send_request(const Request& request) override;
    bool is_healthy() const override;
    std::string get_provider_name() const override;
    nlohmann::json get_rate_limit_status() const override;

private:
    std::string provider_name_;
    bool is_healthy_ = true;
    int request_count_ = 0;
};

} // namespace core
} // namespace aimux