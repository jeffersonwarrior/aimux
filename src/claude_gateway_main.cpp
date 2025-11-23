#include <iostream>
#include <signal.h>
#include <memory>
#include <thread>
#include <chrono>

#include "aimux/gateway/claude_gateway.hpp"
#include "aimux/logging/logger.hpp"

using namespace aimux::gateway;

std::unique_ptr<ClaudeGateway> gateway;
std::atomic<bool> keep_running(true);

void signal_handler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down gracefully..." << std::endl;
    keep_running = false;

    if (gateway) {
        gateway->stop();
    }
}

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --port <port>           Port to bind to (default: 8080)" << std::endl;
    std::cout << "  --bind <address>         Address to bind to (default: 127.0.0.1)" << std::endl;
    std::cout << "  --config <file>          Configuration file path" << std::endl;
    std::cout << "  --log-level <level>      Log level: debug, info, warn, error (default: info)" << std::endl;
    std::cout << "  --request-logging        Enable detailed request logging" << std::endl;
    std::cout << "  --max-size <mb>          Maximum request size in MB (default: 10)" << std::endl;
    std::cout << "  --help, -h               Show this help message" << std::endl;
    std::cout << "" << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  " << program_name << " --port 8080 --bind 0.0.0.0" << std::endl;
    std::cout << "  " << program_name << " --config config.json --request-logging" << std::endl;
    std::cout << "  " << program_name << " --log-level debug --max-size 20" << std::endl;
}

void print_welcome_message(const ClaudeGatewayConfig& config) {
    std::cout << "" << std::endl;
    std::cout << "╔══════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║                    Aimux2 V3.2 ClaudeGateway                 ║" << std::endl;
    std::cout << "║                  Single Unified Endpoint Service             ║" << std::endl;
    std::cout << "╚══════════════════════════════════════════════════════════════╝" << std::endl;
    std::cout << "" << std::endl;
    std::cout << "🚀 Service Starting:" << std::endl;
    std::cout << "   Bind Address: " << config.bind_address << std::endl;
    std::cout << "   Port: " << config.port << std::endl;
    std::cout << "   Log Level: " << config.log_level << std::endl;
    std::cout << "   Metrics Enabled: " << (config.enable_metrics ? "Yes" : "No") << std::endl;
    std::cout << "   CORS Enabled: " << (config.enable_cors ? "Yes" : "No") << std::endl;
    std::cout << "   Request Logging: " << (config.request_logging ? "Yes" : "No") << std::endl;
    std::cout << "" << std::endl;
    std::cout << "📡 Endpoints:" << std::endl;
    std::cout << "   Main:      http://" << config.bind_address << ":" << config.port << "/anthropic/v1/messages" << std::endl;
    std::cout << "   Models:    http://" << config.bind_address << ":" << config.port << "/anthropic/v1/models" << std::endl;
    std::cout << "   Health:    http://" << config.bind_address << ":" << config.port << "/health" << std::endl;
    std::cout << "   Metrics:   http://" << config.bind_address << ":" << config.port << "/metrics" << std::endl;
    std::cout << "   Config:    http://" << config.bind_address << ":" << config.port << "/config" << std::endl;
    std::cout << "   Providers: http://" << config.bind_address << ":" << config.port << "/providers" << std::endl;
    std::cout << "" << std::endl;
    std::cout << "🔗 Claude Code Integration:" << std::endl;
    std::cout << "   export ANTHROPIC_API_URL=http://" << config.bind_address << ":" << config.port << "/anthropic/v1" << std::endl;
    std::cout << "   export ANTHROPIC_API_KEY=dummy-key" << std::endl;
    std::cout << "" << std::endl;
    std::cout << "Press Ctrl+C to stop the service gracefully." << std::endl;
    std::cout << "" << std::endl;
}

void setup_request_callbacks() {
    if (gateway) {
        // Set up request callback for monitoring
        gateway->set_request_callback([](const aimux::core::Request& req,
                                        const aimux::core::Response& resp,
                                        double duration_ms) {
            if (resp.success) {
                aimux::info("✅ Request routed to " + resp.provider_name +
                           " in " + std::to_string(duration_ms) + "ms");
            } else {
                aimux::warn("❌ Request failed via " + resp.provider_name +
                           ": " + resp.error_message);
            }
        });

        // Set up error callback
        gateway->set_error_callback([](const std::string& type, const std::string& message) {
            aimux::error("🚨 " + type + ": " + message);
        });
    }
}

void print_metrics_periodically() {
    if (!gateway) return;

    while (keep_running) {
        std::this_thread::sleep_for(std::chrono::seconds(30));

        if (!keep_running) break;

        auto metrics = gateway->get_metrics();
        aimux::info("📊 Metrics - Total: " + std::to_string(metrics.total_requests.load()) +
                    ", Success: " + std::to_string(metrics.successful_requests.load()) +
                    ", Failed: " + std::to_string(metrics.failed_requests.load()) +
                    ", Success Rate: " + std::to_string(metrics.get_success_rate() * 100).substr(0, 4) + "%" +
                    ", Avg Time: " + std::to_string(metrics.get_average_response_time()).substr(0, 6) + "ms" +
                    ", Uptime: " + std::to_string(metrics.get_uptime_seconds()).substr(0, 6) + "s");
    }
}

int main(int argc, char* argv[]) {
    ClaudeGatewayConfig config;
    std::string config_file;

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return 0;
        }
        else if (arg == "--port" && i + 1 < argc) {
            config.port = std::atoi(argv[++i]);
        }
        else if (arg == "--bind" && i + 1 < argc) {
            config.bind_address = argv[++i];
        }
        else if (arg == "--config" && i + 1 < argc) {
            config_file = argv[++i];
        }
        else if (arg == "--log-level" && i + 1 < argc) {
            config.log_level = argv[++i];
        }
        else if (arg == "--request-logging") {
            config.request_logging = true;
        }
        else if (arg == "--max-size" && i + 1 < argc) {
            config.max_request_size_mb = std::atoi(argv[++i]);
        }
        else {
            std::cerr << "Unknown option: " << arg << std::endl;
            print_usage(argv[0]);
            return 1;
        }
    }

    // Set up signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    try {
        // Initialize logging
        aimux::logging::LoggerRegistry::set_global_level(aimux::logging::LogUtils::string_to_level(config.log_level));
        aimux::info("Starting Aimux2 ClaudeGateway V3.2");

        // Create gateway instance
        gateway = std::make_unique<ClaudeGateway>();

        // Setup callbacks before initialization
        setup_request_callbacks();

        // Print welcome message
        print_welcome_message(config);

        // Initialize gateway
        gateway->initialize(config);

        // Load provider configuration if specified
        if (!config_file.empty()) {
            try {
                gateway->load_provider_config(config_file);
                aimux::info("Loaded provider configuration from: " + config_file);
            } catch (const std::exception& e) {
                aimux::warn("Could not load provider config: " + std::string(e.what()));
                aimux::info("Starting with default configuration...");
            }
        }

        // Start metrics reporting thread
        std::thread metrics_thread(print_metrics_periodically);

        // Start the service
        gateway->start(config.bind_address, config.port);

        // Main service loop
        aimux::info("🎯 ClaudeGateway is running and ready to serve requests!");

        while (keep_running && gateway->is_running()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // Cleanup
        aimux::info("Shutting down...");

        if (metrics_thread.joinable()) {
            metrics_thread.join();
        }

        if (gateway) {
            // Print final metrics
            auto final_metrics = gateway->get_metrics();
            std::cout << "" << std::endl;
            std::cout << "📈 Final Statistics:" << std::endl;
            std::cout << "   Total Requests: " << final_metrics.total_requests.load() << std::endl;
            std::cout << "   Successful Requests: " << final_metrics.successful_requests.load() << std::endl;
            std::cout << "   Failed Requests: " << final_metrics.failed_requests.load() << std::endl;
            std::cout << "   Success Rate: " << (final_metrics.get_success_rate() * 100) << "%" << std::endl;
            std::cout << "   Average Response Time: " << final_metrics.get_average_response_time() << "ms" << std::endl;
            std::cout << "   Total Uptime: " << final_metrics.get_uptime_seconds() << "s" << std::endl;

            gateway->shutdown();
            gateway.reset();
        }

        aimux::info("ClaudeGateway shutdown complete");
        std::cout << "\n👋 Goodbye!" << std::endl;

    } catch (const std::exception& e) {
        aimux::error("Fatal error: " + std::string(e.what()));
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}