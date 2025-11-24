#include "aimux/core/model_registry.hpp"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <regex>
#include <cstdlib>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>

namespace aimux {
namespace core {

// ============================================================================
// ModelInfo JSON Serialization
// ============================================================================

nlohmann::json ModelRegistry::ModelInfo::to_json() const {
    nlohmann::json j;
    j["provider"] = provider;
    j["model_id"] = model_id;
    j["version"] = version;
    j["release_date"] = release_date;
    j["is_available"] = is_available;

    // Convert timestamp to Unix epoch for JSON storage
    auto epoch = std::chrono::system_clock::to_time_t(last_checked);
    j["last_checked"] = static_cast<int64_t>(epoch);

    return j;
}

ModelRegistry::ModelInfo ModelRegistry::ModelInfo::from_json(const nlohmann::json& j) {
    ModelInfo info;
    info.provider = j.value("provider", "");
    info.model_id = j.value("model_id", "");
    info.version = j.value("version", "");
    info.release_date = j.value("release_date", "");
    info.is_available = j.value("is_available", false);

    // Restore timestamp from Unix epoch
    if (j.contains("last_checked")) {
        auto epoch = j["last_checked"].get<int64_t>();
        info.last_checked = std::chrono::system_clock::from_time_t(static_cast<time_t>(epoch));
    }

    return info;
}

// ============================================================================
// Singleton Instance
// ============================================================================

ModelRegistry& ModelRegistry::instance() {
    static ModelRegistry instance;
    return instance;
}

// ============================================================================
// Version Comparison Logic
// ============================================================================

std::tuple<int, int, int, std::string> ModelRegistry::parse_version(const std::string& version) {
    int major = 0, minor = 0, patch = 0;
    std::string prerelease;

    // Handle empty version
    if (version.empty()) {
        return {0, 0, 0, ""};
    }

    // Regex to parse version: major.minor[.patch][-prerelease]
    // Examples: "3.5", "4.0.1", "3.5-rc1", "4.0.2-beta"
    std::regex version_regex(R"(^(\d+)\.(\d+)(?:\.(\d+))?(?:-(.+))?$)");
    std::smatch match;

    if (std::regex_match(version, match, version_regex)) {
        major = std::stoi(match[1].str());
        minor = std::stoi(match[2].str());

        // Patch is optional
        if (match[3].matched) {
            patch = std::stoi(match[3].str());
        }

        // Prerelease suffix is optional
        if (match[4].matched) {
            prerelease = match[4].str();
        }
    } else {
        // Fallback: try to parse at least major.minor
        std::istringstream iss(version);
        std::string token;

        if (std::getline(iss, token, '.')) {
            major = std::stoi(token);
        }
        if (std::getline(iss, token, '.')) {
            minor = std::stoi(token);
        }
        if (std::getline(iss, token, '.')) {
            patch = std::stoi(token);
        }
    }

    return {major, minor, patch, prerelease};
}

int ModelRegistry::compare_versions(const std::string& v1, const std::string& v2) {
    auto [major1, minor1, patch1, pre1] = parse_version(v1);
    auto [major2, minor2, patch2, pre2] = parse_version(v2);

    // Compare major version
    if (major1 != major2) {
        return (major1 > major2) ? 1 : -1;
    }

    // Compare minor version
    if (minor1 != minor2) {
        return (minor1 > minor2) ? 1 : -1;
    }

    // Compare patch version
    if (patch1 != patch2) {
        return (patch1 > patch2) ? 1 : -1;
    }

    // Compare prerelease versions
    // Stable (no prerelease) > any prerelease
    if (pre1.empty() && !pre2.empty()) {
        return 1;  // v1 is stable, v2 is prerelease -> v1 > v2
    }
    if (!pre1.empty() && pre2.empty()) {
        return -1;  // v1 is prerelease, v2 is stable -> v1 < v2
    }

    // Both have prerelease: compare lexicographically
    if (!pre1.empty() && !pre2.empty()) {
        if (pre1 < pre2) return -1;
        if (pre1 > pre2) return 1;
    }

    // Versions are equal
    return 0;
}

// ============================================================================
// Model Selection Logic
// ============================================================================

ModelRegistry::ModelInfo ModelRegistry::select_latest(const std::vector<ModelInfo>& models) {
    if (models.empty()) {
        return ModelInfo();  // Return empty ModelInfo
    }

    if (models.size() == 1) {
        return models[0];
    }

    // Create a copy for sorting
    std::vector<ModelInfo> sorted_models = models;

    // Sort by:
    // 1. Version (descending) - highest version first
    // 2. Release date (descending) - most recent first
    // 3. Model ID (ascending) - alphabetical order
    std::sort(sorted_models.begin(), sorted_models.end(),
        [](const ModelInfo& a, const ModelInfo& b) {
            // Compare versions
            int version_cmp = compare_versions(a.version, b.version);
            if (version_cmp != 0) {
                return version_cmp > 0;  // Descending order (latest first)
            }

            // If versions are equal, compare release dates
            if (a.release_date != b.release_date) {
                return a.release_date > b.release_date;  // Descending (most recent first)
            }

            // If release dates are equal, compare model IDs (alphabetically)
            return a.model_id < b.model_id;  // Ascending order
        }
    );

    return sorted_models[0];  // Return the first (latest) model
}

// ============================================================================
// Registry Management
// ============================================================================

ModelRegistry::ModelInfo ModelRegistry::get_latest_model(const std::string& provider) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = models_by_provider_.find(provider);
    if (it == models_by_provider_.end() || it->second.empty()) {
        return ModelInfo();  // No models found for provider
    }

    return select_latest(it->second);
}

bool ModelRegistry::validate_model(const std::string& provider, const std::string& model_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = models_by_provider_.find(provider);
    if (it == models_by_provider_.end()) {
        return false;
    }

    // Search for the specific model
    for (const auto& model : it->second) {
        if (model.model_id == model_id && model.is_available) {
            return true;
        }
    }

    return false;
}

void ModelRegistry::add_model(const ModelInfo& info) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto& models = models_by_provider_[info.provider];

    // Check if model already exists
    auto it = std::find_if(models.begin(), models.end(),
        [&info](const ModelInfo& m) {
            return m.model_id == info.model_id;
        }
    );

    if (it != models.end()) {
        // Update existing model
        *it = info;
    } else {
        // Add new model
        models.push_back(info);
    }
}

std::vector<ModelRegistry::ModelInfo> ModelRegistry::get_models_for_provider(
    const std::string& provider) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = models_by_provider_.find(provider);
    if (it != models_by_provider_.end()) {
        return it->second;
    }

    return std::vector<ModelInfo>();
}

void ModelRegistry::refresh_available_models() {
    // Load from cache
    auto cached_models = load_cached_models();

    std::lock_guard<std::mutex> lock(mutex_);

    // Update registry with cached models
    for (const auto& [provider, model] : cached_models) {
        auto& models = models_by_provider_[provider];

        // Check if model already exists
        auto it = std::find_if(models.begin(), models.end(),
            [&model](const ModelInfo& m) {
                return m.model_id == model.model_id;
            }
        );

        if (it != models.end()) {
            *it = model;
        } else {
            models.push_back(model);
        }
    }
}

// ============================================================================
// Caching Logic
// ============================================================================

std::string ModelRegistry::get_cache_file_path() const {
    // Get home directory
    const char* home = std::getenv("HOME");
    if (!home) {
        home = getpwuid(getuid())->pw_dir;
    }

    std::string cache_dir = std::string(home) + "/.aimux";
    std::string cache_file = cache_dir + "/model_cache.json";

    // Create directory if it doesn't exist
    struct stat st;
    if (stat(cache_dir.c_str(), &st) != 0) {
        mkdir(cache_dir.c_str(), 0755);
    }

    return cache_file;
}

void ModelRegistry::cache_model_selection(const std::map<std::string, ModelInfo>& models) {
    std::string cache_file = get_cache_file_path();

    nlohmann::json cache_json;
    cache_json["version"] = "1.0";
    cache_json["timestamp"] = static_cast<int64_t>(
        std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())
    );

    nlohmann::json models_json = nlohmann::json::object();
    for (const auto& [provider, model] : models) {
        models_json[provider] = model.to_json();
    }
    cache_json["models"] = models_json;

    // Write to file
    std::ofstream ofs(cache_file);
    if (ofs.is_open()) {
        ofs << cache_json.dump(2);  // Pretty print with 2-space indentation
        ofs.close();
    }
}

std::map<std::string, ModelRegistry::ModelInfo> ModelRegistry::load_cached_models() {
    std::map<std::string, ModelInfo> cached_models;

    std::string cache_file = get_cache_file_path();

    // Check if cache file exists
    struct stat st;
    if (stat(cache_file.c_str(), &st) != 0) {
        return cached_models;  // File doesn't exist
    }

    // Read cache file
    std::ifstream ifs(cache_file);
    if (!ifs.is_open()) {
        return cached_models;  // Can't open file
    }

    try {
        nlohmann::json cache_json;
        ifs >> cache_json;

        // Validate cache version
        if (!cache_json.contains("version") || !cache_json.contains("models")) {
            return cached_models;  // Invalid cache format
        }

        // Load models
        const auto& models_json = cache_json["models"];
        for (auto it = models_json.begin(); it != models_json.end(); ++it) {
            std::string provider = it.key();
            ModelInfo model = ModelInfo::from_json(it.value());
            cached_models[provider] = model;
        }
    } catch (const std::exception& e) {
        // Invalid JSON or parsing error - return empty cache
        cached_models.clear();
    }

    return cached_models;
}

} // namespace core
} // namespace aimux
