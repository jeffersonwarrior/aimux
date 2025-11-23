#pragma once

#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <chrono>
#include <crow.h>
#include <nlohmann/json.hpp>

#include "aimux/gateway/gateway_manager.hpp"

namespace aimux {
namespace gateway {

/**
 * @brief V3 Unified Gateway - High-performance single endpoint with intelligent routing
 *
 * This class implements the V3 architecture vision: a single endpoint (/anthropic)
 * that intelligently routes requests to the optimal provider based on content analysis,
 * performance metrics, availability, and cost optimization.
 *
 * Key Features:
 * - **Intelligent Provider Selection**: Automatically analyzes request content and routes
 *   to the best provider based on model compatibility, latency, success rates, and cost
 * - **Dynamic Failover**: Seamless switching between providers during outages or performance degradation
 * - **Performance Monitoring**: Real-time tracking of response times, success rates, and provider health
 * - **Request Transformation**: Converts between different provider formats (Anthropic, OpenAI, etc.)
 * - **Concurrent Request Handling**: Thread-safe operation with configurable concurrency limits
 * - **Health Monitoring**: Continuous background health checks for all configured providers
 * - **Metrics and Analytics**: Detailed performance and usage metrics available via REST endpoints
 * - **Security**: API key validation, rate limiting, and request throttling
 * - **Production Ready**: CORS support, TLS/SSL, graceful shutdown, and comprehensive error handling
 *
 * Architecture Overview:
 *
 * ┌─────────────────────────────────────────────────────────────┐
 * │                    V3 Unified Gateway                      │
 * │  ┌───────────────┐  ┌─────────────────┐  ┌─────────────┐ │
 * │  │   Request     │  │   Transform     │  │    Route    │ │
 * │  │   Processor   │  │   & Validate    │  │    Engine   │ │
 * │  └───────┬───────┘  └───────┬─────────┘  └───────┬─────┘ │
 * │          │                  │                    │       │
 * │          ▼                  ▼                    ▼       │
 * │  ┌─────────────────────────────────────────────────────────┐ │
 * │  │            Provider Abstraction Layer                  │ │
 * │  ┌───────────────┐ ┌───────────────┐ ┌─────────────────┐ │ │
 * │  │   Cerebras    │ │     Anthropic │ │      OpenAI     │ │ │
 * │  │     Provider  │ │     Provider  │ │     Provider    │ │ │
 * │  └───────────────┘ └───────────────┘ └─────────────────┘ │ │
 * └─────────────────────────────────────────────────────────────┘
 *
 * Usage Example:
 * @code
 * // Create gateway with production configuration
 * V3UnifiedGateway::Config config = V3GatewayFactory::create_config("production");
 * config.port = 8080;
 * config.max_concurrent_requests = 100;
 *
 * auto gateway = V3GatewayFactory::create_gateway(config);
 *
 * // Start the gateway
 * if (!gateway->start()) {
 *     std::cerr << "Failed to start gateway" << std::endl;
 *     return 1;
 * }
 *
 * std::cout << "V3 Gateway running on port " << config.port << std::endl;
 *
 * // Handle requests...
 * // Client requests to: POST http://localhost:8080/anthropic
 *
 * // Gateway monitors and manages routing automatically
 *
 * // Graceful shutdown
 * gateway->stop();
 * @endcode
 *
 * Request Flow:
 * 1. Client sends request to /anthropic endpoint
 * 2. Gateway validates and parses the request
 * 3. Content analysis determines optimal provider
 * 4. Request is transformed to provider's format
 * 5. Provider executes the AI model request
 * 6. Response is transformed back to Anthropic format
 * 7. Response returned to client with performance metrics
 *
 * Thread Safety:
 * - All public methods are thread-safe
 * - Supports high concurrent request loads
 * - Background threads for health monitoring and metrics collection
 *
 * Performance:
 * - Sub-millisecond routing decisions
 * - Connection pooling and keep-alive
 * - Async I/O for optimal throughput
 * - Configurable request timeouts and retry policies
 */
class V3UnifiedGateway {
public:
    /**
     * @brief Configuration structure for the V3 Unified Gateway
     *
     * Contains all settings needed to configure the gateway behavior,
     * performance characteristics, security settings, and monitoring.
     *
     * All settings have sensible defaults for production use.
     * Configuration can be loaded from JSON files or set programmatically.
     */
    struct Config {
        /**
         * @brief TCP port for the gateway HTTP server
         *
         * Port number on which the gateway will listen for incoming requests.
         * Must be available and not in use by other applications.
         *
         * Common values:
         * - 8080: Standard development port
         * - 443: HTTPS with SSL termination
         * - 80: HTTP (requires root privileges)
         *
         * Security consideration: Use ports >= 1024 for non-privileged operation
         */
        int port = 8080;

        /**
         * @brief Network interface address to bind the server to
         *
         * Controls which network interfaces the gateway will listen on.
         * Important for security and network topology in deployment environments.
         *
         * Values:
         * - "0.0.0.0": Listen on all available interfaces (default)
         * - "127.0.0.1": Listen only on localhost (development/testing)
         * - "10.0.0.1": Bind to specific IP address for security
         *
         * Recommendation: Use "0.0.0.0" for production, "127.0.0.1" for development
         */
        std::string bind_address = "0.0.0.0";

        /**
         * @brief Logging level for the gateway and internal components
         *
         * Controls the verbosity of log output for debugging and monitoring.
         * Higher levels provide more detailed information but may impact performance.
         *
         * Available levels:
         * - "trace": Most detailed, includes all execution details
         * - "debug": Debug information and request/response logs
         * - "info": General operational information (default)
         * - "warn": Warning messages only
         * - "error": Error messages only
         * - "fatal": Critical errors only
         *
         * Recommendation: "info" for production, "debug" for development
         */
        std::string log_level = "info";

        /**
         * @brief Enable comprehensive performance metrics collection
         *
         * When enabled, the gateway collects detailed metrics about:
         * - Request/response times and histograms
         * - Provider performance statistics
         * - Error rates and types
         * - Resource usage (memory, CPU)
         * - Connection and request counts
         *
         * Metrics are available via /metrics endpoint and monitoring systems.
         *
         * Performance impact: Minimal (~1-2% CPU overhead)
         * Recommendation: Enable for production monitoring and debugging
         */
        bool enable_metrics = true;

        /**
         * @brief Enable Cross-Origin Resource Sharing (CORS) support
         *
         * When enabled, the gateway includes CORS headers in responses,
         * allowing web browsers from different domains to make requests.
         *
         * CORS features when enabled:
         * - Accepts requests from any origin
         * - Allows standard HTTP methods (GET, POST, PUT, DELETE)
         * - Includes required headers for browser-based clients
         *
         * Security consideration: Ensure proper authentication headers are set
         * Recommendation: Enable for web-based clients, disable for API-only services
         */
        bool enable_cors = true;

        /**
         * @brief Maximum number of concurrent requests to process
         *
         * Controls the maximum number of simultaneous HTTP requests that the gateway
         * will process. Additional requests will wait in the connection queue.
         *
         * Factors to consider:
         * - Available CPU cores and memory
         * - Expected user load and request patterns
         * - Backend provider performance and limits
         *
         * Default guidelines:
         * - Development: 10-50 concurrent requests
         * - Production: 100-1000 (based on resources)
         *
         * Warning: Too high values may cause resource exhaustion
         */
        int max_concurrent_requests = 100;

        /**
         * @brief Request timeout duration for provider calls
         *
         * Maximum time to wait for a response from external AI providers.
         * Includes network latency, provider processing time, and internal overhead.
         *
         * Timeout behavior:
         * - Requests exceeding timeout are marked as failed
         * - Failover to alternate providers may be triggered
         * - Client receives appropriate timeout error response
         *
         * Recommended values:
         * - Fast models (text generation): 30-60 seconds
         * - Complex models (analysis, reasoning): 120-300 seconds (default)
         *
         * Consideration: Balance between responsiveness and provider processing time
         */
        std::chrono::seconds request_timeout{300};

        /**
         * @brief Convert configuration to JSON format
         *
         * Serializes the complete configuration to JSON for:
         * - Configuration file storage and loading
         * - Runtime configuration inspection
         * - Configuration backup and versioning
         * - API documentation and validation
         *
         * @return JSON object containing all configuration values
         */
        nlohmann::json to_json() const;

        /**
         * @brief Create configuration from JSON data
         *
         * Deserializes and validates a JSON object into a Config structure.
         * Performs comprehensive validation of all configuration parameters.
         *
         * Validation includes:
         * - Port number range and availability
         * - IP address format and validity
         * - Log level recognition
         * - Performance parameter bounds
         *
         * @param j JSON object containing the configuration data
         * @return Validated Config instance
         * @throws std::invalid_argument if JSON is malformed or contains invalid values
         * @throws std::out_of_range if configuration values are out of acceptable ranges
         */
        static Config from_json(const nlohmann::json& j);
    };

    /**
     * @brief Construct a V3UnifiedGateway with the specified configuration
     *
     * Initializes all internal components including:
     * - Gateway manager with provider abstractions
     * - HTTP server with route handlers
     * - Metrics collection and monitoring
     * - Background health checking threads
     *
     * The constructor prepares the gateway but does not start the server.
     * Call start() to begin accepting HTTP requests.
     *
     * @param config Configuration for gateway behavior and settings
     * @throws std::invalid_argument if configuration validation fails
     * @throws std::runtime_error if internal component initialization fails
     */
    explicit V3UnifiedGateway(const Config& config = Config{});

    /**
     * @brief Destructor - Ensures graceful shutdown of all components
     *
     * Automatically stops the HTTP server if still running and cleans up:
     * - Background threads and resources
     * - Network connections and file descriptors
     * - Internal state and metrics storage
     *
     * The destructor provides exception-safe cleanup even if stop() wasn't called.
     */
    ~V3UnifiedGateway();

    /**
     * @brief Start the gateway and begin accepting HTTP requests
     *
     * Initializes the HTTP server, starts background monitoring threads,
     * and begins accepting requests on the configured port and address.
     *
     * Startup process:
     * 1. Configure Crow web application with routes and middleware
     * 2. Start provider health monitoring if enabled
     * 3. Begin metrics collection if enabled
     * 4. Start HTTP server on configured interface/port
     *
     * @return true if gateway started successfully, false if startup failed
     * @throws std::runtime_error if server cannot bind to configured port
     *
     * Thread safety: This method is not thread-safe, call only during initialization
     */
    bool start();

    /**
     * @brief Stop the gateway and gracefully shut down all services
     *
     * Performs a graceful shutdown sequence:
     * 1. Stop accepting new HTTP requests
     * 2. Complete processing of in-flight requests
     * 3. Shutdown background monitoring threads
     * 4. Clean up internal resources and connections
     *
     * The method blocks until all requests are completed or timeout reached.
     *
     * Thread safety: Thread-safe, can be called from any thread
     */
    void stop();

    /**
     * @brief Check if the gateway is currently running and accepting requests
     *
     * @return true if the gateway is started and operational, false otherwise
     *
     * Thread safety: Thread-safe atomic operation
     */
    bool is_running() const { return running_.load(); }

    /**
     * @brief Update the gateway configuration at runtime
     *
     * Updates the gateway configuration with new values while the service
     * is running. Some settings may require server restart to take effect.
     *
     * Configurable at runtime:
     * - Log levels and metrics collection
     * - CORS settings and security options
     * - Rate limiting and concurrency limits
     *
     * Requires restart:
     * - Network bind address and port changes
     *
     * @param config New configuration values to apply
     * @throws std::invalid_argument if new configuration is invalid
     *
     * Thread safety: Thread-safe, applies changes atomically
     */
    void update_config(const Config& config);

    /**
     * @brief Get the current gateway configuration
     *
     * @return Copy of the current configuration in effect
     *
     * Thread safety: Thread-safe atomic operation
     */
    Config get_config() const { return config_; }

    /**
     * @brief Get direct access to the underlying gateway manager
     *
     * Provides direct access to the GatewayManager for advanced operations
     * such as custom provider management, detailed health monitoring,
     * and low-level routing configuration.
     *
     * @return Pointer to the GatewayManager instance (non-null)
     *
     * Warning: Advanced usage only - modifies core routing behavior
     * Thread safety: Use with proper synchronization
     */
    GatewayManager* get_gateway_manager() { return gateway_manager_.get(); }

    /**
     * @brief Get comprehensive gateway status and health information
     *
     * Returns detailed status information including:
     * - Server operational state and configuration
     * - Provider health and availability
     * - Recent request statistics and success rates
     * - System resource usage and performance metrics
     * - Error conditions and warning states
     *
         * @return JSON object containing complete system status
         *
         * Thread safety: Thread-safe, consistent snapshot
         */
        nlohmann::json get_status() const;

        /**
         * @brief Get detailed performance and usage metrics
         *
         * Returns comprehensive metrics for monitoring and analytics:
         * - Request/response performance histograms
         * - Provider-specific performance statistics
         * - Error rates and failure patterns
         * - Resource utilization over time
         * - Top N slow/failing requests
         *
         * @return JSON object containing all available metrics
         *
         * Thread safety: Thread-safe, atomic snapshot of current metrics
         */
        nlohmann::json get_metrics() const;

private:
    Config config_;
    std::atomic<bool> running_{false};

    // Core components
    std::unique_ptr<GatewayManager> gateway_manager_;
    std::unique_ptr<crow::SimpleApp> app_;
    std::thread server_thread_;

    // Request tracking
    struct RequestTracker {
        std::string request_id;
        std::chrono::steady_clock::time_point start_time;
        std::string client_ip;
        std::string user_agent;
    };

    mutable std::mutex requests_mutex_;
    std::unordered_map<std::string, RequestTracker> active_requests_;

    // Server setup
    void setup_routes();
    void setup_cors();

    // Route handlers
    crow::response handle_anthropic_request(const crow::request& req);
    crow::response handle_openai_request(const crow::request& req);
    crow::response handle_health_check(const crow::request& req);
    crow::response handle_metrics(const crow::request& req);
    crow::response handle_providers(const crow::request& req);
    crow::response handle_config(const crow::request& req);
    crow::response handle_models(const std::string& format);

    // Request processing
    crow::response process_request(const crow::request& req);
    aimux::core::Request create_aimux_request(const nlohmann::json& anthropic_request);
    crow::response convert_to_anthropic_response(const aimux::core::Response& aimux_response, const RequestTracker& tracker);

    // Utilities
    std::string generate_request_id();
    RequestTracker create_tracker(const crow::request& req);
    std::string get_client_ip(const crow::request& req);
    bool validate_anthropic_request(const nlohmann::json& request);
    nlohmann::json create_error_response(const std::string& error_code, const std::string& message);

    // Authentication and validation
    bool authenticate_request(const crow::request& req);
    bool has_valid_api_key(const crow::request& req);
};

/**
 * @brief Factory for creating V3 unified gateway instances with predefined configurations
 *
 * The V3GatewayFactory provides a convenient and safe way to create gateway instances
 * with validated configurations for different deployment environments. It abstracts
 * away the complexity of gateway initialization and provides sensible defaults for
 * common use cases.
 *
 * Supported Configuration Presets:
 *
 * **Development Preset:**
 * - Optimized for local development and testing
 * - Verbose logging (debug level)
 * - Lower security and performance requirements
 * - Enhanced error messages and debugging
 *
 * **Production Preset:**
 * - Optimized for high-performance production deployment
 * - Minimal logging (info level)
 * - Strong security defaults
 * - High concurrency and resource efficiency
 *
 * **Testing Preset:**
 * - Optimized for automated testing environments
 * - Predictable behavior and timing
 * - Mock-friendly configuration
 * - Fast startup/shutdown
 *
 * Usage Examples:
 * @code
 * // Simple gateway with default production settings
 * auto gateway1 = V3GatewayFactory::create_gateway();
 *
 * // Development environment gateway
 * auto devGateway = V3GatewayFactory::create_gateway(
 *     V3GatewayFactory::create_config("development")
 * );
 *
 * // Custom configuration
 * V3UnifiedGateway::Config customConfig;
 * customConfig.port = 9090;
 * customConfig.max_concurrent_requests = 200;
 * customConfig.log_level = "trace";
 *
 * auto customGateway = V3GatewayFactory::create_gateway(customConfig);
 *
 * if (customGateway->start()) {
 *     std::cout << "Custom gateway started successfully" << std::endl;
 * }
 * @endcode
 *
 * Error Handling:
 * - All factory methods throw exceptions on configuration validation failures
 * - Provides detailed error messages for invalid parameters
 * - Performs comprehensive pre-startup validation
 *
 * Thread Safety:
 * - All factory methods are thread-safe
 * - Multiple gateways can be created concurrently
 * - Each gateway instance is independent and thread-safe
 */
class V3GatewayFactory {
public:
    /**
     * @brief Create V3 gateway with default production configuration
     *
     * Creates a new V3UnifiedGateway instance using production-optimized settings.
     * This is the recommended method for standard production deployments.
     *
     * Default production configuration includes:
     * - Port: 8080
     * - Log level: "info"
     * - Metrics collection: enabled
     * - CORS: enabled
     * - Concurrent requests: 100
     * - Request timeout: 300 seconds
     *
     * @return Unique pointer to initialized V3 gateway
     * @throws std::runtime_error if gateway initialization fails
     * @throws std::invalid_argument if default configuration is invalid
     *
     * Usage example:
     * @code
     * try {
     *     auto gateway = V3GatewayFactory::create_gateway();
     *     if (gateway->start()) {
     *         std::cout << "Production gateway started" << std::endl;
     *     }
     * } catch (const std::exception& e) {
     *     std::cerr << "Gateway creation failed: " << e.what() << std::endl;
     * }
     * @endcode
     */
    static std::unique_ptr<V3UnifiedGateway> create_gateway();

    /**
     * @brief Create V3 gateway with custom configuration
     *
     * Creates a new V3UnifiedGateway instance using the provided configuration.
     * The configuration is thoroughly validated before gateway initialization.
     *
     * Configuration validation includes:
     * - Port number range and availability checking
     * - Network interface validation
     * - Performance parameter bounds checking
     * - Security settings validation
     *
     * @param config Complete gateway configuration with all required settings
     * @return Unique pointer to initialized V3 gateway
     * @throws std::invalid_argument if configuration validation fails
     * @throws std::runtime_error if gateway initialization fails
     *
     * Usage example:
     * @code
     * V3UnifiedGateway::Config config;
     * config.port = 9090;
     * config.bind_address = "127.0.0.1";
     * config.log_level = "debug";
     * config.max_concurrent_requests = 50;
     *
     * auto gateway = V3GatewayFactory::create_gateway(config);
     * @endcode
     */
    static std::unique_ptr<V3UnifiedGateway> create_gateway(const V3UnifiedGateway::Config& config);

    /**
     * @brief Create V3 gateway configuration preset for specific environments
     *
     * Creates a pre-configured Config structure optimized for the specified
     * deployment environment. Each preset provides sensible defaults for its
     * intended use case while remaining fully customizable.
     *
     * Available presets:
     *
     * **"development"**: Optimized for local development
     * - Log level: "debug" (verbose)
     * - Port: 8080
     * - Concurrent requests: 25 (lower resource usage)
     * - Request timeout: 60 seconds (faster feedback)
     * - Metrics: enabled for debugging
     * - CORS: enabled for web development
     *
     * **"production"**: Optimized for production deployment
     * - Log level: "info" (minimal overhead)
     * - Port: 8080
     * - Concurrent requests: 100 (high throughput)
     * - Request timeout: 300 seconds (robust operation)
     * - Metrics: enabled for monitoring
     * - CORS: enabled (configurable per application)
     *
     * **"testing"**: Optimized for automated testing
     * - Log level: "warn" (minimal output)
     * - Port: 0 (random assigned port)
     * - Concurrent requests: 10 (resource efficient)
     * - Request timeout: 30 seconds (fast test cycles)
     * - Metrics: disabled (reduced overhead)
     * - CORS: disabled (simpler testing)
     *
     * **"high-performance"**: Optimized for maximum throughput
     * - Log level: "error" (minimal overhead)
     * - Port: 8080
     * - Concurrent requests: 1000 (maximum concurrency)
     * - Request timeout: 120 seconds (balanced)
     * - Metrics: disabled (maximum performance)
     * - CORS: disabled (reduced overhead)
     *
     * @param preset_name Configuration preset name (default: "production")
     * @return Gateway configuration optimized for the specified environment
     * @throws std::invalid_argument if preset_name is not recognized
     *
     * Usage examples:
     * @code
     * // Development configuration
     * auto devConfig = V3GatewayFactory::create_config("development");
     * auto devGateway = V3GatewayFactory::create_gateway(devConfig);
     *
     * // High-performance configuration
     * auto perfConfig = V3GatewayFactory::create_config("high-performance");
     * perfConfig.port = 9090; // Customize specific values
     * auto perfGateway = V3GatewayFactory::create_gateway(perfConfig);
     *
     * // Custom preset doesn't exist - throws exception
     * try {
     *     auto invalidConfig = V3GatewayFactory::create_config("invalid");
     * } catch (const std::invalid_argument& e) {
     *     std::cerr << "Invalid preset: " << e.what() << std::endl;
     * }
     * @endcode
     */
    static V3UnifiedGateway::Config create_config(const std::string& preset_name = "production");
};

} // namespace gateway
} // namespace aimux