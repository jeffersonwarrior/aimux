#pragma once

#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include <mutex>
#include <filesystem>
#include <functional>
#include <chrono>
#include <atomic>
#include <nlohmann/json.hpp>

namespace aimux {
namespace prettifier {

// Forward declarations
class PrettifierPlugin;
struct PluginManifest;
struct PluginMetadata;

/**
 * @brief Result of plugin loading operations
 */
struct PluginLoadResult {
    bool success = false;
    std::string plugin_name;
    std::string version;
    std::string error_message;
    std::chrono::system_clock::time_point load_time;
};

/**
 * @brief Plugin manifest structure for JSON validation
 */
struct PluginManifest {
    std::string name;
    std::string version;
    std::string description;
    std::string author;
    std::vector<std::string> providers;
    std::vector<std::string> formats;
    std::vector<std::string> capabilities;
    std::string download_url;
    std::string checksum;
    std::vector<std::string> dependencies;
    std::string min_aimux_version;

    // JSON serialization
    nlohmann::json to_json() const;
    static PluginManifest from_json(const nlohmann::json& j);
    bool validate() const;
};

/**
 * @brief Runtime metadata for loaded plugins
 */
struct PluginMetadata {
    PluginManifest manifest;
    std::string path;
    std::chrono::system_clock::time_point loaded_at;
    std::chrono::system_clock::time_point last_used;
    size_t usage_count = 0;
    bool enabled = true;
    std::string plugin_id; // UUID for tracking

    nlohmann::json to_json() const;
};

/**
 * @brief High-performance plugin registry with caching and thread safety
 *
 * The PluginRegistry provides a centralized system for discovering, loading,
 * and managing prettifier plugins. It implements sophisticated caching,
 * thread-safe operations, and performance monitoring to meet enterprise-grade
 * requirements.
 *
 * Key features:
 * - Thread-safe plugin discovery and loading with std::mutex
 * - High-performance caching with LRU eviction
 * - JSON schema validation for plugin manifests
 * - Recursive directory scanning with parallel processing
 * - Memory-efficient storage with move semantics
 * - Real-time metrics collection for performance monitoring
 * - Hot-reloading capabilities for production environments
 * - Security validation to prevent malicious plugin loading
 *
 * Performance targets (from qa/phase1_foundation_qa_plan.md):
 * - Plugin discovery: <100ms for 100 plugins
 * - Memory usage: <10MB for registry with 200 plugins
 * - Thread safety: No data races under 100 concurrent operations
 *
 * Usage example:
 * @code
 * PluginRegistry registry;
 * registry.add_plugin_directory("~/.config/aimux/plugins");
 *
 * auto result = registry.discover_plugins();
 * if (result.success) {
 *     auto plugin = registry.get_prettifier("markdown-normalizer");
 *     if (plugin) {
 *         auto formatted = plugin->postprocess_response(response);
 *     }
 * }
 * @endcode
 */
class PluginRegistry {
public:
    /**
     * @brief Plugin change callback type
     */
    using PluginChangeCallback = std::function<void(const std::string&, const PluginMetadata&)>;

    /**
     * @brief Cache configuration structure
     */
    struct CacheConfig {
        CacheConfig() = default;

        CacheConfig(size_t max_size, std::chrono::minutes ttl_minutes,
                   bool persistence, std::string cache_dir_path)
            : max_cache_size(max_size), ttl(ttl_minutes),
              enable_persistence(persistence), cache_dir(cache_dir_path) {}

        size_t max_cache_size = 100;           // Maximum plugins to cache
        std::chrono::minutes ttl{60};         // Cache TTL
        bool enable_persistence = true;        // Persist cache to disk
        std::string cache_dir = ".aimux_cache"; // Cache directory
    };

    /**
     * @brief Construct a PluginRegistry with optional cache configuration
     *
     * Initializes the registry with thread-safe containers, sets up
     * the caching system, and prepares plugin discovery mechanisms.
     *
     * @param config Optional cache configuration
     * @throws std::runtime_error if cache directory cannot be created
     */
    explicit PluginRegistry(const CacheConfig& config);
    PluginRegistry();

    /**
     * @brief Destructor with proper cleanup
     *
     * Ensures all plugins are safely unloaded, cache is persisted,
     * and resources are properly released.
     */
    ~PluginRegistry();

    // Plugin Discovery and Registration

    /**
     * @brief Add a directory to scan for plugins
     *
     * Adds the specified directory to the search path. The directory
     * will be scanned recursively during plugin discovery.
     *
     * @param directory_path Path to directory containing plugins
     * @throws std::invalid_argument if directory doesn't exist or isn't readable
     */
    void add_plugin_directory(const std::string& directory_path);

    /**
     * @brief Remove a directory from plugin search paths
     *
     * @param directory_path Path to remove from search paths
     * @return true if directory was found and removed
     */
    bool remove_plugin_directory(const std::string& directory_path);

    /**
     * @brief Discover and load all plugins from configured directories
     *
     * Performs recursive scanning of all configured plugin directories,
     * validates plugin manifests, and loads all valid plugins.
     *
     * @param force_reload If true, reloads all plugins even if already loaded
     * @return Aggregate result with counts of successful/failed loads
     */
    PluginLoadResult discover_plugins(bool force_reload = false);

    /**
     * @brief Load a specific plugin from a manifest file
     *
     * Loads a single plugin from its manifest.json file with full
     * validation and security checks.
     *
     * @param manifest_path Path to plugin manifest.json
     * @return Detailed result of the loading operation
     */
    PluginLoadResult load_plugin(const std::string& manifest_path);

    /**
     * @brief Register a plugin instance directly
     *
     * Registers a plugin instance that was created externally,
     * bypassing manifest discovery but still performing validation.
     *
     * @param plugin Shared pointer to plugin instance
     * @param manifest Plugin manifest information
     * @return Result of the registration operation
     */
    PluginLoadResult register_plugin(std::shared_ptr<PrettifierPlugin> plugin,
                                    const PluginManifest& manifest);

    // Plugin Access and Management

    /**
     * @brief Get a prettifier plugin by name
     *
     * Retrieves a plugin instance by its registered name. Updates
     * usage statistics and marks plugin as recently used.
     *
     * @param name Plugin name to retrieve
     * @return Shared pointer to plugin instance, or nullptr if not found
     */
    std::shared_ptr<PrettifierPlugin> get_prettifier(const std::string& name);

    /**
     * @brief Get all loaded plugins
     *
     * @return Vector of shared pointers to all loaded plugins
     */
    std::vector<std::shared_ptr<PrettifierPlugin>> get_all_plugins();

    /**
     * @brief Get plugins that support a specific provider
     *
     * @param provider Provider name (e.g., "cerebras", "openai")
     * @return Vector of plugins compatible with the provider
     */
    std::vector<std::shared_ptr<PrettifierPlugin>> get_plugins_for_provider(const std::string& provider);

    /**
     * @brief Get plugins that support a specific format
     *
     * @param format Format name (e.g., "markdown", "json", "tool-calls")
     * @return Vector of plugins that handle the format
     */
    std::vector<std::shared_ptr<PrettifierPlugin>> get_plugins_for_format(const std::string& format);

    /**
     * @brief Unregister and unload a plugin
     *
     * Safely unloads a plugin, notifies callbacks, and cleans up resources.
     *
     * @param name Plugin name to unload
     * @return true if plugin was found and unloaded
     */
    bool unload_plugin(const std::string& name);

    /**
     * @brief Enable or disable a plugin
     *
     * @param name Plugin name
     * @param enabled Whether plugin should be enabled
     * @return true if operation was successful
     */
    bool set_plugin_enabled(const std::string& name, bool enabled);

    // Metadata and Configuration

    /**
     * @brief Get metadata for a specific plugin
     *
     * @param name Plugin name
     * @return Plugin metadata, or empty optional if not found
     */
    std::optional<PluginMetadata> get_plugin_metadata(const std::string& name);

    /**
     * @brief Get metadata for all loaded plugins
     *
     * @return Map of plugin names to their metadata
     */
    std::unordered_map<std::string, PluginMetadata> get_all_metadata();

    /**
     * @brief Export registry configuration to JSON
     *
     * @return JSON representation of registry state
     */
    nlohmann::json export_configuration() const;

    /**
     * @brief Import registry configuration from JSON
     *
     * @param config JSON configuration to import
     * @return Result of import operation
     */
    PluginLoadResult import_configuration(const nlohmann::json& config);

    // Event Handling and Callbacks

    /**
     * @brief Set callback for plugin load events
     *
     * @param callback Function to call when plugin is loaded
     */
    void set_plugin_loaded_callback(const PluginChangeCallback& callback);

    /**
     * @brief Set callback for plugin unload events
     *
     * @param callback Function to call when plugin is unloaded
     */
    void set_plugin_unloaded_callback(const PluginChangeCallback& callback);

    /**
     * @brief Enable or disable file watching for hot-reloading
     *
     * @param enabled Whether to watch plugin directories for changes
     */
    void enable_file_watching(bool enabled);

    // Caching and Performance

    /**
     * @brief Clear the plugin cache
     *
     * Clears all cached plugin data and forces reload on next access.
     */
    void clear_cache();

    /**
     * @brief Get cache statistics
     *
     * @return JSON with cache performance metrics
     */
    nlohmann::json get_cache_statistics() const;

    /**
     * @brief Optimize cache with LRU eviction
     *
     * Removes least recently used plugins to maintain cache size limits.
     */
    void optimize_cache();

    // Security and Validation

    /**
     * @brief Validate plugin security
     *
     * Performs security checks on loaded plugins including:
     * - File permission validation
     * - Digital signature verification (if available)
     * - Malicious pattern detection
     *
     * @param name Plugin name to validate
     * @return Security validation result
     */
    bool validate_plugin_security(const std::string& name);

    /**
     * @brief Get security report for all plugins
     *
     * @return JSON security report with risk assessment
     */
    nlohmann::json get_security_report() const;

    // Diagnostics and Monitoring

    /**
     * @brief Get comprehensive registry status
     *
     * Returns detailed status information including plugin counts,
     * performance metrics, memory usage, and health indicators.
     *
     * @return JSON status report
     */
    nlohmann::json get_status() const;

    /**
     * @brief Get performance metrics
     *
     * @return JSON with performance counters and timing data
     */
    nlohmann::json get_metrics() const;

    /**
     * @brief Perform health check on all plugins
     *
     * Tests each plugin's functionality and reports any issues.
     *
     * @return JSON health check results
     */
    nlohmann::json health_check() const;

private:
    // Thread safety
    mutable std::mutex registry_mutex_;
    mutable std::mutex cache_mutex_;

    // Plugin storage
    std::unordered_map<std::string, std::shared_ptr<PrettifierPlugin>> plugins_;
    std::unordered_map<std::string, PluginMetadata> plugin_metadata_;
    std::vector<std::string> plugin_directories_;

    // Caching
    CacheConfig cache_config_;
    std::unordered_map<std::string, std::chrono::system_clock::time_point> cache_timestamps_;
    std::chrono::system_clock::time_point last_cache_cleanup_;

    // Event handling
    PluginChangeCallback plugin_loaded_callback_;
    PluginChangeCallback plugin_unloaded_callback_;
    bool file_watching_enabled_ = false;

    // Performance tracking
    mutable std::atomic<size_t> discovery_count_{0};
    mutable std::atomic<size_t> load_count_{0};
    mutable std::atomic<size_t> cache_hits_{0};
    mutable std::atomic<size_t> cache_misses_{0};

    // Internal helper methods

    /**
     * @brief Validate plugin manifest against schema
     */
    bool validate_manifest(const PluginManifest& manifest) const;

    /**
     * @brief Load plugin from manifest and directory
     */
    std::shared_ptr<PrettifierPlugin> load_plugin_from_manifest(
        const PluginManifest& manifest,
        const std::string& plugin_directory);

    /**
     * @brief Scan directory for plugin manifests
     */
    std::vector<std::filesystem::path> scan_directory_manifests(
        const std::string& directory) const;

    /**
     * @brief Update plugin usage statistics
     */
    void update_usage_stats(const std::string& plugin_name);

    /**
     * @brief Persist cache to disk
     */
    void persist_cache() const;

    /**
     * @brief Load cache from disk
     */
    void load_cache();

    /**
     * @brief Generate unique plugin ID
     */
    std::string generate_plugin_id() const;

    /**
     * @brief Check if cache entry is expired
     */
    bool is_cache_expired(const std::string& plugin_name) const;

    /**
     * @brief Cleanup expired cache entries
     */
    void cleanup_expired_cache();
};

} // namespace prettifier
} // namespace aimux