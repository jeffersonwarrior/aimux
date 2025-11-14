#pragma once

#include <string>
#include <nlohmann/json.hpp>

namespace aimux {
namespace cli {

/**
 * @brief Configuration migration tool for JSON -> JSON conversion
 */
class ConfigMigrator {
public:
    /**
     * @brief Migrate legacy configuration to new format
     * @param legacy_file Path to legacy configuration file
     * @param target_file Path to output migrated configuration
     * @return true if migration successful
     */
    static bool migrate_legacy_config(const std::string& legacy_file,
                                 const std::string& target_file);
    
    /**
     * @brief Validate configuration format
     * @param config_file Configuration file to validate
     * @return true if valid
     */
    static bool validate_config(const std::string& config_file);
    
    /**
     * @brief Generate migration report
     * @param legacy_file Legacy configuration file
     * @return Migration report as JSON string
     */
    static std::string generate_migration_report(const std::string& legacy_file);

private:
    /**
     * @brief Detect configuration version
     * @param config_file Configuration file to check
     * @return Version string
     */
    static std::string detect_config_version(const std::string& config_file);
    
    /**
     * @brief Migrate provider configuration
     * @param legacy_provider Legacy provider configuration
     * @return Migrated provider configuration
     */
    static nlohmann::json migrate_provider(const nlohmann::json& legacy_provider);
    
    /**
     * @brief Validate provider configuration
     * @param provider Provider configuration to validate
     * @return true if valid
     */
    static bool validate_provider(const nlohmann::json& provider);
};

/**
 * @brief Command-line interface for migration tool
 */
class MigrationCLI {
public:
    /**
     * @brief Run migration tool with command-line arguments
     * @param argc Argument count
     * @param argv Argument vector
     * @return Exit code
     */
    static int run_migration_cli(int argc, char* argv[]);
    
private:
    /**
     * @brief Print migration tool help
     */
    static void print_help();
    
    /**
     * @brief Print migration version
     */
    static void print_version();
};

} // namespace cli
} // namespace aimux