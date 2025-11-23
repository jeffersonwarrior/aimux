#pragma once

#include <string>
#include <vector>
#include <memory>
#include <future>
#include <optional>
#include <mutex>
#include <chrono>
#include <functional>
#include <atomic>
#include <unordered_map>

#include "aimux/distribution/github_registry.hpp"
#include <nlohmann/json.hpp>

namespace aimux {
namespace distribution {

/**
 * @brief Download progress information
 */
struct DownloadProgress {
    size_t total_bytes = 0;
    size_t downloaded_bytes = 0;
    double speed_bps = 0.0;
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point estimated_completion;
    std::string current_operation;

    double get_progress_percentage() const {
        return total_bytes > 0 ? (static_cast<double>(downloaded_bytes) / total_bytes) * 100.0 : 0.0;
    }

    std::chrono::seconds get_elapsed_time() const {
        return std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now() - start_time);
    }

    std::chrono::seconds get_remaining_time() const {
        if (speed_bps <= 0) return std::chrono::seconds::max();
        size_t remaining_bytes = total_bytes > downloaded_bytes ? total_bytes - downloaded_bytes : 0;
        return std::chrono::seconds(static_cast<size_t>(remaining_bytes / speed_bps));
    }
};

/**
 * @brief HTTP client interface for downloads
 */
class HttpClient {
public:
    virtual ~HttpClient() = default;

    struct Response {
        int status_code = 0;
        std::string body;
        std::unordered_map<std::string, std::string> headers;
        bool success = false;
    };

    virtual std::future<Response> get(const std::string& url,
                                    const std::unordered_map<std::string, std::string>& headers = {}) = 0;
    virtual std::future<bool> download_file(const std::string& url,
                                          const std::string& destination,
                                          std::function<void(const DownloadProgress&)> progress_callback = nullptr) = 0;
    virtual std::future<bool> resume_download(const std::string& url,
                                            const std::string& destination,
                                            size_t resume_from,
                                            std::function<void(const DownloadProgress&)> progress_callback = nullptr) = 0;
    virtual bool supports_resume() const = 0;
    virtual void set_timeout(std::chrono::seconds timeout) = 0;
    virtual void set_max_retries(int retries) = 0;
};

/**
 * @brief Default HTTP client implementation
 */
class DefaultHttpClient : public HttpClient {
public:
    DefaultHttpClient();
    ~DefaultHttpClient() override = default;

    std::future<Response> get(const std::string& url,
                            const std::unordered_map<std::string, std::string>& headers = {}) override;
    std::future<bool> download_file(const std::string& url,
                                  const std::string& destination,
                                  std::function<void(const DownloadProgress&)> progress_callback = nullptr) override;
    std::future<bool> resume_download(const std::string& url,
                                    const std::string& destination,
                                    size_t resume_from,
                                    std::function<void(const DownloadProgress&)> progress_callback = nullptr) override;
    bool supports_resume() const override { return true; }
    void set_timeout(std::chrono::seconds timeout) override;
    void set_max_retries(int retries) override;

    std::chrono::seconds timeout_ = std::chrono::seconds(30);
    int max_retries_ = 3;
    std::mutex client_mutex_;
};

/**
 * @brief Plugin package information
 */
struct PluginPackage {
    std::string id;
    std::string version;
    std::string name;
    std::string description;
    std::string download_url;
    std::string checksum_sha256;
    size_t file_size = 0;
    std::string content_type;

    // Verification
    std::string signature_url;
    std::vector<std::string> certificates;

    // Dependencies
    std::vector<std::string> dependencies;
    std::string minimum_aimux_version;

    nlohmann::json to_json() const;
    static PluginPackage from_json(const nlohmann::json& j);
    bool is_valid() const;
};

/**
 * @brief Installation result information
 */
struct InstallationResult {
    bool installation_success = false;
    std::string plugin_id;
    std::string version;
    std::string installed_path;
    std::string error_message;
    std::vector<std::string> warnings;
    std::chrono::system_clock::time_point installation_time;

    // Dependencies installed alongside
    std::vector<std::pair<std::string, std::string>> installed_dependencies;

    // Rollback information
    std::string backup_path;
    bool can_rollback = false;

    static InstallationResult success(const std::string& plugin_id, const std::string& version);
    static InstallationResult failure(const std::string& plugin_id, const std::string& error);
};

/**
 * @brief Plugin downloader and updater with network resilience
 */
class PluginDownloader {
public:
    struct Config {
        std::string download_directory;
        std::string installation_directory;
        std::string backup_directory;
        std::chrono::seconds download_timeout = std::chrono::seconds(300); // 5 minutes
        std::chrono::seconds connection_timeout = std::chrono::seconds(30);
        int max_retries = 3;
        bool enable_resuming = true;
        bool verify_checksums = true;
        bool verify_signatures = false; // Optional for now
        bool parallel_downloads = true;
        int max_parallel_downloads = 3;
        std::chrono::hours cache_ttl = std::chrono::hours(24);
        bool enable_offline_mode = false;

        Config() : download_directory("~/.config/aimux/downloads"),
                   installation_directory("~/.config/aimux/plugins"),
                   backup_directory("~/.config/aimux/backups") {}
    };

    explicit PluginDownloader(const Config& config = Config());
    ~PluginDownloader();

    // Core download operations
    std::future<InstallationResult> install_plugin(const PluginPackage& package,
                                                  std::function<void(const DownloadProgress&)> progress_callback = nullptr);
    std::future<InstallationResult> install_plugin(const std::string& plugin_id,
                                                  const std::string& version = "latest",
                                                  std::function<void(const DownloadProgress&)> progress_callback = nullptr);

    std::future<bool> uninstall_plugin(const std::string& plugin_id, bool keep_config = false);
    std::future<InstallationResult> update_plugin(const std::string& plugin_id,
                                                 const std::string& target_version = "latest",
                                                 std::function<void(const DownloadProgress&)> progress_callback = nullptr);

    std::future<bool> rollback_plugin(const std::string& plugin_id);

    // Batch operations
    std::future<std::vector<InstallationResult>> install_plugins(const std::vector<PluginPackage>& packages);
    std::future<std::vector<InstallationResult>> update_all_plugins();
    std::future<bool> backup_all_plugins();
    std::future<bool> restore_from_backup(const std::string& backup_path);

    // Download management
    std::future<bool> pause_download(const std::string& plugin_id);
    std::future<bool> resume_download(const std::string& plugin_id);
    std::future<bool> cancel_download(const std::string& plugin_id);

    std::vector<std::string> get_active_downloads() const;
    DownloadProgress get_download_progress(const std::string& plugin_id) const;

    // Network resilience features
    std::future<bool> test_connectivity();
    std::future<bool> download_for_offline_use(const std::vector<PluginPackage>& packages);
    std::future<std::vector<PluginPackage>> get_available_offline_packages();

    // Verification and security
    std::future<bool> verify_package_integrity(const PluginPackage& package, const std::string& file_path);
    std::future<bool> verify_plugin_signature(const PluginPackage& package, const std::string& file_path);
    std::future<bool> scan_for_malware(const std::string& file_path);

    // Utility methods
    std::string get_plugin_path(const std::string& plugin_id) const;
    std::optional<PluginPackage> get_installed_plugin_info(const std::string& plugin_id) const;
    std::vector<std::pair<std::string, std::string>> get_installed_plugins() const;

    // Configuration and state
    void set_http_client(std::unique_ptr<HttpClient> client);
    void set_github_registry(std::shared_ptr<GitHubRegistry> registry);

    bool is_plugin_installed(const std::string& plugin_id) const;
    std::vector<std::string> get_plugin_dependencies(const std::string& plugin_id) const;

    // Cache and cleanup
    std::future<bool> cleanup_downloads();
    std::future<bool> cleanup_cache();
    std::future<bool> repair_installation(const std::string& plugin_id);

    // Statistics
    nlohmann::json get_download_statistics() const;
    std::chrono::system_clock::time_point get_last_update_time(const std::string& plugin_id) const;

private:
    Config config_;
    std::unique_ptr<HttpClient> http_client_;
    std::shared_ptr<GitHubRegistry> github_registry_;
    mutable std::mutex downloader_mutex_;

    // Active downloads tracking
    struct ActiveDownload {
        std::string plugin_id;
        DownloadProgress progress;
        std::atomic<bool> is_paused{false};
        std::atomic<bool> is_cancelled{false};
        std::function<void(const DownloadProgress&)> progress_callback;
    };

    std::unordered_map<std::string, std::unique_ptr<ActiveDownload>> active_downloads_;

    // Internal helper methods
    std::future<PluginPackage> resolve_plugin_package(const std::string& plugin_id, const std::string& version);
    std::future<bool> download_package_file(const std::string& url, const std::string& destination,
                                           const std::string& plugin_id,
                                           std::function<void(const DownloadProgress&)> progress_callback);

    std::future<bool> extract_package(const std::string& package_path, const std::string& destination);
    std::future<bool> validate_plugin_structure(const std::string& plugin_path);
    std::future<bool> create_backup(const std::string& plugin_id);
    std::future<bool> restore_from_backup_internal(const std::string& plugin_id);

    // Checksum and verification
    std::string calculate_file_checksum(const std::string& file_path) const;
    bool verify_checksum(const std::string& file_path, const std::string& expected_checksum) const;

    // File system helpers
    std::string expand_path(const std::string& path) const;
    std::future<bool> create_directories(const std::string& path);
    std::future<bool> remove_directory(const std::string& path);
    bool directory_exists(const std::string& path) const;

    // Progress tracking
    void update_progress(const std::string& plugin_id, size_t downloaded_bytes, size_t total_bytes);
    void notify_progress_callback(const std::string& plugin_id, const DownloadProgress& progress);

    // Network resilience
    std::future<bool> wait_for_connectivity(std::chrono::seconds timeout = std::chrono::seconds(60));
    std::future<bool> download_with_retry(const std::string& url, const std::string& destination,
                                         const std::string& plugin_id, int max_attempts = 3);

    // Dependency resolution
    std::future<std::vector<PluginPackage>> resolve_dependencies(const PluginPackage& package);
    std::future<bool> install_dependencies(const std::vector<PluginPackage>& dependencies,
                                         std::function<void(const DownloadProgress&)> progress_callback);

    // Version management
    std::future<std::vector<std::string>> get_installed_versions(const std::string& plugin_id);
    std::future<bool> remove_previous_versions(const std::string& plugin_id, const std::string& keep_version);

    // Configuration
    nlohmann::json load_plugin_config(const std::string& plugin_id) const;
    std::future<bool> save_plugin_config(const std::string& plugin_id, const nlohmann::json& config);

    // Error handling
    std::string format_error_message(const std::string& operation, const std::string& details) const;
    void log_operation(const std::string& operation, const std::string& details) const;

    // Constants
    static constexpr const char* MANIFEST_FILENAME = "aimux-plugin.json";
    static constexpr const char* PLUGIN_SUBDIR = "plugins";
    static constexpr const char* CACHE_SUBDIR = "cache";
    static constexpr const char* BACKUP_SUBDIR = "backups";
};

} // namespace distribution
} // namespace aimux