/**
 * @file prettifier_api.hpp
 * @brief REST API interface for prettifier status and configuration
 *
 * Provides HTTP endpoints for querying prettifier status, performance metrics,
 * and updating configuration at runtime via the WebUI dashboard.
 *
 * @version 2.2.0
 * @date 2025-11-24
 */

#pragma once

#include <memory>
#include <string>
#include <atomic>
#include <chrono>
#include <nlohmann/json.hpp>
#include "aimux/prettifier/prettifier_plugin.hpp"
#include "aimux/webui/config_validator.hpp"

namespace aimux {
namespace webui {

/**
 * @brief REST API handler for prettifier endpoints
 *
 * Manages HTTP endpoints for:
 * - GET /api/prettifier/status - Retrieve status and metrics
 * - POST /api/prettifier/config - Update configuration
 *
 * Thread-safe for concurrent requests.
 */
class PrettifierAPI {
public:
    /**
     * @brief Construct API handler with prettifier plugin
     *
     * @param plugin Shared pointer to prettifier plugin instance
     */
    explicit PrettifierAPI(const std::shared_ptr<prettifier::PrettifierPlugin>& plugin);

    /**
     * @brief Destructor
     */
    ~PrettifierAPI() = default;

    /**
     * @brief Handle GET /api/prettifier/status request
     *
     * Returns comprehensive status including:
     * - Current status (enabled/disabled)
     * - Version information
     * - Supported providers and formats
     * - Performance metrics
     * - Current configuration
     *
     * @return JSON response object
     *
     * Thread safety: Safe for concurrent calls
     * Performance: <1ms typical response time
     */
    nlohmann::json handle_status_request();

    /**
     * @brief Handle POST /api/prettifier/config request
     *
     * Validates and applies new configuration. Returns:
     * - Success: {"success": true, "applied_config": {...}}
     * - Failure: {"success": false, "error": "...", "details": {...}}
     *
     * @param config JSON configuration to apply
     * @return JSON response indicating success or failure
     *
     * Thread safety: Safe for concurrent calls (uses internal locking)
     * Performance: <5ms typical response time
     */
    nlohmann::json handle_config_request(const nlohmann::json& config);

    /**
     * @brief Get current status as JSON
     *
     * @return JSON object with current status
     */
    nlohmann::json get_status_json() const;

    /**
     * @brief Get format preferences for a specific provider
     *
     * @param provider Provider name (e.g., "anthropic", "openai")
     * @return JSON object with default format and available formats
     */
    nlohmann::json get_provider_formats(const std::string& provider) const;

    /**
     * @brief Get current performance metrics
     *
     * @return JSON object with performance statistics
     */
    nlohmann::json get_performance_metrics() const;

    /**
     * @brief Get current configuration
     *
     * @return JSON object with current settings
     */
    nlohmann::json get_configuration() const;

private:
    std::shared_ptr<prettifier::PrettifierPlugin> plugin_;
    std::shared_ptr<ConfigValidator> validator_;

    // Metrics tracking
    std::chrono::steady_clock::time_point start_time_;
    std::atomic<uint64_t> total_requests_{0};
    std::atomic<uint64_t> successful_requests_{0};
    std::atomic<uint64_t> failed_requests_{0};

    // Current configuration (mutable for thread-safe updates)
    mutable std::mutex config_mutex_;
    nlohmann::json current_config_;

    /**
     * @brief Initialize default configuration
     */
    void initialize_default_config();

    /**
     * @brief Apply validated configuration
     */
    void apply_configuration(const nlohmann::json& config);

    /**
     * @brief Get uptime in seconds
     */
    uint64_t get_uptime_seconds() const;
};

} // namespace webui
} // namespace aimux
