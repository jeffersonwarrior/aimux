#include "aimux/cli/plugin_cli.hpp"
#include <getopt.h>
#include <iostream>
#include <sstream>

namespace aimux::cli {

// ============================================================================
// PluginCLICommandDispatcher Implementation
// ============================================================================

PluginCLICommandDispatcher::PluginCLICommandDispatcher(std::shared_ptr<PluginCLIManager> manager)
    : manager_(std::move(manager)) {
}

CLIResult PluginCLICommandDispatcher::execute(int argc, char* argv[]) {
    if (argc < 2) {
        return CLIResult::error("No command specified. Use --help for usage information.", 1);
    }

    try {
        auto parsed = parse_args(argc, argv);
        return execute_command(parsed);
    } catch (const std::exception& e) {
        return CLIResult::error("Command parsing failed: " + std::string(e.what()));
    }
}

std::string PluginCLICommandDispatcher::get_usage() const {
    std::stringstream usage;
    usage << bold("aimux plugin") << " - Plugin Management CLI\n\n";
    usage << bold("Usage:") << "\n";
    usage << "  aimux plugin <command> [options] [arguments]\n\n";
    usage << bold("Commands:") << "\n";
    usage << "  install   Install one or more plugins\n";
    usage << "  remove    Remove installed plugins\n";
    usage << "  search    Search for plugins in the registry\n";
    usage << "  list      List installed plugins\n";
    usage << "  update    Update installed plugins\n";
    usage << "  info      Show detailed information about a plugin\n";
    usage << "  deps      Show plugin dependencies\n";
    usage << "  rollback  Rollback a plugin to a previous version\n";
    usage << "  cleanup   Clean up caches and temporary files\n";
    usage << "  status    Show plugin system status\n";
    usage << "  help      Show help for a specific command\n\n";
    usage << bold("Global Options:") << "\n";
    usage << "  -v, --verbose     Enable verbose output\n";
    usage << "  -q, --quiet       Suppress non-error output\n";
    usage << "  -f, --force       Skip confirmation prompts\n";
    usage << "  -n, --dry-run     Show what would be done without doing it\n";
    usage << "  --no-color        Disable colored output\n";
    usage << "  --config <path>   Set custom config directory\n";
    usage << "  --help, -h        Show this help message\n\n";
    usage << bold("Examples:") << "\n";
    usage << "  aimux plugin install markdown-prettifier\n";
    usage << "  aimux plugin search \"tool formatter\"\n";
    usage << "  aimux plugin update --all\n";
    usage << "  aimux plugin remove old-plugin --force\n";
    usage << "  aimux plugin info aimux-org/prettifier\n\n";
    usage << dim("For detailed help on a specific command, use:") << "\n";
    usage << dim("  aimux plugin help <command>");
    return usage.str();
}

std::string PluginCLICommandDispatcher::get_command_help(PluginCommand command) const {
    switch (command) {
        case PluginCommand::INSTALL:
            return "Install one or more plugins from the registry.\n\n"
                   "Usage: aimux plugin install [options] <plugin1> [plugin2] ...\n\n"
                   "Options:\n"
                   "  --version <version>  Install specific version (default: latest)\n"
                   "  --yes, -y          Skip confirmation prompts\n"
                   "  --dry-run          Show what would be installed\n\n"
                   "Examples:\n"
                   "  aimux plugin install markdown-prettifier\n"
                   "  aimux plugin install tool-formatter@1.2.0\n"
                   "  aimux plugin install plugin1 plugin2 plugin3";

        case PluginCommand::REMOVE:
            return "Remove installed plugins.\n\n"
                   "Usage: aimux plugin remove [options] <plugin1> [plugin2] ...\n\n"
                   "Options:\n"
                   "  --yes, -y          Skip confirmation prompts\n"
                   "  --dry-run          Show what would be removed\n\n"
                   "Examples:\n"
                   "  aimux plugin remove old-prettifier\n"
                   "  aimux plugin remove plugin1 plugin2 --force";

        case PluginCommand::SEARCH:
            return "Search for plugins in the registry.\n\n"
                   "Usage: aimux plugin search [options] <query>\n\n"
                   "Options:\n"
                   "  --limit <count>    Maximum number of results (default: 20)\n"
                   "  --no-color         Disable colored output\n\n"
                   "Examples:\n"
                   "  aimux plugin search \"markdown\"\n"
                   "  aimux plugin search tool formatter --limit 10";

        case PluginCommand::UPDATE:
            return "Update installed plugins.\n\n"
                   "Usage: aimux plugin update [options] [plugin1] ...\n\n"
                   "Options:\n"
                   "  --all              Update all installed plugins\n"
                   "  --dry-run          Show what would be updated\n\n"
                   "Examples:\n"
                   "  aimux plugin update                    # Update all plugins\n"
                   "  aimux plugin update markdown-formatter # Update specific plugin";

        case PluginCommand::LIST:
            return "List installed plugins.\n\n"
                   "Usage: aimux plugin list [options] [filter1] [filter2] ...\n\n"
                   "Options:\n"
                   "  --updated          Sort by last updated date\n"
                   "  --size             Sort by size\n"
                   "  --no-color         Disable colored output\n\n"
                   "Examples:\n"
                   "  aimux plugin list\n"
                   "  aimux plugin list markdown tool\n"
                   "  aimux plugin list --size";

        case PluginCommand::INFO:
            return "Show detailed information about a plugin.\n\n"
                   "Usage: aimux plugin info <plugin-id>\n\n"
                   "Shows version, dependencies, size, and other metadata.\n\n"
                   "Examples:\n"
                   "  aimux plugin info aimux-org/markdown-prettifier\n"
                   "  aimux plugin info markdown-tool";

        case PluginCommand::DEPENDENCIES:
            return "Show plugin dependencies and their versions.\n\n"
                   "Usage: aimux plugin deps <plugin-id>\n\n"
                   "Options:\n"
                   "  --tree             Show dependency tree\n"
                   "  --show-optional    Include optional dependencies\n\n"
                   "Examples:\n"
                   "  aimux plugin deps aimux-org/complex-plugin\n"
                   "  aimux plugin deps markdown-formatter --tree";

        case PluginCommand::ROLLBACK:
            return "Rollback a plugin to a previous version.\n\n"
                   "Usage: aimux plugin rollback <plugin-id> <version>\n\n"
                   "Options:\n"
                   "  --yes, -y          Skip confirmation prompts\n\n"
                   "Examples:\n"
                   "  aimux plugin rollback markdown-prettifier 1.0.0\n"
                   "  aimux plugin rollback aimux-org/tool 2.3.1";

        case PluginCommand::CLEANUP:
            return "Clean up caches and temporary files.\n\n"
                   "Usage: aimux plugin cleanup [options]\n\n"
                   "Options:\n"
                   "  --verbose          Show detailed cleanup information\n"
                   "  --dry-run          Show what would be cleaned\n\n"
                   "Examples:\n"
                   "  aimux plugin cleanup\n"
                   "  aimux plugin cleanup --verbose";

        case PluginCommand::STATUS:
            return "Show plugin system status and statistics.\n\n"
                   "Usage: aimux plugin status\n\n"
                   "Displays registry cache status, download statistics,\n"
                   "and system health information.\n\n"
                   "Examples:\n"
                   "  aimux plugin status";

        default:
            return "Unknown command. Use 'aimux plugin --help' for available commands.";
    }
}

// ============================================================================
// Command Parsing
// ============================================================================

PluginCLICommandDispatcher::ParsedCommand PluginCLICommandDispatcher::parse_args(int argc, char* argv[]) {
    ParsedCommand parsed;

    // Define long options
    static struct option long_options[] = {
        {"verbose",    no_argument,       nullptr, 'v'},
        {"quiet",      no_argument,       nullptr, 'q'},
        {"force",      no_argument,       nullptr, 'f'},
        {"dry-run",    no_argument,       nullptr, 'n'},
        {"no-color",   no_argument,       nullptr, 'c'},
        {"config",     required_argument, nullptr, 'C'},
        {"help",       no_argument,       nullptr, 'h'},
        {"version",    required_argument, nullptr, 1001},
        {"limit",      required_argument, nullptr, 1002},
        {"all",        no_argument,       nullptr, 1003},
        {"tree",       no_argument,       nullptr, 1004},
        {"yes",        no_argument,       nullptr, 1005},
        {"show-optional", no_argument,    nullptr, 1006},
        {nullptr,      0,                 nullptr, 0}
    };

    // Skip the program name and command
    if (argc < 2) {
        throw std::runtime_error("No command specified");
    }

    std::string command_str = argv[1];

    // Parse command
    if (command_str == "install" || command_str == "add" || command_str == "i") {
        parsed.command = PluginCommand::INSTALL;
    } else if (command_str == "remove" || command_str == "uninstall" || command_str == "rm") {
        parsed.command = PluginCommand::REMOVE;
    } else if (command_str == "search" || command_str == "find") {
        parsed.command = PluginCommand::SEARCH;
    } else if (command_str == "list" || command_str == "ls") {
        parsed.command = PluginCommand::LIST;
    } else if (command_str == "update" || command_str == "upgrade") {
        parsed.command = PluginCommand::UPDATE;
    } else if (command_str == "info" || command_str == "show") {
        parsed.command = PluginCommand::INFO;
    } else if (command_str == "deps" || command_str == "dependencies") {
        parsed.command = PluginCommand::DEPENDENCIES;
    } else if (command_str == "rollback" || command_str == "revert") {
        parsed.command = PluginCommand::ROLLBACK;
    } else if (command_str == "cleanup" || command_str == "clean") {
        parsed.command = PluginCommand::CLEANUP;
    } else if (command_str == "status" || command_str == "stats") {
        parsed.command = PluginCommand::STATUS;
    } else if (command_str == "help" || command_str == "--help" || command_str == "-h") {
        // Handle help differently - return special case
        throw std::runtime_error("help");
    } else {
        throw std::runtime_error("Unknown command: " + command_str);
    }

    // Parse options and arguments
    optind = 2; // Start parsing after command

    int opt;
    while ((opt = getopt_long(argc, argv, "vqfnC:h", long_options, nullptr)) != -1) {
        switch (opt) {
            case 'v':
                parsed.options["verbose"] = "true";
                break;
            case 'q':
                parsed.options["quiet"] = "true";
                break;
            case 'f':
                parsed.options["force"] = "true";
                break;
            case 'n':
                parsed.options["dry-run"] = "true";
                break;
            case 'c':
                parsed.options["no-color"] = "true";
                break;
            case 'C':
                parsed.options["config"] = optarg;
                break;
            case 'h':
                parsed.options["help"] = "true";
                break;
            case 1001: // --version
                parsed.options["version"] = optarg;
                break;
            case 1002: // --limit
                parsed.options["limit"] = optarg;
                break;
            case 1003: // --all
                parsed.options["all"] = "true";
                break;
            case 1004: // --tree
                parsed.options["tree"] = "true";
                break;
            case 1005: // --yes
                parsed.options["yes"] = "true";
                break;
            case 1006: // --show-optional
                parsed.options["show-optional"] = "true";
                break;
            case '?':
                throw std::runtime_error("Unknown option. Use --help for usage information.");
            default:
                break;
        }
    }

    // Parse remaining arguments
    for (int i = optind; i < argc; ++i) {
        parsed.arguments.push_back(argv[i]);
    }

    return parsed;
}

CLIResult PluginCLICommandDispatcher::execute_command(const ParsedCommand& parsed) {
    try {
        configure_options(parsed.options);
        return (this->*get_command_handler(parsed.command))(parsed);
    } catch (const std::exception& e) {
        return CLIResult::error("Command execution failed: " + std::string(e.what()));
    }
}

void PluginCLICommandDispatcher::configure_options(const std::map<std::string, std::string>& options) {
    CLIConfig config = manager_->get_config();

    if (options.count("verbose")) config.verbose = true;
    if (options.count("quiet")) config.quiet = true;
    if (options.count("force")) config.force = true;
    if (options.count("dry-run")) config.dry_run = true;
    if (options.count("no-color")) setenv("NO_COLOR", "1", 1);
    if (options.count("config")) config.config_directory = options.at("config");

    manager_->update_config(config).wait();
}

CLIResult PluginCLICommandDispatcher::handle_install(const ParsedCommand& parsed) {
    if (parsed.arguments.empty()) {
        return CLIResult::error("No plugins specified for installation");
    }

    std::string version = "latest";
    if (parsed.options.count("version")) {
        version = parsed.options.at("version");
    }

    auto result_future = manager_->install(parsed.arguments, version);
    return result_future.get();
}

CLIResult PluginCLICommandDispatcher::handle_remove(const ParsedCommand& parsed) {
    if (parsed.arguments.empty()) {
        return CLIResult::error("No plugins specified for removal");
    }

    auto result_future = manager_->remove(parsed.arguments);
    return result_future.get();
}

CLIResult PluginCLICommandDispatcher::handle_search(const ParsedCommand& parsed) {
    if (parsed.arguments.empty()) {
        return CLIResult::error("No search query specified");
    }

    size_t limit = 20;
    if (parsed.options.count("limit")) {
        limit = std::stoul(parsed.options.at("limit"));
    }

    auto result_future = manager_->search(parsed.arguments[0], limit);
    return result_future.get();
}

CLIResult PluginCLICommandDispatcher::handle_update(const ParsedCommand& parsed) {
    std::vector<std::string> plugins;
    if (!parsed.options.count("all")) {
        plugins = parsed.arguments;
    }

    auto result_future = manager_->update(plugins);
    return result_future.get();
}

CLIResult PluginCLICommandDispatcher::handle_list(const ParsedCommand& parsed) {
    auto result_future = manager_->list(parsed.arguments);
    return result_future.get();
}

CLIResult PluginCLICommandDispatcher::handle_info(const ParsedCommand& parsed) {
    if (parsed.arguments.empty()) {
        return CLIResult::error("No plugin specified");
    }

    auto result_future = manager_->info(parsed.arguments[0]);
    return result_future.get();
}

CLIResult PluginCLICommandDispatcher::handle_dependencies(const ParsedCommand& parsed) {
    if (parsed.arguments.empty()) {
        return CLIResult::error("No plugin specified");
    }

    auto result_future = manager_->dependencies(parsed.arguments[0]);
    return result_future.get();
}

CLIResult PluginCLICommandDispatcher::handle_rollback(const ParsedCommand& parsed) {
    if (parsed.arguments.size() < 2) {
        return CLIResult::error("Plugin ID and version required for rollback");
    }

    auto result_future = manager_->rollback(parsed.arguments[0], parsed.arguments[1]);
    return result_future.get();
}

CLIResult PluginCLICommandDispatcher::handle_cleanup(const ParsedCommand& parsed) {
    auto result_future = manager_->cleanup();
    return result_future.get();
}

CLIResult PluginCLICommandDispatcher::handle_status(const ParsedCommand& parsed) {
    auto result_future = manager_->status();
    return result_future.get();
}

// ============================================================================
// Command Handler Lookup
// ============================================================================

CLIResult (PluginCLICommandDispatcher::*get_command_handler(PluginCommand command))(const ParsedCommand&) {
    switch (command) {
        case PluginCommand::INSTALL:    return &PluginCLICommandDispatcher::handle_install;
        case PluginCommand::REMOVE:     return &PluginCLICommandDispatcher::handle_remove;
        case PluginCommand::SEARCH:     return &PluginCLICommandDispatcher::handle_search;
        case PluginCommand::LIST:       return &PluginCLICommandDispatcher::handle_list;
        case PluginCommand::UPDATE:     return &PluginCLICommandDispatcher::handle_update;
        case PluginCommand::INFO:       return &PluginCLICommandDispatcher::handle_info;
        case PluginCommand::DEPENDENCIES: return &PluginCLICommandDispatcher::handle_dependencies;
        case PluginCommand::ROLLBACK:   return &PluginCLICommandDispatcher::handle_rollback;
        case PluginCommand::CLEANUP:    return &PluginCLICommandDispatcher::handle_cleanup;
        case PluginCommand::STATUS:     return &PluginCLICommandDispatcher::handle_status;
        default:                        throw std::runtime_error("Unhandled command type");
    }
}

} // namespace aimux::cli