#pragma once

#include <string>
#include <vector>
#include <memory>
#include <future>
#include <optional>
#include <regex>
#include <mutex>
#include <chrono>
#include <unordered_map>

#include "aimux/prettifier/plugin_registry.hpp"
#include <nlohmann/json.hpp>

namespace aimux {
namespace distribution {

/**
 * @brief GitHub repository information for plugin packages
 */
struct GitHubRepoInfo {
    std::string owner;                // Repository owner (e.g., "aimux-org")
    std::string name;                 // Repository name (e.g., "markdown-prettifier")
    std::string description;          // Repository description
    std::string default_branch;      // Default branch (main/master)
    std::vector<std::string> topics;  // GitHub topics/tags
    std::string license;             // License identifier
    int stars = 0;                   // Star count
    int forks = 0;                   // Fork count
    std::chrono::system_clock::time_point updated_at;  // Last update
    bool archived = false;           // Repository status

    nlohmann::json to_json() const;
    static GitHubRepoInfo from_json(const nlohmann::json& j);
    bool is_valid() const;
};

/**
 * @brief Plugin release information from GitHub
 */
struct GitHubRelease {
    std::string tag_name;             // Version tag (e.g., "v1.2.0")
    std::string name;                 // Release name
    std::string body;                 // Release notes/description
    bool prerelease = false;          // Pre-release flag
    bool draft = false;               // Draft release flag
    std::chrono::system_clock::time_point published_at;  // Publish date

    struct Asset {
        std::string name;             // Asset filename
        std::string browser_download_url;  // Download URL
        size_t size = 0;             // File size in bytes
        std::string content_type;    // MIME type
        std::string checksum_sha256; // SHA256 checksum
    };

    std::vector<Asset> assets;

    nlohmann::json to_json() const;
    static GitHubRelease from_json(const nlohmann::json& j);
    bool is_compatible_with_current_version() const;
};

/**
 * @brief GitHub API client for plugin registry operations
 */
class GitHubApiClient {
public:
    struct Config {
        std::string base_url;
        std::string api_token;                       // GitHub personal access token
        std::string user_agent;                      // User agent header
        int timeout_seconds;                        // Request timeout
        int rate_limit_per_hour;                    // GitHub rate limit
        std::vector<std::string> trusted_organizations;

        Config() : base_url("https://api.github.com"),
                   user_agent("aimux/2.0.0"),
                   timeout_seconds(30),
                   rate_limit_per_hour(5000),
                   trusted_organizations({"aimux-org", "aimux", "aimux-plugins", "awesome-aimux"}) {}
    };

    explicit GitHubApiClient(const Config& config = Config());
    ~GitHubApiClient() = default;

    // Repository discovery
    std::future<std::vector<GitHubRepoInfo>> discover_plugins_from_org(const std::string& org);
    std::future<std::optional<GitHubRepoInfo>> get_repository_info(const std::string& owner, const std::string& name);
    std::future<bool> validate_repository_structure(const std::string& owner, const std::string& name);

    // Release operations
    std::future<std::vector<GitHubRelease>> get_releases(const std::string& owner, const std::string& name);
    std::future<std::optional<GitHubRelease>> get_latest_release(const std::string& owner, const std::string& name);
    std::future<std::optional<GitHubRelease>> get_release_by_tag(const std::string& owner, const std::string& name, const std::string& tag);

    // File operations
    std::future<std::string> get_file_content(const std::string& owner, const std::string& name, const std::string& path, const std::string& ref = "main");
    std::future<bool> file_exists(const std::string& owner, const std::string& name, const std::string& path, const std::string& ref = "main");

    // Security validation
    std::future<bool> is_trusted_repository(const std::string& owner, const std::string& name);
    std::future<bool> validate_repository_ownership(const std::string& owner, const std::string& name);
    std::future<std::vector<std::string>> scan_repository_files(const std::string& owner, const std::string& name, const std::string& ref = "main");

    // Rate limiting
    bool is_rate_limited() const;
    std::chrono::system_clock::time_point get_rate_limit_reset() const;
    int get_remaining_requests() const;

private:
    Config config_;
    std::mutex rate_limit_mutex_;
    std::chrono::system_clock::time_point rate_limit_reset_;
    int remaining_requests_ = 0;
    std::regex owner_name_pattern_;
    std::regex repository_name_pattern_;

    // HTTP helper methods
    std::future<nlohmann::json> make_api_request(const std::string& url);
    std::future<bool> make_head_request(const std::string& url);
    void update_rate_limit_from_headers(const std::unordered_map<std::string, std::string>& headers);

    // Validation helpers
    bool is_valid_owner(const std::string& owner) const;
    bool is_valid_repository_name(const std::string& name) const;
    bool is_trusted_organization(const std::string& org) const;

    // Error handling
    struct ApiError {
        int status_code;
        std::string message;
        std::string detail;
        bool is_rate_limit = false;
    };

    std::optional<ApiError> handle_api_error(int status_code, const std::string& response);
};

/**
 * @brief GitHub-based plugin registry for discovering and managing remote plugins
 */
class GitHubRegistry {
public:
    struct Config {
        std::vector<std::string> organizations;
        std::string cache_directory;
        std::chrono::hours cache_ttl;              // Cache validity period
        size_t max_cache_entries;                  // Maximum cached plugins
        bool enable_security_validation;           // Enable security checks
        bool enable_dependency_validation;         // Validate plugin dependencies
        std::string minimum_version_requirement;   // Minimum plugin version
        std::vector<std::string> blocked_plugins;  // Blocked plugin list
        std::vector<std::string> trusted_developers; // Explicitly trusted developers

        Config() : organizations({"aimux-org", "aimux-plugins"}),
                   cache_directory("~/.config/aimux/registry_cache"),
                   cache_ttl(std::chrono::hours(24)),
                   max_cache_entries(1000),
                   enable_security_validation(true),
                   enable_dependency_validation(true) {}
    };

    explicit GitHubRegistry(const Config& config = Config());
    ~GitHubRegistry();

    // Registry operations
    std::future<bool> initialize();
    std::future<bool> refresh_cache();
    std::future<bool> clear_cache();

    // Plugin discovery
    std::future<std::vector<GitHubRepoInfo>> search_plugins(const std::string& query);
    std::future<std::vector<GitHubRepoInfo>> get_popular_plugins(int limit = 50);
    std::future<std::vector<GitHubRepoInfo>> get_recently_updated_plugins(int limit = 50);
    std::future<std::vector<GitHubRepoInfo>> get_plugins_by_topic(const std::string& topic);

    // Plugin information
    std::future<std::optional<GitHubRepoInfo>> get_plugin_info(const std::string& plugin_id);
    std::future<std::vector<GitHubRelease>> get_plugin_releases(const std::string& plugin_id);
    std::future<std::optional<GitHubRelease>> get_latest_plugin_release(const std::string& plugin_id);
    std::future<prettifier::PluginManifest> get_plugin_manifest(const std::string& plugin_id, const std::string& version = "latest");

    // Plugin validation
    std::future<bool> validate_plugin(const std::string& plugin_id, const std::string& version = "latest");
    std::future<bool> is_plugin_compatible(const std::string& plugin_id, const std::string& version = "latest");
    std::future<std::vector<std::string>> get_plugin_dependencies(const std::string& plugin_id, const std::string& version = "latest");

    // Security operations
    std::future<bool> is_plugin_safe(const std::string& plugin_id);
    std::future<bool> scan_plugin_for_malware(const std::string& plugin_id, const std::string& version = "latest");
    std::future<std::vector<std::string>> get_plugin_security_report(const std::string& plugin_id);

    // Registry statistics
    nlohmann::json get_registry_statistics() const;
    bool is_initialized() const { return !organizations_.empty(); }

private:
    Config config_;
    std::unique_ptr<GitHubApiClient> api_client_;
    mutable std::mutex registry_mutex_;

    // Cached data
    std::vector<std::string> organizations_;
    std::unordered_map<std::string, GitHubRepoInfo> cached_repositories_;
    std::unordered_map<std::string, std::vector<GitHubRelease>> cached_releases_;
    std::unordered_map<std::string, std::chrono::system_clock::time_point> cache_timestamps_;

    // Validation helpers
    bool is_plugin_blocked(const std::string& plugin_id) const;
    bool is_version_compatible(const std::string& version) const;
    std::future<bool> validate_plugin_structure(const GitHubRepoInfo& repo);
    std::future<bool> validate_plugin_manifest(const prettifier::PluginManifest& manifest);
    std::future<bool> validate_plugin_security(const std::string& plugin_id);

    // Cache management
    std::filesystem::path get_cache_file_path(const std::string& key) const;
    bool is_cache_valid(const std::string& key) const;
    void update_cache_timestamp(const std::string& key);
    void cleanup_expired_cache();
    std::future<bool> save_cache_to_disk();
    std::future<bool> load_cache_from_disk();

    struct PluginId {
        std::string owner;
        std::string name;
        bool is_valid() const { return !owner.empty() && !name.empty(); }

        std::string to_string() const {
            return owner + "/" + name;
        }

        static PluginId from_string(const std::string& plugin_id) {
            auto slash_pos = plugin_id.find('/');
            if (slash_pos == std::string::npos) {
                return {};
            }
            return {
                .owner = plugin_id.substr(0, slash_pos),
                .name = plugin_id.substr(slash_pos + 1)
            };
        }
    };

    PluginId parse_plugin_id(const std::string& plugin_id) const;
    std::string format_plugin_id(const std::string& owner, const std::string& name) const;
};

} // namespace distribution
} // namespace aimux