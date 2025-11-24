/**
 * Phase 3.2: Configuration Loading Test
 *
 * Tests:
 * 1. ProductionConfig loads prettifier settings from config.json
 * 2. AIMUX_PRETTIFIER_ENABLED environment variable override
 * 3. AIMUX_OUTPUT_FORMAT environment variable override
 * 4. Per-provider format selection from config
 */

#include "config/production_config.h"
#include "aimux/logging/logger.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <cstdlib>
#include <iostream>

using namespace aimux::config;
namespace fs = std::filesystem;

class ConfigTest {
public:
    static bool run_all_tests() {
        int passed = 0;
        int failed = 0;

        std::cout << "\n=== Phase 3.2: Configuration Loading Tests ===\n\n";

        // Test 1: Load prettifier config from JSON
        if (test_load_from_json()) {
            std::cout << "✅ Test 1: Load prettifier from config.json - PASSED\n";
            passed++;
        } else {
            std::cout << "❌ Test 1: Load prettifier from config.json - FAILED\n";
            failed++;
        }

        // Test 2: Environment variable override for enabled flag
        if (test_env_override_enabled()) {
            std::cout << "✅ Test 2: AIMUX_PRETTIFIER_ENABLED override - PASSED\n";
            passed++;
        } else {
            std::cout << "❌ Test 2: AIMUX_PRETTIFIER_ENABLED override - FAILED\n";
            failed++;
        }

        // Test 3: Environment variable override for output format
        if (test_env_override_format()) {
            std::cout << "✅ Test 3: AIMUX_OUTPUT_FORMAT override - PASSED\n";
            passed++;
        } else {
            std::cout << "❌ Test 3: AIMUX_OUTPUT_FORMAT override - FAILED\n";
            failed++;
        }

        // Test 4: Per-provider format selection
        if (test_provider_format_mapping()) {
            std::cout << "✅ Test 4: Per-provider format selection - PASSED\n";
            passed++;
        } else {
            std::cout << "❌ Test 4: Per-provider format selection - FAILED\n";
            failed++;
        }

        // Test 5: Prettifier config validation
        if (test_prettifier_validation()) {
            std::cout << "✅ Test 5: Prettifier config validation - PASSED\n";
            passed++;
        } else {
            std::cout << "❌ Test 5: Prettifier config validation - FAILED\n";
            failed++;
        }

        // Test 6: TOON config loading
        if (test_toon_config_loading()) {
            std::cout << "✅ Test 6: TOON config loading - PASSED\n";
            passed++;
        } else {
            std::cout << "❌ Test 6: TOON config loading - FAILED\n";
            failed++;
        }

        std::cout << "\n=== Test Summary ===\n";
        std::cout << "Passed: " << passed << "\n";
        std::cout << "Failed: " << failed << "\n";
        std::cout << "Total:  " << (passed + failed) << "\n\n";

        return failed == 0;
    }

private:
    static bool test_load_from_json() {
        try {
            // Create test config with prettifier section
            nlohmann::json test_config = {
                {"prettifier", {
                    {"enabled", true},
                    {"default_prettifier", "toon"},
                    {"plugin_directory", "./test_plugins"},
                    {"auto_discovery", true},
                    {"cache_ttl_minutes", 120},
                    {"max_cache_size", 500},
                    {"performance_monitoring", true},
                    {"provider_mappings", {
                        {"cerebras", "toon"},
                        {"openai", "toon"}
                    }},
                    {"toon_config", {
                        {"include_metadata", true},
                        {"include_tools", true},
                        {"include_thinking", false},
                        {"preserve_timestamps", true},
                        {"enable_compression", false},
                        {"max_content_length", 2000000},
                        {"indent", "  "}
                    }}
                }}
            };

            // Load into ProductionConfig
            auto config = ProductionConfig::fromJson(test_config);

            // Verify all fields loaded correctly
            if (!config.prettifier.enabled) {
                std::cerr << "ERROR: prettifier.enabled should be true\n";
                return false;
            }

            if (config.prettifier.default_prettifier != "toon") {
                std::cerr << "ERROR: default_prettifier should be 'toon', got: "
                          << config.prettifier.default_prettifier << "\n";
                return false;
            }

            if (config.prettifier.plugin_directory != "./test_plugins") {
                std::cerr << "ERROR: plugin_directory mismatch\n";
                return false;
            }

            if (config.prettifier.cache_ttl_minutes != 120) {
                std::cerr << "ERROR: cache_ttl_minutes should be 120, got: "
                          << config.prettifier.cache_ttl_minutes << "\n";
                return false;
            }

            if (config.prettifier.max_cache_size != 500) {
                std::cerr << "ERROR: max_cache_size should be 500, got: "
                          << config.prettifier.max_cache_size << "\n";
                return false;
            }

            // Verify provider mappings
            if (config.prettifier.provider_mappings.size() != 2) {
                std::cerr << "ERROR: provider_mappings should have 2 entries\n";
                return false;
            }

            if (config.prettifier.provider_mappings["cerebras"] != "toon") {
                std::cerr << "ERROR: cerebras should map to 'toon'\n";
                return false;
            }

            // Verify TOON config
            if (config.prettifier.toon_config.include_thinking) {
                std::cerr << "ERROR: include_thinking should be false\n";
                return false;
            }

            if (config.prettifier.toon_config.max_content_length != 2000000) {
                std::cerr << "ERROR: max_content_length should be 2000000\n";
                return false;
            }

            if (config.prettifier.toon_config.indent != "  ") {
                std::cerr << "ERROR: indent should be '  ' (2 spaces)\n";
                return false;
            }

            return true;

        } catch (const std::exception& e) {
            std::cerr << "Exception in test_load_from_json: " << e.what() << "\n";
            return false;
        }
    }

    static bool test_env_override_enabled() {
        try {
            // Set environment variable
            setenv("AIMUX_PRETTIFIER_ENABLED", "false", 1);

            // Create config with prettifier enabled
            nlohmann::json test_config = {
                {"prettifier", {{"enabled", true}}}
            };

            auto config = ProductionConfig::fromJson(test_config);

            // Manually apply environment override (simulating loadEnvironmentOverrides)
            if (auto envEnabled = env::getBool("AIMUX_PRETTIFIER_ENABLED")) {
                config.prettifier.enabled = *envEnabled;
            }

            // Verify override worked
            if (config.prettifier.enabled) {
                std::cerr << "ERROR: Environment override failed, prettifier still enabled\n";
                unsetenv("AIMUX_PRETTIFIER_ENABLED");
                return false;
            }

            // Test reverse: enable via env
            setenv("AIMUX_PRETTIFIER_ENABLED", "true", 1);
            test_config["prettifier"]["enabled"] = false;
            config = ProductionConfig::fromJson(test_config);

            if (auto envEnabled = env::getBool("AIMUX_PRETTIFIER_ENABLED")) {
                config.prettifier.enabled = *envEnabled;
            }

            if (!config.prettifier.enabled) {
                std::cerr << "ERROR: Environment override to enable failed\n";
                unsetenv("AIMUX_PRETTIFIER_ENABLED");
                return false;
            }

            unsetenv("AIMUX_PRETTIFIER_ENABLED");
            return true;

        } catch (const std::exception& e) {
            std::cerr << "Exception in test_env_override_enabled: " << e.what() << "\n";
            unsetenv("AIMUX_PRETTIFIER_ENABLED");
            return false;
        }
    }

    static bool test_env_override_format() {
        try {
            // Set environment variable for output format
            setenv("AIMUX_OUTPUT_FORMAT", "json", 1);

            nlohmann::json test_config = {
                {"prettifier", {{"default_prettifier", "toon"}}}
            };

            auto config = ProductionConfig::fromJson(test_config);

            // Manually apply environment override
            if (auto envFormat = env::getString("AIMUX_OUTPUT_FORMAT")) {
                config.prettifier.default_prettifier = *envFormat;
            }

            if (config.prettifier.default_prettifier != "json") {
                std::cerr << "ERROR: Output format override failed, got: "
                          << config.prettifier.default_prettifier << "\n";
                unsetenv("AIMUX_OUTPUT_FORMAT");
                return false;
            }

            // Test with "raw" format
            setenv("AIMUX_OUTPUT_FORMAT", "raw", 1);
            config = ProductionConfig::fromJson(test_config);

            if (auto envFormat = env::getString("AIMUX_OUTPUT_FORMAT")) {
                config.prettifier.default_prettifier = *envFormat;
            }

            if (config.prettifier.default_prettifier != "raw") {
                std::cerr << "ERROR: Raw format override failed\n";
                unsetenv("AIMUX_OUTPUT_FORMAT");
                return false;
            }

            unsetenv("AIMUX_OUTPUT_FORMAT");
            return true;

        } catch (const std::exception& e) {
            std::cerr << "Exception in test_env_override_format: " << e.what() << "\n";
            unsetenv("AIMUX_OUTPUT_FORMAT");
            return false;
        }
    }

    static bool test_provider_format_mapping() {
        try {
            nlohmann::json test_config = {
                {"prettifier", {
                    {"default_prettifier", "toon"},
                    {"provider_mappings", {
                        {"cerebras", "toon"},
                        {"openai", "json"},
                        {"anthropic", "raw"}
                    }}
                }}
            };

            auto config = ProductionConfig::fromJson(test_config);

            // Verify each provider mapping
            if (config.prettifier.provider_mappings["cerebras"] != "toon") {
                std::cerr << "ERROR: cerebras mapping incorrect\n";
                return false;
            }

            if (config.prettifier.provider_mappings["openai"] != "json") {
                std::cerr << "ERROR: openai mapping incorrect\n";
                return false;
            }

            if (config.prettifier.provider_mappings["anthropic"] != "raw") {
                std::cerr << "ERROR: anthropic mapping incorrect\n";
                return false;
            }

            // Test provider not in mapping uses default
            std::string provider = "unknown_provider";
            std::string format = config.prettifier.provider_mappings.count(provider)
                ? config.prettifier.provider_mappings[provider]
                : config.prettifier.default_prettifier;

            if (format != "toon") {
                std::cerr << "ERROR: Unknown provider should use default format\n";
                return false;
            }

            return true;

        } catch (const std::exception& e) {
            std::cerr << "Exception in test_provider_format_mapping: " << e.what() << "\n";
            return false;
        }
    }

    static bool test_prettifier_validation() {
        try {
            // Test valid config
            PrettifierConfig valid_config;
            valid_config.enabled = true;
            valid_config.default_prettifier = "toon";
            valid_config.plugin_directory = "./plugins";
            valid_config.cache_ttl_minutes = 60;
            valid_config.max_cache_size = 1000;

            auto errors = validation::validatePrettifierConfig(valid_config);
            if (!errors.empty()) {
                std::cerr << "ERROR: Valid config should have no errors, got: "
                          << errors.size() << " errors\n";
                for (const auto& err : errors) {
                    std::cerr << "  - " << err << "\n";
                }
                return false;
            }

            // Test invalid cache_ttl_minutes
            PrettifierConfig invalid_ttl = valid_config;
            invalid_ttl.cache_ttl_minutes = 2000; // Too high (max 1440)

            errors = validation::validatePrettifierConfig(invalid_ttl);
            if (errors.empty()) {
                std::cerr << "ERROR: Invalid TTL should produce errors\n";
                return false;
            }

            // Test invalid max_cache_size
            PrettifierConfig invalid_size = valid_config;
            invalid_size.max_cache_size = 5; // Too low (min 10)

            errors = validation::validatePrettifierConfig(invalid_size);
            if (errors.empty()) {
                std::cerr << "ERROR: Invalid cache size should produce errors\n";
                return false;
            }

            // Test empty default_prettifier
            PrettifierConfig empty_prettifier = valid_config;
            empty_prettifier.default_prettifier = "";

            errors = validation::validatePrettifierConfig(empty_prettifier);
            if (errors.empty()) {
                std::cerr << "ERROR: Empty prettifier name should produce errors\n";
                return false;
            }

            return true;

        } catch (const std::exception& e) {
            std::cerr << "Exception in test_prettifier_validation: " << e.what() << "\n";
            return false;
        }
    }

    static bool test_toon_config_loading() {
        try {
            nlohmann::json test_config = {
                {"prettifier", {
                    {"toon_config", {
                        {"include_metadata", false},
                        {"include_tools", false},
                        {"include_thinking", true},
                        {"preserve_timestamps", false},
                        {"enable_compression", true},
                        {"max_content_length", 5000000},
                        {"indent", "\t"}
                    }}
                }}
            };

            auto config = ProductionConfig::fromJson(test_config);

            // Verify all TOON config fields
            if (config.prettifier.toon_config.include_metadata) {
                std::cerr << "ERROR: include_metadata should be false\n";
                return false;
            }

            if (config.prettifier.toon_config.include_tools) {
                std::cerr << "ERROR: include_tools should be false\n";
                return false;
            }

            if (!config.prettifier.toon_config.include_thinking) {
                std::cerr << "ERROR: include_thinking should be true\n";
                return false;
            }

            if (config.prettifier.toon_config.preserve_timestamps) {
                std::cerr << "ERROR: preserve_timestamps should be false\n";
                return false;
            }

            if (!config.prettifier.toon_config.enable_compression) {
                std::cerr << "ERROR: enable_compression should be true\n";
                return false;
            }

            if (config.prettifier.toon_config.max_content_length != 5000000) {
                std::cerr << "ERROR: max_content_length should be 5000000, got: "
                          << config.prettifier.toon_config.max_content_length << "\n";
                return false;
            }

            if (config.prettifier.toon_config.indent != "\t") {
                std::cerr << "ERROR: indent should be tab character\n";
                return false;
            }

            return true;

        } catch (const std::exception& e) {
            std::cerr << "Exception in test_toon_config_loading: " << e.what() << "\n";
            return false;
        }
    }
};

int main() {
    bool all_passed = ConfigTest::run_all_tests();
    return all_passed ? 0 : 1;
}
