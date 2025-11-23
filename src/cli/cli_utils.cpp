#include "aimux/cli/plugin_cli.hpp"
#include <algorithm>
#include <iomanip>

namespace aimux::cli::cli_utils {

// ============================================================================
// Text Formatting
// ============================================================================

std::string colorize(const std::string& text, const std::string& color) {
    static const std::map<std::string, std::string> colors = {
        {"black", "30"},
        {"red", "31"},
        {"green", "32"},
        {"yellow", "33"},
        {"blue", "34"},
        {"magenta", "35"},
        {"cyan", "36"},
        {"white", "37"},
        {"bright_black", "90"},
        {"bright_red", "91"},
        {"bright_green", "92"},
        {"bright_yellow", "93"},
        {"bright_blue", "94"},
        {"bright_magenta", "95"},
        {"bright_cyan", "96"},
        {"bright_white", "97"}
    };

    auto it = colors.find(color);
    if (it == colors.end()) {
        return text;
    }

    const char* term_color = std::getenv("NO_COLOR");
    if (term_color && std::string(term_color) != "0") {
        return text; // Respect NO_COLOR environment variable
    }

    return "\033[" + it->second + "m" + text + "\033[0m";
}

std::string bold(const std::string& text) {
    const char* term_color = std::getenv("NO_COLOR");
    if (term_color && std::string(term_color) != "0") {
        return text;
    }
    return "\033[1m" + text + "\033[0m";
}

std::string dim(const std::string& text) {
    const char* term_color = std::getenv("NO_COLOR");
    if (term_color && std::string(term_color) != "0") {
        return text;
    }
    return "\033[2m" + text + "\033[0m";
}

std::string success_color(const std::string& text) {
    return colorize(text, "green");
}

std::string warning_color(const std::string& text) {
    return colorize(text, "yellow");
}

std::string error_color(const std::string& text) {
    return colorize(text, "red");
}

// ============================================================================
// Table Formatting
// ============================================================================

std::string format_table(const std::vector<TableColumn>& columns,
                        const std::vector<std::vector<std::string>>& rows) {
    if (columns.empty()) return "";

    std::stringstream output;

    // Format header
    for (const auto& col : columns) {
        std::string header = col.header;
        if (header.length() > col.width) {
            header = header.substr(0, col.width - 3) + "...";
        }

        if (col.align_right) {
            output << std::setw(col.width) << std::right << bold(header) << " │ ";
        } else {
            output << std::setw(col.width) << std::left << bold(header) << " │ ";
        }
    }
    output << "\n";

    // Separator line
    std::string separator;
    for (const auto& col : columns) {
        separator += std::string(col.width, '-') + "─┼─";
    }
    if (!separator.empty()) {
        separator = separator.substr(0, separator.length() - 3);
    }
    output << separator << "\n";

    // Format rows
    for (const auto& row : rows) {
        for (size_t i = 0; i < columns.size() && i < row.size(); ++i) {
            const auto& col = columns[i];
            std::string cell = row[i];

            if (cell.length() > col.width) {
                cell = cell.substr(0, col.width - 3) + "...";
            }

            if (col.align_right) {
                output << std::setw(col.width) << std::right << cell << " │ ";
            } else {
                output << std::setw(col.width) << std::left << cell << " │ ";
            }
        }
        output << "\n";
    }

    return output.str();
}

// ============================================================================
// Plugin Information Formatting
// ============================================================================

std::string format_plugin_info(const PluginPackage& plugin) {
    std::stringstream output;

    output << bold("Plugin Information") << "\n";
    output << std::string(plugin.name.length() + 18, '=') << "\n\n";

    output << bold("Name:") << "        " << plugin.name << "\n";
    output << bold("ID:") << "          " << plugin.id << "\n";
    output << bold("Version:") << "     " << plugin.version << "\n";
    output << bold("Owner:") << "       " << plugin.owner << "\n";

    if (!plugin.description.empty()) {
        output << bold("Description:") << " " << plugin.description << "\n";
    }

    output << bold("File Size:") << "   " << std::to_string(plugin.file_size / 1024) << " KB\n";
    output << bold("Type:") << "        " << plugin.content_type << "\n";

    if (!plugin.download_url.empty()) {
        output << bold("Download:") << "    " << dim(plugin.download_url) << "\n";
    }

    if (!plugin.dependencies.empty()) {
        output << "\n" << bold("Dependencies:") << " " << plugin.dependencies.size() << " required\n";
        for (const auto& dep : plugin.dependencies) {
            output << "  • " << dep << "\n";
        }
    }

    if (!plugin.checksum_sha256.empty()) {
        output << "\n" << bold("SHA256:") << "      " << dim(plugin.checksum_sha256) << "\n";
    }

    return output.str();
}

std::string format_installation_plan(const InstallationPlan& plan) {
    std::stringstream output;

    output << bold("Installation Plan") << "\n";
    output << std::string(18, '=') << "\n\n";

    if (!plan.plugins_to_install.empty()) {
        output << bold("Plugins to Install:") << " " << plan.plugins_to_install.size() << "\n";
        for (const auto& plugin : plan.plugins_to_install) {
            output << "  • " << bold(plugin.id) << " "
                   << dim("(" + plugin.version + ", " +
                          std::to_string(plugin.file_size / 1024) + " KB)") << "\n";
        }
        output << "\n";
    }

    if (!plan.plugins_to_update.empty()) {
        output << bold("Plugins to Update:") << " " << plan.plugins_to_update.size() << "\n";
        for (const auto& plugin : plan.plugins_to_update) {
            output << "  • " << bold(plugin.id) << " "
                   << dim("→ " + plugin.version + ", " +
                          std::to_string(plugin.file_size / 1024) + " KB)") << "\n";
        }
        output << "\n";
    }

    size_t total_size = plan.total_size();
    output << bold("Total Size:") << " " << std::to_string(total_size / 1024 / 1024) << " MB\n\n";

    if (plan.has_warnings()) {
        output << warning_color(bold("Warnings:")) << " " << plan.warnings.size() << "\n";
        for (const auto& warning : plan.warnings) {
            output << "  ⚠ " << dim(warning) << "\n";
        }
        output << "\n";
    }

    if (plan.has_conflicts()) {
        output << error_color(bold("Conflicts:")) << " " << plan.conflicts.size() << "\n";
        output << format_conflicts(plan.conflicts);
        output << "\n";
    }

    return output.str();
}

std::string format_conflicts(const std::vector<aimux::distribution::DependencyConflict>& conflicts) {
    std::stringstream output;

    for (const auto& conflict : conflicts) {
        output << "  ✗ " << error_color(conflict.dependency_id) << ": "
               << dim(conflict.description) << "\n";

        if (!conflict.conflicting_plugins.empty()) {
            output << "    In plugins: " << conflict.conflicting_plugins[0];
            for (size_t i = 1; i < conflict.conflicting_plugins.size(); ++i) {
                output << ", " << conflict.conflicting_plugins[i];
            }
            output << "\n";
        }

        if (!conflict.conflicting_versions.empty()) {
            output << "    Required versions: ";
            for (size_t i = 0; i < conflict.conflicting_versions.size(); ++i) {
                if (i > 0) output << " vs ";
                output << conflict.conflicting_versions[i].to_string();
            }
            output << "\n";
        }
    }

    return output.str();
}

// ============================================================================
// Progress Bar
// ============================================================================

std::string create_progress_bar(size_t current, size_t total, size_t width) {
    if (total == 0) return "[" + std::string(width - 2, ' ') + "]";

    double progress = static_cast<double>(current) / total;
    size_t filled = static_cast<size_t>(progress * (width - 2));

    std::string bar = "[";
    bar += std::string(filled, '=');
    if (filled < width - 2) {
        bar += ">";
    }
    bar += std::string(width - 2 - filled, ' ');
    bar += "]";

    std::string percentage = std::to_string(static_cast<int>(progress * 100)) + "%";

    // Add percentage at the end if there's space
    if (width > 10) {
        size_t space_for_percent = 5;
        if (filled > width - 2 - space_for_percent) {
            filled = width - 2 - space_for_percent;
            bar = "[" + std::string(filled, '=') + ">" + std::string(width - 2 - space_for_percent - filled - 1, ' ') + "]";
        }
        bar += " " + percentage;
    }

    return bar;
}

} // namespace aimux::cli::cli_utils