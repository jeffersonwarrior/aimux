#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include "config/production_config.h"
#include "aimux/prettifier/toon_formatter.hpp"

using namespace aimux::config;
using namespace aimux::prettifier;

class PrettifierConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_config_path_ = std::filesystem::temp_directory_path() / "test_prettifier_config.json";
    }

    void TearDown() override {
        std::filesystem::remove(test_config_path_);
    }

    std::filesystem::path test_config_path_;
};

// Test basic configuration loading
TEST_F(PrettifierConfigTest, BasicConfigurationLoading) {
    std::string config_json = R"({
        "providers": [],
        "system": {
            "environment": "test",
            "log_level": "info"
        },
        "webui": {
            "enabled": true,
            "port": 8080
        },
        "security": {
            "api_key_encryption": true
        },
        "daemon": {
            "enabled": false
        },
        "prettifier": {
            "enabled": true,
            "default_prettifier": "toon",
            "plugin_directory": "./test_plugins",
            "auto_discovery": false,
            "cache_ttl_minutes": 120,
            "max_cache_size": 2000,
            "performance_monitoring": false,
            "provider_mappings": {
                "cerebras": "enhanced_toon",
                "openai": "standard_toon"
            },
            "toon_config": {
                "include_metadata": false,
                "include_tools": true,
                "include_thinking": false,
                "preserve_timestamps": true,
                "enable_compression": true,
                "max_content_length": 500000,
                "indent": "  "
            }
        }
    })";

    // Write test configuration
    std::ofstream file(test_config_path_);
    file << config_json;
    file.close();

    // Load configuration
    auto& manager = ProductionConfigManager::getInstance();
    ASSERT_TRUE(manager.loadConfig(test_config_path_.string(), false));

    const auto& config = manager.getConfig();

    // Verify prettifier configuration
    EXPECT_TRUE(config.prettifier.enabled);
    EXPECT_EQ(config.prettifier.default_prettifier, "toon");
    EXPECT_EQ(config.prettifier.plugin_directory, "./test_plugins");
    EXPECT_FALSE(config.prettifier.auto_discovery);
    EXPECT_EQ(config.prettifier.cache_ttl_minutes, 120);
    EXPECT_EQ(config.prettifier.max_cache_size, 2000);
    EXPECT_FALSE(config.prettifier.performance_monitoring);

    // Verify provider mappings
    EXPECT_EQ(config.prettifier.provider_mappings.at("cerebras"), "enhanced_toon");
    EXPECT_EQ(config.prettifier.provider_mappings.at("openai"), "standard_toon");

    // Verify TOON configuration
    EXPECT_FALSE(config.prettifier.toon_config.include_metadata);
    EXPECT_TRUE(config.prettifier.toon_config.include_tools);
    EXPECT_FALSE(config.prettifier.toon_config.include_thinking);
    EXPECT_TRUE(config.prettifier.toon_config.preserve_timestamps);
    EXPECT_TRUE(config.prettifier.toon_config.enable_compression);
    EXPECT_EQ(config.prettifier.toon_config.max_content_length, 500000);
    EXPECT_EQ(config.prettifier.toon_config.indent, "  ");
}

// Test default configuration values
TEST_F(PrettifierConfigTest, DefaultConfigurationValues) {
    std::string config_json = R"({
        "providers": [],
        "system": {"environment": "test"},
        "webui": {"enabled": true},
        "security": {"api_key_encryption": true},
        "daemon": {"enabled": false}
    })";

    std::ofstream file(test_config_path_);
    file << config_json;
    file.close();

    auto& manager = ProductionConfigManager::getInstance();
    ASSERT_TRUE(manager.loadConfig(test_config_path_.string(), false));

    const auto& config = manager.getConfig();

    // Verify default values
    EXPECT_TRUE(config.prettifier.enabled);
    EXPECT_EQ(config.prettifier.default_prettifier, "toon");
    EXPECT_EQ(config.prettifier.plugin_directory, "./plugins");
    EXPECT_TRUE(config.prettifier.auto_discovery);
    EXPECT_EQ(config.prettifier.cache_ttl_minutes, 60);
    EXPECT_EQ(config.prettifier.max_cache_size, 1000);
    EXPECT_TRUE(config.prettifier.performance_monitoring);
    EXPECT_TRUE(config.prettifier.provider_mappings.empty());

    // Verify default TOON configuration
    EXPECT_TRUE(config.prettifier.toon_config.include_metadata);
    EXPECT_TRUE(config.prettifier.toon_config.include_tools);
    EXPECT_TRUE(config.prettifier.toon_config.include_thinking);
    EXPECT_TRUE(config.prettifier.toon_config.preserve_timestamps);
    EXPECT_FALSE(config.prettifier.toon_config.enable_compression);
    EXPECT_EQ(config.prettifier.toon_config.max_content_length, 1000000);
    EXPECT_EQ(config.prettifier.toon_config.indent, "    ");
}

// Test configuration validation
TEST_F(PrettifierConfigTest, ConfigurationValidation) {
    // Test invalid configuration
    std::string invalid_config_json = R"({
        "providers": [],
        "system": {"environment": "test"},
        "webui": {"enabled": true},
        "security": {"api_key_encryption": true},
        "daemon": {"enabled": false},
        "prettifier": {
            "enabled": "not_boolean",
            "default_prettifier": "",
            "plugin_directory": "",
            "cache_ttl_minutes": 0,
            "max_cache_size": 5,
            "toon_config": {
                "max_content_length": 20000000
            }
        }
    })";

    std::ofstream file(test_config_path_);
    file << invalid_config_json;
    file.close();

    auto& manager = ProductionConfigManager::getInstance();
    ASSERT_TRUE(manager.loadConfig(test_config_path_.string(), false));

    auto errors = manager.validateConfig();
    EXPECT_FALSE(errors.empty());

    // Check for specific validation errors
    bool found_enabled_error = false;
    bool found_prettifier_error = false;
    bool found_cache_ttl_error = false;
    bool found_cache_size_error = false;
    bool found_content_length_error = false;

    for (const auto& error : errors) {
        if (error.find("prettifier") != std::string::npos) {
            found_prettifier_error = true;
        }
        if (error.find("default_prettifier") != std::string::npos) {
            found_prettifier_error = true;
        }
        if (error.find("cache_ttl_minutes") != std::string::npos) {
            found_cache_ttl_error = true;
        }
        if (error.find("max_cache_size") != std::string::npos) {
            found_cache_size_error = true;
        }
        if (error.find("max_content_length") != std::string::npos) {
            found_content_length_error = true;
        }
    }

    EXPECT_TRUE(found_prettifier_error);
}

// Test configuration serialization
TEST_F(PrettifierConfigTest, ConfigurationSerialization) {
    ProductionConfig config;

    // Set prettifier configuration
    config.prettifier.enabled = true;
    config.prettifier.default_prettifier = "test_formatter";
    config.prettifier.plugin_directory = "/opt/aimux/plugins";
    config.prettifier.auto_discovery = false;
    config.prettifier.cache_ttl_minutes = 180;
    config.prettifier.max_cache_size = 5000;
    config.prettifier.performance_monitoring = true;
    config.prettifier.provider_mappings = {{"test_provider", "test_formatter"}};

    config.prettifier.toon_config.include_metadata = false;
    config.prettifier.toon_config.include_tools = true;
    config.prettifier.toon_config.include_thinking = false;
    config.prettifier.toon_config.preserve_timestamps = true;
    config.prettifier.toon_config.enable_compression = true;
    config.prettifier.toon_config.max_content_length = 2000000;
    config.prettifier.toon_config.indent = "\t";

    // Serialize to JSON
    auto json = config.toJson(false);

    EXPECT_TRUE(json.contains("prettifier"));
    const auto& prettifier_json = json["prettifier"];

    EXPECT_EQ(prettifier_json["enabled"], true);
    EXPECT_EQ(prettifier_json["default_prettifier"], "test_formatter");
    EXPECT_EQ(prettifier_json["plugin_directory"], "/opt/aimux/plugins");
    EXPECT_EQ(prettifier_json["auto_discovery"], false);
    EXPECT_EQ(prettifier_json["cache_ttl_minutes"], 180);
    EXPECT_EQ(prettifier_json["max_cache_size"], 5000);
    EXPECT_EQ(prettifier_json["performance_monitoring"], true);

    EXPECT_TRUE(prettifier_json.contains("provider_mappings"));
    EXPECT_EQ(prettifier_json["provider_mappings"]["test_provider"], "test_formatter");

    EXPECT_TRUE(prettifier_json.contains("toon_config"));
    const auto& toon_json = prettifier_json["toon_config"];
    EXPECT_EQ(toon_json["include_metadata"], false);
    EXPECT_EQ(toon_json["include_tools"], true);
    EXPECT_EQ(toon_json["include_thinking"], false);
    EXPECT_EQ(toon_json["preserve_timestamps"], true);
    EXPECT_EQ(toon_json["enable_compression"], true);
    EXPECT_EQ(toon_json["max_content_length"], 2000000);
    EXPECT_EQ(toon_json["indent"], "\t");

    // Test round-trip serialization
    auto restored_config = ProductionConfig::fromJson(json);
    EXPECT_EQ(restored_config.prettifier.enabled, config.prettifier.enabled);
    EXPECT_EQ(restored_config.prettifier.default_prettifier, config.prettifier.default_prettifier);
    EXPECT_EQ(restored_config.prettifier.plugin_directory, config.prettifier.plugin_directory);
    EXPECT_EQ(restored_config.prettifier.auto_discovery, config.prettifier.auto_discovery);
    EXPECT_EQ(restored_config.prettifier.cache_ttl_minutes, config.prettifier.cache_ttl_minutes);
    EXPECT_EQ(restored_config.prettifier.max_cache_size, config.prettifier.max_cache_size);
    EXPECT_EQ(restored_config.prettifier.performance_monitoring, config.prettifier.performance_monitoring);
    EXPECT_EQ(restored_config.prettifier.provider_mappings, config.prettifier.provider_mappings);

    EXPECT_EQ(restored_config.prettifier.toon_config.include_metadata, config.prettifier.toon_config.include_metadata);
    EXPECT_EQ(restored_config.prettifier.toon_config.include_tools, config.prettifier.toon_config.include_tools);
    EXPECT_EQ(restored_config.prettifier.toon_config.include_thinking, config.prettifier.toon_config.include_thinking);
    EXPECT_EQ(restored_config.prettifier.toon_config.preserve_timestamps, config.prettifier.toon_config.preserve_timestamps);
    EXPECT_EQ(restored_config.prettifier.toon_config.enable_compression, config.prettifier.toon_config.enable_compression);
    EXPECT_EQ(restored_config.prettifier.toon_config.max_content_length, config.prettifier.toon_config.max_content_length);
    EXPECT_EQ(restored_config.prettifier.toon_config.indent, config.prettifier.toon_config.indent);
}

// Test production template includes prettifier configuration
TEST_F(PrettifierConfigTest, ProductionTemplateIncludesPrettifier) {
    auto& manager = ProductionConfigManager::getInstance();
    auto config = manager.createProductionTemplate();

    // Verify prettifier section exists in template
    EXPECT_TRUE(config.prettifier.enabled);
    EXPECT_EQ(config.prettifier.default_prettifier, "toon");
    EXPECT_EQ(config.prettifier.plugin_directory, "./plugins");
    EXPECT_TRUE(config.prettifier.auto_discovery);
    EXPECT_EQ(config.prettifier.cache_ttl_minutes, 60);
    EXPECT_EQ(config.prettifier.max_cache_size, 1000);
    EXPECT_TRUE(config.prettifier.performance_monitoring);

    // Verify default provider mappings
    EXPECT_FALSE(config.prettifier.provider_mappings.empty());
    EXPECT_EQ(config.prettifier.provider_mappings.at("cerebras"), "toon");
    EXPECT_EQ(config.prettifier.provider_mappings.at("openai"), "toon");
    EXPECT_EQ(config.prettifier.provider_mappings.at("anthropic"), "toon");
    EXPECT_EQ(config.prettifier.provider_mappings.at("synthetic"), "toon");

    // Verify TOON configuration defaults
    EXPECT_TRUE(config.prettifier.toon_config.include_metadata);
    EXPECT_TRUE(config.prettifier.toon_config.include_tools);
    EXPECT_TRUE(config.prettifier.toon_config.include_thinking);
    EXPECT_TRUE(config.prettifier.toon_config.preserve_timestamps);
    EXPECT_FALSE(config.prettifier.toon_config.enable_compression);
    EXPECT_EQ(config.prettifier.toon_config.max_content_length, 1000000);
    EXPECT_EQ(config.prettifier.toon_config.indent, "    ");
}

// Test JSON schema validation includes prettifier
TEST_F(PrettifierConfigTest, JsonSchemaValidationIncludesPrettifier) {
    // Valid configuration with prettifier
    std::string valid_config = R"({
        "providers": [],
        "system": {"environment": "test"},
        "webui": {"enabled": true},
        "security": {"api_key_encryption": true},
        "daemon": {"enabled": false},
        "prettifier": {
            "enabled": true,
            "default_prettifier": "toon",
            "plugin_directory": "./plugins",
            "auto_discovery": true,
            "cache_ttl_minutes": 60,
            "max_cache_size": 1000,
            "performance_monitoring": true,
            "provider_mappings": {},
            "toon_config": {
                "include_metadata": true,
                "include_tools": true,
                "include_thinking": true,
                "preserve_timestamps": true,
                "enable_compression": false,
                "max_content_length": 1000000,
                "indent": "    "
            }
        }
    })";

    auto valid_json = nlohmann::json::parse(valid_config);
    auto validation_result = validation::validateConfigWithSchema(valid_json);
    EXPECT_TRUE(validation_result["valid"]);

    // Invalid configuration - missing prettifier section
    std::string missing_prettifier = R"({
        "providers": [],
        "system": {"environment": "test"},
        "webui": {"enabled": true},
        "security": {"api_key_encryption": true},
        "daemon": {"enabled": false}
    })";

    auto missing_json = nlohmann::json::parse(missing_prettifier);
    auto missing_result = validation::validateConfigWithSchema(missing_json);
    EXPECT_FALSE(missing_result["valid"]);

    bool found_missing_error = false;
    for (const auto& error : missing_result["errors"]) {
        if (error.get<std::string>().find("prettifier") != std::string::npos) {
            found_missing_error = true;
            break;
        }
    }
    EXPECT_TRUE(found_missing_error);

    // Invalid configuration - wrong types in prettifier
    std::string wrong_types = R"({
        "providers": [],
        "system": {"environment": "test"},
        "webui": {"enabled": true},
        "security": {"api_key_encryption": true},
        "daemon": {"enabled": false},
        "prettifier": {
            "enabled": "not_boolean",
            "default_prettifier": 123,
            "cache_ttl_minutes": "not_number",
            "max_cache_size": "not_number",
            "performance_monitoring": "not_boolean",
            "provider_mappings": "not_object",
            "toon_config": "not_object"
        }
    })";

    auto wrong_types_json = nlohmann::json::parse(wrong_types);
    auto wrong_types_result = validation::validateConfigWithSchema(wrong_types_json);
    EXPECT_FALSE(wrong_types_result["valid"]);

    // Should have multiple type errors
    int prettifier_type_errors = 0;
    for (const auto& error : wrong_types_result["errors"]) {
        std::string error_str = error.get<std::string>();
        if (error_str.find("Prettifier") != std::string::npos &&
            error_str.find("must be") != std::string::npos) {
            prettifier_type_errors++;
        }
    }
    EXPECT_GE(prettifier_type_errors, 5);
}