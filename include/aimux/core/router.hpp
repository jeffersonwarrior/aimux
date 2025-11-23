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
 * @brief Core router class that manages provider selection and intelligent failover
 *
 * The Router is the central component of Aimux that handles intelligent routing
 * of API requests to the most appropriate provider. It maintains a pool of
 * providers, monitors their health, and implements sophisticated failover
 * strategies to ensure high availability and optimal performance.
 *
 * Key features:
 * - Intelligent provider selection based on health, latency, and success rates
 * - Automatic failover to backup providers on failures
 * - Rate limiting and quota management per provider
 * - Real-time performance monitoring and metrics collection
 * - Thread-safe operation for concurrent request handling
 *
 * Usage example:
 * @code
 * std::vector<ProviderConfig> providers = {
 *     {"cerebras", "https://api.cerebras.ai", "key1", {"llama3.1-70b"}, 60, true},
 *     {"zai", "https://api.z.ai", "key2", {"gpt-4"}, 60, true}
 * };
 *
 * Router router(providers);
 *
 * Request request;
 * request.model = "gpt-4";
 * request.method = "POST";
 * request.data = nlohmann::json{{"content", "Hello world"}};
 *
 * Response response = router.route(request);
 * @endcode
 */
class Router {
public:
    /**
     * @brief Construct a Router with the specified providers
     *
     * The router will initialize the failover manager and load balancer,
     * and start background health monitoring for all enabled providers.
     *
     * @param providers Vector of provider configurations to manage
     * @throws std::invalid_argument if providers vector is empty
     * @throws std::runtime_error if failover manager initialization fails
     */
    explicit Router(const std::vector<ProviderConfig>& providers);

    /**
     * @brief Route a request to the best available provider
     *
     * This method implements intelligent routing by:
     * 1. Evaluating provider health and availability
     * 2. Considering performance metrics and response times
     * 3. Applying load balancing algorithm
     * 4. Handling automatic failover if needed
     *
     * @param request The request to route containing model, method, and data
     * @return Response from the selected provider with success status and timing
     * @throws std::runtime_error if no suitable provider is available
     * @throws std::invalid_argument if request validation fails
     *
     * Thread safety: This method is thread-safe and can be called concurrently
     */
    Response route(const Request& request);

    /**
     * @brief Get comprehensive health status of all providers
     *
     * Returns detailed health information including:
     * - Provider availability status
     * - Recent success/failure rates
     * - Average response times
     * - Error counts and types
     * - Rate limiting status
     *
     * @return JSON string with provider health information in structured format
     *
     * Example output:
     * @code
     * {
     *   "providers": {
     *     "cerebras": {
     *       "healthy": true,
     *       "success_rate": 0.95,
     *       "avg_response_time_ms": 850,
     *       "last_check": "2025-11-14T15:30:45Z"
     *     }
     *   },
     *   "overall_health": "healthy"
     * }
     * @endcode
     */
    std::string get_health_status() const;

    /**
     * @brief Get comprehensive performance metrics
     *
     * Returns detailed performance data including:
     * - Request counts per provider
     * - Average response times and percentiles
     * - Error rates by type
     * - Failover statistics
     * - Rate limiting metrics
     * - Load balancing distribution
     *
     * @return JSON string with performance data in structured format
     *
     * Example output:
     * @code
     * {
     *   "total_requests": 1250,
     *   "success_rate": 0.98,
     *   "avg_response_time_ms": 1200,
     *   "providers": {
     *     "cerebras": {
     *       "requests": 750,
     *       "response_times": {"p50": 800, "p95": 1500}
     *     }
     *   },
     *   "failovers": 25,
     *   "uptime_percent": 99.8
     * }
     * @endcode
     */
    std::string get_metrics() const;

private:
    std::vector<ProviderConfig> providers_;
    std::unique_ptr<FailoverManager> failover_manager_;
    std::unique_ptr<LoadBalancer> load_balancer_;
};

/**
 * @brief Configuration for a provider in the Aimux routing system
 *
 * This structure defines all the parameters needed to configure an external
 * provider for the routing system. Each provider represents an external API
 * service that can handle AI model requests.
 *
 * The configuration includes connection details, rate limiting, supported models,
 * and runtime behavior settings.
 *
 * All fields have sensible defaults except for name, endpoint, and api_key
 * which are required for proper provider operation.
 */
struct ProviderConfig {
    /**
     * @brief Unique human-readable name for this provider
     *
     * Used for identification in logs, metrics, and configuration files.
     * Must be unique across all providers in the router instance.
     *
     * Examples: "cerebras", "zai", "anthropic", "openai"
     */
    std::string name;

    /**
     * @brief Full URL endpoint for the provider's API
     *
     * Must include the protocol (http/https) and full path to the API endpoint.
     * Used for routing requests to the provider's servers.
     *
     * Examples:
     * - "https://api.cerebras.ai/v1/completions"
     * - "https://api.z.ai/v1/chat/completions"
     * - "https://api.anthropic.com/v1/messages"
     */
    std::string endpoint;

    /**
     * @brief API authentication key for the provider
     *
     * This is the secret API key used to authenticate requests to the provider.
     * Should be kept confidential and refreshed periodically as required by
     * the provider's security policies.
     *
     * Security note: This field will be redacted in logs and configuration exports
     */
    std::string api_key;

    /**
     * @brief List of AI models supported by this provider
     *
     * Vector containing the model identifiers that this provider can handle.
     * Used by the router to determine which providers are suitable for a given request.
     *
     * Examples:
     * - {"llama3.1-70b", "llama3.1-8b"}
     * - {"gpt-4", "gpt-3.5-turbo"}
     * - {"claude-3-opus", "claude-3-sonnet"}
     */
    std::vector<std::string> models;

    /**
     * @brief Maximum number of requests allowed per minute (rate limiting)
     *
     * Implements rate limiting to prevent exceeding provider quota limits.
     * When this limit is reached, the router will temporarily route requests
     * to alternative providers.
     *
     * Default: 60 (1 request per second)
     * Range: 1-3600 (1 per hour to 60 per second)
     */
    int max_requests_per_minute = 60;

    /**
     * @brief Whether this provider is currently enabled for routing
     *
     * When false, the provider will not be considered for request routing
     * but health checks will continue to monitor its status. Allows for
     * graceful provider downtime or maintenance periods.
     *
     * Default: true
     */
    bool enabled = true;

    /**
     * @brief Convert configuration to JSON format
     *
     * Serializes the provider configuration to JSON for storage, logging,
     * and API responses. The api_key field will be redacted for security.
     *
     * @return JSON representation of the configuration
     */
    nlohmann::json to_json() const;

    /**
     * @brief Create configuration from JSON data
     *
     * Deserializes a JSON object into a ProviderConfig structure.
     * Performs validation of required fields and parameter ranges.
     *
     * @param j JSON object containing the configuration data
     * @return ProviderConfig instance created from the JSON data
     * @throws std::invalid_argument if JSON is malformed or missing required fields
     */
    static ProviderConfig from_json(const nlohmann::json& j);
};

/**
 * @brief Structure representing an API request to be routed
 *
 * This structure encapsulates all the information needed to route a request
 * to an external AI provider. It contains the target model, HTTP method,
 * request payload, and optional metadata for request tracking.
 *
 * Requests are created by gateway components and passed to the Router
 * for intelligent routing to the most appropriate provider.
 */
struct Request {
    /**
     * @brief Target model name for the request
     *
     * The AI model identifier that should process this request. Used by the
     * router to select providers that support the specified model.
     *
     * Examples: "gpt-4", "llama3.1-70b", "claude-3-opus"
     */
    std::string model;

    /**
     * @brief HTTP method to use for the request
     *
     * Specifies the HTTP method for the provider API call. Most AI providers
     * use POST requests, but some may support GET for simple queries.
     *
     * Common values: "POST", "GET", "PUT", "DELETE"
     * Default usage: "POST" for most AI completion requests
     */
    std::string method;

    /**
     * @brief Request payload data
     *
     * JSON object containing the request parameters and data that will be
     * sent to the provider. The structure depends on the provider's API
     * specification and the model being used.
     *
     * Example structure for chat completion:
     * @code
     * {
     *   "messages": [
     *     {"role": "user", "content": "Hello, world!"}
     *   ],
     *   "max_tokens": 1000,
     *   "temperature": 0.7
     * }
     * @endcode
     */
    nlohmann::json data;

    /**
     * @brief Convert request to JSON format
     *
     * Serializes the request structure to JSON for logging, debugging,
     * and API communication. All request data is included for full traceability.
     *
     * @return JSON representation of the request
     */
    nlohmann::json to_json() const;

    /**
     * @brief Create request from JSON data
     *
     * Deserializes a JSON object into a Request structure.
     * Validates required fields and data format.
     *
     * @param j JSON object containing the request data
     * @return Request instance created from the JSON data
     * @throws std::invalid_argument if JSON is malformed or missing required fields
     */
    static Request from_json(const nlohmann::json& j);
};

/**
 * @brief Structure representing the response from a provider
 *
 * This structure encapsulates the result of routing a request to a provider,
 * including success status, response data, timing information, and metadata.
 *
 * Used by the Router to return results to callers and for collecting performance
 * metrics and monitoring statistics.
 */
struct Response {
    /**
     * @brief Indicates whether the request was successful
     *
     * True if the provider returned a successful response with valid data.
     * False if the request failed due to provider errors, timeouts, or other issues.
     *
     * When false, check error_message and status_code for failure details.
     */
    bool success = false;

    /**
     * @brief Response data payload from the provider
     *
     * Contains the actual response content from the provider API.
     * Format depends on the provider and requested model.
     *
     * For successful requests, this typically contains:
     * - AI model completions or responses
     * - Metadata about the response
     * - Usage statistics (tokens, pricing)
     *
     * For failed requests, may contain:
     * - Error details if some data was received
     * - Empty string if complete failure
     */
    std::string data;

    /**
     * @brief Human-readable error message (if applicable)
     *
     * Contains a descriptive error message when success is false.
     * Provides information about what went wrong during request processing.
     *
     * Examples:
     * - "Provider timeout after 30 seconds"
     * - "Rate limit exceeded for provider cerebras"
     * - "Invalid API authentication credentials"
     * - "Model 'gpt-5' not supported by provider"
     */
    std::string error_message;

    /**
     * @brief HTTP status code from the provider response
     *
     * The HTTP status code returned by the provider's API.
     * Useful for understanding the type of error for debugging and monitoring.
     *
     * Common success codes: 200, 201, 202
     * Common error codes: 400 (Bad Request), 401 (Unauthorized), 429 (Rate Limited), 500 (Server Error)
     * Default: 0 (no response received)
     */
    int status_code = 0;

    /**
     * @brief Total response time in milliseconds
     *
     * Measures the complete time from request initiation to response receipt.
     * Used for performance monitoring, provider selection, and SLA tracking.
     *
     * Includes network latency, provider processing time, and internal routing overhead.
     * Precision: milliseconds with fractional part for sub-millisecond timing
     */
    double response_time_ms = 0.0;

    /**
     * @brief Name of the provider that handled the request
     *
     * Identifies which provider was selected and used for this request.
     * Essential for routing metrics, failover tracking, and performance analysis.
     *
     * Example values: "cerebras", "zai", "anthropic"
     */
    std::string provider_name;

    /**
     * @brief Convert response to JSON format
     *
     * Serializes the response structure to JSON for logging, monitoring,
     * and API communication. Includes all performance and diagnostic data.
     *
     * @return JSON representation of the response
     */
    nlohmann::json to_json() const;

    /**
     * @brief Create response from JSON data
     *
     * Deserializes a JSON object into a Response structure.
     * Validates data consistency and required fields.
     *
     * @param j JSON object containing the response data
     * @return Response instance created from the JSON data
     * @throws std::invalid_argument if JSON is malformed or contains invalid data
     */
    static Response from_json(const nlohmann::json& j);
};

} // namespace core
} // namespace aimux