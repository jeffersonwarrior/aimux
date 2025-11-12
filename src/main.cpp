#include <fstream>
#include <iostream>
#include <string>
#include "aimux/core/router.hpp"
#include "aimux/logging/logger.hpp"

// Global logger
static std::shared_ptr<aimux::logging::Logger> logger;

// Simple version reader
std::string get_version() {
    std::ifstream version_file("version.txt");
    std::string version = "2.0"; // fallback
    
    if (version_file.is_open()) {
        std::getline(version_file, version);
        version_file.close();
    }
    
    return version;
}

void print_version() {
    std::string version = get_version();
    std::cout << "Version " << version << " - Jefferson Nunn, Claude Code, Crush Code, GLM 4.6, Sonnet 4.5, GPT-5\n";
    std::cout << "(c) 2025 Zackor, LLC. All Rights Reserved\n";
}

void print_help() {
    std::string version = get_version();
    std::cout << "Version " << version << " - Jefferson Nunn, Claude Code, Crush Code, GLM 4.6, Sonnet 4.5, GPT-5\n\n";
    std::cout << "USAGE:\n";
    std::cout << "    aimux [OPTIONS]\n\n";
    std::cout << "OPTIONS:\n";
    std::cout << "    -h, --help      Show this help message\n";
    std::cout << "    -v, --version   Show version information\n";
    std::cout << "    -t, --test      Test router functionality\n";
    std::cout << "    -l, --log-level Set logging level (trace, debug, info, warn, error, fatal)\n";
    std::cout << "\nFor more information, see: https://github.com/aimux/aimux\n";
}

void test_router() {
    using namespace aimux::core;
    
    logger->info("Starting router test");
    
    // Create test provider configurations
    std::vector<ProviderConfig> providers = {
        {"cerebras", "https://api.cerebras.ai", "test-key", {"llama3.1-70b"}, 60, true},
        {"zai", "https://api.z.ai", "test-key", {"gpt-4"}, 60, true},
        {"synthetic", "https://synthetic.ai", "test-key", {"claude-3"}, 60, true}
    };
    
    logger->info("Created provider configurations", nlohmann::json{
        {"provider_count", providers.size()}
    });
    
    // Create router
    Router router(providers);
    logger->info("Router initialized successfully");
    
    // Test health status
    logger->info("Getting provider health status");
    std::cout << "\n=== Provider Health Status ===\n";
    std::cout << router.get_health_status() << std::endl;
    
    // Test metrics
    logger->info("Getting router metrics");
    std::cout << "\n=== Router Metrics ===\n";
    std::cout << router.get_metrics() << std::endl;
    
    // Test basic request routing
    logger->info("Testing request routing");
    Request test_request;
    test_request.model = "gpt-4";
    test_request.method = "POST";
    test_request.data = {{"message", "Hello, world!"}};
    
    Response response = router.route(test_request);
    logger->info("Request routed", nlohmann::json{
        {"provider", response.provider_name},
        {"success", response.success},
        {"response_time_ms", response.response_time_ms}
    });
    
    std::cout << "\n=== Test Request ===\n";
    std::cout << "Routed to: " << response.provider_name << std::endl;
    std::cout << "Success: " << (response.success ? "true" : "false") << std::endl;
    std::cout << "Response time: " << response.response_time_ms << "ms" << std::endl;
    
    // Get logger statistics
    std::cout << "\n=== Logger Statistics ===\n";
    std::cout << logger->get_statistics().dump(2) << std::endl;
    
    logger->info("Router test completed successfully");
}

void setup_logging(const std::string& level_str = "info") {
    // Parse log level
    aimux::logging::LogLevel level = aimux::logging::LogUtils::string_to_level(level_str);
    
    // Get global logger
    logger = aimux::logging::LoggerRegistry::get_logger("aimux-main", "aimux.log");
    logger->set_level(level);
    logger->set_console_enabled(level >= aimux::logging::LogLevel::INFO); // Disable console spam for debug/trace
    
    // Add default fields
    logger->add_default_field("version", get_version());
    logger->add_default_field("build_type", "Debug");
    
    logger->info("Aimux logger initialized", nlohmann::json{
        {"log_level", level_str},
        "log_file", "aimux.log",
        "console_enabled", level >= aimux::logging::LogLevel::INFO
    });
}

int main(int argc, char* argv[]) {
    // Check for logging level first
    std::string log_level = "info";
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if ((arg == "-l" || arg == "--log-level") && i + 1 < argc) {
            log_level = argv[i + 1];
            break;
        }
    }
    
    // Setup logging
    setup_logging(log_level);
    
    try {
        if (argc < 2) {
            print_help();
            return 0;
        }
        
        std::string first_arg = argv[1];
        
        if (first_arg == "-h" || first_arg == "--help") {
            print_help();
            return 0;
        } else if (first_arg == "-v" || first_arg == "--version") {
            print_version();
            return 0;
        } else if (first_arg == "-t" || first_arg == "--test") {
            test_router();
            return 0;
        } else {
            std::cerr << "Error: Unknown argument '" << first_arg << "'\n";
            print_help();
            return 1;
        }
    } catch (const std::exception& e) {
        logger->fatal("Unhandled exception", nlohmann::json{
            {"error", e.what()}
        });
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    
    // Flush all logs
    aimux::logging::LoggerRegistry::flush_all();
}