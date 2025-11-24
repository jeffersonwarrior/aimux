#pragma once

#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <mutex>
#include <nlohmann/json.hpp>

namespace aimux {
namespace core {

/**
 * @brief ModelRegistry maintains a registry of available AI models across providers
 *
 * This singleton class manages model discovery, version comparison, and caching
 * for all AI providers (Anthropic, OpenAI, Cerebras, etc.). It ensures thread-safe
 * access to model information and provides utilities for selecting the latest models.
 */
class ModelRegistry {
public:
    /**
     * @brief Information about a specific AI model
     */
    struct ModelInfo {
        std::string provider;                    // Provider name (e.g., "anthropic", "openai")
        std::string model_id;                    // Full model identifier (e.g., "claude-3-5-sonnet-20241022")
        std::string version;                     // Semantic version (e.g., "3.5", "4.0")
        std::string release_date;                // ISO 8601 date (e.g., "2024-10-22")
        bool is_available;                       // Whether model passed validation test
        std::chrono::system_clock::time_point last_checked;  // Last validation timestamp

        /**
         * @brief Default constructor
         */
        ModelInfo() : is_available(false), last_checked(std::chrono::system_clock::now()) {}

        /**
         * @brief Constructor with full parameters
         */
        ModelInfo(const std::string& prov, const std::string& id, const std::string& ver,
                  const std::string& date, bool available = false)
            : provider(prov), model_id(id), version(ver), release_date(date),
              is_available(available), last_checked(std::chrono::system_clock::now()) {}

        /**
         * @brief Convert to JSON for serialization
         */
        nlohmann::json to_json() const;

        /**
         * @brief Create from JSON deserialization
         */
        static ModelInfo from_json(const nlohmann::json& j);
    };

    /**
     * @brief Get singleton instance (thread-safe)
     */
    static ModelRegistry& instance();

    // Prevent copying and assignment
    ModelRegistry(const ModelRegistry&) = delete;
    ModelRegistry& operator=(const ModelRegistry&) = delete;

    /**
     * @brief Get the latest model for a specific provider
     * @param provider Provider name (e.g., "anthropic", "openai")
     * @return ModelInfo for the latest model, or empty ModelInfo if none found
     */
    ModelInfo get_latest_model(const std::string& provider);

    /**
     * @brief Validate if a specific model is available
     * @param provider Provider name
     * @param model_id Model identifier
     * @return true if model exists and is marked as available
     */
    bool validate_model(const std::string& provider, const std::string& model_id);

    /**
     * @brief Refresh available models from cache or providers
     * This method is called during initialization to load cached model data
     */
    void refresh_available_models();

    /**
     * @brief Cache model selections to persistent storage
     * @param models Map of provider -> ModelInfo to cache
     */
    void cache_model_selection(const std::map<std::string, ModelInfo>& models);

    /**
     * @brief Load cached models from persistent storage
     * @return Map of provider -> ModelInfo loaded from cache
     */
    std::map<std::string, ModelInfo> load_cached_models();

    /**
     * @brief Add or update a model in the registry
     * @param info ModelInfo to add/update
     */
    void add_model(const ModelInfo& info);

    /**
     * @brief Get all models for a specific provider
     * @param provider Provider name
     * @return Vector of all models for the provider
     */
    std::vector<ModelInfo> get_models_for_provider(const std::string& provider);

    /**
     * @brief Compare two version strings
     * @param v1 First version string (e.g., "3.5", "4.0")
     * @param v2 Second version string
     * @return -1 if v1 < v2, 0 if v1 == v2, 1 if v1 > v2
     *
     * Handles formats:
     * - Major.Minor (e.g., "3.5", "4.0")
     * - Major.Minor.Patch (e.g., "3.5.1", "4.0.2")
     * - Pre-release suffixes (e.g., "4.0-rc1", "3.5-beta")
     */
    static int compare_versions(const std::string& v1, const std::string& v2);

    /**
     * @brief Select the latest model from a list
     * @param models Vector of ModelInfo to search
     * @return ModelInfo with the latest version, or empty ModelInfo if list is empty
     *
     * Selection criteria (in order):
     * 1. Highest version number (via compare_versions)
     * 2. Most recent release_date (if versions are equal)
     * 3. Alphabetically by model_id (as tiebreaker)
     */
    static ModelInfo select_latest(const std::vector<ModelInfo>& models);

private:
    /**
     * @brief Private constructor for singleton
     */
    ModelRegistry() = default;

    /**
     * @brief Parse version string into components
     * @param version Version string (e.g., "3.5.1-rc2")
     * @return Tuple of (major, minor, patch, prerelease_suffix)
     */
    static std::tuple<int, int, int, std::string> parse_version(const std::string& version);

    /**
     * @brief Get cache file path
     * @return Absolute path to cache file (e.g., ~/.aimux/model_cache.json)
     */
    std::string get_cache_file_path() const;

    /**
     * @brief Internal storage for all models (provider -> list of models)
     */
    std::map<std::string, std::vector<ModelInfo>> models_by_provider_;

    /**
     * @brief Mutex for thread-safe access
     */
    mutable std::mutex mutex_;
};

} // namespace core
} // namespace aimux
