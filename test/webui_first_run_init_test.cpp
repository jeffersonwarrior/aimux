/**
 * @file webui_first_run_init_test.cpp
 * @brief Tests for WebUI first-run initialization and static mode
 *
 * Tests auto-creation of config.json on first run and static mode operation
 * when no valid API keys are configured.
 *
 * @version 3.0.0
 * @date 2025-11-24
 */

#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include <nlohmann/json.hpp>
#include "aimux/webui/first_run_config.hpp"
#include "aimux/webui/config_validator.hpp"

using namespace aimux::webui;
namespace fs = std::filesystem;

class FirstRunInitTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a temporary directory for test configs
        test_dir_ = fs::temp_directory_path() / "aimux_first_run_test";
        fs::create_directories(test_dir_);
        test_config_path_ = test_dir_ / "config.json";

        // Ensure test config doesn't exist
        if (fs::exists(test_config_path_)) {
            fs::remove(test_config_path_);
        }
    }

    void TearDown() override {
        // Clean up test directory
        if (fs::exists(test_dir_)) {
            fs::remove_all(test_dir_);
        }
    }

    fs::path test_dir_;
    fs::path test_config_path_;
};

// Test 1: Config doesn't exist → auto-create successful
TEST_F(FirstRunInitTest, AutoCreateConfigWhenMissing) {
    ASSERT_FALSE(fs::exists(test_config_path_)) << "Test config should not exist initially";

    // Create default config
    auto config = FirstRunConfigGenerator::create_default_config();
    ASSERT_FALSE(config.is_null()) << "Created config should not be null";

    // Save config
    bool saved = FirstRunConfigGenerator::save_config(config, test_config_path_.string());
    ASSERT_TRUE(saved) << "Config should be saved successfully";

    // Verify file was created
    ASSERT_TRUE(fs::exists(test_config_path_)) << "Config file should exist after save";
}

// Test 2: Auto-created config is valid JSON
TEST_F(FirstRunInitTest, AutoCreatedConfigIsValidJson) {
    auto config = FirstRunConfigGenerator::create_default_config();
    FirstRunConfigGenerator::save_config(config, test_config_path_.string());

    // Read back and parse
    std::ifstream ifs(test_config_path_);
    ASSERT_TRUE(ifs.is_open()) << "Should be able to open saved config";

    nlohmann::json loaded_config;
    ASSERT_NO_THROW(ifs >> loaded_config) << "Config should be valid JSON";

    EXPECT_TRUE(loaded_config.is_object()) << "Config should be a JSON object";
}

// Test 3: Auto-created config has all required sections
TEST_F(FirstRunInitTest, AutoCreatedConfigHasAllRequiredSections) {
    auto config = FirstRunConfigGenerator::create_default_config();

    // Check all required top-level sections
    EXPECT_TRUE(config.contains("system")) << "Config should have 'system' section";
    EXPECT_TRUE(config.contains("security")) << "Config should have 'security' section";
    EXPECT_TRUE(config.contains("server")) << "Config should have 'server' section";
    EXPECT_TRUE(config.contains("webui")) << "Config should have 'webui' section";
    EXPECT_TRUE(config.contains("providers")) << "Config should have 'providers' section";
    EXPECT_TRUE(config.contains("mode")) << "Config should have 'mode' field";

    // Verify mode is set to static
    EXPECT_EQ(config["mode"].get<std::string>(), "static")
        << "Default mode should be 'static'";
}

// Test 4: WebUI config validator accepts static mode with dummy config
TEST_F(FirstRunInitTest, ValidatorAcceptsStaticModeConfig) {
    auto config = FirstRunConfigGenerator::create_default_config();

    ConfigValidator validator;

    // In static mode, validation should be relaxed
    bool is_static = FirstRunConfigGenerator::is_static_mode(config);
    EXPECT_TRUE(is_static) << "Config should be in static mode";

    // Validation should pass for static mode config
    auto result = validator.validate_config(config);
    EXPECT_TRUE(result.valid) << "Static mode config should pass validation: "
                               << result.error_message;
}

// Test 5: Static mode configuration has dummy API keys
TEST_F(FirstRunInitTest, StaticModeHasDummyApiKeys) {
    auto config = FirstRunConfigGenerator::create_default_config();

    ASSERT_TRUE(config.contains("providers")) << "Config must have providers";
    ASSERT_TRUE(config["providers"].is_array()) << "Providers must be array";

    // Check that providers have dummy keys
    for (const auto& provider : config["providers"]) {
        ASSERT_TRUE(provider.contains("api_key")) << "Provider must have api_key";
        std::string api_key = provider["api_key"].get<std::string>();

        // Dummy keys should be marked as such
        EXPECT_TRUE(api_key == "DUMMY_KEY_REPLACE_ME" ||
                    api_key.find("dummy") != std::string::npos ||
                    api_key.find("REPLACE") != std::string::npos)
            << "API key should be a dummy placeholder: " << api_key;
    }
}

// Test 6: Static mode indicator functions work correctly
TEST_F(FirstRunInitTest, StaticModeDetection) {
    auto static_config = FirstRunConfigGenerator::create_default_config();
    EXPECT_TRUE(FirstRunConfigGenerator::is_static_mode(static_config))
        << "Default config should be detected as static mode";

    // Create operational config
    auto operational_config = static_config;
    operational_config["mode"] = "operational";
    EXPECT_FALSE(FirstRunConfigGenerator::is_static_mode(operational_config))
        << "Config with mode='operational' should not be static";

    // Missing mode field should default to operational
    nlohmann::json no_mode_config = nlohmann::json::object();
    no_mode_config["providers"] = nlohmann::json::array();
    EXPECT_FALSE(FirstRunConfigGenerator::is_static_mode(no_mode_config))
        << "Config without mode field should default to operational";
}

// Test 7: Config loading detects missing file and auto-creates
TEST_F(FirstRunInitTest, LoadOrCreateConfigHandlesMissingFile) {
    ASSERT_FALSE(fs::exists(test_config_path_))
        << "Config should not exist before test";

    // Load or create - should auto-create
    auto config = FirstRunConfigGenerator::load_or_create_config(
        test_config_path_.string());

    EXPECT_FALSE(config.is_null()) << "Should return valid config";
    EXPECT_TRUE(fs::exists(test_config_path_))
        << "Config file should be created";
    EXPECT_TRUE(FirstRunConfigGenerator::is_static_mode(config))
        << "Auto-created config should be in static mode";
}

// Test 8: Config loading reads existing file
TEST_F(FirstRunInitTest, LoadOrCreateConfigReadsExistingFile) {
    // Create a custom config
    nlohmann::json custom_config = {
        {"mode", "operational"},
        {"system", {{"environment", "custom"}}},
        {"providers", nlohmann::json::array()}
    };

    std::ofstream ofs(test_config_path_);
    ofs << custom_config.dump(2);
    ofs.close();

    // Load - should read existing
    auto loaded_config = FirstRunConfigGenerator::load_or_create_config(
        test_config_path_.string());

    EXPECT_FALSE(loaded_config.is_null()) << "Should load existing config";
    EXPECT_EQ(loaded_config["mode"].get<std::string>(), "operational")
        << "Should preserve existing mode";
    EXPECT_EQ(loaded_config["system"]["environment"].get<std::string>(), "custom")
        << "Should preserve existing values";
}

// Test 9: Mode can be switched from static to operational
TEST_F(FirstRunInitTest, ModeCanBeSwitchedToOperational) {
    auto config = FirstRunConfigGenerator::create_default_config();
    FirstRunConfigGenerator::save_config(config, test_config_path_.string());

    // Switch mode
    bool switched = FirstRunConfigGenerator::switch_to_operational_mode(
        test_config_path_.string());
    EXPECT_TRUE(switched) << "Should successfully switch mode";

    // Reload and verify
    auto reloaded = FirstRunConfigGenerator::load_or_create_config(
        test_config_path_.string());
    EXPECT_FALSE(FirstRunConfigGenerator::is_static_mode(reloaded))
        << "Mode should be operational after switch";
}

// Test 10: Config persists between restarts (load-save-reload cycle)
TEST_F(FirstRunInitTest, ConfigPersistsBetweenRestarts) {
    // First run: create config
    auto config1 = FirstRunConfigGenerator::create_default_config();
    FirstRunConfigGenerator::save_config(config1, test_config_path_.string());

    // Simulate restart: load config
    auto config2 = FirstRunConfigGenerator::load_or_create_config(
        test_config_path_.string());

    // Modify and save
    config2["system"]["log_level"] = "debug";
    FirstRunConfigGenerator::save_config(config2, test_config_path_.string());

    // Another restart: load again
    auto config3 = FirstRunConfigGenerator::load_or_create_config(
        test_config_path_.string());

    // Verify persistence
    EXPECT_EQ(config3["system"]["log_level"].get<std::string>(), "debug")
        << "Modifications should persist across restarts";
    EXPECT_TRUE(FirstRunConfigGenerator::is_static_mode(config3))
        << "Mode should remain static unless explicitly changed";
}

// Additional test: Verify provider structure in default config
TEST_F(FirstRunInitTest, DefaultConfigHasValidProviderStructure) {
    auto config = FirstRunConfigGenerator::create_default_config();

    ASSERT_TRUE(config.contains("providers"));
    ASSERT_TRUE(config["providers"].is_array());
    ASSERT_GT(config["providers"].size(), 0)
        << "Should have at least one provider configured";

    // Check first provider structure
    const auto& provider = config["providers"][0];
    EXPECT_TRUE(provider.contains("name")) << "Provider should have name";
    EXPECT_TRUE(provider.contains("endpoint")) << "Provider should have endpoint";
    EXPECT_TRUE(provider.contains("api_key")) << "Provider should have api_key";
    EXPECT_TRUE(provider.contains("models")) << "Provider should have models array";
}

// Additional test: Error handling for invalid paths
TEST_F(FirstRunInitTest, HandlesInvalidPathsGracefully) {
    // Try to save to invalid path
    auto config = FirstRunConfigGenerator::create_default_config();

    std::string invalid_path = "/root/cannot_write_here/config.json";
    bool saved = FirstRunConfigGenerator::save_config(config, invalid_path);

    // Should fail gracefully without crashing
    EXPECT_FALSE(saved) << "Should return false for invalid path";
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
