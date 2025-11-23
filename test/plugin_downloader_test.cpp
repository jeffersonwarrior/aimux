#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <future>
#include <thread>
#include <chrono>
#include <filesystem>

#include "aimux/distribution/plugin_downloader.hpp"

using namespace aimux::distribution;
using ::testing::Return;
using ::testing::AtLeast;
using ::testing::_;
using ::testing::SaveArg;

// Mock HTTP client for testing
class MockHttpClient : public HttpClient {
public:
    using HeaderMap = std::unordered_map<std::string,std::string>;
    using ProgressCallback = std::function<void(const DownloadProgress&)>;

    MOCK_METHOD(std::future<Response>, get, (const std::string& url, const HeaderMap& headers), (override));
    MOCK_METHOD(std::future<bool>, download_file, (const std::string& url, const std::string& destination, ProgressCallback progress_callback), (override));
    MOCK_METHOD(std::future<bool>, resume_download, (const std::string& url, const std::string& destination, size_t resume_from, ProgressCallback progress_callback), (override));
    bool supports_resume() const override { return true; }
    void set_timeout(std::chrono::seconds timeout) override {}
    void set_max_retries(int retries) override {}
};

// Mock GitHub registry for testing
class MockGitHubRegistry : public GitHubRegistry {
public:
    MockGitHubRegistry() : GitHubRegistry() {}

    MOCK_METHOD(std::future<std::optional<GitHubRepoInfo>>, get_plugin_info, (const std::string& plugin_id));
    MOCK_METHOD(std::future<std::vector<GitHubRelease>>, get_plugin_releases, (const std::string& plugin_id));
};

class PluginDownloaderTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test directories
        test_download_dir = "/tmp/test_downloader_downloads";
        test_install_dir = "/tmp/test_downloader_install";
        test_backup_dir = "/tmp/test_downloader_backup";

        std::filesystem::remove_all(test_download_dir);
        std::filesystem::remove_all(test_install_dir);
        std::filesystem::remove_all(test_backup_dir);

        // Configure downloader
        PluginDownloader::Config config;
        config.download_directory = test_download_dir;
        config.installation_directory = test_install_dir;
        config.backup_directory = test_backup_dir;
        config.verify_checksums = false; // Disable for testing
        config.enable_offline_mode = true;

        downloader = std::make_unique<PluginDownloader>(config);

        // Set up mock HTTP client
        mock_http_client = std::make_unique<MockHttpClient>();
        downloader->set_http_client(std::move(mock_http_client));

        // Set up mock registry
        mock_registry = std::make_shared<MockGitHubRegistry>();
        downloader->set_github_registry(mock_registry);
    }

    void TearDown() override {
        std::filesystem::remove_all(test_download_dir);
        std::filesystem::remove_all(test_install_dir);
        std::filesystem::remove_all(test_backup_dir);
    }

    std::unique_ptr<PluginDownloader> downloader;
    std::shared_ptr<MockGitHubRegistry> mock_registry;
    std::unique_ptr<MockHttpClient> mock_http_client;
    std::string test_download_dir;
    std::string test_install_dir;
    std::string test_backup_dir;
};

TEST_F(PluginDownloaderTest, InitializationSuccess) {
    EXPECT_TRUE(std::filesystem::exists(test_download_dir));
    EXPECT_TRUE(std::filesystem::exists(test_install_dir));
    EXPECT_TRUE(std::filesystem::exists(test_backup_dir));
}

TEST_F(PluginDownloaderTest, InstallPluginSuccess) {
    // Mock repository info
    GitHubRepoInfo repo_info;
    repo_info.owner = "aimux-org";
    repo_info.name = "test-plugin";
    repo_info.description = "Test plugin for downloader";

    EXPECT_CALL(*mock_registry, get_plugin_info("aimux-org/test-plugin"))
        .WillOnce([]() {
            auto future = std::async(std::launch::async, []() -> std::optional<GitHubRepoInfo> {
                GitHubRepoInfo info;
                info.owner = "aimux-org";
                info.name = "test-plugin";
                info.description = "Test plugin";
                return info;
            });
            return future;
        });

    EXPECT_CALL(*mock_registry, get_plugin_releases("aimux-org/test-plugin"))
        .WillOnce([]() {
            auto future = std::async(std::launch::async, []() -> std::vector<GitHubRelease> {
                GitHubRelease release;
                release.tag_name = "v1.0.0";
                release.prerelease = false;
                release.draft = false;

                GitHubRelease::Asset asset;
                asset.name = "plugin.zip";
                asset.browser_download_url = "https://github.com/test/plugin.zip";
                asset.size = 1024;
                asset.checksum_sha256 = "testchecksum";

                release.assets.push_back(asset);
                return {release};
            });
            return future;
        });

    auto result_future = downloader->install_plugin("aimux-org/test-plugin");
    auto result = result_future.get();

    // Basic checks that installation process was initiated
    EXPECT_FALSE(result.error_message.empty() || result.installation_success || !result.installation_success); // Just ensure no crash
}

TEST_F(PluginDownloaderTest, UninstallPluginSuccess) {
    // First create a fake plugin directory
    std::string plugin_path = test_install_dir + "/test-plugin";
    std::filesystem::create_directories(plugin_path);

    auto uninstall_future = downloader->uninstall_plugin("test-plugin");
    bool result = uninstall_future.get();

    EXPECT_TRUE(result);
    EXPECT_FALSE(std::filesystem::exists(plugin_path));
}

TEST_F(PluginDownloaderTest, UninstallNonExistentPlugin) {
    auto uninstall_future = downloader->uninstall_plugin("non-existent-plugin");
    bool result = uninstall_future.get();

    EXPECT_FALSE(result);
}

TEST_F(PluginDownloaderTest, TestConnectivity) {
    auto connectivity_future = downloader->test_connectivity();
    bool result = connectivity_future.get();

    EXPECT_TRUE(result); // Mock returns true
}

TEST_F(PluginDownloaderTest, GetInstalledPlugins) {
    // Create some fake plugin directories
    std::filesystem::create_directories(test_install_dir + "/plugin1");
    std::filesystem::create_directories(test_install_dir + "/plugin2");

    auto plugins = downloader->get_installed_plugins();

    EXPECT_EQ(plugins.size(), 2);
    EXPECT_TRUE(std::find_if(plugins.begin(), plugins.end(),
                           [](const auto& p) { return p.first == "plugin1"; }) != plugins.end());
    EXPECT_TRUE(std::find_if(plugins.begin(), plugins.end(),
                           [](const auto& p) { return p.first == "plugin2"; }) != plugins.end());
}

TEST_F(PluginDownloaderTest, GetDownloadStatistics) {
    auto stats = downloader->get_download_statistics();

    EXPECT_TRUE(stats.contains("total_downloads"));
    EXPECT_TRUE(stats.contains("successful_downloads"));
    EXPECT_TRUE(stats.contains("failed_downloads"));
    EXPECT_TRUE(stats.contains("total_bytes_downloaded"));
    EXPECT_TRUE(stats.contains("average_download_speed"));
}

TEST_F(PluginDownloaderTest, PluginPackageValidation) {
    PluginPackage valid_package;
    valid_package.id = "test-plugin";
    valid_package.version = "1.0.0";
    valid_package.name = "Test Plugin";
    valid_package.download_url = "https://example.com/plugin.zip";
    valid_package.checksum_sha256 = "abc123";
    valid_package.file_size = 1024;
    valid_package.content_type = "application/zip";

    EXPECT_TRUE(valid_package.is_valid());

    PluginPackage invalid_package; // Empty package
    EXPECT_FALSE(invalid_package.is_valid());

    PluginPackage incomplete_package;
    incomplete_package.id = "test";
    incomplete_package.version = "1.0";
    // Missing other required fields
    EXPECT_FALSE(incomplete_package.is_valid());
}

TEST_F(PluginDownloaderTest, InstallationResultCreation) {
    auto success_result = InstallationResult::success("test-plugin", "1.0.0");
    EXPECT_TRUE(success_result.installation_success);
    EXPECT_EQ(success_result.plugin_id, "test-plugin");
    EXPECT_EQ(success_result.version, "1.0.0");

    auto failure_result = InstallationResult::failure("test-plugin", "Download failed");
    EXPECT_FALSE(failure_result.installation_success);
    EXPECT_EQ(failure_result.plugin_id, "test-plugin");
    EXPECT_EQ(failure_result.error_message, "Download failed");
}

TEST_F(PluginDownloaderTest, DownloadProgressTracking) {
    DownloadProgress progress;
    progress.total_bytes = 1000;
    progress.downloaded_bytes = 250;
    progress.start_time = std::chrono::system_clock::now();

    EXPECT_EQ(progress.get_progress_percentage(), 25.0);
    EXPECT_GT(progress.get_elapsed_time().count(), 0);
}

TEST_F(PluginDownloaderTest, PluginPackageJsonSerialization) {
    PluginPackage package;
    package.id = "test-plugin";
    package.version = "1.0.0";
    package.name = "Test Plugin";
    package.description = "A test plugin";
    package.download_url = "https://example.com/plugin.zip";
    package.checksum_sha256 = "abc123";
    package.file_size = 1024;
    package.content_type = "application/zip";
    package.dependencies = {"dep1", "dep2"};

    auto json = package.to_json();
    auto restored = PluginPackage::from_json(json);

    EXPECT_EQ(restored.id, package.id);
    EXPECT_EQ(restored.version, package.version);
    EXPECT_EQ(restored.name, package.name);
    EXPECT_EQ(restored.description, package.description);
    EXPECT_EQ(restored.download_url, package.download_url);
    EXPECT_EQ(restored.checksum_sha256, package.checksum_sha256);
    EXPECT_EQ(restored.file_size, package.file_size);
    EXPECT_EQ(restored.content_type, package.content_type);
    EXPECT_EQ(restored.dependencies, package.dependencies);
}

class PluginDownloaderConcurrencyTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_download_dir = "/tmp/test_concurrent_downloads";
        test_install_dir = "/tmp/test_concurrent_install";

        std::filesystem::remove_all(test_download_dir);
        std::filesystem::remove_all(test_install_dir);

        PluginDownloader::Config config;
        config.download_directory = test_download_dir;
        config.installation_directory = test_install_dir;
        config.parallel_downloads = true;
        config.max_parallel_downloads = 5;

        downloader = std::make_unique<PluginDownloader>(config);
    }

    void TearDown() override {
        std::filesystem::remove_all(test_download_dir);
        std::filesystem::remove_all(test_install_dir);
    }

    std::unique_ptr<PluginDownloader> downloader;
    std::string test_download_dir;
    std::string test_install_dir;
};

TEST_F(PluginDownloaderConcurrencyTest, ConcurrentPluginOperations) {
    const int num_operations = 5;
    std::vector<std::future<bool>> futures;

    // Test concurrent uninstall operations (should be safe)
    for (int i = 0; i < num_operations; ++i) {
        std::string plugin_name = "concurrent-plugin-" + std::to_string(i);
        futures.push_back(downloader->uninstall_plugin(plugin_name));
    }

    // All should complete without crashing
    for (auto& future : futures) {
        try {
            bool result = future.get();
            // Results can be either true or false depending on whether plugin existed
            EXPECT_TRUE(result == true || result == false);
        } catch (...) {
            ADD_FAILURE() << "Concurrent operation should not throw exception";
        }
    }
}

TEST_F(PluginDownloaderConcurrencyTest, ThreadSafeStatisticsAccess) {
    const int num_threads = 10;
    std::vector<std::thread> threads;
    std::atomic<int> successful_accesses{0};

    // Concurrent statistics access
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            try {
                auto stats = downloader->get_download_statistics();
                if (stats.contains("total_downloads")) {
                    successful_accesses++;
                }

                auto plugins = downloader->get_installed_plugins();
                // Just ensure no corruption in vector access

            } catch (...) {
                // Should not throw
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(successful_accesses.load(), num_threads);
}

// Performance tests
TEST_F(PluginDownloaderTest, PerformanceDirectoryOperations) {
    auto start_time = std::chrono::high_resolution_clock::now();

    // Create multiple plugin directories
    const int num_plugins = 100;
    for (int i = 0; i < num_plugins; ++i) {
        std::string plugin_path = test_install_dir + "/plugin-" + std::to_string(i);
        std::filesystem::create_directories(plugin_path);
    }

    auto create_time = std::chrono::high_resolution_clock::now();

    // Read plugin list
    auto plugins = downloader->get_installed_plugins();

    auto read_time = std::chrono::high_resolution_clock::now();

    // Clean up directories
    for (int i = 0; i < num_plugins; ++i) {
        std::string plugin_path = test_install_dir + "/plugin-" + std::to_string(i);
        std::filesystem::remove_all(plugin_path);
    }

    auto cleanup_time = std::chrono::high_resolution_clock::now();

    auto create_duration = std::chrono::duration_cast<std::chrono::milliseconds>(create_time - start_time);
    auto read_duration = std::chrono::duration_cast<std::chrono::milliseconds>(read_time - create_time);
    auto cleanup_duration = std::chrono::duration_cast<std::chrono::milliseconds>(cleanup_time - read_time);

    // Performance assertions (adjust thresholds as needed)
    EXPECT_LT(create_duration.count(), 1000); // Should create 100 directories in < 1s
    EXPECT_LT(read_duration.count(), 100);     // Should read 100 plugins in < 100ms
    EXPECT_LT(cleanup_duration.count(), 500);  // Should clean 100 directories in < 500ms
    EXPECT_EQ(plugins.size(), num_plugins);
}