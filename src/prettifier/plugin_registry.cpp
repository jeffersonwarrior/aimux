#include "aimux/prettifier/plugin_registry.hpp"
#include "aimux/prettifier/prettifier_plugin.hpp"
#include <fstream>
#include <algorithm>
#include <random>
#include <sstream>
#include <regex>
#include <atomic>

namespace aimux {
namespace prettifier {

// PluginManifest implementation
nlohmann::json PluginManifest::to_json() const {
    nlohmann::json j;
    j["name"] = name;
    j["version"] = version;
    j["description"] = description;
    j["author"] = author;
    j["providers"] = providers;
    j["formats"] = formats;
    j["capabilities"] = capabilities;
    j["download_url"] = download_url;
    j["checksum"] = checksum;
    j["dependencies"] = dependencies;
    j["min_aimux_version"] = min_aimux_version;
    return j;
}

PluginManifest PluginManifest::from_json(const nlohmann::json& j) {
    PluginManifest manifest;
    if (j.contains("name")) j.at("name").get_to(manifest.name);
    if (j.contains("version")) j.at("version").get_to(manifest.version);
    if (j.contains("description")) j.at("description").get_to(manifest.description);
    if (j.contains("author")) j.at("author").get_to(manifest.author);
    if (j.contains("providers")) j.at("providers").get_to(manifest.providers);
    if (j.contains("formats")) j.at("formats").get_to(manifest.formats);
    if (j.contains("capabilities")) j.at("capabilities").get_to(manifest.capabilities);
    if (j.contains("download_url")) j.at("download_url").get_to(manifest.download_url);
    if (j.contains("checksum")) j.at("checksum").get_to(manifest.checksum);
    if (j.contains("dependencies")) j.at("dependencies").get_to(manifest.dependencies);
    if (j.contains("min_aimux_version")) j.at("min_aimux_version").get_to(manifest.min_aimux_version);
    return manifest;
}

bool PluginManifest::validate() const {
    std::regex name_regex(R"(^[a-zA-Z0-9_-]+$)");
    if (!std::regex_match(name, name_regex)) return false;
    if (name.empty() || version.empty()) return false;
    return true;
}

// PluginMetadata implementation
nlohmann::json PluginMetadata::to_json() const {
    nlohmann::json j = manifest.to_json();
    j["path"] = path;
    j["loaded_at"] = std::chrono::duration_cast<std::chrono::seconds>(
        loaded_at.time_since_epoch()).count();
    j["last_used"] = std::chrono::duration_cast<std::chrono::seconds>(
        last_used.time_since_epoch()).count();
    j["usage_count"] = usage_count;
    j["enabled"] = enabled;
    j["plugin_id"] = plugin_id;
    return j;
}

// PluginRegistry implementation
PluginRegistry::PluginRegistry(const CacheConfig& config)
    : cache_config_(config),
      last_cache_cleanup_(std::chrono::system_clock::now()) {
    if (cache_config_.enable_persistence) {
        std::filesystem::create_directories(cache_config_.cache_dir);
    }
    load_cache();
}

PluginRegistry::PluginRegistry()
    : cache_config_(CacheConfig{}),
      last_cache_cleanup_(std::chrono::system_clock::now()) {
    if (cache_config_.enable_persistence) {
        std::filesystem::create_directories(cache_config_.cache_dir);
    }
    load_cache();
}

PluginRegistry::~PluginRegistry() {
    try {
        if (cache_config_.enable_persistence) {
            persist_cache();
        }
        std::lock_guard<std::mutex> lock(registry_mutex_);
        plugins_.clear();
        plugin_metadata_.clear();
    } catch (const std::exception& e) {
        printf("[ERROR] PluginRegistry destructor error: %s\n", e.what());
    }
}

void PluginRegistry::add_plugin_directory(const std::string& directory_path) {
    std::filesystem::path path(directory_path);
    if (!std::filesystem::exists(path)) {
        throw std::invalid_argument("Plugin directory does not exist: " + directory_path);
    }
    if (!std::filesystem::is_directory(path)) {
        throw std::invalid_argument("Path is not a directory: " + directory_path);
    }
    std::lock_guard<std::mutex> lock(registry_mutex_);
    auto it = std::find(plugin_directories_.begin(), plugin_directories_.end(), directory_path);
    if (it == plugin_directories_.end()) {
        plugin_directories_.push_back(directory_path);
    }
}

bool PluginRegistry::remove_plugin_directory(const std::string& directory_path) {
    std::lock_guard<std::mutex> lock(registry_mutex_);
    auto it = std::find(plugin_directories_.begin(), plugin_directories_.end(), directory_path);
    if (it != plugin_directories_.end()) {
        plugin_directories_.erase(it);
        return true;
    }
    return false;
}

PluginLoadResult PluginRegistry::register_plugin(std::shared_ptr<PrettifierPlugin> plugin,
                                                const PluginManifest& manifest) {
    PluginLoadResult result;
    result.plugin_name = manifest.name;
    result.version = manifest.version;
    result.success = false;

    auto now = std::chrono::system_clock::now();
    result.load_time = now;

    try {
        if (!validate_manifest(manifest)) {
            result.error_message = "Invalid manifest: " + manifest.name;
            return result;
        }

        std::lock_guard<std::mutex> lock(registry_mutex_);

        PluginMetadata metadata;
        metadata.manifest = manifest;
        metadata.loaded_at = now;
        metadata.last_used = now;
        metadata.usage_count = 0;
        metadata.enabled = true;
        metadata.plugin_id = generate_plugin_id();

        plugins_[manifest.name] = plugin;
        plugin_metadata_[manifest.name] = metadata;

        result.success = true;
        load_count_++;

        if (plugin_loaded_callback_) {
            try {
                plugin_loaded_callback_(manifest.name, metadata);
            } catch (const std::exception& e) {
                printf("[ERROR] Plugin loaded callback error: %s\n", e.what());
            }
        }

    } catch (const std::exception& e) {
        result.error_message = "Registration error: " + std::string(e.what());
    }

    return result;
}

std::shared_ptr<PrettifierPlugin> PluginRegistry::get_prettifier(const std::string& name) {
    std::lock_guard<std::mutex> lock(registry_mutex_);
    auto it = plugins_.find(name);
    if (it == plugins_.end()) {
        return nullptr;
    }
    update_usage_stats(name);
    return it->second;
}

std::vector<std::shared_ptr<PrettifierPlugin>> PluginRegistry::get_all_plugins() {
    std::lock_guard<std::mutex> lock(registry_mutex_);
    std::vector<std::shared_ptr<PrettifierPlugin>> result;
    result.reserve(plugins_.size());
    for (const auto& [name, plugin] : plugins_) {
        if (plugin_metadata_[name].enabled) {
            result.push_back(plugin);
        }
    }
    return result;
}

bool PluginRegistry::unload_plugin(const std::string& name) {
    std::lock_guard<std::mutex> lock(registry_mutex_);
    auto plugin_it = plugins_.find(name);
    if (plugin_it == plugins_.end()) {
        return false;
    }
    const auto& metadata = plugin_metadata_[name];
    if (plugin_unloaded_callback_) {
        try {
            plugin_unloaded_callback_(name, metadata);
        } catch (const std::exception& e) {
            printf("[ERROR] Plugin unloaded callback error: %s\n", e.what());
        }
    }
    plugins_.erase(plugin_it);
    plugin_metadata_.erase(name);
    {
        std::lock_guard<std::mutex> cache_lock(cache_mutex_);
        cache_timestamps_.erase(name);
    }
    return true;
}

std::optional<PluginMetadata> PluginRegistry::get_plugin_metadata(const std::string& name) {
    std::lock_guard<std::mutex> lock(registry_mutex_);
    auto it = plugin_metadata_.find(name);
    if (it != plugin_metadata_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::unordered_map<std::string, PluginMetadata> PluginRegistry::get_all_metadata() {
    std::lock_guard<std::mutex> lock(registry_mutex_);
    return plugin_metadata_;
}

nlohmann::json PluginRegistry::get_status() const {
    std::lock_guard<std::mutex> lock(registry_mutex_);
    nlohmann::json status;
    status["total_plugins"] = plugins_.size();
    status["enabled_plugins"] = std::count_if(plugin_metadata_.begin(), plugin_metadata_.end(),
        [](const auto& pair) { return pair.second.enabled; });
    status["plugin_directories"] = plugin_directories_.size();
    status["discovery_count"] = discovery_count_.load();
    status["load_count"] = load_count_.load();
    status["cache_hits"] = cache_hits_.load();
    status["cache_misses"] = cache_misses_.load();
    return status;
}

void PluginRegistry::update_usage_stats(const std::string& plugin_name) {
    auto it = plugin_metadata_.find(plugin_name);
    if (it != plugin_metadata_.end()) {
        it->second.usage_count++;
        it->second.last_used = std::chrono::system_clock::now();
    }
}

void PluginRegistry::persist_cache() const {
    // TODO: Implement cache persistence
}

void PluginRegistry::load_cache() {
    // TODO: Implement cache loading
}

std::string PluginRegistry::generate_plugin_id() const {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    std::stringstream ss;
    ss << "plugin_";
    for (int i = 0; i < 32; ++i) {
        ss << std::hex << dis(gen);
    }
    return ss.str();
}

bool PluginRegistry::is_cache_expired(const std::string& plugin_name) const {
    auto it = cache_timestamps_.find(plugin_name);
    if (it == cache_timestamps_.end()) {
        return true;
    }
    auto now = std::chrono::system_clock::now();
    auto age = now - it->second;
    return age > cache_config_.ttl;
}

void PluginRegistry::cleanup_expired_cache() {
    // TODO: Implement cache cleanup
}

bool PluginRegistry::validate_manifest(const PluginManifest& manifest) const {
    return manifest.validate();
}

std::shared_ptr<PrettifierPlugin> PluginRegistry::load_plugin_from_manifest(
    const PluginManifest& /*manifest*/,
    const std::string& /*plugin_directory*/) {
    // TODO: Implement actual plugin loading
    return nullptr;
}

std::vector<std::filesystem::path> PluginRegistry::scan_directory_manifests(
    const std::string& directory) const {
    std::vector<std::filesystem::path> manifests;
    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
            if (entry.is_regular_file() && entry.path().filename() == "manifest.json") {
                manifests.push_back(entry.path());
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        printf("[ERROR] Filesystem error: %s\n", e.what());
    }
    return manifests;
}

} // namespace prettifier
} // namespace aimux