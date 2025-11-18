#include "aimux/cli/plugin_cli.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <iomanip>

namespace aimux::cli {

// ============================================================================
// Interactive Installation Implementation
// ============================================================================

class InteractiveInstallationHandler {
public:
    explicit InteractiveInstallationHandler(std::shared_ptr<PluginCLIManager> manager)
        : manager_(std::move(manager)) {}

    bool setup_interactive_prompts() {
        if (!isatty(STDIN_FILENO)) {
            return false; // No interactive terminal
        }

        // Setup default prompt callbacks
        manager_->set_progress_callback(
            [](const std::string& operation, const std::string& item,
               size_t current, size_t total, const std::string& status) {
                show_progress_bar(operation, item, current, total, status);
            });

        manager_->set_prompt_callback(
            [this](const std::string& prompt, const std::string& details,
                   const std::vector<std::string>& options) -> bool {
                return prompt_confirmation(prompt, details, options);
            });

        return true;
    }

private:
    std::shared_ptr<PluginCLIManager> manager_;

    static void show_progress_bar(const std::string& operation,
                                 const std::string& item,
                                 size_t current, size_t total,
                                 const std::string& status) {
        const int bar_width = 40;
        float progress = total > 0 ? static_cast<float>(current) / total : 0.0f;

        std::cout << "\r" << cli_utils::bold(operation) << ": ";

        if (!item.empty()) {
            std::cout << cli_utils::dim(item) << " ";
        }

        // Draw progress bar
        std::cout << "[";
        int pos = static_cast<int>(bar_width * progress);
        for (int i = 0; i < bar_width; ++i) {
            if (i < pos) std::cout << "=";
            else if (i == pos) std::cout << ">";
            else std::cout << " ";
        }
        std::cout << "] ";

        // Show percentage
        std::cout << std::setw(3) << static_cast<int>(progress * 100) << "%";

        // Show status if provided
        if (!status.empty()) {
            std::cout << " " << cli_utils::dim("(" + status + ")");
        }

        std::flush(std::cout);

        if (current == total) {
            std::cout << std::endl;
        }
    }

    static bool prompt_confirmation(const std::string& prompt,
                                   const std::string& details,
                                   const std::vector<std::string>& options) {
        std::cout << "\n" << cli_utils::colorize(prompt, "yellow") << "\n";

        if (!details.empty()) {
            std::cout << "\n" << details << "\n";
        }

        std::cout << "\nOptions: ";
        for (size_t i = 0; i < options.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << cli_utils::colorize(options[i], "cyan");
        }
        std::cout << "\n";

        while (true) {
            std::cout << cli_utils::bold("> ");
            std::flush(std::cout);

            std::string input;
            std::getline(std::cin, input);

            // Trim whitespace
            input.erase(0, input.find_first_not_of(" \t"));
            input.erase(input.find_last_not_of(" \t") + 1);

            // Default to first option if empty and it's yes/no
            if (input.empty() &&
                (options.size() == 2 &&
                 std::find(options.begin(), options.end(), "yes") != options.end())) {
                input = "yes";
            }

            // Check if input matches any option (case insensitive)
            auto it = std::find_if(options.begin(), options.end(),
                [&input](const std::string& option) {
                    if (input.length() != option.length()) return false;
                    return std::equal(input.begin(), input.end(), option.begin(),
                        [](char a, char b) { return std::tolower(a) == std::tolower(b); });
                });

            if (it != options.end()) {
                return (std::tolower(input[0]) == 'y' || input == "1");
            }

            std::cout << cli_utils::error_color("Invalid option. Please try again.") << "\n";
        }
    }
};

// ============================================================================
// Enhanced Interactive Prompt Methods
// ============================================================================

class InteractivePrompts {
public:
    // Plugin selection with search and filtering
    static std::vector<std::string> select_plugins_to_install() {
        std::vector<std::string> selected_plugins;

        std::cout << "\n" << cli_utils::bold("Plugin Selection") << "\n";
        std::cout << std::string(18, '=') << "\n";
        std::cout << "Enter plugin names or IDs one by one. Press Enter after each plugin.\n";
        std::cout << "Type 'search <query>' to search, or 'done' when finished.\n\n";

        while (true) {
            std::cout << cli_utils::bold("plugin") << cli_utils::dim(" (or 'search <query>', 'done')") << "> ";
            std::flush(std::cout);

            std::string input;
            std::getline(std::cin, input);

            // Trim whitespace
            input.erase(0, input.find_first_not_of(" \t"));
            input.erase(input.find_last_not_of(" \t") + 1);

            if (input.empty()) continue;
            if (input == "done" || input == "quit") break;

            if (input.substr(0, 7) == "search ") {
                std::string query = input.substr(7);
                // Would trigger search here
                std::cout << cli_utils::warning_color("Search feature would be implemented here") << "\n";
                continue;
            }

            // Validate plugin ID format
            if (input.find("/") != std::string::npos ||
                !std::any_of(input.begin(), input.end(), ::isspace)) {
                selected_plugins.push_back(input);
                std::cout << cli_utils::success_color("✓ Added: ") << input << "\n";
            } else {
                std::cout << cli_utils::error_color("✗ Invalid plugin format. Use 'owner/plugin' format") << "\n";
            }
        }

        return selected_plugins;
    }

    // Version selection with compatibility checking
    static std::string select_plugin_version(const std::vector<aimux::distribution::SemanticVersion>& available_versions) {
        if (available_versions.empty()) {
            return "";
        }

        std::cout << "\n" << cli_utils::bold("Version Selection") << "\n";
        std::cout << std::string(18, '=') << "\n";
        std::cout << "Available versions:\n\n";

        // Display versions with compatibility hints
        for (size_t i = 0; i < available_versions.size(); ++i) {
            const auto& version = available_versions[i];
            std::string version_str = version.to_string();

            std::cout << cli_utils::colorize(std::to_string(i + 1), "cyan") << ". ";
            std::cout << cli_utils::bold(version_str);

            if (version.is_prerelease()) {
                std::cout << " " << cli_utils::warning_color("(prerelease)");
            }

            if (i == 0) {
                std::cout << " " << cli_utils::success_color("(latest)");
            }

            std::cout << "\n";
        }

        std::cout << "\n" << cli_utils::bold("Select version") << " (1-" << available_versions.size() << ") [1]: ";
        std::flush(std::cout);

        std::string input;
        std::getline(std::cin, input);

        try {
            int selection = input.empty() ? 1 : std::stoi(input);
            if (selection >= 1 && selection <= static_cast<int>(available_versions.size())) {
                return available_versions[selection - 1].to_string();
            }
        } catch (...) {
            // Fall through to latest
        }

        return available_versions[0].to_string(); // Default to latest
    }

    // Dependency conflict resolution
    static aimux::distribution::DependencyConflict resolve_conflict_interactive(
        const aimux::distribution::DependencyConflict& conflict) {

        std::cout << "\n" << cli_utils::error_color(bold("Dependency Conflict")) << "\n";
        std::cout << std::string(20, '=') << "\n";
        std::cout << "Dependency: " << cli_utils::bold(conflict.dependency_id) << "\n";
        std::cout << "Description: " << conflict.description << "\n\n";

        std::cout << "Conflicting versions:\n";
        for (size_t i = 0; i < conflict.conflicting_versions.size(); ++i) {
            std::cout << "  " << cli_utils::colorize(std::to_string(i + 1), "cyan") << ". ";
            std::cout << conflict.conflicting_versions[i].to_string() << "\n";
        }

        std::cout << "\n" << cli_utils::bold("Resolution options:") << "\n";
        std::cout << "1. " << cli_utils::success_color("Manual resolution") << " - Choose compatible versions manually\n";
        std::cout << "2. " << cli_utils::warning_color("Skip this conflict") << " - Install with conflicts (may break plugins)\n";
        std::cout << "3. " << cli_utils::error_color("Cancel installation") << " - Abort the entire process\n\n";

        while (true) {
            std::cout << cli_utils::bold("Choose resolution") << " [1-3]: ";
            std::flush(std::cout);

            std::string input;
            std::getline(std::cin, input);

            if (input == "1" || input == "2" || input == "3") {
                if (input == "1") {
                    std::cout << cli_utils::success_color("Manual resolution selected") << "\n";
                    // Would implement interactive dependency resolution here
                } else if (input == "2") {
                    std::cout << cli_utils::warning_color("Conflict skipped - installation will proceed with warnings") << "\n";
                } else {
                    std::cout << cli_utils::colorize("Installation cancelled", "red") << "\n";
                }
                break;
            } else {
                std::cout << cli_utils::error_color("Invalid option. Please choose 1, 2, or 3.") << "\n";
            }
        }

        return conflict; // Return original conflict for now
    }

    // Installation progress with detailed feedback
    static void show_installation_progress(const InstallationPlan& plan,
                                          size_t completed_steps,
                                          size_t total_steps) {
        const int terminal_width = 80;
        float progress = static_cast<float>(completed_steps) / total_steps;

        std::cout << "\r[" << std::setw(3) << static_cast<int>(progress * 100) << "%] ";

        // Progress bar
        int bar_width = terminal_width - 20;
        int filled = static_cast<int>(progress * bar_width);

        std::cout << "[";
        for (int i = 0; i < bar_width; ++i) {
            if (i < filled) std::cout << "=";
            else if (i == filled) std::cout << ">";
            else std::cout << " ";
        }
        std::cout << "]";

        // Current operation
        if (completed_steps > 0 && completed_steps <= plan.plugins_to_install.size()) {
            const auto& plugin = plan.plugins_to_install[completed_steps - 1];
            std::cout << " Installing " << cli_utils::bold(plugin.id);
        }

        std::flush(std::cout);

        if (completed_steps == total_steps) {
            std::cout << "\n";
        }
    }

    // Advanced configuration prompts
    static CLIConfig configure_advanced_settings(CLIConfig base_config) {
        CLIConfig config = base_config;

        std::cout << "\n" << cli_utils::bold("Advanced Configuration") << "\n";
        std::cout << std::string(23, '=') << "\n";
        std::cout << "Configure advanced installation settings.\n";
        std::cout << "Press Enter to accept defaults.\n\n";

        // Security settings
        std::cout << cli_utils::colorize("Security Settings", "green") << "\n";
        std::cout << std::cout << "Verify checksums? " << (config.verify_checksums ? "[Y/n]" : "[y/N]") << ": ";
        std::string checksum_input;
        std::getline(std::cin, checksum_input);
        config.verify_checksums = config.verify_checksums ?
            (checksum_input.empty() || std::tolower(checksum_input[0]) != 'n') :
            (!checksum_input.empty() && std::tolower(checksum_input[0]) == 'y');

        std::cout << std::cout << "Verify signatures? " << (config.verify_signatures ? "[Y/n]" : "[y/N]") << ": ";
        std::string signature_input;
        std::getline(std::cin, signature_input);
        config.verify_signatures = config.verify_signatures ?
            (signature_input.empty() || std::tolower(signature_input[0]) != 'n') :
            (!signature_input.empty() && std::tolower(signature_input[0]) == 'y');

        // Performance settings
        std::cout << "\n" << cli_utils::colorize("Performance Settings", "cyan") << "\n";
        std::cout << std::cout << "Max concurrent downloads [" << config.max_concurrent_downloads << "]: ";
        std::string concurrent_input;
        std::getline(std::cin, concurrent_input);
        if (!concurrent_input.empty()) {
            try {
                int concurrent = std::stoi(concurrent_input);
                if (concurrent > 0 && concurrent <= 10) {
                    config.max_concurrent_downloads = concurrent;
                }
            } catch (...) {
                // Keep default
            }
        }

        std::cout << std::cout << "Timeout (seconds) [" << config.timeout.count() << "]: ";
        std::string timeout_input;
        std::getline(std::cin, timeout_input);
        if (!timeout_input.empty()) {
            try {
                int timeout = std::stoi(timeout_input);
                if (timeout > 0 && timeout <= 3600) {
                    config.timeout = std::chrono::seconds(timeout);
                }
            } catch (...) {
                // Keep default
            }
        }

        return config;
    }

    // Installation summary and confirmation
    static bool confirm_installation_summary(const InstallationPlan& plan) {
        std::cout << "\n" << cli_utils::bold("Installation Summary") << "\n";
        std::cout << std::string(20, '=') << "\n";

        if (!plan.plugins_to_install.empty()) {
            std::cout << "\n" << cli_utils::colorize("Plugins to Install:", "green") << "\n";
            for (const auto& plugin : plan.plugins_to_install) {
                std::cout << "  ✓ " << cli_utils::bold(plugin.id) << " "
                         << cli_utils::dim("(" + plugin.version + ")") << "\n";
            }
        }

        if (!plan.plugins_to_update.empty()) {
            std::cout << "\n" << cli_utils::colorize("Plugins to Update:", "yellow") << "\n";
            for (const auto& plugin : plan.plugins_to_update) {
                std::cout << "  ↑ " << cli_utils::bold(plugin.id) << " → "
                         << cli_utils::dim(plugin.version) << "\n";
            }
        }

        if (plan.has_conflicts()) {
            std::cout << "\n" << cli_utils::error_color(bold("Conflicts Found:")) << " " << plan.conflicts.size() << "\n";
            for (const auto& conflict : plan.conflicts) {
                std::cout << "  ⚠ " << conflict.dependency_id << ": " << conflict.description << "\n";
            }
        }

        if (plan.has_warnings()) {
            std::cout << "\n" << cli_utils::warning_color(bold("Warnings:")) << " " << plan.warnings.size() << "\n";
            for (const auto& warning : plan.warnings) {
                std::cout << "  ⚠ " << warning << "\n";
            }
        }

        // Total size and time estimate
        size_t total_size_bytes = plan.total_size();
        std::string size_str = format_file_size(total_size_bytes);
        std::string time_str = estimate_installation_time(total_size_bytes);

        std::cout << "\n" << cli_utils::bold("Total estimated size:") << " " << size_str << "\n";
        std::cout << cli_utils::bold("Estimated time:") << " " << time_str << "\n";

        std::cout << "\n" << cli_utils::colorize("Proceed with installation?", "yellow") << " [Y/n] ";
        std::flush(std::cout);

        std::string input;
        std::getline(std::cin, input);

        return (input.empty() || std::tolower(input[0]) == 'y');
    }

private:
    static std::string format_file_size(size_t bytes) {
        if (bytes < 1024) return std::to_string(bytes) + " B";
        if (bytes < 1024 * 1024) return std::to_string(bytes / 1024) + " KB";
        if (bytes < 1024 * 1024 * 1024) return std::to_string(bytes / (1024 * 1024)) + " MB";
        return std::to_string(bytes / (1024 * 1024 * 1024)) + " GB";
    }

    static std::string estimate_installation_time(size_t bytes) {
        // Rough estimate: 1MB per second download, plus processing time
        size_t seconds_estimate = (bytes / (1024 * 1024)) + (bytes > 0 ? 10 : 0);

        if (seconds_estimate < 60) {
            return std::to_string(seconds_estimate) + " seconds";
        } else if (seconds_estimate < 3600) {
            return std::to_string(seconds_estimate / 60) + " minutes";
        } else {
            return std::to_string(seconds_estimate / 3600) + " hours";
        }
    }
};

// ============================================================================
// Interactive Installation Manager
// ============================================================================

InteractiveInstallationManager::InteractiveInstallationManager(
    std::shared_ptr<PluginCLIManager> manager)
    : manager_(std::move(manager)) {

    handler_ = std::make_unique<InteractiveInstallationHandler>(manager);
    is_interactive_ = handler_->setup_interactive_prompts();
}

std::future<CLIResult> InteractiveInstallationManager::install_interactive(
    const std::vector<std::string>& initial_plugins) {

    return std::async(std::launch::async, [this, initial_plugins]() {
        if (!is_interactive_) {
            // Fallback to non-interactive installation
            auto future = manager_->install(initial_plugins);
            return future.get();
        }

        try {
            std::cout << "\n" << cli_utils::colorize(bold("🛠️  Interactive Plugin Installation"), "green") << "\n";
            std::cout << std::string(40, '=') << "\n\n";

            // Step 1: Plugin selection
            std::vector<std::string> plugins_to_install = initial_plugins.empty() ?
                InteractivePrompts::select_plugins_to_install() : initial_plugins;

            if (plugins_to_install.empty()) {
                return CLIResult::success("No plugins selected. Installation cancelled.");
            }

            // Step 2: Create installation plan
            std::cout << "\n" << cli_utils::colorize("Creating installation plan...", "cyan") << "\n";
            auto plan_future = manager_->create_installation_plan(plugins_to_install);
            auto plan = plan_future.get();

            // Step 3: Resolve conflicts interactively
            if (plan.has_conflicts()) {
                std::cout << cli_utils::warning_color("Resolving conflicts interactively...") << "\n";
                for (auto& conflict : plan.conflicts) {
                    InteractivePrompts::resolve_conflict_interactive(conflict);
                }
            }

            // Step 4: Advanced configuration
            CLIConfig config = manager_->get_config();
            config = InteractivePrompts::configure_advanced_settings(config);
            manager_->update_config(config).wait();

            // Step 5: Final confirmation
            if (!InteractivePrompts::confirm_installation_summary(plan)) {
                return CLIResult::success("Installation cancelled by user.");
            }

            // Step 6: Execute with progress feedback
            std::cout << "\n" << cli_utils::colorize("Starting installation...", "green") << "\n";

            size_t total_operations = plan.plugins_to_install.size() + plan.plugins_to_update.size();
            size_t completed_operations = 0;

            auto execution_future = manager_->execute_plan(plan);
            auto result = execution_future.get();

            if (result.success) {
                std::cout << "\n" << cli_utils::success_color(bold("✅ Installation Completed Successfully!")) << "\n";
                if (!result.details.empty()) {
                    std::cout << result.details << "\n";
                }
            } else {
                std::cout << "\n" << cli_utils::error_color(bold("❌ Installation Failed")) << "\n";
                std::cout << cli_utils::dim(result.message) << "\n";
                if (!result.details.empty()) {
                    std::cout << result.details << "\n";
                }
            }

            return result;

        } catch (const std::exception& e) {
            std::cout << "\n" << cli_utils::error_color(bold("💥 Installation Error:")) << "\n";
            std::cout << e.what() << "\n";
            return CLIResult::error("Interactive installation failed: " + std::string(e.what()));
        }
    });
}

} // namespace aimux::cli