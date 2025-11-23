#include "aimux/cli/plugin_cli.hpp"
#include <iostream>
#include <memory>

int main(int argc, char* argv[]) {
    try {
        // Create CLI manager
        auto manager = std::make_shared<aimux::cli::PluginCLIManager>();

        // Initialize the manager
        auto init_future = manager->initialize();
        auto init_result = init_future.get();

        if (!init_result.success) {
            std::cerr << "Failed to initialize CLI manager: " << init_result.message << std::endl;
            return init_result.exit_code;
        }

        // Create command dispatcher
        aimux::cli::PluginCLICommandDispatcher dispatcher(manager);

        // Execute command
        auto result = dispatcher.execute(argc, argv);

        if (!result.success && !result.details.empty()) {
            std::cerr << aimux::cli::cli_utils::dim(result.details) << std::endl;
        }

        return result.exit_code;

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}