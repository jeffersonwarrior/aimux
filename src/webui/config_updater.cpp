#include "aimux/webui/config_updater.hpp"
#include <fstream>
#include <iostream>

namespace aimux {
namespace webui {

bool ConfigUpdater::update_system_config(const nlohmann::json& new_config) {
    try {
        // Load current configuration
        nlohmann::json current_config;
        std::ifstream config_file("config/default.json");
        if (config_file.is_open()) {
            config_file >> current_config;
            config_file.close();
        }
        
        // Merge new configuration
        for (auto it = new_config.begin(); it != new_config.end(); ++it) {
            current_config[it.key()] = it.value();
        }
        
        // Validate configuration
        if (!validate_config(current_config)) {
            return false;
        }
        
        // Save updated configuration
        std::ofstream out_file("config/default.json");
        if (out_file.is_open()) {
            out_file << current_config.dump(4);
            out_file.close();
            return true;
        }
        
        return false;
    } catch (const std::exception& e) {
        std::cerr << "Configuration update failed: " << e.what() << "\n";
        return false;
    }
}

bool ConfigUpdater::validate_config(const nlohmann::json& config) {
    // Validate required sections
    if (!config.contains("version") || !config.contains("providers")) {
        std::cerr << "Invalid configuration: missing required sections\n";
        return false;
    }
    
    // Validate provider configurations
    auto providers = config["providers"];
    for (auto it = providers.begin(); it != providers.end(); ++it) {
        auto provider = it.value();
        
        if (!provider.contains("enabled") || !provider.contains("api_key")) {
            std::cerr << "Invalid provider configuration: " << it.key() << "\n";
            return false;
        }
        
        if (provider["enabled"].get<bool>() && 
            (provider["api_key"].get<std::string>().empty() || 
             provider["api_key"].get<std::string>() == "REPLACE_WITH_YOUR_API_KEY")) {
            std::cerr << "Provider " << it.key() << " is enabled but missing API key\n";
            return false;
        }
    }
    
    return true;
}

} // namespace webui
} // namespace aimux
