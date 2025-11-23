#include "aimux/cli/migrate.hpp"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

namespace aimux {
namespace cli {

bool ConfigMigrator::migrate_legacy_config(const std::string& legacy_file,
                                         const std::string& target_file) {
    try {
        // Read legacy configuration
        std::ifstream legacy_stream(legacy_file);
        if (!legacy_stream.is_open()) {
            std::cerr << "Error: Cannot open legacy configuration file: " << legacy_file << "\n";
            return false;
        }
        
        nlohmann::json legacy_config;
        legacy_stream >> legacy_config;
        legacy_stream.close();
        
        // Detect legacy version
        std::string version = detect_config_version(legacy_file);
        std::cout << "Detected legacy version: " << version << "\n";
        
        // Migrate to new format
        nlohmann::json new_config;
        new_config["version"] = "2.0.0";
        new_config["migrated_from"] = version;
        new_config["migration_timestamp"] = std::time(nullptr);
        
        // Migrate providers section
        new_config["providers"] = nlohmann::json::array();
        
        if (legacy_config.contains("providers")) {
            for (const auto& legacy_provider : legacy_config["providers"]) {
                auto migrated_provider = migrate_provider(legacy_provider);
                new_config["providers"].push_back(migrated_provider);
            }
        }
        
        // Add default daemon configuration
        new_config["daemon"] = nlohmann::json::object();
        new_config["daemon"]["port"] = legacy_config.value("port", 8080);
        new_config["daemon"]["host"] = "localhost";
        
        // Add default logging configuration
        new_config["logging"] = nlohmann::json::object();
        new_config["logging"]["level"] = legacy_config.value("log_level", "info");
        new_config["logging"]["file"] = "aimux.log";
        
        // Write migrated configuration
        std::ofstream target_stream(target_file);
        if (!target_stream.is_open()) {
            std::cerr << "Error: Cannot create target configuration file: " << target_file << "\n";
            return false;
        }
        
        target_stream << new_config.dump(2);
        target_stream.close();
        
        std::cout << "✓ Configuration migrated successfully\n";
        std::cout << "  Legacy: " << legacy_file << "\n";
        std::cout << "  Target: " << target_file << "\n";
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: Migration failed: " << e.what() << "\n";
        return false;
    }
}

bool ConfigMigrator::validate_config(const std::string& config_file) {
    try {
        std::ifstream stream(config_file);
        if (!stream.is_open()) {
            return false;
        }
        
        nlohmann::json config;
        stream >> config;
        stream.close();
        
        // Basic structure validation
        if (!config.contains("version")) {
            std::cerr << "Error: Configuration missing version field\n";
            return false;
        }
        
        if (!config.contains("providers")) {
            std::cerr << "Error: Configuration missing providers field\n";
            return false;
        }
        
        // Validate each provider
        for (const auto& provider : config["providers"]) {
            if (!validate_provider(provider)) {
                return false;
            }
        }
        
        std::cout << "✓ Configuration validation passed\n";
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: Configuration validation failed: " << e.what() << "\n";
        return false;
    }
}

std::string ConfigMigrator::generate_migration_report(const std::string& legacy_file) {
    try {
        std::ifstream stream(legacy_file);
        if (!stream.is_open()) {
            return R"({"error": "Cannot open legacy configuration file"})";
        }
        
        nlohmann::json legacy_config;
        stream >> legacy_config;
        stream.close();
        
        nlohmann::json report;
        report["legacy_file"] = legacy_file;
        report["detected_version"] = detect_config_version(legacy_file);
        report["migration_status"] = "ready";
        report["providers_found"] = legacy_config.contains("providers") ? 
            legacy_config["providers"].size() : 0;
        
        // List providers that need migration
        report["providers_to_migrate"] = nlohmann::json::array();
        
        if (legacy_config.contains("providers")) {
            for (const auto& provider : legacy_config["providers"]) {
                std::string provider_name = provider.value("name", "unknown");
                report["providers_to_migrate"].push_back(provider_name);
            }
        }
        
        report["migration_recommendations"] = {
            "Test migrated configuration before deployment",
            "Backup original configuration file",
            "Verify provider API keys after migration"
        };
        
        return report.dump(2);
        
    } catch (const std::exception& e) {
        nlohmann::json error_report;
        error_report["error"] = "Migration report generation failed";
        error_report["details"] = e.what();
        return error_report.dump();
    }
}

std::string ConfigMigrator::detect_config_version(const std::string& config_file) {
    // Simple version detection based on file content
    std::ifstream stream(config_file);
    if (!stream.is_open()) {
        return "unknown";
    }
    
    std::string content((std::istreambuf_iterator<char>(stream)),
                       std::istreambuf_iterator<char>());
    stream.close();
    
    if (content.find("\"aimux\"") != std::string::npos) {
        return "1.0";
    } else if (content.find("\"version\"") != std::string::npos) {
        return "2.0";
    } else {
        return "legacy";
    }
}

nlohmann::json ConfigMigrator::migrate_provider(const nlohmann::json& legacy_provider) {
    nlohmann::json migrated_provider;
    
    // Map common fields
    migrated_provider["name"] = legacy_provider.value("name", "");
    migrated_provider["endpoint"] = legacy_provider.value("endpoint", "");
    migrated_provider["api_key"] = legacy_provider.value("api_key", "");
    migrated_provider["enabled"] = legacy_provider.value("enabled", true);
    migrated_provider["max_requests_per_minute"] = legacy_provider.value("max_requests_per_minute", 60);
    
    // Handle models field
    if (legacy_provider.contains("models")) {
        migrated_provider["models"] = legacy_provider["models"];
    } else if (legacy_provider.contains("model")) {
        // Single model to array conversion
        migrated_provider["models"] = nlohmann::json::array({legacy_provider["model"]});
    } else {
        migrated_provider["models"] = nlohmann::json::array();
    }
    
    return migrated_provider;
}

bool ConfigMigrator::validate_provider(const nlohmann::json& provider) {
    // Required fields
    if (!provider.contains("name") || provider["name"].get<std::string>().empty()) {
        std::cerr << "Error: Provider missing name\n";
        return false;
    }
    
    if (!provider.contains("endpoint") || provider["endpoint"].get<std::string>().empty()) {
        std::cerr << "Error: Provider '" << provider["name"] << "' missing endpoint\n";
        return false;
    }
    
    if (!provider.contains("api_key") || provider["api_key"].get<std::string>().empty()) {
        std::cerr << "Error: Provider '" << provider["name"] << "' missing API key\n";
        return false;
    }
    
    return true;
}

int MigrationCLI::run_migration_cli(int argc, char* argv[]) {
    if (argc < 2) {
        print_help();
        return 1;
    }
    
    std::string command = argv[1];
    
    if (command == "migrate") {
        if (argc < 4) {
            std::cerr << "Usage: aimux migrate <legacy_file> <target_file>\n";
            return 1;
        }
        
        std::string legacy_file = argv[2];
        std::string target_file = argv[3];
        
        bool success = ConfigMigrator::migrate_legacy_config(legacy_file, target_file);
        return success ? 0 : 1;
        
    } else if (command == "validate") {
        if (argc < 3) {
            std::cerr << "Usage: aimux validate <config_file>\n";
            return 1;
        }
        
        std::string config_file = argv[2];
        bool valid = ConfigMigrator::validate_config(config_file);
        return valid ? 0 : 1;
        
    } else if (command == "report") {
        if (argc < 3) {
            std::cerr << "Usage: aimux report <legacy_file>\n";
            return 1;
        }
        
        std::string legacy_file = argv[2];
        std::string report = ConfigMigrator::generate_migration_report(legacy_file);
        std::cout << report << "\n";
        return 0;
        
    } else if (command == "--help" || command == "-h") {
        print_help();
        return 0;
        
    } else if (command == "--version" || command == "-v") {
        print_version();
        return 0;
        
    } else {
        std::cerr << "Error: Unknown command '" << command << "'\n";
        print_help();
        return 1;
    }
}

void MigrationCLI::print_help() {
    std::cout << "Aimux2 Configuration Migration Tool\n\n";
    std::cout << "USAGE:\n";
    std::cout << "    aimux migrate <legacy_file> <target_file>    Migrate configuration\n";
    std::cout << "    aimux validate <config_file>               Validate configuration\n";
    std::cout << "    aimux report <legacy_file>                  Generate migration report\n\n";
    std::cout << "EXAMPLES:\n";
    std::cout << "    aimux migrate old_config.json config.json\n";
    std::cout << "    aimux validate config.json\n";
    std::cout << "    aimux report old_config.json\n\n";
    std::cout << "For more information, see: https://github.com/aimux/aimux\n";
}

void MigrationCLI::print_version() {
    std::cout << "Aimux2 Migration Tool v2.0.0\n";
    std::cout << "(c) 2025 Zackor, LLC. All Rights Reserved\n";
}

} // namespace cli
} // namespace aimux