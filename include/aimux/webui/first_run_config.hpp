/**
 * @file first_run_config.hpp
 * @brief First-run configuration initialization for AIMUX WebUI
 *
 * Provides automatic config.json generation on first run with static mode support.
 * Static mode allows WebUI to start without valid API keys, displaying a setup UI.
 *
 * @version 3.0.0
 * @date 2025-11-24
 */

#pragma once

#include <string>
#include <nlohmann/json.hpp>

namespace aimux {
namespace webui {

/**
 * @brief Generates and manages first-run configuration
 *
 * This class handles:
 * - Auto-creation of config.json on first run
 * - Default configuration with dummy API keys
 * - Static mode operation (no API calls until keys configured)
 * - Mode switching from static to operational
 */
class FirstRunConfigGenerator {
public:
    /**
     * @brief Create a default configuration for first run
     *
     * Creates a complete config with:
     * - All required sections (system, security, server, webui, providers)
     * - mode="static" to prevent API calls with dummy keys
     * - Dummy API keys marked as "DUMMY_KEY_REPLACE_ME"
     * - Safe defaults for all settings
     *
     * @return nlohmann::json Default configuration object
     */
    static nlohmann::json create_default_config();

    /**
     * @brief Save configuration to file
     *
     * Saves the JSON configuration to the specified path with
     * proper formatting (2-space indentation).
     *
     * @param config Configuration object to save
     * @param config_path Path where config should be saved
     * @return bool True if saved successfully, false on error
     */
    static bool save_config(const nlohmann::json& config, const std::string& config_path);

    /**
     * @brief Load existing config or create new one if missing
     *
     * This is the main entry point for first-run initialization:
     * - If config file exists: load and return it
     * - If config missing: create default, save, and return it
     *
     * @param config_path Path to config file
     * @return nlohmann::json Loaded or newly created config
     */
    static nlohmann::json load_or_create_config(const std::string& config_path);

    /**
     * @brief Check if config is in static mode
     *
     * Static mode means:
     * - WebUI serves UI but doesn't make API calls
     * - API keys can be dummy/placeholder values
     * - Shows "waiting for configuration" UI
     *
     * @param config Configuration to check
     * @return bool True if config has mode="static"
     */
    static bool is_static_mode(const nlohmann::json& config);

    /**
     * @brief Switch configuration from static to operational mode
     *
     * Updates the config file to set mode="operational".
     * Should be called after user provides real API keys.
     *
     * @param config_path Path to config file to update
     * @return bool True if switched successfully, false on error
     */
    static bool switch_to_operational_mode(const std::string& config_path);

    /**
     * @brief Validate that config has all required sections
     *
     * Checks for presence of:
     * - system, security, server, webui, providers sections
     * - Required fields within each section
     *
     * @param config Configuration to validate
     * @return bool True if all required sections present
     */
    static bool has_required_sections(const nlohmann::json& config);

    /**
     * @brief Create default system section
     * @return nlohmann::json System configuration
     */
    static nlohmann::json create_system_section();

    /**
     * @brief Create default security section
     * @return nlohmann::json Security configuration
     */
    static nlohmann::json create_security_section();

    /**
     * @brief Create default server section
     * @return nlohmann::json Server configuration
     */
    static nlohmann::json create_server_section();

    /**
     * @brief Create default webui section
     * @return nlohmann::json WebUI configuration
     */
    static nlohmann::json create_webui_section();

    /**
     * @brief Create default providers array with dummy keys
     * @return nlohmann::json Array of provider configurations
     */
    static nlohmann::json create_providers_section();

private:
    /**
     * @brief Load configuration from file
     *
     * @param config_path Path to config file
     * @param out_config Output parameter for loaded config
     * @return bool True if loaded successfully
     */
    static bool load_config_from_file(const std::string& config_path,
                                      nlohmann::json& out_config);
};

} // namespace webui
} // namespace aimux
