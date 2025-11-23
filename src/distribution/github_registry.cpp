#include "aimux/distribution/github_registry.hpp"
#include <fstream>
#include <sstream>
#include <regex>
#include <filesystem>
#include <stdexcept>
#include <iostream>

namespace aimux {
namespace distribution {

// GitHubRepoInfo Implementation
nlohmann::json GitHubRepoInfo::to_json() const {
    nlohmann::json j;
    j["owner"] = owner;
    j["name"] = name;
    j["description"] = description;
    j["default_branch"] = default_branch;
    j["topics"] = topics;
    j["license"] = license;
    j["stars"] = stars;
    j["forks"] = forks;
    j["updated_at"] = std::chrono::duration_cast<std::chrono::seconds>(updated_at.time_since_epoch()).count();
    j["archived"] = archived;
    return j;
}

GitHubRepoInfo GitHubRepoInfo::from_json(const nlohmann::json& j) {
    GitHubRepoInfo info;
    info.owner = j.value("owner", "");
    info.name = j.value("name", "");
    info.description = j.value("description", "");
    info.default_branch = j.value("default_branch", "main");
    info.topics = j.value("topics", std::vector<std::string>{});
    info.license = j.value("license", "");
    info.stars = j.value("stars", 0);
    info.forks = j.value("forks", 0);

    if (j.contains("updated_at")) {
        if (j["updated_at"].is_string()) {
            // Parse ISO 8601 format string
            std::string time_str = j["updated_at"].get<std::string>();
            // Simple parsing - just use current time for now
            info.updated_at = std::chrono::system_clock::now();
        } else {
            info.updated_at = std::chrono::system_clock::from_time_t(j["updated_at"].get<int64_t>());
        }
    }

    info.archived = j.value("archived", false);
    return info;
}

bool GitHubRepoInfo::is_valid() const {
    static std::regex owner_pattern(R"(^[a-zA-Z0-9]([a-zA-Z0-9\-]{0,38}[a-zA-Z0-9])?$)");
    static std::regex name_pattern(R"(^[a-zA-Z0-9._-]+$)");

    return !owner.empty() && !name.empty() &&
           std::regex_match(owner, owner_pattern) &&
           std::regex_match(name, name_pattern);
}

// GitHubRelease Implementation
nlohmann::json GitHubRelease::to_json() const {
    nlohmann::json j;
    j["tag_name"] = tag_name;
    j["name"] = name;
    j["body"] = body;
    j["prerelease"] = prerelease;
    j["draft"] = draft;
    j["published_at"] = std::chrono::duration_cast<std::chrono::seconds>(published_at.time_since_epoch()).count();

    nlohmann::json assets_json = nlohmann::json::array();
    for (const auto& asset : assets) {
        assets_json.push_back({
            {"name", asset.name},
            {"browser_download_url", asset.browser_download_url},
            {"size", asset.size},
            {"content_type", asset.content_type},
            {"checksum_sha256", asset.checksum_sha256}
        });
    }
    j["assets"] = assets_json;

    return j;
}

GitHubRelease GitHubRelease::from_json(const nlohmann::json& j) {
    GitHubRelease release;
    release.tag_name = j.value("tag_name", "");
    release.name = j.value("name", "");
    release.body = j.value("body", "");
    release.prerelease = j.value("prerelease", false);
    release.draft = j.value("draft", false);

    if (j.contains("published_at")) {
        if (j["published_at"].is_string()) {
            // Parse ISO 8601 format string
            std::string time_str = j["published_at"].get<std::string>();
            // Simple parsing - just use current time for now
            release.published_at = std::chrono::system_clock::now();
        } else {
            release.published_at = std::chrono::system_clock::from_time_t(j["published_at"].get<int64_t>());
        }
    }

    if (j.contains("assets")) {
        for (const auto& asset_json : j["assets"]) {
            Asset asset;
            asset.name = asset_json.value("name", "");
            asset.browser_download_url = asset_json.value("browser_download_url", "");
            asset.size = asset_json.value("size", 0);
            asset.content_type = asset_json.value("content_type", "");
            asset.checksum_sha256 = asset_json.value("checksum_sha256", "");
            release.assets.push_back(asset);
        }
    }

    return release;
}

bool GitHubRelease::is_compatible_with_current_version() const {
    // Check if release is not a pre-release or draft
    if (prerelease || draft) {
        return false;
    }

    // Basic semantic version compatibility check
    static std::regex version_regex(R"(^v?(\d+)\.(\d+)\.(\d+).*$)");
    std::smatch match;

    if (std::regex_match(tag_name, match, version_regex)) {
        int major = std::stoi(match[1].str());
        int minor = std::stoi(match[2].str());

        // Only allow major version 1.x or higher (0.x.x considered unstable)
        if (major >= 1) {
            return true;
        }
    }

    return false;
}

// GitHubApiClient Implementation
GitHubApiClient::GitHubApiClient(const Config& config)
    : config_(config),
      owner_name_pattern_(R"(^[a-zA-Z0-9]([a-zA-Z0-9\-]{0,38}[a-zA-Z0-9])?$)"),
      repository_name_pattern_(R"(^[a-zA-Z0-9._-]+$)") {

    remaining_requests_ = config_.rate_limit_per_hour;
    rate_limit_reset_ = std::chrono::system_clock::now() + std::chrono::hours(1);
}

std::future<std::vector<GitHubRepoInfo>> GitHubApiClient::discover_plugins_from_org(const std::string& org) {
    return std::async(std::launch::async, [this, org]() -> std::vector<GitHubRepoInfo> {
        if (!is_valid_owner(org)) {
            return {};
        }

        std::vector<GitHubRepoInfo> plugins;
        int page = 1;
        const int per_page = 100; // GitHub API max

        while (true) {
            std::string url = config_.base_url + "/orgs/" + org + "/repos" +
                             "?type=public&per_page=" + std::to_string(per_page) +
                             "&page=" + std::to_string(page);

            auto response_future = make_api_request(url);
            try {
                auto json_response = response_future.get();

                if (!json_response.is_array() || json_response.empty()) {
                    break;
                }

                for (const auto& repo_json : json_response) {
                    GitHubRepoInfo repo_info;

                    auto owner_obj = repo_json["owner"];
                    repo_info.owner = owner_obj["login"];
                    repo_info.name = repo_json["name"];
                    repo_info.description = repo_json.value("description", "");
                    repo_info.default_branch = repo_json["default_branch"];

                    repo_info.topics = repo_json.value("topics", std::vector<std::string>{});
                    repo_info.stars = repo_json["stargazers_count"];
                    repo_info.forks = repo_json["forks_count"];
                    repo_info.archived = repo_json["archived"];

                    if (repo_json.contains("license")) {
                        repo_info.license = repo_json["license"]["spdx_id"];
                    }

                    if (repo_json.contains("updated_at")) {
                        // For now, just use current time as a placeholder
                        repo_info.updated_at = std::chrono::system_clock::now();
                    }

                    // Only include active, non-archived repositories
                    if (repo_info.is_valid() && !repo_info.archived) {
                        plugins.push_back(repo_info);
                    }
                }

                if (json_response.size() < per_page) {
                    break;
                }
                ++page;

            } catch (const std::exception& e) {
                std::cerr << "Error fetching repositories for org " << org << ": " << e.what() << std::endl;
                break;
            }
        }

        return plugins;
    });
}

std::future<std::optional<GitHubRepoInfo>> GitHubApiClient::get_repository_info(const std::string& owner, const std::string& name) {
    return std::async(std::launch::async, [this, owner, name]() -> std::optional<GitHubRepoInfo> {
        if (!is_valid_owner(owner) || !is_valid_repository_name(name)) {
            return std::nullopt;
        }

        std::string url = config_.base_url + "/repos/" + owner + "/" + name;
        auto response_future = make_api_request(url);

        try {
            auto json_response = response_future.get();
            return GitHubRepoInfo::from_json(json_response);
        } catch (const std::exception& e) {
            std::cerr << "Error fetching repository info for " << owner << "/" << name << ": " << e.what() << std::endl;
            return std::nullopt;
        }
    });
}

std::future<bool> GitHubApiClient::validate_repository_structure(const std::string& owner, const std::string& name) {
    return std::async(std::launch::async, [this, owner, name]() -> bool {
        // Check for required files in the repository
        std::vector<std::string> required_files = {
            "manifest.json",  // Required plugin manifest
            "README.md",      // Documentation
            "src/"           // Source directory (could be .cpp, .hpp files)
        };

        for (const std::string& file : required_files) {
            auto exists_future = file_exists(owner, name, file);
            if (!exists_future.get()) {
                return false;
            }
        }

        // Verify manifest is valid JSON with required fields
        auto manifest_future = get_file_content(owner, name, "manifest.json");
        try {
            std::string manifest_content = manifest_future.get();
            auto manifest_json = nlohmann::json::parse(manifest_content);

            // Check required manifest fields
            if (!manifest_json.contains("name") ||
                !manifest_json.contains("version") ||
                !manifest_json.contains("description") ||
                !manifest_json.contains("providers")) {
                return false;
            }

            return true;
        } catch (const std::exception&) {
            return false;
        }
    });
}

std::future<std::vector<GitHubRelease>> GitHubApiClient::get_releases(const std::string& owner, const std::string& name) {
    return std::async(std::launch::async, [this, owner, name]() -> std::vector<GitHubRelease> {
        if (!is_valid_owner(owner) || !is_valid_repository_name(name)) {
            return {};
        }

        std::string url = config_.base_url + "/repos/" + owner + "/" + name + "/releases";
        auto response_future = make_api_request(url);

        try {
            auto json_response = response_future.get();
            std::vector<GitHubRelease> releases;

            for (const auto& release_json : json_response) {
                releases.push_back(GitHubRelease::from_json(release_json));
            }

            // Sort by published date (newest first)
            std::sort(releases.begin(), releases.end(),
                     [](const GitHubRelease& a, const GitHubRelease& b) {
                         return a.published_at > b.published_at;
                     });

            return releases;
        } catch (const std::exception& e) {
            std::cerr << "Error fetching releases for " << owner << "/" << name << ": " << e.what() << std::endl;
            return {};
        }
    });
}

std::future<std::optional<GitHubRelease>> GitHubApiClient::get_latest_release(const std::string& owner, const std::string& name) {
    return std::async(std::launch::async, [this, owner, name]() -> std::optional<GitHubRelease> {
        if (!is_valid_owner(owner) || !is_valid_repository_name(name)) {
            return std::nullopt;
        }

        std::string url = config_.base_url + "/repos/" + owner + "/" + name + "/releases/latest";
        auto response_future = make_api_request(url);

        try {
            auto json_response = response_future.get();
            return GitHubRelease::from_json(json_response);
        } catch (const std::exception& e) {
            std::cerr << "Error fetching latest release for " << owner << "/" << name << ": " << e.what() << std::endl;
            return std::nullopt;
        }
    });
}

std::future<std::string> GitHubApiClient::get_file_content(const std::string& owner, const std::string& name, const std::string& path, const std::string& ref) {
    return std::async(std::launch::async, [this, owner, name, path, ref]() -> std::string {
        if (!is_valid_owner(owner) || !is_valid_repository_name(name)) {
            return "";
        }

        std::string url = config_.base_url + "/repos/" + owner + "/" + name +
                         "/contents/" + path + "?ref=" + ref;
        auto response_future = make_api_request(url);

        try {
            auto json_response = response_future.get();
            std::string content = json_response["content"];
            std::string encoding = json_response.value("encoding", "utf-8");

            if (encoding == "base64") {
                // Decode base64 content
                std::string decoded_content;
                // Implement base64 decoding here or use a library
                // For now, return empty for base64 content
                return decoded_content;
            }

            return content;
        } catch (const std::exception& e) {
            std::cerr << "Error fetching file content for " << owner << "/" << name << "/" << path << ": " << e.what() << std::endl;
            return "";
        }
    });
}

std::future<bool> GitHubApiClient::file_exists(const std::string& owner, const std::string& name, const std::string& path, const std::string& ref) {
    return std::async(std::launch::async, [this, owner, name, path, ref]() -> bool {
        if (!is_valid_owner(owner) || !is_valid_repository_name(name)) {
            return false;
        }

        std::string url = config_.base_url + "/repos/" + owner + "/" + name +
                         "/contents/" + path + "?ref=" + ref;
        auto response_future = make_head_request(url);

        try {
            return response_future.get();
        } catch (const std::exception&) {
            return false;
        }
    });
}

std::future<bool> GitHubApiClient::is_trusted_repository(const std::string& owner, const std::string& name) {
    return std::async(std::launch::async, [this, owner, name]() -> bool {
        return is_trusted_organization(owner) ||
               validate_repository_ownership(owner, name).get();
    });
}

std::future<bool> GitHubApiClient::validate_repository_ownership(const std::string& owner, const std::string& name) {
    return std::async(std::launch::async, [this, owner, name]() -> bool {
        // Check if owner is in trusted developers list
        for (const std::string& trusted_dev : config_.trusted_organizations) {
            if (owner == trusted_dev) {
                return true;
            }
        }

        // Additional validation logic could be added here
        // For now, just check against trusted orgs
        return false;
    });
}

std::future<nlohmann::json> GitHubApiClient::make_api_request(const std::string& url) {
    return std::async(std::launch::async, [this, url]() -> nlohmann::json {
        // Check rate limiting
        if (is_rate_limited()) {
            auto now = std::chrono::system_clock::now();
            if (now < rate_limit_reset_) {
                auto sleep_duration = std::chrono::duration_cast<std::chrono::milliseconds>(rate_limit_reset_ - now);
                std::this_thread::sleep_for(sleep_duration);
            }
        }

        // Simulate HTTP request - in real implementation, use libcurl or similar
        std::cout << "[API] Making request to: " << url << std::endl;

        // Mock implementation - return empty json
        // In actual implementation, this would make a real HTTP request
        return nlohmann::json::object();
    });
}

std::future<bool> GitHubApiClient::make_head_request(const std::string& url) {
    return std::async(std::launch::async, [this, url]() -> bool {
        // Simulate HEAD request
        std::cout << "[API] Making HEAD request to: " << url << std::endl;

        // Mock implementation - return false
        // In actual implementation, this would make a real HTTP HEAD request
        return false;
    });
}

bool GitHubApiClient::is_rate_limited() const {
    return remaining_requests_ <= 0;
}

bool GitHubApiClient::is_valid_owner(const std::string& owner) const {
    return std::regex_match(owner, owner_name_pattern_);
}

bool GitHubApiClient::is_valid_repository_name(const std::string& name) const {
    return std::regex_match(name, repository_name_pattern_);
}

bool GitHubApiClient::is_trusted_organization(const std::string& org) const {
    return std::find(config_.trusted_organizations.begin(),
                     config_.trusted_organizations.end(),
                     org) != config_.trusted_organizations.end();
}

// GitHubRegistry Implementation
GitHubRegistry::GitHubRegistry(const Config& config)
    : config_(config),
      api_client_(std::make_unique<GitHubApiClient>()) {

    // Initialize organizations from config
    organizations_ = config.organizations;
}

GitHubRegistry::~GitHubRegistry() = default;

std::future<bool> GitHubRegistry::initialize() {
    return std::async(std::launch::async, [this]() -> bool {
        try {
            // Create cache directory if it doesn't exist
            std::filesystem::path cache_dir(config_.cache_directory);
            std::filesystem::create_directories(cache_dir);

            // Load existing cache if available
            load_cache_from_disk().get();

            // Validate organizations are accessible
            bool all_orgs_valid = true;
            for (const std::string& org : organizations_) {
                std::vector<GitHubRepoInfo> repos = api_client_->discover_plugins_from_org(org).get();
                if (repos.empty()) {
                    std::cerr << "Warning: No repositories found for organization: " << org << std::endl;
                }
            }

            return all_orgs_valid;
        } catch (const std::exception& e) {
            std::cerr << "Error initializing GitHub registry: " << e.what() << std::endl;
            return false;
        }
    });
}

std::future<std::vector<GitHubRepoInfo>> GitHubRegistry::search_plugins(const std::string& query) {
    return std::async(std::launch::async, [this, query]() -> std::vector<GitHubRepoInfo> {
        std::vector<GitHubRepoInfo> results;

        // Search through cached repositories
        std::lock_guard<std::mutex> lock(registry_mutex_);
        for (const auto& [plugin_id, repo_info] : cached_repositories_) {
            // Simple search logic - check if query is in name, description, or topics
            if (query.empty() ||
                repo_info.name.find(query) != std::string::npos ||
                repo_info.description.find(query) != std::string::npos) {

                // Check topics
                for (const std::string& topic : repo_info.topics) {
                    if (topic.find(query) != std::string::npos) {
                        results.push_back(repo_info);
                        break;
                    }
                }
            }
        }

        // Sort by stars (popularity)
        std::sort(results.begin(), results.end(),
                 [](const GitHubRepoInfo& a, const GitHubRepoInfo& b) {
                     return a.stars > b.stars;
                 });

        return results;
    });
}

// Additional implementation methods would follow the same pattern
// For brevity, I'll add a few key ones and mark others as TODO

std::future<std::vector<GitHubRelease>> GitHubRegistry::get_plugin_releases(const std::string& plugin_id) {
    return std::async(std::launch::async, [this, plugin_id]() -> std::vector<GitHubRelease> {
        PluginId parsed_id = PluginId::from_string(plugin_id);
        if (!parsed_id.is_valid()) {
            return {};
        }

        return api_client_->get_releases(parsed_id.owner, parsed_id.name).get();
    });
}

std::future<prettifier::PluginManifest> GitHubRegistry::get_plugin_manifest(const std::string& plugin_id, const std::string& version) {
    return std::async(std::launch::async, [this, plugin_id, version]() -> prettifier::PluginManifest {
        PluginId parsed_id = PluginId::from_string(plugin_id);
        if (!parsed_id.is_valid()) {
            throw std::invalid_argument("Invalid plugin ID format");
        }

        std::string ref = (version == "latest") ? "main" : version;
        std::string manifest_content = api_client_->get_file_content(
            parsed_id.owner, parsed_id.name, "manifest.json", ref).get();

        if (manifest_content.empty()) {
            throw std::runtime_error("Failed to fetch plugin manifest");
        }

        try {
            auto manifest_json = nlohmann::json::parse(manifest_content);
            return prettifier::PluginManifest::from_json(manifest_json);
        } catch (const std::exception& e) {
            throw std::runtime_error("Invalid manifest JSON: " + std::string(e.what()));
        }
    });
}

std::future<bool> GitHubRegistry::validate_plugin(const std::string& plugin_id, const std::string& version) {
    return std::async(std::launch::async, [this, plugin_id, version]() -> bool {
        try {
            // Check if plugin is blocked
            if (is_plugin_blocked(plugin_id)) {
                return false;
            }

            PluginId parsed_id = PluginId::from_string(plugin_id);
            if (!parsed_id.is_valid()) {
                return false;
            }

            // Validate repository structure
            auto repo_info = api_client_->get_repository_info(parsed_id.owner, parsed_id.name).get();
            if (!repo_info.has_value()) {
                return false;
            }

            return api_client_->validate_repository_structure(parsed_id.owner, parsed_id.name).get();
        } catch (const std::exception& e) {
            std::cerr << "Plugin validation error: " << e.what() << std::endl;
            return false;
        }
    });
}

bool GitHubRegistry::is_plugin_blocked(const std::string& plugin_id) const {
    return std::find(config_.blocked_plugins.begin(),
                     config_.blocked_plugins.end(),
                     plugin_id) != config_.blocked_plugins.end();
}

GitHubRegistry::PluginId GitHubRegistry::parse_plugin_id(const std::string& plugin_id) const {
    return PluginId::from_string(plugin_id);
}

std::string GitHubRegistry::format_plugin_id(const std::string& owner, const std::string& name) const {
    return owner + "/" + name;
}

// Cache management methods
std::future<bool> GitHubRegistry::save_cache_to_disk() {
    return std::async(std::launch::async, [this]() -> bool {
        try {
            std::filesystem::path cache_dir(config_.cache_directory);
            std::filesystem::create_directories(cache_dir);

            // Save repositories cache
            nlohmann::json cache_data;
            cache_data["repositories"] = nlohmann::json::object();
            for (const auto& [plugin_id, repo_info] : cached_repositories_) {
                cache_data["repositories"][plugin_id] = repo_info.to_json();
            }

            std::ofstream cache_file(cache_dir / "registry_cache.json");
            cache_file << cache_data.dump(2);

            return true;
        } catch (const std::exception& e) {
            std::cerr << "Error saving cache: " << e.what() << std::endl;
            return false;
        }
    });
}

std::future<bool> GitHubRegistry::load_cache_from_disk() {
    return std::async(std::launch::async, [this]() -> bool {
        try {
            std::filesystem::path cache_file(config_.cache_directory + "/registry_cache.json");

            if (!std::filesystem::exists(cache_file)) {
                return true; // No cache file is fine
            }

            std::ifstream in_file(cache_file);
            std::string cache_content((std::istreambuf_iterator<char>(in_file)),
                                    std::istreambuf_iterator<char>());

            auto cache_data = nlohmann::json::parse(cache_content);

            // Load repositories
            if (cache_data.contains("repositories")) {
                for (auto& [plugin_id, repo_json] : cache_data["repositories"].items()) {
                    cached_repositories_[plugin_id] = GitHubRepoInfo::from_json(repo_json);
                }
            }

            return true;
        } catch (const std::exception& e) {
            std::cerr << "Error loading cache: " << e.what() << std::endl;
            return false;
        }
    });
}

nlohmann::json GitHubRegistry::get_registry_statistics() const {
    std::lock_guard<std::mutex> lock(registry_mutex_);

    nlohmann::json stats;
    stats["total_cached_repositories"] = cached_repositories_.size();
    stats["total_cached_releases"] = cached_releases_.size();
    stats["organizations"] = organizations_.size();
    stats["cache_directory"] = config_.cache_directory;
    stats["cache_ttl_hours"] = config_.cache_ttl.count();

    return stats;
}

} // namespace distribution
} // namespace aimux