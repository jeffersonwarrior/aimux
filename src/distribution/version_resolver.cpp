#include "aimux/distribution/version_resolver.hpp"
#include <algorithm>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <climits>
#include <future>
#include <thread>

namespace aimux {
namespace distribution {

// SemanticVersion implementation
SemanticVersion::SemanticVersion(const std::string& version_string) {
    *this = parse(version_string);
}

std::string SemanticVersion::to_string() const {
    std::string result = std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);

    if (!prerelease.empty()) {
        result += "-" + prerelease;
    }

    if (!build.empty()) {
        result += "+" + build;
    }

    return result;
}

bool SemanticVersion::is_valid() const {
    return major >= 0 && minor >= 0 && patch >= 0;
}

SemanticVersion SemanticVersion::parse(const std::string& version_string) {
    SemanticVersion version;

    if (!is_valid_version_string(version_string)) {
        return version; // Return invalid version
    }

    // Core version regex: ^v?(\d+)\.(\d+)\.(\d+)(?:-([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?(?:\+([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?$
    std::regex version_regex(R"(^v?(\d+)\.(\d+)\.(\d+)(?:-([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?(?:\+([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?$)");
    std::smatch matches;

    if (std::regex_match(version_string, matches, version_regex)) {
        version.major = std::stoi(matches[1].str());
        version.minor = std::stoi(matches[2].str());
        version.patch = std::stoi(matches[3].str());

        if (matches[4].matched) {
            version.prerelease = matches[4].str();
        }

        if (matches[5].matched) {
            version.build = matches[5].str();
        }
    }

    return version;
}

bool SemanticVersion::is_valid_version_string(const std::string& version_string) {
    std::regex valid_regex(R"(^v?\d+\.\d+\.\d+(?:-[0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*)?(?:\+[0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*)?$)");
    return std::regex_match(version_string, valid_regex);
}

bool SemanticVersion::operator==(const SemanticVersion& other) const {
    return major == other.major && minor == other.minor && patch == other.patch &&
           prerelease == other.prerelease; // Build metadata ignored for comparison
}

bool SemanticVersion::operator!=(const SemanticVersion& other) const {
    return !(*this == other);
}

int SemanticVersion::compare_components(const SemanticVersion& other) const {
    if (major != other.major) return (major > other.major) ? 1 : -1;
    if (minor != other.minor) return (minor > other.minor) ? 1 : -1;
    if (patch != other.patch) return (patch > other.patch) ? 1 : -1;
    return 0;
}

int SemanticVersion::compare_prerelease(const SemanticVersion& other) const {
    bool this_prerelease = is_prerelease();
    bool other_prerelease = other.is_prerelease();

    if (!this_prerelease && !other_prerelease) return 0;
    if (!this_prerelease && other_prerelease) return 1;  // Stable > prerelease
    if (this_prerelease && !other_prerelease) return -1; // Prerelease < stable

    // Both are prerelease, compare components
    auto this_components = parse_prerelease(prerelease);
    auto other_components = parse_prerelease(other.prerelease);

    size_t max_len = std::max(this_components.size(), other_components.size());
    for (size_t i = 0; i < max_len; ++i) {
        if (i >= this_components.size()) return -1; // Shorter prerelease is lower
        if (i >= other_components.size()) return 1;  // Longer prerelease is higher

        if (this_components[i] < other_components[i]) return -1;
        if (this_components[i] > other_components[i]) return 1;
    }

    return 0;
}

bool SemanticVersion::operator<(const SemanticVersion& other) const {
    int component_diff = compare_components(other);
    if (component_diff != 0) return component_diff < 0;
    return compare_prerelease(other) < 0;
}

bool SemanticVersion::operator<=(const SemanticVersion& other) const {
    return *this < other || *this == other;
}

bool SemanticVersion::operator>(const SemanticVersion& other) const {
    return !(*this <= other);
}

bool SemanticVersion::operator>=(const SemanticVersion& other) const {
    return !(*this < other);
}

bool SemanticVersion::is_compatible_with(const SemanticVersion& required) const {
    if (major != required.major) return false; // Major version must match

    if (major == 0) {
        // For version 0.x.x, all changes are breaking
        return *this == required;
    }

    // For major >= 1, minor version can be >= required
    return minor >= required.minor;
}

bool SemanticVersion::is_prerelease() const {
    return !prerelease.empty();
}

std::vector<SemanticVersion::PrereleaseComponent> SemanticVersion::parse_prerelease(const std::string& prerelease) const {
    std::vector<PrereleaseComponent> components;

    if (prerelease.empty()) return components;

    // Split by dots for multi-component prereleases
    std::stringstream ss(prerelease);
    std::string part;

    while (std::getline(ss, part, '.')) {
        PrereleaseComponent component;

        // Check if numeric
        if (std::all_of(part.begin(), part.end(), ::isdigit)) {
            component.type = PrereleaseComponent::Type::NUMBER;
            component.number = std::stoi(part);
            component.value = part;
        } else {
            component.type = PrereleaseComponent::Type::IDENTIFIER;
            component.value = part;
        }

        components.push_back(component);
    }

    return components;
}

bool SemanticVersion::PrereleaseComponent::operator<(const PrereleaseComponent& other) const {
    // Numeric < identifier
    if (type == Type::NUMBER && other.type == Type::IDENTIFIER) return true;
    if (type == Type::IDENTIFIER && other.type == Type::NUMBER) return false;

    // Same type: compare
    if (type == Type::NUMBER && other.type == Type::NUMBER) {
        return number < other.number;
    }

    // Identifiers: lexicographic comparison
    return value < other.value;
}

bool SemanticVersion::PrereleaseComponent::operator==(const PrereleaseComponent& other) const {
    return type == other.type && value == other.value;
}

// VersionConstraint implementation
std::string VersionConstraint::to_string() const {
    switch (op) {
        case Operator::EXACT:          return "==" + version.to_string();
        case Operator::GREATER:        return ">" + version.to_string();
        case Operator::GREATER_EQUAL:  return ">=" + version.to_string();
        case Operator::LESSER:         return "<" + version.to_string();
        case Operator::LESSER_EQUAL:   return "<=" + version.to_string();
        case Operator::CARET:          return "^" + version.to_string();
        case Operator::TILDE:          return "~" + version.to_string();
        case Operator::WILDCARD: {
            std::string ver_str = version.to_string();
            size_t last_dot = ver_str.find_last_of('.');
            if (last_dot != std::string::npos) {
                return ver_str.substr(0, last_dot) + ".*";
            }
            return ver_str + ".*";
        }
        case Operator::RANGE:          return version.to_string() + " - " + upper_bound.to_string();
        default:                       return version.to_string();
    }
}

bool VersionConstraint::accepts(const SemanticVersion& candidate) const {
    switch (op) {
        case Operator::EXACT:
            return candidate == version;

        case Operator::GREATER:
            return candidate > version;

        case Operator::GREATER_EQUAL:
            return candidate >= version;

        case Operator::LESSER:
            return candidate < version;

        case Operator::LESSER_EQUAL:
            return candidate <= version;

        case Operator::CARET:
            return candidate >= version && candidate.is_compatible_with(version);

        case Operator::TILDE:
            // Tilde allows patch-level changes if specified, minor-level if not
            if (version.minor == 0 && version.patch == 0) {
                return candidate.major == version.major; // ~1.0.0 only allows major
            }
            return candidate.major == version.major && candidate.minor == version.minor;

        case Operator::WILDCARD:
            return candidate.major == version.major &&
                   (version.minor == -1 || candidate.minor == version.minor);

        case Operator::RANGE:
            return candidate >= version && candidate <= upper_bound;

        default:
            return false;
    }
}

bool VersionConstraint::is_valid() const {
    return version.is_valid() &&
           (op != Operator::RANGE || upper_bound.is_valid()) &&
           (op == Operator::EXACT || op == Operator::GREATER || op == Operator::GREATER_EQUAL ||
            op == Operator::LESSER || op == Operator::LESSER_EQUAL || op == Operator::CARET ||
            op == Operator::TILDE || op == Operator::WILDCARD || op == Operator::RANGE);
}

std::vector<VersionConstraint> VersionConstraint::parse_range(const std::string& range_string) {
    std::vector<VersionConstraint> constraints;

    // Handle OR conditions (split by ||)
    std::vector<std::string> or_parts;
    std::stringstream ss(range_string);
    std::string part;

    while (std::getline(ss, part, '|')) {
        // Remove spaces and handle ||
        std::string trimmed = part;
        trimmed.erase(std::remove_if(trimmed.begin(), trimmed.end(), ::isspace), trimmed.end());
        if (!trimmed.empty()) {
            or_parts.push_back(trimmed);
        }
    }

    for (const auto& or_part : or_parts) {
        // Handle AND conditions (split by spaces)
        std::vector<std::string> and_parts;
        std::stringstream ss2(or_part);
        std::string and_part;

        while (ss2 >> and_part) {
            and_parts.push_back(and_part);
        }

        for (const auto& constraint_str : and_parts) {
            constraints.push_back(from_string(constraint_str));
        }
    }

    return constraints;
}

VersionConstraint VersionConstraint::from_string(const std::string& constraint_string) {
    std::string str = constraint_string;

    // Remove leading 'v' if present
    if (str.starts_with("v")) {
        str = str.substr(1);
    }

    // Exact versions (no operator prefix)
    if (std::regex_match(str, std::regex(R"(^\d+\.\d+\.\d+.*$)"))) {
        return VersionConstraint(Operator::EXACT, SemanticVersion::parse(str));
    }

    // Operator constraints
    if (str.starts_with("==")) {
        return VersionConstraint(Operator::EXACT, SemanticVersion::parse(str.substr(2)));
    }
    if (str.starts_with(">=")) {
        return VersionConstraint(Operator::GREATER_EQUAL, SemanticVersion::parse(str.substr(2)));
    }
    if (str.starts_with(">")) {
        return VersionConstraint(Operator::GREATER, SemanticVersion::parse(str.substr(1)));
    }
    if (str.starts_with("<=")) {
        return VersionConstraint(Operator::LESSER_EQUAL, SemanticVersion::parse(str.substr(2)));
    }
    if (str.starts_with("<")) {
        return VersionConstraint(Operator::LESSER, SemanticVersion::parse(str.substr(1)));
    }
    if (str.starts_with("^")) {
        return VersionConstraint(Operator::CARET, SemanticVersion::parse(str.substr(1)));
    }
    if (str.starts_with("~")) {
        return VersionConstraint(Operator::TILDE, SemanticVersion::parse(str.substr(1)));
    }

    // Wildcard and range
    if (str.contains("-")) {
        size_t dash_pos = str.find(" - ");
        if (dash_pos != std::string::npos) {
            std::string lower = str.substr(0, dash_pos);
            std::string upper = str.substr(dash_pos + 3);
            return VersionConstraint(Operator::RANGE,
                                   SemanticVersion::parse(lower),
                                   SemanticVersion::parse(upper));
        }
    }

    if (str.contains("*")) {
        std::string version_part = str.substr(0, str.find('*'));
        return VersionConstraint(Operator::WILDCARD, SemanticVersion::parse(version_part));
    }

    // Default to exact
    return VersionConstraint(Operator::EXACT, SemanticVersion::parse(str));
}

// PluginDependency implementation
nlohmann::json PluginDependency::to_json() const {
    nlohmann::json j;
    j["plugin_id"] = plugin_id;
    j["display_name"] = display_name;
    j["version_constraint"] = version_constraint.to_string();
    j["optional"] = optional;
    j["reason"] = reason;
    j["provides"] = provides;
    j["conflicts_with"] = conflicts_with;
    return j;
}

PluginDependency PluginDependency::from_json(const nlohmann::json& j) {
    PluginDependency dependency;
    dependency.plugin_id = j.value("plugin_id", "");
    dependency.display_name = j.value("display_name", "");
    dependency.version_constraint = VersionConstraint::from_string(j.value("version_constraint", ""));
    dependency.optional = j.value("optional", false);
    dependency.reason = j.value("reason", "");
    dependency.provides = j.value("provides", std::vector<std::string>{});
    dependency.conflicts_with = j.value("conflicts_with", "");
    return dependency;
}

bool PluginDependency::is_compatible_with(const SemanticVersion& version) const {
    return version_constraint.accepts(version);
}

// DependencyNode implementation
nlohmann::json DependencyNode::to_json() const {
    nlohmann::json j;
    j["plugin_id"] = plugin_id;
    j["selected_version"] = selected_version.to_string();
    j["package"] = package.to_json();
    j["dependencies"] = dependencies;
    j["depth"] = depth;
    j["is_optional"] = is_optional;
    return j;
}

DependencyNode DependencyNode::from_json(const nlohmann::json& j) {
    DependencyNode node;
    node.plugin_id = j.value("plugin_id", "");
    node.selected_version = SemanticVersion::parse(j.value("selected_version", ""));
    node.package = PluginPackage::from_json(j.value("package", nlohmann::json{}));
    node.dependencies = j.value("dependencies", std::vector<std::string>{});
    node.depth = j.value("depth", 0);
    node.is_optional = j.value("is_optional", false);
    return node;
}

// DependencyConflict implementation
std::string DependencyConflict::to_string() const {
    std::stringstream ss;
    ss << "Conflict: ";

    switch (type) {
        case Type::VERSION_CONFLICT:
            ss << "Version conflict for " << dependency_id;
            if (!conflicting_versions.empty()) {
                ss << " (versions: ";
                for (size_t i = 0; i < conflicting_versions.size(); ++i) {
                    if (i > 0) ss << " vs ";
                    ss << conflicting_versions[i].to_string();
                }
                ss << ")";
            }
            break;
        case Type::CIRCULAR_DEPENDENCY:
            ss << "Circular dependency detected: ";
            for (size_t i = 0; i < conflicting_plugins.size(); ++i) {
                if (i > 0) ss << " -> ";
                ss << conflicting_plugins[i];
            }
            ss << " -> " << conflicting_plugins[0];
            break;
        case Type::MISSING_DEPENDENCY:
            ss << "Missing dependency: " << dependency_id;
            break;
        case Type::MUTUALLY_EXCLUSIVE:
            ss << "Mutually exclusive plugins: ";
            for (size_t i = 0; i < conflicting_plugins.size(); ++i) {
                if (i > 0) ss << " and ";
                ss << conflicting_plugins[i];
            }
            break;
        case Type::INSUFFICIENT_VERSION:
            ss << "Insufficient version for " << dependency_id;
            break;
    }

    if (!description.empty()) {
        ss << " - " << description;
    }

    return ss.str();
}

nlohmann::json DependencyConflict::to_json() const {
    nlohmann::json j;
    j["type"] = static_cast<int>(type);
    j["conflicting_plugins"] = conflicting_plugins;
    j["dependency_id"] = dependency_id;
    j["description"] = description;

    nlohmann::json versions_json = nlohmann::json::array();
    for (const auto& version : conflicting_versions) {
        versions_json.push_back(version.to_string());
    }
    j["conflicting_versions"] = versions_json;

    if (suggested_resolution.has_value()) {
        j["suggested_resolution"] = suggested_resolution->to_string();
    }

    return j;
}

// ResolutionResult implementation
ResolutionResult ResolutionResult::success(const std::vector<DependencyNode>& plugins) {
    ResolutionResult result;
    result.resolution_success = true;
    result.resolved_plugins = plugins;
    result.dependencies_resolved = plugins.size();
    return result;
}

ResolutionResult ResolutionResult::failure(const std::vector<DependencyConflict>& conflicts) {
    ResolutionResult result;
    result.success = false;
    result.conflicts = conflicts;
    return result;
}

// VersionResolver implementation
VersionResolver::VersionResolver(const ResolverConfig& config) : config_(config) {
    last_resolution_time_ = std::chrono::system_clock::now();
}

std::future<ResolutionResult> VersionResolver::resolve_dependencies(const std::vector<PluginPackage>& root_plugins) {
    return std::async(std::launch::async, [this, root_plugins]() -> ResolutionResult {
        std::lock_guard<std::mutex> lock(resolver_mutex_);

        // Clear state
        visited_plugins_.clear();
        current_resolution_.clear();
        current_conflicts_.clear();

        try {
            return resolve_dependencies_internal(root_plugins);
        } catch (const std::exception& e) {
            log_resolution_step("error", "Resolution failed: " + std::string(e.what()));
            return ResolutionResult::failure({});
        }
    });
}

ResolutionResult VersionResolver::resolve_dependencies_internal(const std::vector<PluginPackage>& root_plugins) {
    log_resolution_step("start", "Resolving dependencies for " + std::to_string(root_plugins.size()) + " plugins");

    ResolutionResult result;

    // Process each root plugin
    for (const auto& package : root_plugins) {
        if (!visit_dependency_node(package.id, SemanticVersion::parse(package.version), 0)) {
            result.conflicts.insert(result.conflicts.end(),
                                  current_conflicts_.begin(),
                                  current_conflicts_.end());
            return result;
        }
    }

    // Build result from current_resolution_
    for (const auto& [plugin_id, node] : current_resolution_) {
        result.resolved_plugins.push_back(node);
    }

    result.resolution_success = true;
    result.total_plugins_processed = visited_plugins_.size();
    result.dependencies_resolved = current_resolution_.size();

    log_resolution_step("success", "Successfully resolved " + std::to_string(result.dependencies_resolved) + " dependencies");
    return result;
}

bool VersionResolver::visit_dependency_node(const std::string& plugin_id,
                                          const SemanticVersion& version,
                                          int depth) {
    // Check for circular dependencies
    if (current_resolution_.count(plugin_id) && current_resolution_[plugin_id].in_path) {
        DependencyConflict conflict;
        conflict.type = DependencyConflict::Type::CIRCULAR_DEPENDENCY;
        conflict.description = "Circular dependency detected";
        current_conflicts_.push_back(conflict);
        return false;
    }

    // Check depth limit
    if (depth > config_.max_resolution_depth) {
        DependencyConflict conflict;
        conflict.type = DependencyConflict::Type::VERSION_CONFLICT;
        conflict.description = "Maximum dependency depth exceeded";
        current_conflicts_.push_back(conflict);
        return false;
    }

    // Mark as visited and in current path
    visited_plugins_.insert(plugin_id);

    DependencyNode node;
    node.plugin_id = plugin_id;
    node.selected_version = version;
    node.depth = depth;
    node.visited = true;
    node.in_path = true;

    // Get plugin package information
    auto cached_versions = get_cached_versions(plugin_id);
    if (!cached_versions.has_value()) {
        // Would fetch from registry here - simplified for now
        log_resolution_step("info", "Could not fetch versions for " + plugin_id);
        return false;
    }

    // Find matching version
    PluginPackage selected_package;
    bool found = false;
    for (const auto& package : cached_versions.value()) {
        if (SemanticVersion::parse(package.version) == version) {
            selected_package = package;
            found = true;
            break;
        }
    }

    if (!found) {
        DependencyConflict conflict;
        conflict.type = DependencyConflict::Type::MISSING_DEPENDENCY;
        conflict.dependency_id = plugin_id;
        conflict.description = "Required version not found: " + version.to_string();
        current_conflicts_.push_back(conflict);
        node.in_path = false;
        return false;
    }

    node.package = selected_package;
    current_resolution_[plugin_id] = node;

    // Process dependencies
    for (const auto& dep_id : selected_package.dependencies) {
        if (!visit_dependency_node(dep_id, SemanticVersion::parse("1.0.0"), depth + 1)) {
            node.in_path = false;
            return false;
        }
    }

    node.in_path = false;
    return true;
}

void VersionResolver::set_registry(std::shared_ptr<GitHubRegistry> registry) {
    registry_ = std::move(registry);
}

void VersionResolver::set_downloader(std::shared_ptr<PluginDownloader> downloader) {
    downloader_ = std::move(downloader);
}

void VersionResolver::log_resolution_step(const std::string& step, const std::string& details) const {
    if (config_.enable_resolution_logging) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);

        std::cout << "[" << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S")
                  << "] [resolver:" << step << "] " << details << std::endl;
    }
}

bool VersionResolver::satisfies_constraint(const SemanticVersion& version, const VersionConstraint& constraint) const {
    return constraint.accepts(version);
}

nlohmann::json VersionResolver::get_resolution_statistics() const {
    nlohmann::json stats;
    stats["total_resolutions"] = resolution_stats_["total"] + 0;
    stats["successful_resolutions"] = resolution_stats_["success"] + 0;
    stats["failed_resolutions"] = resolution_stats_["failure"] + 0;
    stats["cache_hits"] = resolution_stats_["cache_hit"] + 0;
    stats["cache_size"] = version_cache_.size() + resolution_cache_.size();
    return stats;
}

} // namespace distribution
} // namespace aimux