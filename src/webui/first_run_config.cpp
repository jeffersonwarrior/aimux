/**
 * @file first_run_config.cpp
 * @brief Implementation of first-run configuration generator
 */

#include "aimux/webui/first_run_config.hpp"
#include <fstream>
#include <iostream>
#include <filesystem>

namespace aimux {
namespace webui {

// =============================================================================
// Section Creators
// =============================================================================

nlohmann::json FirstRunConfigGenerator::create_system_section() {
    return nlohmann::json{
        {"environment", "development"},
        {"log_level", "info"},
        {"log_file", ""},
        {"enable_file_logging", false}
    };
}

nlohmann::json FirstRunConfigGenerator::create_security_section() {
    return nlohmann::json{
        {"enable_cors", true},
        {"allowed_origins", nlohmann::json::array({"*"})},
        {"api_key_encryption", false},
        {"require_authentication", false},
        {"auth_token", ""}
    };
}

nlohmann::json FirstRunConfigGenerator::create_server_section() {
    return nlohmann::json{
        {"host", "0.0.0.0"},
        {"port", 8080},
        {"ssl_enabled", false},
        {"ssl_port", 8443},
        {"max_connections", 100},
        {"timeout_ms", 30000}
    };
}

nlohmann::json FirstRunConfigGenerator::create_webui_section() {
    return nlohmann::json{
        {"enabled", true},
        {"port", 8080},
        {"bind_address", "0.0.0.0"},
        {"ssl_enabled", false},
        {"cors_enabled", true},
        {"api_docs", true},
        {"real_time_metrics", true}
    };
}

nlohmann::json FirstRunConfigGenerator::create_providers_section() {
    nlohmann::json providers = nlohmann::json::array();

    // Anthropic provider with dummy key
    providers.push_back({
        {"name", "anthropic"},
        {"endpoint", "https://api.anthropic.com/v1"},
        {"api_key", "DUMMY_KEY_REPLACE_ME"},
        {"models", nlohmann::json::array({"claude-3-5-sonnet-20241022", "claude-3-opus-20240229"})},
        {"enabled", false},
        {"max_requests_per_minute", 60},
        {"timeout_ms", 30000}
    });

    // OpenAI provider with dummy key
    providers.push_back({
        {"name", "openai"},
        {"endpoint", "https://api.openai.com/v1"},
        {"api_key", "DUMMY_KEY_REPLACE_ME"},
        {"models", nlohmann::json::array({"gpt-4o", "gpt-4-turbo"})},
        {"enabled", false},
        {"max_requests_per_minute", 60},
        {"timeout_ms", 30000}
    });

    // Cerebras provider with dummy key
    providers.push_back({
        {"name", "cerebras"},
        {"endpoint", "https://api.cerebras.ai/v1"},
        {"api_key", "DUMMY_KEY_REPLACE_ME"},
        {"models", nlohmann::json::array({"llama3.1-8b", "llama3.1-70b"})},
        {"enabled", false},
        {"max_requests_per_minute", 120},
        {"timeout_ms", 15000}
    });

    return providers;
}

// =============================================================================
// Main Configuration Creation
// =============================================================================

nlohmann::json FirstRunConfigGenerator::create_default_config() {
    nlohmann::json config;

    // Set mode to static (no API calls until keys configured)
    config["mode"] = "static";

    // Add all required sections
    config["system"] = create_system_section();
    config["security"] = create_security_section();
    config["server"] = create_server_section();
    config["webui"] = create_webui_section();
    config["providers"] = create_providers_section();

    // Add optional sections with safe defaults
    config["daemon"] = {
        {"enabled", false},
        {"pid_file", "/var/run/aimux.pid"}
    };

    config["prettifier"] = {
        {"enabled", true},
        {"default_prettifier", "toon"},
        {"auto_discovery", false}
    };

    return config;
}

// =============================================================================
// File Operations
// =============================================================================

bool FirstRunConfigGenerator::save_config(const nlohmann::json& config,
                                         const std::string& config_path) {
    try {
        // Ensure parent directory exists
        std::filesystem::path path(config_path);
        if (path.has_parent_path()) {
            std::filesystem::create_directories(path.parent_path());
        }

        // Write config with pretty formatting
        std::ofstream ofs(config_path);
        if (!ofs.is_open()) {
            std::cerr << "Failed to open config file for writing: " << config_path << std::endl;
            return false;
        }

        ofs << config.dump(2) << std::endl;
        ofs.close();

        return true;

    } catch (const std::exception& e) {
        std::cerr << "Error saving config: " << e.what() << std::endl;
        return false;
    }
}

bool FirstRunConfigGenerator::load_config_from_file(const std::string& config_path,
                                                   nlohmann::json& out_config) {
    try {
        std::ifstream ifs(config_path);
        if (!ifs.is_open()) {
            return false;
        }

        ifs >> out_config;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "Error loading config: " << e.what() << std::endl;
        return false;
    }
}

nlohmann::json FirstRunConfigGenerator::load_or_create_config(const std::string& config_path) {
    // Try to load existing config
    nlohmann::json config;
    if (load_config_from_file(config_path, config)) {
        std::cout << "Loaded existing configuration from: " << config_path << std::endl;
        return config;
    }

    // Config doesn't exist - create default
    std::cout << "First run detected - no config found at: " << config_path << std::endl;
    std::cout << "Creating default configuration in static mode..." << std::endl;

    config = create_default_config();

    // Save to disk
    if (save_config(config, config_path)) {
        std::cout << "Successfully created default config at: " << config_path << std::endl;
        std::cout << "WebUI will start in STATIC MODE (no API calls)" << std::endl;
        std::cout << "Please configure API keys in the config file to enable providers" << std::endl;
    } else {
        std::cerr << "Warning: Failed to save default config to disk" << std::endl;
        std::cerr << "WebUI will use in-memory configuration" << std::endl;
    }

    return config;
}

// =============================================================================
// Mode Management
// =============================================================================

bool FirstRunConfigGenerator::is_static_mode(const nlohmann::json& config) {
    if (!config.contains("mode")) {
        return false; // No mode field = operational (backward compatibility)
    }

    try {
        std::string mode = config["mode"].get<std::string>();
        return mode == "static";
    } catch (const std::exception& e) {
        std::cerr << "Error checking mode: " << e.what() << std::endl;
        return false;
    }
}

bool FirstRunConfigGenerator::switch_to_operational_mode(const std::string& config_path) {
    try {
        // Load existing config
        nlohmann::json config;
        if (!load_config_from_file(config_path, config)) {
            std::cerr << "Cannot switch mode: config file not found" << std::endl;
            return false;
        }

        // Update mode
        config["mode"] = "operational";

        // Save back
        if (!save_config(config, config_path)) {
            std::cerr << "Failed to save config after mode switch" << std::endl;
            return false;
        }

        std::cout << "Switched to operational mode: " << config_path << std::endl;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "Error switching mode: " << e.what() << std::endl;
        return false;
    }
}

// =============================================================================
// Validation
// =============================================================================

bool FirstRunConfigGenerator::has_required_sections(const nlohmann::json& config) {
    if (!config.is_object()) {
        return false;
    }

    // Check all required top-level sections
    const std::vector<std::string> required_sections = {
        "system", "security", "server", "webui", "providers"
    };

    for (const auto& section : required_sections) {
        if (!config.contains(section)) {
            std::cerr << "Missing required section: " << section << std::endl;
            return false;
        }
    }

    // Validate providers is an array
    if (!config["providers"].is_array()) {
        std::cerr << "providers section must be an array" << std::endl;
        return false;
    }

    return true;
}

} // namespace webui
} // namespace aimux
