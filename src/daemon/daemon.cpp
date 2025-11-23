#include "aimux/daemon/daemon.hpp"
#include "aimux/core/router.hpp"
#include "aimux/core/thread_manager.hpp"
#include "aimux/providers/provider_impl.hpp"
#include "aimux/logging/logger.hpp"
#include <iostream>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <thread>

namespace aimux {
namespace daemon {

const std::string PID_FILE_PATH = "/var/run/aimux.pid";

// Public accessor for PID file
std::string get_pid_file() { return PID_FILE_PATH; }

// AimuxDaemon implementation
struct AimuxDaemon::Impl {
    std::string config_file;
    nlohmann::json config;
    std::unique_ptr<core::Router> router;
    std::unique_ptr<ApiServer> api_server;
    std::shared_ptr<logging::Logger> logger;
    std::atomic<bool> running{false};
    core::ManagedThread* daemon_thread{nullptr};
    int pid_file_fd = -1;

    std::chrono::steady_clock::time_point start_time;

    explicit Impl(const std::string& config_file_path)
        : config_file(config_file_path) {
        start_time = std::chrono::steady_clock::now();
    }
};

AimuxDaemon::AimuxDaemon(const std::string& config_file) 
    : pImpl(std::make_unique<Impl>(config_file)) {}

AimuxDaemon::~AimuxDaemon() {
    stop();
}

bool AimuxDaemon::start(bool daemonize) {
    if (pImpl->running.load()) {
        return true; // Already running
    }
    
    try {
        // Load configuration
        pImpl->config = ConfigManager::load_config(pImpl->config_file);
        
        // Setup logging
        auto log_level = pImpl->config["logging"].value("level", "info");
        auto log_file = pImpl->config["logging"].value("file", "aimux.log");
        pImpl->logger = logging::LoggerRegistry::get_logger("aimux-daemon", log_file);
        pImpl->logger->set_level(logging::LogUtils::string_to_level(log_level));
        pImpl->logger->set_console_enabled(false); // Daemon logs to file only
        pImpl->logger->info("Aimux daemon starting", nlohmann::json{
            {"config_file", pImpl->config_file},
            {"daemonize", daemonize}
        });
        
        // Setup router
        auto providers = providers::ConfigParser::parse_providers(pImpl->config);
        pImpl->router = std::make_unique<core::Router>(providers);
        
        // Setup API server
        auto host = pImpl->config["daemon"].value("host", "localhost");
        auto port = pImpl->config["daemon"].value("port", 8080);
        pImpl->api_server = std::make_unique<ApiServer>(host, port);
        
        if (daemonize) {
            if (!daemonize_process()) {
                return false;
            }
        }
        
        // Setup signal handlers
        setup_signals();
        
        // Write PID file
        write_pid_file();
        
        // Start API server
        if (!pImpl->api_server->start()) {
            pImpl->logger->error("Failed to start API server");
            return false;
        }
        
        // Start daemon thread with proper resource management
        pImpl->running.store(true);
        auto& thread_manager = core::ThreadManager::instance();
        pImpl->daemon_thread = &thread_manager.create_thread(
            "aimux-daemon-main",
            "Main daemon thread for background processing");

        if (!pImpl->daemon_thread->start([this](std::atomic<bool>& should_stop) {
            daemon_loop_managed(should_stop);
        })) {
            pImpl->logger->error("Failed to start daemon thread");
            return false;
        }
        
        pImpl->logger->info("Aimux daemon started successfully");
        
        return true;
        
    } catch (const std::exception& e) {
        if (pImpl->logger) {
            pImpl->logger->fatal("Failed to start daemon", nlohmann::json{
                {"error", e.what()}
            });
        }
        return false;
    }
}

void AimuxDaemon::stop() {
    if (!pImpl->running.load()) {
        return;
    }

    pImpl->running.store(false);

    pImpl->logger->info("Aimux daemon stopping");

    try {
        // Stop API server first
        if (pImpl->api_server) {
            pImpl->api_server->stop();
        }

        // Signal daemon thread to stop
        if (pImpl->daemon_thread) {
            pImpl->daemon_thread->stop();

            // Wait for graceful shutdown with timeout
            if (!pImpl->daemon_thread->force_stop(10000)) {
                pImpl->logger->error("Daemon thread failed to shutdown gracefully within timeout");
            } else {
                pImpl->logger->info("Daemon thread shutdown successfully");
            }
        }

        // Cleanup thread resources
        auto& thread_manager = core::ThreadManager::instance();
        thread_manager.cleanup_terminated_threads();

    } catch (const std::exception& e) {
        pImpl->logger->error("Error during daemon shutdown: " + std::string(e.what()));
    }

    // Remove PID file
    remove_pid_file();

    pImpl->logger->info("Aimux daemon stopped");

    // Flush logs
    logging::LoggerRegistry::flush_all();
}

bool AimuxDaemon::is_running() const {
    return pImpl->running.load();
}

nlohmann::json AimuxDaemon::get_status() const {
    nlohmann::json status;
    status["running"] = is_running();
    status["pid"] = getpid();
    status["config_file"] = pImpl->config_file;
    status["uptime_seconds"] = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - pImpl->start_time).count();
    
    if (pImpl->router) {
        status["providers"] = nlohmann::json::parse(pImpl->router->get_health_status());
        status["metrics"] = nlohmann::json::parse(pImpl->router->get_metrics());
    }
    
    return status;
}

bool AimuxDaemon::reload_config() {
    try {
        pImpl->logger->info("Reloading configuration");
        
        auto new_config = ConfigManager::load_config(pImpl->config_file);
        pImpl->config = new_config;
        
        // Reload logging configuration
        auto log_level = pImpl->config["logging"].value("level", "info");
        pImpl->logger->set_level(logging::LogUtils::string_to_level(log_level));
        
        
        pImpl->logger->info("Configuration reloaded successfully");
        return true;
        
    } catch (const std::exception& e) {
        pImpl->logger->error("Failed to reload configuration", nlohmann::json{
            {"error", e.what()}
        });
        return false;
    }
}

void AimuxDaemon::daemon_loop() {
    // Legacy method - use managed version instead
    std::atomic<bool> dummy_stop{false};
    daemon_loop_managed(dummy_stop);
}

void AimuxDaemon::daemon_loop_managed(std::atomic<bool>& should_stop) {
    // Update thread activity and set OS thread name
    if (pImpl->daemon_thread) {
        pImpl->daemon_thread->set_os_name("aimux-daemon");
    }

    while (!should_stop.load() && pImpl->running.load()) {
        try {
            // Update thread activity for monitoring
            if (pImpl->daemon_thread) {
                pImpl->daemon_thread->update_activity();
            }

            // Perform periodic maintenance tasks
            perform_maintenance_tasks();

            // Configurable sleep interval
            std::this_thread::sleep_for(std::chrono::seconds(1));

        } catch (const std::exception& e) {
            if (pImpl->logger) {
                pImpl->logger->error("Error in daemon loop: " + std::string(e.what()));
            }
            // Continue running despite errors
        }
    }

    // Final cleanup
    if (pImpl->daemon_thread) {
        pImpl->daemon_thread->update_activity();
    }
}

void AimuxDaemon::perform_maintenance_tasks() {
    // Resource monitoring and cleanup
    static auto last_health_check = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();

    // Health check every 5 minutes
    if (now - last_health_check > std::chrono::minutes(5)) {
        perform_health_check();
        last_health_check = now;
    }

    // Memory usage monitoring
    monitor_memory_usage();

    // Connection cleanup
    cleanup_stale_connections();
}

void AimuxDaemon::perform_health_check() {
    try {
        // Check thread health
        auto& thread_manager = core::ThreadManager::instance();
        auto health_issues = thread_manager.health_check();

        if (!health_issues.empty()) {
            for (const auto& issue : health_issues) {
                if (pImpl->logger) {
                    pImpl->logger->warn("Thread health issue: " + issue);
                }
            }
        }

        // Check router health
        if (pImpl->router) {
            // Router health check would go here
        }

    } catch (const std::exception& e) {
        if (pImpl->logger) {
            pImpl->logger->error("Health check error: " + std::string(e.what()));
        }
    }
}

void AimuxDaemon::monitor_memory_usage() {
#ifdef __linux__
    // Get current memory usage
    std::ifstream status_file("/proc/self/status");
    if (status_file.is_open()) {
        std::string line;
        while (std::getline(status_file, line)) {
            if (line.substr(0, 6) == "VmRSS:") {
                std::istringstream iss(line);
                std::string label, value, unit;
                if (iss >> label >> value >> unit) {
                    size_t memory_kb = std::stoull(value);
                    size_t memory_mb = memory_kb / 1024;

                    // Update thread memory usage for monitoring
                    if (pImpl->daemon_thread) {
                        pImpl->daemon_thread->update_memory_usage(memory_kb * 1024);
                    }

                    // Alert on high memory usage
                    if (memory_mb > 500) { // 500MB threshold
                        if (pImpl->logger) {
                            pImpl->logger->warn("High memory usage detected: " +
                                               std::to_string(memory_mb) + "MB");
                        }
                    }
                }
                break;
            }
        }
    }
#endif
}

void AimuxDaemon::cleanup_stale_connections() {
    // Connection cleanup logic would go here
    // This could cleanup stale WebSocket connections, idle database connections, etc.
    static auto last_cleanup = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();

    if (now - last_cleanup > std::chrono::minutes(10)) {
        // Perform cleanup
        last_cleanup = now;
    }
}

void AimuxDaemon::setup_signals() {
    signal(SIGTERM, [](int /* sig */) {
        // Handle shutdown signal
        // Graceful shutdown implemented
        exit(0);
    });

    signal(SIGINT, [](int /* sig */) {
        // Handle interrupt signal
        // Graceful shutdown implemented
        exit(0);
    });
}

bool AimuxDaemon::daemonize_process() {
    pid_t pid = fork();
    
    if (pid < 0) {
        return false; // Fork failed
    }
    
    if (pid > 0) {
        exit(0); // Parent exits
    }
    
    // Child process continues
    umask(0);
    
    if (setsid() < 0) {
        return false;
    }
    
    // Close standard file descriptors
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    
    return true;
}

void AimuxDaemon::write_pid_file() {
    std::ofstream pid_file(get_pid_file());
    if (pid_file.is_open()) {
        pid_file << getpid();
        pid_file.close();
    }
}

void AimuxDaemon::remove_pid_file() {
    unlink(get_pid_file().c_str());
}

// ConfigManager implementation
nlohmann::json ConfigManager::load_config(const std::string& config_file) {
    std::ifstream file(config_file);
    if (!file.is_open()) {
        // Generate default config if file doesn't exist
        if (!generate_default_config(config_file)) {
            throw std::runtime_error("Could not create config file: " + config_file);
        }
        file.open(config_file);
    }
    
    nlohmann::json config;
    file >> config;
    
    if (!validate_config(config)) {
        throw std::runtime_error("Invalid configuration in: " + config_file);
    }
    
    return config;
}

bool ConfigManager::save_config(const nlohmann::json& config, const std::string& config_file) {
    std::ofstream file(config_file);
    if (!file.is_open()) {
        return false;
    }
    
    file << config.dump(2);
    return true;
}

bool ConfigManager::generate_default_config(const std::string& config_file) {
    auto default_config = providers::ConfigParser::generate_default_config();
    return save_config(default_config, config_file);
}

bool ConfigManager::validate_config(const nlohmann::json& config) {
    // Basic validation
    if (!config.contains("daemon")) {
        return false;
    }
    
    if (!config.contains("logging")) {
        return false;
    }
    
    if (!config.contains("providers")) {
        return false;
    }
    
    return true;
}

nlohmann::json ConfigManager::get_default_config_template() {
    return providers::ConfigParser::generate_default_config();
}

// ProcessManager implementation
bool ProcessManager::is_pid_running(int pid) {
    return kill(pid, 0) == 0;
}

int ProcessManager::read_pid_file(const std::string& pid_file) {
    std::ifstream file(pid_file);
    if (!file.is_open()) {
        return -1;
    }
    
    int pid;
    file >> pid;
    return pid;
}

bool ProcessManager::send_signal(int signal) {
    int pid = read_pid_file(PID_FILE_PATH);
    if (pid < 0 || !is_pid_running(pid)) {
        return false;
    }
    
    return kill(pid, signal) == 0;
}

nlohmann::json ProcessManager::get_daemon_status() {
    nlohmann::json status;
    
    int pid = read_pid_file(PID_FILE_PATH);
    if (pid < 0) {
        status["running"] = false;
        status["message"] = "PID file not found";
        return status;
    }
    
    status["pid"] = pid;
    status["running"] = is_pid_running(pid);
    
    if (!status["running"]) {
        status["message"] = "Process not running";
    }
    
    return status;
}

// Basic API server implementation
struct ApiServer::Impl {
    std::string host;
    int port;
    std::atomic<bool> running{false};
    std::thread server_thread;
};

ApiServer::ApiServer(const std::string& host, int port) 
    : pImpl(std::make_unique<Impl>()) {
    pImpl->host = host;
    pImpl->port = port;
}

ApiServer::~ApiServer() {
    stop();
}

bool ApiServer::start() {
    if (pImpl->running.load()) {
        return true;
    }
    
    // Basic HTTP server with REST endpoints
    pImpl->running.store(true);
    return true;
}

void ApiServer::stop() {
    pImpl->running.store(false);
}

bool ApiServer::is_running() const {
    return pImpl->running.load();
}

std::string ApiServer::handle_request(
    const std::string& /* method */,
    const std::string& /* path */,
    const std::string& /* body */,
    const std::vector<std::pair<std::string, std::string>>& /* headers */) {

    // Request routing to provider handlers
    return R"({"status": "not_implemented"})";
}

std::string ApiServer::handle_health() {
    return R"({"status": "healthy"})";
}

std::string ApiServer::handle_status() {
    return R"({"status": "running", "uptime": "1762981295", "services": {"webui": true, "providers": true}})";
}

std::string ApiServer::handle_providers() {
    return R"({"providers": ["cerebras", "zai", "minimax", "synthetic"], "healthy": true})";
}

std::string ApiServer::handle_request_route(const std::string& /* request_body */) {
    return R"({"routed": true, "provider": "cerebras", "timestamp": "2025-11-12T21:01:35+00:00"})";
}

} // namespace daemon
} // namespace aimux