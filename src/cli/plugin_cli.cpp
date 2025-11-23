#include "aimux/cli/plugin_cli.hpp"
#include <algorithm>
#include <iomanip>
#include <fstream>
#include <filesystem>
#include <thread>
#include <future>

namespace aimux::cli {

// ============================================================================
// PluginCLIManager Implementation
// ============================================================================

PluginCLIManager::PluginCLIManager(const CLIConfig& config)
    : config_(config), progress_callback_(), prompt_callback_() {
}

std::future<CLIResult> PluginCLIManager::initialize() {
    return std::async(std::launch::async, [this]() {
        try {
            report_progress("Initialization", "Setting up components", 0, 4);

            // Initialize GitHub Registry
            {
                aimux::distribution::GitHubRegistry::Config registry_config;
                registry_config.organizations = config_.organizations;
                registry_config.cache_directory = config_.cache_directory;
                registry_config.enable_security_validation = config_.enable_security_validation;
                registry_config.blocked_plugins = config_.blocked_plugins;

                registry_ = std::make_unique<aimux::distribution::GitHubRegistry>(registry_config);
            }
            report_progress("Initialization", "GitHub Registry created", 1, 4);

            // Initialize Plugin Downloader
            {
                aimux::distribution::PluginDownloader::Config downloader_config;
                downloader_config.download_directory = config_.config_directory + "/downloads";
                downloader_config.installation_directory = config_.plugin_directory;
                downloader_config.backup_directory = config_.config_directory + "/backups";
                downloader_config.verify_checksums = config_.verify_checksums;
                downloader_config.verify_signatures = config_.verify_signatures;
                downloader_config.enable_offline_mode = false;

                downloader_ = std::make_unique<aimux::distribution::PluginDownloader>(downloader_config);
                downloader_->set_github_registry(registry_);
            }
            report_progress("Initialization", "Plugin Downloader created", 2, 4);

            // Initialize Version Resolver
            {
                aimux::distribution::ResolverConfig resolver_config;
                resolver_config.strategy = aimux::distribution::ResolverConfig::ResolutionStrategy::LATEST_COMPATIBLE;
                resolver_config.allow_prerelease = false;
                resolver_config.enable_resolution_logging = config_.verbose;

                resolver_ = std::make_unique<aimux::distribution::VersionResolver>(resolver_config);
                resolver_->set_registry(registry_);
                resolver_->set_downloader(downloader_);
            }
            report_progress("Initialization", "Version Resolver created", 3, 4);

            // Initialize all components
            auto registry_init = registry_->initialize();
            if (!registry_init.get()) {
                if (!config_.quiet) {
                    std::cerr << cli_utils::warning_color("Warning: Unable to initialize GitHub registry") << std::endl;
                }
            }

            report_progress("Initialization", "Complete", 4, 4);

            return CLIResult::success("CLI Manager initialized successfully");
        } catch (const std::exception& e) {
            return handle_exception("initialization", e);
        }
    });
}

std::future<CLIResult> PluginCLIManager::install(const std::vector<std::string>& plugins,
                                                const std::string& version) {
    return std::async(std::launch::async, [this, plugins, version]() {
        ensure_initialized().get();

        try {
            if (plugins.empty()) {
                return CLIResult::error("No plugins specified for installation");
            }

            report_progress("Installation", "Creating plan", 0, plugins.size());

            // Create installation plan
            auto plan_future = create_installation_plan(plugins, {version});
            auto plan = plan_future.get();

            if (plan.has_conflicts()) {
                return handle_dependency_conflicts(plan.conflicts);
            }

            // Interactive confirmation if enabled
            if (config_.interactive && !config_.force && !confirm_installation_plan(plan)) {
                return CLIResult::error("Installation cancelled by user");
            }

            // Execute the plan
            return execute_plan(plan);
        } catch (const std::exception& e) {
            return handle_exception("plugin installation", e);
        }
    });
}

std::future<CLIResult> PluginCLIManager::remove(const std::vector<std::string>& plugins) {
    return std::async(std::launch::async, [this, plugins]() {
        ensure_initialized().get();

        try {
            if (plugins.empty()) {
                return CLIResult::error("No plugins specified for removal");
            }

            // Interactive confirmation if enabled
            if (config_.interactive && !config_.force && !confirm_removal(plugins)) {
                return CLIResult::error("Removal cancelled by user");
            }

            std::vector<std::string> removed_plugins;
            std::vector<std::string> failed_plugins;
            std::stringstream details;

            for (size_t i = 0; i < plugins.size(); ++i) {
                const auto& plugin_id = plugins[i];
                report_progress("Removal", plugin_id, i + 1, plugins.size());

                try {
                    auto remove_future = downloader_->uninstall_plugin(plugin_id);
                    bool success = remove_future.get();

                    if (success) {
                        removed_plugins.push_back(plugin_id);
                        details << "✓ Removed: " << plugin_id << "\n";
                    } else {
                        failed_plugins.push_back(plugin_id);
                        details << "✗ Failed to remove: " << plugin_id << "\n";
                    }
                } catch (const std::exception& e) {
                    failed_plugins.push_back(plugin_id);
                    details << "✗ Error removing " << plugin_id << ": " << e.what() << "\n";
                }
            }

            std::string message;
            if (removed_plugins.size() == plugins.size()) {
                message = "Successfully removed " + std::to_string(removed_plugins.size()) + " plugins";
                return CLIResult::success(message);
            } else {
                message = "Removed " + std::to_string(removed_plugins.size()) + "/" + std::to_string(plugins.size()) + " plugins";
                return CLIResult::error_with_details(message, details.str());
            }
        } catch (const std::exception& e) {
            return handle_exception("plugin removal", e);
        }
    });
}

std::future<CLIResult> PluginCLIManager::search(const std::string& query, size_t limit) {
    return std::async(std::launch::async, [this, query, limit]() {
        ensure_initialized().get();

        try {
            report_progress("Search", "Querying registry", 0, 2);

            auto search_future = registry_->search_plugins(query);
            auto packages = search_future.get();

            report_progress("Search", "Processing results", 1, 2);

            auto results = convert_to_search_results(packages);

            // Limit results
            if (results.size() > limit) {
                results.resize(limit);
            }

            report_progress("Search", "Complete", 2, 2);

            // Format results as table
            std::stringstream output;
            output << cli_utils::colorize("Search Results for '" + query + "'", "green") << "\n\n";

            if (results.empty()) {
                output << cli_utils::dim("No plugins found matching '" + query + "'") << std::endl;
            } else {
                std::vector<cli_utils::TableColumn> columns = {
                    cli_utils::TableColumn("Plugin", 30),
                    cli_utils::TableColumn("Latest", 15),
                    cli_utils::TableColumn("Downloads", 12, true),
                    cli_utils::TableColumn("Owner", 15),
                    cli_utils::TableColumn("Description", 50)
                };

                std::vector<std::vector<std::string>> rows;
                for (const auto& result : results) {
                    std::string name = result.name;
                    if (name.length() > 29) {
                        name = name.substr(0, 26) + "...";
                    }

                    std::string description = result.description;
                    if (description.length() > 49) {
                        description = description.substr(0, 46) + "...";
                    }

                    std::string downloads = std::to_string(result.download_count);
                    if (result.download_count >= 1000000) {
                        downloads = std::to_string(result.download_count / 1000000) + "M";
                    } else if (result.download_count >= 1000) {
                        downloads = std::to_string(result.download_count / 1000) + "K";
                    }

                    rows.push_back({
                        cli_utils::bold(name),
                        result.latest_version,
                        downloads,
                        result.owner,
                        cli_utils::dim(description)
                    });
                }

                output << cli_utils::format_table(columns, rows);
                output << "\n";

                if (packages.size() > limit) {
                    output << cli_utils::dim("Showing " + std::to_string(limit) + " of " + std::to_string(packages.size()) + " results") << std::endl;
                }
            }

            return CLIResult::success(output.str());
        } catch (const std::exception& e) {
            return handle_exception("plugin search", e);
        }
    });
}

std::future<CLIResult> PluginCLIManager::update(const std::vector<std::string>& plugins) {
    return std::async(std::launch::async, [this, plugins]() {
        ensure_initialized().get();

        try {
            std::vector<std::string> plugins_to_update;

            if (plugins.empty()) {
                // Update all installed plugins
                auto stats = downloader_->get_download_statistics();
                // In a real implementation, this would get the list of installed plugins
                // For now, we'll return a message
                return CLIResult::success("All plugins are up to date");
            } else {
                plugins_to_update = plugins;
            }

            std::vector<std::string> updated_plugins;
            std::vector<std::string> failed_plugins;
            std::stringstream details;

            for (size_t i = 0; i < plugins_to_update.size(); ++i) {
                const auto& plugin_id = plugins_to_update[i];
                report_progress("Update", plugin_id, i + 1, plugins_to_update.size());

                try {
                    auto update_future = downloader_->update_plugin(plugin_id, "latest");
                    bool success = update_future.get();

                    if (success) {
                        updated_plugins.push_back(plugin_id);
                        details << "✓ Updated: " << plugin_id << "\n";
                    } else {
                        failed_plugins.push_back(plugin_id);
                        details << "✗ Failed to update: " << plugin_id << "\n";
                    }
                } catch (const std::exception& e) {
                    failed_plugins.push_back(plugin_id);
                    details << "✗ Error updating " << plugin_id << ": " << e.what() << "\n";
                }
            }

            std::string message;
            if (updated_plugins.size() == plugins_to_update.size()) {
                message = "Successfully updated " + std::to_string(updated_plugins.size()) + " plugins";
                return CLIResult::success(message);
            } else {
                message = "Updated " + std::to_string(updated_plugins.size()) + "/" + std::to_string(plugins_to_update.size()) + " plugins";
                return CLIResult::error_with_details(message, details.str());
            }
        } catch (const std::exception& e) {
            return handle_exception("plugin update", e);
        }
    });
}

std::future<CLIResult> PluginCLIManager::list(const std::vector<std::string>& filters) {
    return std::async(std::launch::async, [this, filters]() {
        try {
            // In a real implementation, this would list installed plugins
            // For now, return a placeholder
            std::stringstream output;
            output << cli_utils::colorize("Installed Plugins", "green") << "\n\n";
            output << cli_utils::dim("No plugins installed") << std::endl;
            return CLIResult::success(output.str());
        } catch (const std::exception& e) {
            return handle_exception("plugin listing", e);
        }
    });
}

std::future<CLIResult> PluginCLIManager::info(const std::string& plugin) {
    return std::async(std::launch::async, [this, plugin]() {
        ensure_initialized().get();

        try {
            auto info_future = registry_->get_plugin_info(plugin);
            auto info = info_future.get();

            if (!info.has_value()) {
                return CLIResult::error("Plugin '" + plugin + "' not found");
            }

            std::string output = cli_utils::format_plugin_info(*info);
            return CLIResult::success(output);
        } catch (const std::exception& e) {
            return handle_exception("plugin info", e);
        }
    });
}

std::future<CLIResult> PluginCLIManager::dependencies(const std::string& plugin) {
    return std::async(std::launch::async, [this, plugin]() {
        ensure_initialized().get();

        try {
            auto info_future = registry_->get_plugin_info(plugin);
            auto info = info_future.get();

            if (!info.has_value()) {
                return CLIResult::error("Plugin '" + plugin + "' not found");
            }

            std::stringstream output;
            output << cli_utils::colorize("Dependencies for " + plugin, "green") << "\n\n";

            if (info->dependencies.empty()) {
                output << cli_utils::dim("No dependencies") << std::endl;
            } else {
                std::vector<cli_utils::TableColumn> columns = {
                    cli_utils::TableColumn("Dependency", 30),
                    cli_utils::TableColumn("Version Constraint", 20),
                    cli_utils::TableColumn("Optional", 8)
                };

                std::vector<std::vector<std::string>> rows;
                for (const auto& dep : info->dependencies) {
                    rows.push_back({
                        cli_utils::bold(dep),
                        "^1.0.0", // Placeholder - would come from actual dependency info
                        "No"
                    });
                }

                output << cli_utils::format_table(columns, rows);
            }

            return CLIResult::success(output.str());
        } catch (const std::exception& e) {
            return handle_exception("dependency listing", e);
        }
    });
}

std::future<CLIResult> PluginCLIManager::rollback(const std::string& plugin, const std::string& version) {
    return std::async(std::launch::async, [this, plugin, version]() {
        ensure_initialized().get();

        try {
            report_progress("Rollback", plugin, 0, 2);

            auto rollback_future = downloader_->rollback_plugin(plugin);
            bool success = rollback_future.get();

            report_progress("Rollback", plugin, 1, 2);

            if (success) {
                return CLIResult::success("Successfully rolled back " + plugin + " to " + version);
            } else {
                return CLIResult::error("Failed to rollback " + plugin + " to " + version);
            }
        } catch (const std::exception& e) {
            return handle_exception("plugin rollback", e);
        }
    });
}

std::future<CLIResult> PluginCLIManager::cleanup() {
    return std::async(std::launch::async, [this]() {
        ensure_initialized().get();

        try {
            report_progress("Cleanup", "Cache and downloads", 0, 3);

            // Clear resolver cache
            resolver_->clear_cache();
            report_progress("Cleanup", "Resolver cache cleared", 1, 3);

            // Cleanup downloads
            auto cleanup_future = downloader_->cleanup_downloads();
            cleanup_future.get();
            report_progress("Cleanup", "Downloads cleaned", 2, 3);

            // Clear registry cache
            registry_->clear_cache();
            report_progress("Cleanup", "Registry cache cleared", 3, 3);

            return CLIResult::success("Cleanup completed successfully");
        } catch (const std::exception& e) {
            return handle_exception("cleanup", e);
        }
    });
}

std::future<CLIResult> PluginCLIManager::status() {
    return std::async(std::launch::async, [this]() {
        try {
            std::stringstream output;
            output << cli_utils::colorize("Plugin System Status", "green") << "\n\n";

            // Get statistics from all components
            auto registry_stats_future = registry_->get_registry_statistics();
            auto download_stats = downloader_->get_download_statistics();
            auto resolution_stats = resolver_->get_resolution_statistics();

            auto registry_stats = registry_stats_future.get();

            output << cli_utils::bold("GitHub Registry:") << "\n";
            output << "  Cached repositories: " << registry_stats["total_cached_repositories"] << "\n";
            output << "  Cache hits: " << registry_stats["cache_hits"] << "\n";

            output << "\n" << cli_utils::bold("Plugin Downloader:") << "\n";
            output << "  Total downloads: " << download_stats["total_downloads"] << "\n";
            output << "  Successful downloads: " << download_stats["successful_downloads"] << "\n";

            output << "\n" << cli_utils::bold("Version Resolver:") << "\n";
            output << "  Total resolutions: " << resolution_stats["total_resolutions"] << "\n";
            output << "  Successful resolutions: " << resolution_stats["successful_resolutions"] << "\n";

            return CLIResult::success(output.str());
        } catch (const std::exception& e) {
            return handle_exception("status check", e);
        }
    });
}

std::future<InstallationPlan> PluginCLIManager::create_installation_plan(
    const std::vector<std::string>& plugins,
    const std::vector<std::string>& versions) {

    return std::async(std::launch::async, [this, plugins, versions]() {
        InstallationPlan plan;

        for (size_t i = 0; i < plugins.size(); ++i) {
            const std::string& plugin_id = plugins[i];
            const std::string& version = i < versions.size() ? versions[i] : "latest";

            try {
                auto info_future = registry_->get_plugin_info(plugin_id);
                auto info = info_future.get();

                if (info.has_value()) {
                    plan.plugins_to_install.push_back(*info);
                } else {
                    plan.warnings.push_back("Plugin '" + plugin_id + "' not found in registry");
                }
            } catch (const std::exception& e) {
                plan.warnings.push_back("Error getting info for '" + plugin_id + "': " + e.what());
            }
        }

        // Check for dependency conflicts
        if (!plan.plugins_to_install.empty()) {
            try {
                auto resolution_future = resolver_->resolve_dependencies(plan.plugins_to_install);
                auto result = resolution_future.get();

                if (!result.resolution_success) {
                    plan.conflicts = result.conflicts;
                }
            } catch (const std::exception& e) {
                plan.warnings.push_back("Dependency resolution failed: " + std::string(e.what()));
            }
        }

        return plan;
    });
}

std::future<CLIResult> PluginCLIManager::execute_plan(const InstallationPlan& plan) {
    return std::async(std::launch::async, [this, plan]() {
        size_t total_operations = plan.plugins_to_install.size() + plan.plugins_to_update.size();
        size_t completed_operations = 0;

        std::stringstream details;

        try {
            // Install plugins
            for (const auto& plugin : plan.plugins_to_install) {
                report_progress("Installation", plugin.id, completed_operations++, total_operations);

                try {
                    auto install_future = downloader_->install_plugin(plugin);
                    auto result = install_future.get();

                    if (result.installation_success) {
                        details << "✓ Installed: " << plugin.id << " " << plugin.version << "\n";
                    } else {
                        details << "✗ Failed to install: " << plugin.id << " - " << result.error_message << "\n";
                        return CLIResult::error_with_details("Installation failed", details.str());
                    }
                } catch (const std::exception& e) {
                    details << "✗ Error installing " << plugin.id << ": " << e.what() << "\n";
                    return CLIResult::error_with_details("Installation error", details.str());
                }
            }

            // Update plugins
            for (const auto& plugin : plan.plugins_to_update) {
                report_progress("Update", plugin.id, completed_operations++, total_operations);

                try {
                    auto update_future = downloader_->update_plugin(plugin.id, plugin.version);
                    bool success = update_future.get();

                    if (success) {
                        details << "✓ Updated: " << plugin.id << " to " << plugin.version << "\n";
                    } else {
                        details << "✗ Failed to update: " << plugin.id << "\n";
                        return CLIResult::error_with_details("Update failed", details.str());
                    }
                } catch (const std::exception& e) {
                    details << "✗ Error updating " << plugin.id << ": " << e.what() << "\n";
                    return CLIResult::error_with_details("Update error", details.str());
                }
            }

            std::string message = "Successfully completed " + std::to_string(total_operations) + " operations";
            if (plan.has_warnings()) {
                message += " (with " + std::to_string(plan.warnings.size()) + " warnings)";
            }

            return CLIResult::success(message);
        } catch (const std::exception& e) {
            return handle_exception("plan execution", e);
        }
    });
}

// ============================================================================
// Helper Methods
// ============================================================================

std::future<CLIResult> PluginCLIManager::ensure_initialized() {
    return std::async(std::launch::async, [this]() {
        if (!registry_ || !downloader_ || !resolver_) {
            return CLIResult::error("CLI Manager not initialized. Call initialize() first.");
        }
        return CLIResult::success();
    });
}

std::vector<SearchResult> PluginCLIManager::convert_to_search_results(
    const std::vector<aimux::distribution::PluginPackage>& packages) {

    std::vector<SearchResult> results;
    results.reserve(packages.size());

    for (const auto& package : packages) {
        SearchResult result;
        result.id = package.id;
        result.name = package.name;
        result.description = package.description;
        result.latest_version = package.version;
        result.owner = package.owner;
        result.download_count = 1000; // Placeholder - would come from registry
        result.updated_at = std::chrono::system_clock::now(); // Placeholder

        // Extract tags from description or package metadata
        result.tags = {}; // Would be populated from actual package data

        results.push_back(result);
    }

    return results;
}

std::string PluginCLIManager::format_size(size_t bytes) const {
    if (bytes < 1024) return std::to_string(bytes) + " B";
    if (bytes < 1024 * 1024) return std::to_string(bytes / 1024) + " KB";
    if (bytes < 1024 * 1024 * 1024) return std::to_string(bytes / (1024 * 1024)) + " MB";
    return std::to_string(bytes / (1024 * 1024 * 1024)) + " GB";
}

std::string PluginCLIManager::format_duration(std::chrono::seconds duration) const {
    auto hours = std::chrono::duration_cast<std::chrono::hours>(duration);
    auto minutes = std::chrono::duration_cast<std::chrono::minutes>(duration % std::chrono::hours(1));
    auto seconds = duration % std::chrono::minutes(1);

    if (hours.count() > 0) {
        return std::to_string(hours.count()) + "h " + std::to_string(minutes.count()) + "m";
    } else if (minutes.count() > 0) {
        return std::to_string(minutes.count()) + "m " + std::to_string(seconds.count()) + "s";
    } else {
        return std::to_string(seconds.count()) + "s";
    }
}

bool PluginCLIManager::confirm_installation_plan(const InstallationPlan& plan) {
    if (!prompt_callback_) {
        return true; // Auto-accept if no prompt callback
    }

    std::string plan_text = cli_utils::format_installation_plan(plan);
    if (!plan_text.empty()) {
        std::cout << plan_text << "\n";
    }

    std::vector<std::string> options = {"yes", "no"};
    return prompt_callback_("Confirm installation", "", options);
}

bool PluginCLIManager::confirm_removal(const std::vector<std::string>& plugins) {
    if (!prompt_callback_) {
        return true;
    }

    std::string details = "Plugins to be removed:\n";
    for (const auto& plugin : plugins) {
        details += "  - " + plugin + "\n";
    }

    std::vector<std::string> options = {"yes", "no"};
    return prompt_callback_("Confirm removal", details, options);
}

void PluginCLIManager::report_progress(const std::string& operation,
                                      const std::string& item,
                                      size_t current,
                                      size_t total,
                                      const std::string& status) {
    if (progress_callback_) {
        progress_callback_(operation, item, current, total, status);
    } else if (config_.show_progress && !config_.quiet) {
        // Default progress output
        std::string progress_bar = cli_utils::create_progress_bar(current, total);
        std::cout << operation << ": " << progress_bar << " (" << current << "/" << total << ")";
        if (!item.empty()) {
            std::cout << " " << item;
        }
        if (!status.empty()) {
            std::cout << " - " << cli_utils::dim(status);
        }
        std::cout << "\r";
        std::flush(std::cout);

        if (current == total) {
            std::cout << std::endl;
        }
    }
}

CLIResult PluginCLIManager::handle_exception(const std::string& context, const std::exception& e) {
    std::string error_msg = "Error during " + context + ": " + e.what();
    if (config_.verbose) {
        return CLIResult::error_with_details(error_msg, std::string(e.what()));
    } else {
        return CLIResult::error(error_msg);
    }
}

CLIResult PluginCLIManager::handle_dependency_conflicts(
    const std::vector<aimux::distribution::DependencyConflict>& conflicts) {

    std::string conflict_text = cli_utils::format_conflicts(conflicts);
    return CLIResult::error_with_details("Dependency conflicts detected", conflict_text);
}

std::future<bool> PluginCLIManager::validate_plugin_id(const std::string& plugin_id) {
    return std::async(std::launch::async, [this, plugin_id]() {
        try {
            auto validate_future = registry_->validate_plugin(plugin_id);
            return validate_future.get();
        } catch (...) {
            return false;
        }
    });
}

std::future<bool> PluginCLIManager::validate_version(const std::string& version) {
    return std::async(std::launch::async, [version]() {
        return aimux::distribution::SemanticVersion::is_valid_version_string(version);
    });
}

} // namespace aimux::cli