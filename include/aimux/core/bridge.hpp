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
 * @brief Bridge interface for connecting with different AI providers
 *
 * The Bridge class serves as the foundational abstraction layer between the Aimux router
 * and various AI service providers. It provides a unified interface for sending requests
 * to different providers while handling provider-specific details internally.
 *
 * @component Core Bridging System
 * @api contract Bridge API Specification v1.0
 * @since v1.0.0
 *
 * @example Request Routing
 * @code
 * auto bridge = std::make_shared<ClaudeBridge>(config);
 * Request req = create_completion_request("Hello, world!");
 * Response resp = bridge->send_request(req);
 * @endcode
 *
 * @thread Thread-safe: All implementations must be thread-safe
 * @perf Target: < 100ms average response time
 * @security Must handle sensitive data (API keys) securely
 *
 * @see Router for request routing logic
 * @see Request for request structure details
 * @see Response for response structure details
 */
class Bridge {
public:
    virtual ~Bridge() = default;

    /**
     * @brief Send a request to the provider
     *
     * This is the primary method for sending requests to AI providers. The implementation
     * must handle all provider-specific format conversion, authentication, and error handling.
     *
     * @param request The request to send containing prompt, model, and configuration
     * @return Response from the provider containing generated content and metadata
     *
     * @throws std::runtime_error if the provider is unavailable
     * @throws std::invalid_argument if the request is malformed
     *
     * @thread Thread-safe: Must handle concurrent requests safely
     * @perf Target: < 500ms for completion requests, < 100ms for health checks
     * @api Input validation performed internally
     *
     * @since v1.0.0
     * @note Response time depends on provider and model complexity
     */
    virtual Response send_request(const Request& request) = 0;
    
    /**
     * @brief Check if the provider is healthy and available
     *
     * Performs a lightweight health check against the provider API.
     * This should be used by the failover system to determine provider availability.
     *
     * @return true if provider is responding and within rate limits
     * @return false if provider is unavailable or rate limited
     *
     * @thread Thread-safe: Must be safe for concurrent health checks
     * @perf Target: < 100ms response time for health checks
     * @api May provider different authentication than normal requests
     *
     * @since v1.0.0
     * @note Implementation should be lightweight to avoid impacting provider rate limits
     */
    virtual bool is_healthy() const = 0;

    /**
     * @brief Get provider name and version information
     *
     * Returns the canonical name of the AI provider this bridge communicates with.
     * Used by the router for provider selection and logging.
     *
     * @return Provider identifier in format "provider_name[.version]"
     *
     * @thread Thread-safe
     * @api String format: "claude", "cerabras", "zai", etc.
     *
     * @since v1.0.0
     * @example Returns "claude" for Anthropic Claude provider
     */
    virtual std::string get_provider_name() const = 0;

    /**
     * @brief Get current rate limit status and quotas
     *
     * Provides detailed information about current rate limit usage, quotas,
     * and timing information. Used by the router for load balancing
     * and avoiding rate limit exceeded responses.
     *
     * @return JSON object with rate limit information:
     *   - "requests_used": Current used requests
     *   - "requests_limit": Maximum allowed requests
     *   - "tokens_used": Current used tokens
     *   - "tokens_limit": Maximum allowed tokens
     *   - "reset_time": Unix timestamp when limits reset
     *   - "retry_after": Seconds to wait before next request
     *
     * @thread Thread-safe: Must be synchronized with actual request tracking
     * @perf Target: O(1) lookup time, no API calls
     *
     * @since v1.0.0
     * @note Information is cached locally and updated after each request
     */
    virtual nlohmann::json get_rate_limit_status() const = 0;
};

/**
 * @brief Factory for creating provider-specific bridge instances
 *
 * The BridgeFactory abstracts the creation and configuration of different
 * provider bridges. It handles provider discovery, configuration validation,
 * and instance creation with proper dependency injection.
 *
 * @component Provider Management System
 * @api Factory Pattern Implementation
 * @since v1.0.0
 *
 * @example Creating a Claude Bridge
 * @code
 * auto factory = BridgeFactory::getInstance();
 * auto bridge = factory.create_bridge("claude", claude_config);
 * @endcode
 *
 * @thread Thread-safe: Singleton implementation with thread safety
 * @security Validates provider credentials before instance creation
 *
 * @see Bridge for the interface being implemented
 * @see ProviderConfig for configuration structure
 */
class BridgeFactory {
public:
    /**
     * @brief Get singleton instance of the factory
     *
     * Implements the singleton pattern for centralized bridge creation.
     * Thread-safe initialization with double-checked locking pattern.
     *
     * @return Reference to the factory instance
     *
     * @thread Thread-safe: Initialized once on first call
     * @perf Target: O(1) access after initialization
     *
     * @since v1.0.0
     * @note Thread-safe initialization using call_once pattern
     */
    static BridgeFactory& getInstance();

    /**
     * @brief Create a bridge for the specified provider
     *
     * Creates a provider-specific bridge instance with the given configuration.
     * Validates the configuration and provider availability before creation.
     *
     * @param provider_name Name of provider (e.g., "claude", "cerebras", "zai")
     * @param config Provider configuration including API keys and settings
     * @return Unique pointer to configured bridge instance
     *
     * @throws std:: invalid_argument if provider_name is not supported
     * @throws std::runtime_error if configuration validation fails
     * @throws std::bad_alloc if bridge instance creation fails
     *
     * @thread Thread-safe: May be called concurrently for different providers
     * @security Validates and encrypts API keys before bridge creation
     * @api Configuration schema validation performed internally
     *
     * @since v1.0.0
     * @note Generated bridges are ready for immediate use
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
 * @brief Concrete bridge implementation
 */
class ConcreteBridge : public Bridge {
public:
    explicit ConcreteBridge(const std::string& provider_name);
    
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