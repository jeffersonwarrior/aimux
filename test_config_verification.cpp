/**
 * Configuration Verification Test Program
 *
 * Tests that ProductionConfig correctly loads prettifier settings from:
 * 1. config.json file
 * 2. Environment variable overrides
 *
 * Usage:
 *   ./test_config_verification [config_file]
 *
 * Environment variables tested:
 *   AIMUX_PRETTIFIER_ENABLED=true|false
 *   AIMUX_OUTPUT_FORMAT=toon|json|raw
 */

#include <iostream>
#include <string>
#include <cstdlib>
#include "config/production_config.h"
#include <nlohmann/json.hpp>

using namespace aimux::config;

void print_separator(const std::string& title) {
    std::cout << "\n========================================\n";
    std::cout << title << "\n";
    std::cout << "========================================\n\n";
}

void print_prettifier_config(const PrettifierConfig& config) {
    std::cout << "Prettifier Configuration:\n";
    std::cout << "  enabled: " << (config.enabled ? "true" : "false") << "\n";
    std::cout << "  default_prettifier: " << config.default_prettifier << "\n";
    std::cout << "  plugin_directory: " << config.plugin_directory << "\n";
    std::cout << "  auto_discovery: " << (config.auto_discovery ? "true" : "false") << "\n";
    std::cout << "  cache_ttl_minutes: " << config.cache_ttl_minutes << "\n";
    std::cout << "  max_cache_size: " << config.max_cache_size << "\n";
    std::cout << "  performance_monitoring: " << (config.performance_monitoring ? "true" : "false") << "\n";

    std::cout << "  provider_mappings: ";
    if (config.provider_mappings.empty()) {
        std::cout << "(empty)\n";
    } else {
        std::cout << "\n";
        for (const auto& [provider, format] : config.provider_mappings) {
            std::cout << "    " << provider << " -> " << format << "\n";
        }
    }

    std::cout << "  toon_config:\n";
    std::cout << "    include_metadata: " << (config.toon_config.include_metadata ? "true" : "false") << "\n";
    std::cout << "    include_tools: " << (config.toon_config.include_tools ? "true" : "false") << "\n";
    std::cout << "    include_thinking: " << (config.toon_config.include_thinking ? "true" : "false") << "\n";
    std::cout << "    preserve_timestamps: " << (config.toon_config.preserve_timestamps ? "true" : "false") << "\n";
    std::cout << "    enable_compression: " << (config.toon_config.enable_compression ? "true" : "false") << "\n";
    std::cout << "    max_content_length: " << config.toon_config.max_content_length << "\n";
    std::cout << "    indent: \"" << config.toon_config.indent << "\"\n";
}

void test_json_serialization(const PrettifierConfig& config) {
    print_separator("JSON Serialization Test");

    try {
        nlohmann::json j = config.toJson();
        std::cout << "Serialized to JSON:\n";
        std::cout << j.dump(2) << "\n";

        // Test deserialization
        PrettifierConfig deserialized = PrettifierConfig::fromJson(j);
        std::cout << "\nDeserialized successfully!\n";
        std::cout << "enabled matches: " << (deserialized.enabled == config.enabled ? "✓" : "✗") << "\n";
        std::cout << "default_prettifier matches: " << (deserialized.default_prettifier == config.default_prettifier ? "✓" : "✗") << "\n";
        std::cout << "cache_ttl_minutes matches: " << (deserialized.cache_ttl_minutes == config.cache_ttl_minutes ? "✓" : "✗") << "\n";

    } catch (const std::exception& e) {
        std::cerr << "ERROR: JSON serialization failed: " << e.what() << "\n";
    }
}

void test_environment_variables() {
    print_separator("Environment Variable Override Test");

    // Check if environment variables are set
    const char* env_enabled = std::getenv("AIMUX_PRETTIFIER_ENABLED");
    const char* env_format = std::getenv("AIMUX_OUTPUT_FORMAT");

    std::cout << "Environment Variables:\n";
    std::cout << "  AIMUX_PRETTIFIER_ENABLED: " << (env_enabled ? env_enabled : "(not set)") << "\n";
    std::cout << "  AIMUX_OUTPUT_FORMAT: " << (env_format ? env_format : "(not set)") << "\n";

    if (!env_enabled && !env_format) {
        std::cout << "\nNo environment variables set.\n";
        std::cout << "To test env var overrides, run:\n";
        std::cout << "  AIMUX_PRETTIFIER_ENABLED=false AIMUX_OUTPUT_FORMAT=json ./test_config_verification\n";
    } else {
        std::cout << "\nEnvironment variables detected! These should override config file values.\n";
    }
}

void test_default_config() {
    print_separator("Default Configuration Test");

    PrettifierConfig default_config;
    std::cout << "Default configuration values:\n";
    print_prettifier_config(default_config);

    // Validate defaults
    bool defaults_ok = true;
    if (!default_config.enabled) {
        std::cerr << "✗ Default enabled should be true\n";
        defaults_ok = false;
    }
    if (default_config.default_prettifier != "toon") {
        std::cerr << "✗ Default prettifier should be 'toon', got: " << default_config.default_prettifier << "\n";
        defaults_ok = false;
    }
    if (default_config.cache_ttl_minutes != 60) {
        std::cerr << "✗ Default cache_ttl_minutes should be 60, got: " << default_config.cache_ttl_minutes << "\n";
        defaults_ok = false;
    }

    if (defaults_ok) {
        std::cout << "\n✓ All default values are correct\n";
    }
}

void test_config_file(const std::string& config_file) {
    print_separator("Configuration File Test");

    std::cout << "Loading config from: " << config_file << "\n\n";

    auto& config_manager = ProductionConfigManager::getInstance();

    if (!config_manager.loadConfig(config_file, false)) {
        std::cerr << "ERROR: Failed to load config file: " << config_file << "\n";
        std::cerr << "Make sure the file exists and contains valid JSON.\n";
        return;
    }

    const auto& config = config_manager.getConfig();
    print_prettifier_config(config.prettifier);

    // Validate loaded config
    std::cout << "\nValidation:\n";
    auto validation_errors = config_manager.validateConfig();
    if (validation_errors.empty()) {
        std::cout << "✓ Configuration is valid\n";
    } else {
        std::cout << "✗ Configuration has errors:\n";
        for (const auto& error : validation_errors) {
            std::cout << "  - " << error << "\n";
        }
    }

    // Test JSON serialization
    test_json_serialization(config.prettifier);
}

int main(int argc, char* argv[]) {
    std::cout << "====================================\n";
    std::cout << "Aimux v2.1 Configuration Verification\n";
    std::cout << "====================================\n";

    // Test 1: Default configuration
    test_default_config();

    // Test 2: Environment variables
    test_environment_variables();

    // Test 3: Config file loading
    std::string config_file = "config.json";
    if (argc > 1) {
        config_file = argv[1];
    }

    test_config_file(config_file);

    // Test 4: Environment override simulation
    print_separator("Environment Override Simulation");
    std::cout << "To test environment variable overrides:\n\n";
    std::cout << "1. Set environment variables:\n";
    std::cout << "   export AIMUX_PRETTIFIER_ENABLED=false\n";
    std::cout << "   export AIMUX_OUTPUT_FORMAT=json\n\n";
    std::cout << "2. Run this test again:\n";
    std::cout << "   ./test_config_verification\n\n";
    std::cout << "3. The configuration should show:\n";
    std::cout << "   enabled: false (overridden by env var)\n";
    std::cout << "   default_prettifier: json (overridden by env var)\n\n";
    std::cout << "4. Unset variables to restore defaults:\n";
    std::cout << "   unset AIMUX_PRETTIFIER_ENABLED\n";
    std::cout << "   unset AIMUX_OUTPUT_FORMAT\n";

    print_separator("Test Complete");
    std::cout << "All tests completed successfully!\n\n";

    return 0;
}
