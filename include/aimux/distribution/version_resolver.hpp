#pragma once

#include <string>
#include <vector>
#include <memory>
#include <future>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <regex>
#include <mutex>

#include "aimux/distribution/plugin_downloader.hpp"
#include <nlohmann/json.hpp>

namespace aimux {
namespace distribution {

/**
 * @brief Semantic version representation for plugin version management
 */
struct SemanticVersion {
    int major = 0;
    int minor = 0;
    int patch = 0;
    std::string prerelease;    // e.g., "alpha", "beta.1", "rc.2"
    std::string build;         // build metadata, ignored for version comparison

    SemanticVersion() = default;
    explicit SemanticVersion(const std::string& version_string);
    explicit SemanticVersion(int maj, int min, int pat, const std::string& pre = "", const std::string& bld = "")
        : major(maj), minor(min), patch(pat), prerelease(pre), build(bld) {}

    std::string to_string() const;
    bool is_valid() const;

    // Comparison operators following semver.org rules
    bool operator==(const SemanticVersion& other) const;
    bool operator!=(const SemanticVersion& other) const;
    bool operator<(const SemanticVersion& other) const;
    bool operator<=(const SemanticVersion& other) const;
    bool operator>(const SemanticVersion& other) const;
    bool operator>=(const SemanticVersion& other) const;

    // Compatibility checks
    bool is_compatible_with(const SemanticVersion& required) const;
    bool accepts_breaking_changes() const;
    bool is_prerelease() const;
    bool is_stable() const { return !is_prerelease(); }

    static SemanticVersion parse(const std::string& version_string);
    static bool is_valid_version_string(const std::string& version_string);

public:
    struct PrereleaseComponent {
        enum class Type { IDENTIFIER, NUMBER } type;
        std::string value;
        int number = 0;

        bool operator<(const PrereleaseComponent& other) const;
        bool operator==(const PrereleaseComponent& other) const;
    };

private:

    std::vector<PrereleaseComponent> parse_prerelease(const std::string& prerelease) const;
    int compare_components(const SemanticVersion& other) const;
    int compare_prerelease(const SemanticVersion& other) const;
};

/**
 * @brief Version constraint specification
 */
struct VersionConstraint {
    enum class Operator {
        EXACT,          // == 1.2.3
        GREATER,        // > 1.2.3
        GREATER_EQUAL,  // >= 1.2.3
        LESSER,         // < 1.2.3
        LESSER_EQUAL,   // <= 1.2.3
        CARET,          // ^1.2.3  (compatible with version)
        TILDE,          // ~1.2.3  (patch-only updates)
        WILDCARD,       // 1.2.*   (patch wildcard)
        RANGE,          // 1.2.3 - 2.3.4
        OR              // >=1.2.0 <2.0.0 || >=3.0.0
    };

    Operator op = Operator::EXACT;
    SemanticVersion version;
    SemanticVersion upper_bound; // For RANGE and CARET operators

    VersionConstraint() = default;
    VersionConstraint(Operator operation, const SemanticVersion& ver, const SemanticVersion& upper = SemanticVersion())
        : op(operation), version(ver), upper_bound(upper) {}

    std::string to_string() const;
    bool accepts(const SemanticVersion& candidate) const;
    bool is_valid() const;

    static std::vector<VersionConstraint> parse_range(const std::string& range_string);
    static VersionConstraint from_string(const std::string& constraint_string);
};

/**
 * @brief Plugin dependency information
 */
struct PluginDependency {
    std::string plugin_id;                   // e.g., "aimux-org/markdown-normalizer"
    std::string display_name;                // Human-readable name
    VersionConstraint version_constraint;    // Required version range
    bool optional = false;
    std::string reason;                      // Why this dependency is needed
    std::vector<std::string> provides;       // Capabilities this dependency provides
    std::string conflicts_with;               // Plugins that conflict with this

    nlohmann::json to_json() const;
    static PluginDependency from_json(const nlohmann::json& j);
    bool is_compatible_with(const SemanticVersion& version) const;
};

/**
 * @brief Dependency node for graph resolution
 */
struct DependencyNode {
    std::string plugin_id;
    SemanticVersion selected_version;
    PluginPackage package;
    std::vector<std::string> dependencies;
    int depth = 0;
    bool is_optional = false;

    // For graph traversal
    bool visited = false;
    bool in_path = false;  // For cycle detection

    nlohmann::json to_json() const;
    static DependencyNode from_json(const nlohmann::json& j);
};

/**
 * @brief Conflicting dependency information
 */
struct DependencyConflict {
    enum class Type {
        VERSION_CONFLICT,    // Two plugins require different versions of the same dependency
        CIRCULAR_DEPENDENCY, // A -> B -> A cycle detected
        MISSING_DEPENDENCY,  // Required dependency not found
        MUTUALLY_EXCLUSIVE, // Plugins explicitly conflict
        INSUFFICIENT_VERSION // Dependency version too old
    };

    Type type;
    std::vector<std::string> conflicting_plugins;
    std::string dependency_id;
    std::string description;
    std::vector<SemanticVersion> conflicting_versions;
    std::optional<SemanticVersion> suggested_resolution;

    std::string to_string() const;
    nlohmann::json to_json() const;
};

/**
 * @brief Resolution result
 */
struct ResolutionResult {
    bool resolution_success = false;
    std::vector<DependencyNode> resolved_plugins;
    std::vector<DependencyConflict> conflicts;
    std::unordered_map<std::string, std::string> resolution_notes;
    std::unordered_set<std::string> optional_plugins_skipped;

    // Statistics
    int total_plugins_processed = 0;
    int dependencies_resolved = 0;
    int conflicts_resolved = 0;
    int optional_included = 0;
    int optional_excluded = 0;

    static ResolutionResult success(const std::vector<DependencyNode>& plugins);
    static ResolutionResult failure(const std::vector<DependencyConflict>& conflicts);
};

/**
 * @brief Configuration for version resolution behavior
 */
struct ResolverConfig {
    // Version selection strategy
    enum class ResolutionStrategy {
        LATEST_COMPATIBLE,  // Use latest compatible version
        MINIMUM_COMPATIBLE, // Use minimum compatible version
        PREFER_STABLE,      // Prefer stable over prerelease
        PREFER_PRERELEASE,  // Prefer latest prerelease
        USER_PROMPT         // Prompt user when conflicts exist
    } strategy = ResolutionStrategy::LATEST_COMPATIBLE;

    // Conflict handling
    bool allow_prerelease = false;
    bool allow_breaking_changes = false;
    bool auto_resolve_conflicts = false;
    bool prefer_installed_versions = true;
    int max_resolution_depth = 50;
    std::chrono::seconds registry_timeout = std::chrono::seconds(30);

    // Dependency behavior
    bool include_optional_dependencies = true;
    bool skip_test_dependencies = true;
    bool trust_developer_dependencies = false;

    // Logging and debugging
    bool enable_resolution_logging = false;
    bool include_resolution_graph = false;
    bool cache_resolution_results = true;

    nlohmann::json to_json() const;
    static ResolverConfig from_json(const nlohmann::json& j);
};

/**
 * @brief Advanced version conflict resolution and dependency management system
 */
class VersionResolver {
public:
    explicit VersionResolver(const ResolverConfig& config = ResolverConfig{});
    ~VersionResolver() = default;

    // Core resolution methods
    std::future<ResolutionResult> resolve_dependencies(const std::vector<PluginPackage>& root_plugins);
    std::future<ResolutionResult> resolve_dependencies(const std::string& plugin_id, const std::string& version = "latest");
    std::future<std::optional<std::vector<PluginPackage>>> get_dependency_chain(const std::string& plugin_id, const std::string& version = "latest");

    // Version compatibility checks
    std::future<bool> are_plugins_compatible(const std::vector<std::pair<std::string, SemanticVersion>>& plugins);
    std::future<std::vector<SemanticVersion>> find_compatible_versions(const std::string& plugin_id, const VersionConstraint& constraint);
    std::future<bool> can_upgrade(const std::string& plugin_id, const SemanticVersion& target_version);
    std::future<bool> can_downgrade(const std::string& plugin_id, const SemanticVersion& target_version);

    // Conflict analysis and resolution
    std::future<std::vector<DependencyConflict>> detect_conflicts(const std::vector<PluginPackage>& plugins);
    std::future<ResolutionResult> resolve_conflicts(const std::vector<DependencyConflict>& conflicts,
                                                   const std::vector<PluginPackage>& plugins);
    std::future<std::vector<std::string>> suggest_solutions(const DependencyConflict& conflict);

    // Dependency graph analysis
    std::future<nlohmann::json> build_dependency_graph(const std::vector<std::string>& plugin_ids);
    std::future<std::vector<std::string>> detect_circular_dependencies();
    std::future<std::vector<std::string>> find_common_ancestors(const std::string& plugin1, const std::string& plugin2);
    std::future<int> calculate_dependency_distance(const std::string& from, const std::string& to);

    // Lock file management
    std::future<bool> resolve_lockfile(const std::string& lockfile_path);
    std::future<std::string> generate_lockfile(const std::vector<PluginPackage>& resolved_plugins);
    std::future<bool> verify_lockfile_consistency(const std::string& lockfile_path);

    // Version constraint validation
    bool satisfies_constraint(const SemanticVersion& version, const VersionConstraint& constraint) const;
    std::vector<VersionConstraint> parse_version_range(const std::string& range_string);
    std::string normalize_constraint_string(const std::string& constraint) const;

    // Utility methods
    void set_registry(std::shared_ptr<GitHubRegistry> registry);
    void set_downloader(std::shared_ptr<PluginDownloader> downloader);
    void update_config(const ResolverConfig& config);

    // Statistics and monitoring
    nlohmann::json get_resolution_statistics() const;
    std::chrono::system_clock::time_point get_last_resolution_time() const;
    void clear_cache();

private:
    ResolverConfig config_;
    std::shared_ptr<GitHubRegistry> registry_;
    std::shared_ptr<PluginDownloader> downloader_;
    mutable std::mutex resolver_mutex_;

    // Caching
    std::unordered_map<std::string, std::vector<PluginPackage>> version_cache_;
    std::unordered_map<std::string, ResolutionResult> resolution_cache_;
    std::chrono::system_clock::time_point last_resolution_time_;
    std::unordered_map<std::string, int> resolution_stats_;

    // Internal resolution state
    std::unordered_set<std::string> visited_plugins_;
    std::unordered_map<std::string, DependencyNode> current_resolution_;
    std::vector<DependencyConflict> current_conflicts_;

    // Core resolution algorithms
    ResolutionResult resolve_dependencies_internal(const std::vector<PluginPackage>& root_plugins);
    SemanticVersion select_optimal_version(const std::string& plugin_id,
                                          const VersionConstraint& constraint,
                                          const std::vector<SemanticVersion>& available_versions);

    // Dependency graph traversal
    bool visit_dependency_node(const std::string& plugin_id,
                              const SemanticVersion& version,
                              int depth);
    std::vector<std::string> find_dependency_path(const std::string& from, const std::string& to);

    // Conflict resolution strategies
    ResolutionResult resolve_version_conflicts(const std::unordered_map<std::string, std::vector<SemanticVersion>>& conflicts);
    ResolutionResult resolve_circular_conflicts(const std::vector<std::vector<std::string>>& cycles);
    ResolutionResult resolve_mutually_exclusive_conflicts(const std::vector<std::pair<std::string, std::string>>& exclusives);

    // Version compatibility logic
    bool is_version_compatible(const SemanticVersion& available, const SemanticVersion& required) const;
    bool can_coexist(const std::string& plugin1, const SemanticVersion& version1,
                    const std::string& plugin2, const SemanticVersion& version2) const;

    // Caching and utilities
    void cache_version_list(const std::string& plugin_id, const std::vector<PluginPackage>& versions);
    std::optional<std::vector<PluginPackage>> get_cached_versions(const std::string& plugin_id) const;
    void invalidate_cache(const std::string& plugin_id = "");

    // Logging and debugging
    void log_resolution_step(const std::string& step, const std::string& details) const;
    void log_conflict(const DependencyConflict& conflict) const;
    nlohmann::json build_resolution_tree() const;

    // Constants
    static constexpr int MAX_RESOLUTION_DEPTH = 50;
    static constexpr int MAX_CONFLICT_RESOLUTION_ATTEMPTS = 10;
    static constexpr std::chrono::hours CACHE_TTL = std::chrono::hours(1);
};

} // namespace distribution
} // namespace aimux