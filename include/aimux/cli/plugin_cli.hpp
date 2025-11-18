#pragma once

#include <string>
#include <vector>
#include <memory>
#include <future>
#include <functional>
#include <map>
#include <chrono>
#include <iostream>
#include <sstream>

#include "aimux/distribution/github_registry.hpp"
#include "aimux/distribution/plugin_downloader.hpp"
#include "aimux/distribution/version_resolver.hpp"

namespace aimux::cli {

/**
 * @brief CLI command types for plugin management
 */
enum class PluginCommand {
    INSTALL,
    REMOVE,
    SEARCH,
    UPDATE,
    LIST,
    INFO,
    DEPENDENCIES,
    ROLLBACK,
    CLEANUP,
    STATUS
};

/**
 * @brief CLI result with success status and message
 */
struct CLIResult {
    bool success;
    std::string message;
    std::string details;
    int exit_code;

    static CLIResult success(const std::string& message = "") {
        return {true, message, "", 0};
    }

    static CLIResult error(const std::string& error, int code = 1) {
        return {false, error, "", code};
    }

    static CLIResult error_with_details(const std::string& error, const std::string& details, int code = 1) {
        return {false, error, details, code};
    }
};

/**
 * @brief CLI configuration for command execution
 */
struct CLIConfig {
    bool verbose = false;
    bool quiet = false;
    bool interactive = true;
    bool force = false;
    bool dry_run = false;

    std::vector<std::string> organizations = {"aimux-org", "aimux-plugins"};
    std::string config_directory = "~/.config/aimux";
    std::string plugin_directory = "~/.config/aimux/plugins";
    std::string cache_directory = "~/.config/aimux/cache";

    std::chrono::seconds timeout{300};
    size_t max_concurrent_downloads = 3;

    // Security settings
    bool verify_checksums = true;
    bool verify_signatures = false;
    bool enable_security_validation = true;
    std::vector<std::string> blocked_plugins;

    // Display settings
    bool show_dependencies = true;
    bool show_versions = true;
    bool show_progress = true;
    bool show_warnings = true;
};

/**
 * @brief Plugin search result
 */
struct SearchResult {
    std::string id;
    std::string name;
    std::string description;
    std::string latest_version;
    std::string owner;
    size_t download_count;
    std::chrono::system_clock::time_point updated_at;
    std::vector<std::string> tags;

    bool matches_query(const std::string& query) const {
        std::string lower_query;
        lower_query.reserve(query.length());
        for (char c : query) {
            lower_query += std::tolower(c);
        }

        std::string lower_name, lower_desc;
        lower_name.reserve(name.length());
        lower_desc.reserve(description.length());
        for (char c : name) lower_name += std::tolower(c);
        for (char c : description) lower_desc += std::tolower(c);

        return lower_name.find(lower_query) != std::string::npos ||
               lower_desc.find(lower_query) != std::string::npos ||
               lower_tags_find(lower_query);
    }

private:
    bool lower_tags_find(const std::string& query) const {
        for (const auto& tag : tags) {
            std::string lower_tag;
            lower_tag.reserve(tag.length());
            for (char c : tag) lower_tag += std::tolower(c);
            if (lower_tag.find(query) != std::string::npos) {
                return true;
            }
        }
        return false;
    }
};

/**
 * @brief Installation plan with dependency analysis
 */
struct InstallationPlan {
    std::vector<PluginPackage> plugins_to_install;
    std::vector<PluginPackage> plugins_to_update;
    std::vector<aimux::distribution::DependencyConflict> conflicts;
    std::vector<std::string> warnings;

    size_t total_size() const {
        size_t total = 0;
        for (const auto& plugin : plugins_to_install) {
            total += plugin.file_size;
        }
        for (const auto& plugin : plugins_to_update) {
            total += plugin.file_size;
        }
        return total;
    }

    bool has_conflicts() const {
        return !conflicts.empty();
    }

    bool has_warnings() const {
        return !warnings.empty();
    }
};

/**
 * @brief Progress callback for download/installation operations
 */
using ProgressCallback = std::function<void(const std::string& operation,
                                           const std::string& item,
                                           size_t current,
                                           size_t total,
                                           const std::string& status)>;

/**
 * @brief Interactive prompt callback for user confirmation
 */
using PromptCallback = std::function<bool(const std::string& prompt,
                                         const std::string& details,
                                         const std::vector<std::string>& options)>;

/**
 * @brief Main CLI plugin manager
 */
class PluginCLIManager {
public:
    explicit PluginCLIManager(const CLIConfig& config = {});
    ~PluginCLIManager() = default;

    // Initialize the CLI manager and underlying components
    std::future<CLIResult> initialize();

    // Core CLI commands
    std::future<CLIResult> install(const std::vector<std::string>& plugins,
                                   const std::string& version = "latest");
    std::future<CLIResult> remove(const std::vector<std::string>& plugins);
    std::future<CLIResult> search(const std::string& query,
                                 size_t limit = 20);
    std::future<CLIResult> update(const std::vector<std::string>& plugins = {});
    std::future<CLIResult> list(const std::vector<std::string>& filters = {});
    std::future<CLIResult> info(const std::string& plugin);
    std::future<CLIResult> dependencies(const std::string& plugin);
    std::future<CLIResult> rollback(const std::string& plugin,
                                   const std::string& version);
    std::future<CLIResult> cleanup();
    std::future<CLIResult> status();

    // Advanced operations
    std::future<InstallationPlan> create_installation_plan(
        const std::vector<std::string>& plugins,
        const std::vector<std::string>& versions = {});

    std::future<CLIResult> execute_plan(const InstallationPlan& plan);
    std::future<CLIResult> validate_dependencies(const std::vector<std::string>& plugins);

    // Configuration management
    CLIConfig get_config() const { return config_; }
    std::future<CLIResult> update_config(const CLIConfig& new_config);

    // Callback management
    void set_progress_callback(ProgressCallback callback) { progress_callback_ = std::move(callback); }
    void set_prompt_callback(PromptCallback callback) { prompt_callback_ = std::move(callback); }

    // Statistics and diagnostics
    std::future<std::map<std::string, std::string>> get_statistics();
    std::future<std::vector<std::string>> diagnose_issues();

private:
    CLIConfig config_;

    // Core distribution components
    std::unique_ptr<aimux::distribution::GitHubRegistry> registry_;
    std::unique_ptr<aimux::distribution::PluginDownloader> downloader_;
    std::unique_ptr<aimux::distribution::VersionResolver> resolver_;

    // Callbacks
    ProgressCallback progress_callback_;
    PromptCallback prompt_callback_;

    // Helper methods
    std::future<CLIResult> ensure_initialized();
    std::vector<SearchResult> convert_to_search_results(const std::vector<aimux::distribution::PluginPackage>& packages);
    std::string format_size(size_t bytes) const;
    std::string format_duration(std::chrono::seconds duration) const;

    // Interactive prompts
    bool confirm_installation_plan(const InstallationPlan& plan);
    bool confirm_removal(const std::vector<std::string>& plugins);
    std::string select_version(const std::vector<aimux::distribution::SemanticVersion>& available_versions);

    // Progress reporting
    void report_progress(const std::string& operation,
                        const std::string& item,
                        size_t current,
                        size_t total,
                        const std::string& status = "");

    // Error handling
    CLIResult handle_exception(const std::string& context, const std::exception& e);
    CLIResult handle_dependency_conflicts(const std::vector<aimux::distribution::DependencyConflict>& conflicts);

    // Validation
    std::future<bool> validate_plugin_id(const std::string& plugin_id);
    std::future<bool> validate_version(const std::string& version);
};

/**
 * @brief CLI command parser and dispatcher
 */
class PluginCLICommandDispatcher {
public:
    explicit PluginCLICommandDispatcher(std::shared_ptr<PluginCLIManager> manager);
    ~PluginCLICommandDispatcher() = default;

    // Parse command line arguments and execute
    CLIResult execute(int argc, char* argv[]);

    // Help and usage
    std::string get_usage() const;
    std::string get_command_help(PluginCommand command) const;

private:
    std::shared_ptr<PluginCLIManager> manager_;

    // Command parsing
    struct ParsedCommand {
        PluginCommand command;
        std::map<std::string, std::string> options;
        std::vector<std::string> arguments;
    };

    ParsedCommand parse_args(int argc, char* argv[]);
    CLIResult execute_command(const ParsedCommand& parsed);

    // Command handlers
    CLIResult handle_install(const ParsedCommand& parsed);
    CLIResult handle_remove(const ParsedCommand& parsed);
    CLIResult handle_search(const ParsedCommand& parsed);
    CLIResult handle_update(const ParsedCommand& parsed);
    CLIResult handle_list(const ParsedCommand& parsed);
    CLIResult handle_info(const ParsedCommand& parsed);
    CLIResult handle_dependencies(const ParsedCommand& parsed);
    CLIResult handle_rollback(const ParsedCommand& parsed);
    CLIResult handle_cleanup(const ParsedCommand& parsed);
    CLIResult handle_status(const ParsedCommand& parsed);
};

/**
 * @brief Utility functions for CLI formatting and display
 */
namespace cli_utils {
    // Text formatting
    std::string colorize(const std::string& text, const std::string& color);
    std::string bold(const std::string& text);
    std::string dim(const std::string& text);
    std::string success_color(const std::string& text);
    std::string warning_color(const std::string& text);
    std::string error_color(const std::string& text);

    // Table formatting
    struct TableColumn {
        std::string header;
        size_t width;
        bool align_right = false;

        TableColumn(const std::string& h, size_t w = 20, bool align = false)
            : header(h), width(w), align_right(align) {}
    };

    std::string format_table(const std::vector<TableColumn>& columns,
                           const std::vector<std::vector<std::string>>& rows);

    // Plugin information formatting
    std::string format_plugin_info(const PluginPackage& plugin);
    std::string format_installation_plan(const InstallationPlan& plan);
    std::string format_conflicts(const std::vector<aimux::distribution::DependencyConflict>& conflicts);

    // Progress bar
    std::string create_progress_bar(size_t current, size_t total, size_t width = 40);
}

// ============================================================================
// Batch Operations Configuration
// ============================================================================

struct BatchConfig {
    bool parallel = false;
    size_t max_parallel_installs = 3;
    bool stop_on_error = false;
    bool confirm_updates = true;
    std::chrono::milliseconds install_delay{1000};
};

struct ExportOptions {
    bool include_dependencies = true;
    bool include_metadata = true;
    bool include_versions = true;
};

// ============================================================================
// Interactive Installation Manager
// ============================================================================

class InteractiveInstallationManager {
public:
    explicit InteractiveInstallationManager(std::shared_ptr<PluginCLIManager> manager);
    ~InteractiveInstallationManager() = default;

    std::future<CLIResult> install_interactive(const std::vector<std::string>& initial_plugins = {});
    bool is_interactive() const { return is_interactive_; }

private:
    std::shared_ptr<PluginCLIManager> manager_;
    std::unique_ptr<class InteractiveInstallationHandler> handler_;
    bool is_interactive_;
};

// ============================================================================
// Batch Operations Manager
// ============================================================================

class BatchOperationsManager {
public:
    explicit BatchOperationsManager(std::shared_ptr<PluginCLIManager> manager);
    ~BatchOperationsManager() = default;

    std::future<CLIResult> load_from_manifest(const std::string& manifest_path);
    std::future<CLIResult> batch_install(const std::vector<std::string>& plugins,
                                        const BatchConfig& config = {});
    std::future<CLIResult> batch_update(const std::vector<std::string>& plugins,
                                       const BatchConfig& config = {});
    std::future<CLIResult> export_manifest(const std::string& output_path,
                                          const ExportOptions& options = {});
    std::future<CLIResult> validate_plugin_set(const std::vector<std::string>& plugins);

    void cancel_operation();
    bool is_operation_running() const;

private:
    std::shared_ptr<PluginCLIManager> manager_;
    std::atomic<bool> running_;

    CLIResult batch_install_sequential(const std::vector<std::string>& plugins,
                                     const BatchConfig& config);
    CLIResult batch_install_parallel(const std::vector<std::string>& plugins,
                                    const BatchConfig& config);
    bool confirm_batch_updates(const std::vector<std::pair<std::string, std::string>>& candidates);
    CLIResult execute_batch_updates(const std::vector<std::pair<std::string, std::string>>& candidates,
                                   const BatchConfig& config);
};

// ============================================================================
// Configuration Manager
// ============================================================================

class ConfigManager {
public:
    ConfigManager(std::shared_ptr<PluginCLIManager> manager,
                  const std::string& config_file = "~/.config/aimux/plugin_config.json");
    ~ConfigManager() = default;

    std::future<CLIResult> load_config();
    std::future<CLIResult> save_config(const CLIConfig& config);
    std::future<CLIResult> reset_config();

private:
    std::shared_ptr<PluginCLIManager> manager_;
    std::string config_file_;
};

} // namespace aimux::cli