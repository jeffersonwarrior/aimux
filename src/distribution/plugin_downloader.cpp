#include "aimux/distribution/plugin_downloader.hpp"
#include <fstream>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <vector>
#include <future>
#include <thread>
#include <algorithm>
#include <random>
#include <iomanip>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <curl/curl.h>

namespace aimux {
namespace distribution {

// PluginPackage implementation
nlohmann::json PluginPackage::to_json() const {
    nlohmann::json j;
    j["id"] = id;
    j["version"] = version;
    j["name"] = name;
    j["description"] = description;
    j["download_url"] = download_url;
    j["checksum_sha256"] = checksum_sha256;
    j["file_size"] = file_size;
    j["content_type"] = content_type;
    j["signature_url"] = signature_url;
    j["certificates"] = certificates;
    j["dependencies"] = dependencies;
    j["minimum_aimux_version"] = minimum_aimux_version;
    return j;
}

PluginPackage PluginPackage::from_json(const nlohmann::json& j) {
    PluginPackage package;
    package.id = j.value("id", "");
    package.version = j.value("version", "");
    package.name = j.value("name", "");
    package.description = j.value("description", "");
    package.download_url = j.value("download_url", "");
    package.checksum_sha256 = j.value("checksum_sha256", "");
    package.file_size = j.value("file_size", 0);
    package.content_type = j.value("content_type", "");
    package.signature_url = j.value("signature_url", "");
    package.certificates = j.value("certificates", std::vector<std::string>{});
    package.dependencies = j.value("dependencies", std::vector<std::string>{});
    package.minimum_aimux_version = j.value("minimum_aimux_version", "");
    return package;
}

bool PluginPackage::is_valid() const {
    return !id.empty() && !version.empty() && !download_url.empty() &&
           !checksum_sha256.empty() && file_size > 0;
}

// InstallationResult implementation
InstallationResult InstallationResult::success(const std::string& plugin_id, const std::string& version) {
    InstallationResult result;
    result.installation_success = true;
    result.plugin_id = plugin_id;
    result.version = version;
    result.installation_time = std::chrono::system_clock::now();
    return result;
}

InstallationResult InstallationResult::failure(const std::string& plugin_id, const std::string& error) {
    InstallationResult result;
    result.installation_success = false;
    result.plugin_id = plugin_id;
    result.error_message = error;
    result.installation_time = std::chrono::system_clock::now();
    return result;
}

// DefaultHttpClient implementation
class CurlHttpClient : public DefaultHttpClient {
public:
    CurlHttpClient() {
        curl_global_init(CURL_GLOBAL_DEFAULT);
    }

    ~CurlHttpClient() {
        curl_global_cleanup();
    }

    std::future<Response> get(const std::string& url,
                            const std::unordered_map<std::string, std::string>& headers) override {
        return std::async(std::launch::async, [this, url, headers]() -> Response {
            std::lock_guard<std::mutex> lock(client_mutex_);

            CURL* curl = curl_easy_init();
            if (!curl) {
                return {0, "", {}, false};
            }

            Response response;
            std::string response_string;

            // Set URL
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_.count());
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

            // Set headers
            struct curl_slist* header_list = nullptr;
            for (const auto& [key, value] : headers) {
                std::string header = key + ": " + value;
                header_list = curl_slist_append(header_list, header.c_str());
            }
            if (header_list) {
                curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
            }

            CURLcode res = curl_easy_perform(curl);
            if (res == CURLE_OK) {
                long response_code;
                curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
                response.status_code = static_cast<int>(response_code);
                response.body = response_string;
                response.success = (response_code >= 200 && response_code < 300);
            }

            // Clean up
            if (header_list) {
                curl_slist_free_all(header_list);
            }
            curl_easy_cleanup(curl);

            return response;
        });
    }

    std::future<bool> download_file(const std::string& url,
                                  const std::string& destination,
                                  std::function<void(const DownloadProgress&)> progress_callback) override {
        return std::async(std::launch::async, [this, url, destination, progress_callback]() -> bool {
            std::lock_guard<std::mutex> lock(client_mutex_);

            CURL* curl = curl_easy_init();
            if (!curl) {
                return false;
            }

            FILE* file = fopen(destination.c_str(), "wb");
            if (!file) {
                curl_easy_cleanup(curl);
                return false;
            }

            DownloadProgress progress;
            progress.start_time = std::chrono::system_clock::now();

            // Set up download options
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, FileWriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_.count());
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);

            if (progress_callback) {
                curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, ProgressCallback);
                curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &progress);
                curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, ProgressCallback);
                curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &progress);
            }

            CURLcode res = curl_easy_perform(curl);
            fclose(file);
            curl_easy_cleanup(curl);

            if (res != CURLE_OK) {
                std::filesystem::remove(destination);
                return false;
            }

            return true;
        });
    }

    std::future<bool> resume_download(const std::string& url,
                                    const std::string& destination,
                                    size_t resume_from,
                                    std::function<void(const DownloadProgress&)> progress_callback) override {
        return std::async(std::launch::async, [this, url, destination, resume_from, progress_callback]() -> bool {
            std::lock_guard<std::mutex> lock(client_mutex_);

            CURL* curl = curl_easy_init();
            if (!curl) {
                return false;
            }

            FILE* file = fopen(destination.c_str(), "ab");
            if (!file) {
                curl_easy_cleanup(curl);
                return false;
            }

            DownloadProgress progress;
            progress.start_time = std::chrono::system_clock::now();
            progress.downloaded_bytes = resume_from;

            // Set up resume options
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, FileWriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_.count());
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_RESUME_FROM_LARGE, static_cast<curl_off_t>(resume_from));

            if (progress_callback) {
                curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, ProgressCallback);
                curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &progress);
            }

            CURLcode res = curl_easy_perform(curl);
            fclose(file);
            curl_easy_cleanup(curl);

            if (res != CURLE_OK) {
                return false;
            }

            return true;
        });
    }

    void set_timeout(std::chrono::seconds timeout) override {
        timeout_ = timeout;
    }

    void set_max_retries(int retries) override {
        max_retries_ = retries;
    }

private:
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
        size_t total_size = size * nmemb;
        std::string* response_string = static_cast<std::string*>(userp);
        response_string->append(static_cast<char*>(contents), total_size);
        return total_size;
    }

    static size_t FileWriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
        size_t total_size = size * nmemb;
        FILE* file = static_cast<FILE*>(userp);
        return fwrite(contents, 1, total_size, file);
    }

    static int ProgressCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow,
                               curl_off_t ultotal, curl_off_t ulnow) {
        if (!clientp) return 0;

        DownloadProgress* progress = static_cast<DownloadProgress*>(clientp);
        progress->downloaded_bytes = static_cast<size_t>(dlnow);
        progress->total_bytes = static_cast<size_t>(dltotal);

        auto now = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - progress->start_time);
        if (elapsed.count() > 0) {
            progress->speed_bps = static_cast<double>(progress->downloaded_bytes) / elapsed.count();
        }

        return 0;
    }
};

DefaultHttpClient::DefaultHttpClient() {}

// PluginDownloader implementation
PluginDownloader::PluginDownloader(const Config& config) : config_(config) {
    http_client_ = std::make_unique<DefaultHttpClient>();
    http_client_->set_timeout(config_.download_timeout);

    // Ensure directories exist
    std::filesystem::create_directories(expand_path(config_.download_directory));
    std::filesystem::create_directories(expand_path(config_.installation_directory));
    std::filesystem::create_directories(expand_path(config_.backup_directory));
}

PluginDownloader::~PluginDownloader() = default;

std::future<InstallationResult> PluginDownloader::install_plugin(const std::string& plugin_id,
                                                                const std::string& version,
                                                                std::function<void(const DownloadProgress&)> progress_callback) {
    return std::async(std::launch::async, [this, plugin_id, version, progress_callback]() -> InstallationResult {
        try {
            // Resolve plugin package information
            auto package_future = resolve_plugin_package(plugin_id, version);
            auto package = package_future.get();

            if (!package.is_valid()) {
                return InstallationResult::failure(plugin_id, "Failed to resolve plugin package information");
            }

            return install_plugin(package, progress_callback).get();
        } catch (const std::exception& e) {
            return InstallationResult::failure(plugin_id, std::string("Installation failed: ") + e.what());
        }
    });
}

std::future<InstallationResult> PluginDownloader::install_plugin(const PluginPackage& package,
                                                                std::function<void(const DownloadProgress&)> progress_callback) {
    return std::async(std::launch::async, [this, package, progress_callback]() -> InstallationResult {
        std::string plugin_id = package.id;

        try {
            log_operation("install", "Starting installation for " + plugin_id + " v" + package.version);

            // Check if plugin is already installed
            if (is_plugin_installed(plugin_id)) {
                return InstallationResult::failure(plugin_id, "Plugin is already installed");
            }

            // Create backup area for this installation
            if (!create_backup(plugin_id).get()) {
                log_operation("warning", "Failed to create backup for " + plugin_id);
            }

            // Download plugin package
            std::string download_path = expand_path(config_.download_directory) + "/" +
                                       plugin_id + "-v" + package.version + ".zip";

            if (!download_with_retry(package.download_url, download_path, plugin_id).get()) {
                return InstallationResult::failure(plugin_id, "Download failed");
            }

            // Verify package integrity
            if (config_.verify_checksums && !verify_checksum(download_path, package.checksum_sha256)) {
                std::filesystem::remove(download_path);
                return InstallationResult::failure(plugin_id, "Checksum verification failed");
            }

            // Optional signature verification
            if (config_.verify_signatures && !verify_plugin_signature(package, download_path).get()) {
                std::filesystem::remove(download_path);
                return InstallationResult::failure(plugin_id, "Signature verification failed");
            }

            // Optional malware scan
            if (!scan_for_malware(download_path).get()) {
                std::filesystem::remove(download_path);
                return InstallationResult::failure(plugin_id, "Security scan detected potential issues");
            }

            // Extract and install package
            std::string install_path = expand_path(config_.installation_directory) + "/" + plugin_id;
            std::filesystem::create_directories(install_path);

            if (!extract_package(download_path, install_path).get()) {
                std::filesystem::remove_all(install_path);
                std::filesystem::remove(download_path);
                return InstallationResult::failure(plugin_id, "Package extraction failed");
            }

            // Validate plugin structure
            if (!validate_plugin_structure(install_path).get()) {
                std::filesystem::remove_all(install_path);
                std::filesystem::remove(download_path);
                return InstallationResult::failure(plugin_id, "Plugin structure validation failed");
            }

            // Install dependencies
            std::vector<PluginPackage> dependencies = resolve_dependencies(package).get();
            if (!install_dependencies(dependencies, progress_callback).get()) {
                std::filesystem::remove_all(install_path);
                std::filesystem::remove(download_path);
                return InstallationResult::failure(plugin_id, "Dependency installation failed");
            }

            // Save plugin configuration
            nlohmann::json plugin_config = package.to_json();
            plugin_config["installed_at"] = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();

            save_plugin_config(plugin_id, plugin_config).get();

            // Clean up download
            std::filesystem::remove(download_path);

            log_operation("success", "Successfully installed " + plugin_id + " v" + package.version);

            InstallationResult result = InstallationResult::success(plugin_id, package.version);
            result.installed_path = install_path;
            for (const auto& dep : dependencies) {
                result.installed_dependencies.emplace_back(dep.id, dep.version);
            }
            return result;

        } catch (const std::exception& e) {
            log_operation("error", "Installation failed for " + plugin_id + ": " + e.what());
            return InstallationResult::failure(plugin_id, std::string("Installation error: ") + e.what());
        }
    });
}

std::future<bool> PluginDownloader::uninstall_plugin(const std::string& plugin_id, bool keep_config) {
    return std::async(std::launch::async, [this, plugin_id, keep_config]() -> bool {
        try {
            if (!is_plugin_installed(plugin_id)) {
                return false;
            }

            std::string install_path = get_plugin_path(plugin_id);
            if (install_path.empty() || !directory_exists(install_path)) {
                return false;
            }

            // Create final backup unless user explicitly wants to keep config
            if (!keep_config) {
                create_backup(plugin_id).wait();

                // Remove plugin configuration
                std::string config_path = expand_path(config_.download_directory) + "/" + plugin_id + ".json";
                std::filesystem::remove(config_path);
            }

            // Remove installation directory
            return remove_directory(install_path).get();

        } catch (const std::exception& e) {
            log_operation("error", "Uninstall failed for " + plugin_id + ": " + e.what());
            return false;
        }
    });
}

std::future<InstallationResult> PluginDownloader::update_plugin(const std::string& plugin_id,
                                                               const std::string& target_version,
                                                               std::function<void(const DownloadProgress&)> progress_callback) {
    return std::async(std::launch::async, [this, plugin_id, target_version, progress_callback]() -> InstallationResult {
        try {
            if (!is_plugin_installed(plugin_id)) {
                return InstallationResult::failure(plugin_id, "Plugin is not installed");
            }

            // Download and install new version
            auto install_result = install_plugin(plugin_id, target_version, progress_callback).get();

            if (!install_result.installation_success) {
                return install_result;
            }

            // Clean up old version(s) if needed
            if (!target_version.empty()) {
                remove_previous_versions(plugin_id, install_result.version).wait();
            }

            log_operation("success", "Updated " + plugin_id + " to v" + install_result.version);
            return install_result;

        } catch (const std::exception& e) {
            return InstallationResult::failure(plugin_id, std::string("Update failed: ") + e.what());
        }
    });
}

// Helper methods implementation
std::future<PluginPackage> PluginDownloader::resolve_plugin_package(const std::string& plugin_id, const std::string& version) {
    return std::async(std::launch::async, [this, plugin_id, version]() -> PluginPackage {
        if (!github_registry_) {
            return {}; // Return invalid package if no registry
        }

        try {
            // Parse plugin id (owner/repo format)
            // Simple plugin id parsing for now
            auto slash_pos = plugin_id.find('/');
            if (slash_pos == std::string::npos) {
                return {};
            }

            // Get repository info
            auto repo_info_future = github_registry_->get_plugin_info(plugin_id);
            auto repo_info = repo_info_future.get();

            if (!repo_info.has_value()) {
                return {};
            }

            // Get releases
            auto releases_future = github_registry_->get_plugin_releases(plugin_id);
            auto releases = releases_future.get();

            // Find target version
            GitHubRelease target_release;
            for (const auto& release : releases) {
                if (version == "latest" || release.tag_name == version ||
                    release.tag_name == "v" + version) {
                    target_release = release;
                    break;
                }
            }

            if (!target_release.is_compatible_with_current_version()) {
                return {};
            }

            // Create package from release info
            PluginPackage package;
            package.id = plugin_id;
            package.version = target_release.tag_name;
            package.name = repo_info->name;
            package.description = repo_info->description;
            package.content_type = "application/zip";

            // Find primary asset (prefer .zip files)
            for (const auto& asset : target_release.assets) {
                if (asset.name.ends_with(".zip") || asset.name.ends_with(".tar.gz")) {
                    package.download_url = asset.browser_download_url;
                    package.file_size = asset.size;
                    package.checksum_sha256 = asset.checksum_sha256;
                    break;
                }
            }

            return package;

        } catch (...) {
            return {};
        }
    });
}

std::future<bool> PluginDownloader::download_with_retry(const std::string& url, const std::string& destination,
                                                       const std::string& plugin_id, int max_attempts) {
    return std::async(std::launch::async, [this, url, destination, plugin_id, max_attempts]() -> bool {
        for (int attempt = 1; attempt <= max_attempts; ++attempt) {
            try {
                auto download_future = http_client_->download_file(url, destination);
                if (download_future.get()) {
                    return true;
                }

                if (attempt < max_attempts) {
                    log_operation("retry", "Download attempt " + std::to_string(attempt) + " failed for " + plugin_id +
                                    ", retrying in " + std::to_string(attempt * 2) + " seconds");
                    std::this_thread::sleep_for(std::chrono::seconds(attempt * 2));
                }
            } catch (...) {
                if (attempt == max_attempts) {
                    return false;
                }
            }
        }
        return false;
    });
}

bool PluginDownloader::verify_checksum(const std::string& file_path, const std::string& expected_checksum) const {
    std::string actual_checksum = calculate_file_checksum(file_path);
    return actual_checksum == expected_checksum;
}

std::string PluginDownloader::calculate_file_checksum(const std::string& file_path) const {
    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        return "";
    }

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        return "";
    }

    if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1) {
        EVP_MD_CTX_free(ctx);
        return "";
    }

    constexpr size_t buffer_size = 8192;
    std::vector<char> buffer(buffer_size);

    while (file.read(buffer.data(), buffer_size)) {
        if (EVP_DigestUpdate(ctx, buffer.data(), file.gcount()) != 1) {
            EVP_MD_CTX_free(ctx);
            return "";
        }
    }

    if (file.gcount() > 0) {
        if (EVP_DigestUpdate(ctx, buffer.data(), file.gcount()) != 1) {
            EVP_MD_CTX_free(ctx);
            return "";
        }
    }

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_length = 0;

    if (EVP_DigestFinal_ex(ctx, hash, &hash_length) != 1) {
        EVP_MD_CTX_free(ctx);
        return "";
    }

    EVP_MD_CTX_free(ctx);

    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (unsigned int i = 0; i < hash_length; ++i) {
        ss << std::setw(2) << static_cast<unsigned int>(hash[i]);
    }

    return ss.str();
}

std::future<bool> PluginDownloader::extract_package(const std::string& package_path, const std::string& destination) {
    return std::async(std::launch::async, [package_path, destination]() -> bool {
        // For now, implement a simple mkdir-based installation
        // In a real implementation, this would unzip the package

        // Create destination if it doesn't exist
        std::filesystem::create_directories(destination);

        // Create a basic manifest file indicating successful installation
        nlohmann::json manifest;
        manifest["installed"] = true;
        manifest["installed_at"] = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        std::ofstream manifest_file(destination + "/manifest.json");
        manifest_file << manifest.dump(4);
        manifest_file.close();

        return true;
    });
}

std::future<bool> PluginDownloader::validate_plugin_structure(const std::string& plugin_path) {
    return std::async(std::launch::async, [plugin_path]() -> bool {
        // Check for required plugin files
        if (!std::filesystem::exists(plugin_path)) {
            return false;
        }

        bool has_manifest = std::filesystem::exists(plugin_path + "/" + MANIFEST_FILENAME);
        bool has_dir_exists = std::filesystem::is_directory(plugin_path);

        return has_dir_exists; // Basic validation for now
    });
}

std::string PluginDownloader::expand_path(const std::string& path) const {
    if (path.empty()) {
        return path;
    }

    std::string expanded = path;
    if (expanded.starts_with("~/")) {
        const char* home = std::getenv("HOME");
        if (home) {
            expanded.replace(0, 2, home);
        }
    }
    return expanded;
}

bool PluginDownloader::is_plugin_installed(const std::string& plugin_id) const {
    std::string plugin_path = get_plugin_path(plugin_id);
    return !plugin_path.empty() && directory_exists(plugin_path);
}

std::string PluginDownloader::get_plugin_path(const std::string& plugin_id) const {
    return expand_path(config_.installation_directory) + "/" + plugin_id;
}

bool PluginDownloader::directory_exists(const std::string& path) const {
    return std::filesystem::exists(path) && std::filesystem::is_directory(path);
}

void PluginDownloader::log_operation(const std::string& operation, const std::string& details) const {
    // Simple logging - in real implementation would use proper logging framework
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    std::cout << "[" << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S")
              << "] [" << operation << "] " << details << std::endl;
}

// Stubs for remaining required methods
std::future<bool> PluginDownloader::test_connectivity() {
    return std::async(std::launch::async, []() -> bool {
        // Simple connectivity test - in real implementation would ping GitHub API
        return true;
    });
}

std::future<bool> PluginDownloader::verify_plugin_signature(const PluginPackage& package, const std::string& file_path) {
    return std::async(std::launch::async, []() -> bool {
        // Stub implementation - would implement actual signature verification
        return true; // For now, assume valid
    });
}

std::future<bool> PluginDownloader::scan_for_malware(const std::string& file_path) {
    return std::async(std::launch::async, [file_path]() -> bool {
        // Stub implementation - would integrate with antivirus/malware scanning
        return true; // For now, assume safe
    });
}

std::future<std::vector<PluginPackage>> PluginDownloader::resolve_dependencies(const PluginPackage& package) {
    return std::async(std::launch::async, [package]() -> std::vector<PluginPackage> {
        // Stub implementation - would resolve actual dependencies
        return std::vector<PluginPackage>{};
    });
}

std::future<bool> PluginDownloader::install_dependencies(const std::vector<PluginPackage>& dependencies,
                                                        std::function<void(const DownloadProgress&)> progress_callback) {
    return std::async(std::launch::async, [dependencies]() -> bool {
        // Stub implementation - would install actual dependencies
        return true;
    });
}

std::vector<std::pair<std::string, std::string>> PluginDownloader::get_installed_plugins() const {
    std::vector<std::pair<std::string, std::string>> plugins;
    std::string install_dir = expand_path(config_.installation_directory);

    if (directory_exists(install_dir)) {
        for (const auto& entry : std::filesystem::directory_iterator(install_dir)) {
            if (entry.is_directory()) {
                plugins.emplace_back(entry.path().filename().string(), "unknown");
            }
        }
    }

    return plugins;
}

nlohmann::json PluginDownloader::get_download_statistics() const {
    nlohmann::json stats;
    stats["total_downloads"] = 0;
    stats["successful_downloads"] = 0;
    stats["failed_downloads"] = 0;
    stats["total_bytes_downloaded"] = 0;
    stats["average_download_speed"] = 0.0;
    return stats;
}

void PluginDownloader::set_http_client(std::unique_ptr<HttpClient> client) {
    http_client_ = std::move(client);
}

void PluginDownloader::set_github_registry(std::shared_ptr<GitHubRegistry> registry) {
    github_registry_ = std::move(registry);
}

std::future<bool> PluginDownloader::create_backup(const std::string& plugin_id) {
    return std::async(std::launch::async, [this, plugin_id]() -> bool {
        try {
            std::string plugin_path = get_plugin_path(plugin_id);
            std::string backup_path = expand_path(config_.backup_directory) + "/" + plugin_id + "_" +
                                     std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
                                         std::chrono::system_clock::now().time_since_epoch()).count());

            if (!directory_exists(plugin_path)) {
                return false;
            }

            std::filesystem::create_directories(expand_path(config_.backup_directory));
            std::filesystem::copy(plugin_path, backup_path, std::filesystem::copy_options::recursive);

            return true;
        } catch (...) {
            return false;
        }
    });
}

std::future<bool> PluginDownloader::save_plugin_config(const std::string& plugin_id, const nlohmann::json& config) {
    return std::async(std::launch::async, [this, plugin_id, config]() -> bool {
        try {
            std::string config_path = expand_path(config_.download_directory) + "/" + plugin_id + ".json";
            std::filesystem::create_directories(expand_path(config_.download_directory));

            std::ofstream file(config_path);
            file << config.dump(4);
            file.close();

            return true;
        } catch (...) {
            return false;
        }
    });
}

std::future<bool> PluginDownloader::remove_directory(const std::string& path) {
    return std::async(std::launch::async, [path]() -> bool {
        try {
            return std::filesystem::remove_all(path) > 0;
        } catch (...) {
            return false;
        }
    });
}

std::future<bool> PluginDownloader::remove_previous_versions(const std::string& plugin_id, const std::string& keep_version) {
    return std::async(std::launch::async, [plugin_id, keep_version]() -> bool {
        // Stub implementation - would handle version cleanup
        return true;
    });
}

} // namespace distribution
} // namespace aimux