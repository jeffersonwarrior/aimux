#pragma once

#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <signal.h>
#include <nlohmann/json.hpp>

namespace aimux {
namespace daemon {

/**
 * @brief Daemon server for background operation
 */
class AimuxDaemon {
public:
    /**
     * @brief Constructor
     * @param config_file Path to configuration file
     */
    explicit AimuxDaemon(const std::string& config_file = "config.json");
    
    /**
     * @brief Destructor
     */
    ~AimuxDaemon();
    
    /**
     * @brief Start daemon in background
     * @param daemonize Whether to fork to background
     * @return true if started successfully
     */
    bool start(bool daemonize = true);
    
    /**
     * @brief Stop daemon
     */
    void stop();
    
    /**
     * @brief Check if daemon is running
     * @return true if running
     */
    bool is_running() const;
    
    /**
     * @brief Get daemon status
     * @return JSON status information
     */
    nlohmann::json get_status() const;
    
    /**
     * @brief Reload configuration
     * @return true if reloaded successfully
     */
    bool reload_config();

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl;
    
    /**
     * @brief Daemon main loop (legacy)
     */
    void daemon_loop();

    /**
     * @brief Managed daemon loop with proper resource handling
     * @param should_stop Atomic stop flag for graceful shutdown
     */
    void daemon_loop_managed(std::atomic<bool>& should_stop);

    /**
     * @brief Perform periodic maintenance tasks
     */
    void perform_maintenance_tasks();

    /**
     * @brief Perform system health checks
     */
    void perform_health_check();

    /**
     * @brief Monitor memory usage and alert on thresholds
     */
    void monitor_memory_usage();

    /**
     * @brief Cleanup stale connections and resources
     */
    void cleanup_stale_connections();
    
    /**
     * @brief Setup signal handlers
     */
    void setup_signals();
    
    /**
     * @brief Fork to background
     * @return true if forked successfully
     */
    bool daemonize_process();
    
    /**
     * @brief Write PID file
     */
    void write_pid_file();
    
    /**
     * @brief Remove PID file
     */
    void remove_pid_file();
};

/**
 * @brief HTTP API server for daemon
 */
class ApiServer {
public:
    /**
     * @brief Constructor
     * @param host Server host
     * @param port Server port
     */
    explicit ApiServer(const std::string& host = "localhost", int port = 8080);
    
    /**
     * @brief Destructor
     */
    ~ApiServer();
    
    /**
     * @brief Start API server
     * @return true if started successfully
     */
    bool start();
    
    /**
     * @brief Stop API server
     */
    void stop();
    
    /**
     * @brief Check if server is running
     * @return true if running
     */
    bool is_running() const;

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl;
    
    /**
     * @brief Handle HTTP request
     * @param method HTTP method
     * @param path Request path
     * @param body Request body
     * @param headers Request headers
     * @return HTTP response
     */
    std::string handle_request(
        const std::string& method,
        const std::string& path,
        const std::string& body,
        const std::vector<std::pair<std::string, std::string>>& headers
    );
    
    /**
     * @brief API endpoints
     */
    std::string handle_health();
    std::string handle_status();
    std::string handle_providers();
    std::string handle_request_route(const std::string& request_body);
};

/**
 * @brief Process management utilities
 */
class ProcessManager {
public:
    /**
     * @brief Check if PID is running
     * @param pid Process ID
     * @return true if running
     */
    static bool is_pid_running(int pid);
    
    /**
     * @brief Read PID from file
     * @param pid_file Path to PID file
     * @return PID or -1 if error
     */
    static int read_pid_file(const std::string& pid_file);
    
    /**
     * @brief Send signal to daemon
     * @param signal Signal to send
     * @return true if sent successfully
     */
    static bool send_signal(int signal);
    
    /**
     * @brief Get daemon status from PID file
     * @return Status information
     */
    static nlohmann::json get_daemon_status();

private:
    static const std::string PID_FILE;
};

/**
 * @brief Configuration manager
 */
class ConfigManager {
public:
    /**
     * @brief Load configuration from file
     * @param config_file Path to config file
     * @return Loaded configuration
     */
    static nlohmann::json load_config(const std::string& config_file);
    
    /**
     * @brief Save configuration to file
     * @param config Configuration to save
     * @param config_file Path to save file
     * @return true if saved successfully
     */
    static bool save_config(const nlohmann::json& config, const std::string& config_file);
    
    /**
     * @brief Generate default config file
     * @param config_file Path where to save default config
     * @return true if generated successfully
     */
    static bool generate_default_config(const std::string& config_file);
    
    /**
     * @brief Validate configuration
     * @param config Configuration to validate
     * @return true if valid
     */
    static bool validate_config(const nlohmann::json& config);

private:
    /**
     * @brief Get default config template
     */
    static nlohmann::json get_default_config_template();
};

} // namespace daemon
} // namespace aimux