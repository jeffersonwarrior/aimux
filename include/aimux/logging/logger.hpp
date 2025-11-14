#pragma once

#include <string>
#include <memory>
#include <fstream>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <nlohmann/json.hpp>

namespace aimux {
namespace logging {

/**
 * @brief Log levels
 */
enum class LogLevel {
    TRACE = 0,
    DEBUG = 1,
    INFO = 2,
    WARN = 3,
    ERROR = 4,
    FATAL = 5
};

/**
 * @brief Logger class for structured JSON logging
 */
class Logger {
public:
    /**
     * @brief Constructor
     * @param name Logger name
     * @param log_file Path to log file (optional)
     */
    explicit Logger(const std::string& name, const std::string& log_file = "");
    
    /**
     * @brief Destructor
     */
    ~Logger();
    
    /**
     * @brief Set minimum log level
     * @param level Minimum level to log
     */
    void set_level(LogLevel level);
    
    /**
     * @brief Enable/disable console output
     * @param enabled Whether to enable console logging
     */
    void set_console_enabled(bool enabled);
    
    /**
     * @brief Add default field to all log entries
     * @param key Field key
     * @param value Field value
     */
    void add_default_field(const std::string& key, const nlohmann::json& value);
    
    /**
     * @brief Remove default field
     * @param key Field key to remove
     */
    void remove_default_field(const std::string& key);
    
    /**
     * @brief Log a message at specified level
     * @param level Log level
     * @param message Log message
     * @param data Additional structured data
     */
    void log(LogLevel level, const std::string& message, const nlohmann::json& data = nlohmann::json{});
    
    /**
     * @brief Convenience logging methods
     */
    void trace(const std::string& message, const nlohmann::json& data = nlohmann::json{});
    void debug(const std::string& message, const nlohmann::json& data = nlohmann::json{});
    void info(const std::string& message, const nlohmann::json& data = nlohmann::json{});
    void warn(const std::string& message, const nlohmann::json& data = nlohmann::json{});
    void error(const std::string& message, const nlohmann::json& data = nlohmann::json{});
    void fatal(const std::string& message, const nlohmann::json& data = nlohmann::json{});
    
    /**
     * @brief Get logger statistics
     * @return JSON with logging metrics
     */
    nlohmann::json get_statistics() const;
    
    /**
     * @brief Flush all pending log entries
     */
    void flush();

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl;
    
    /**
     * @brief Convert log level to string
     * @param level Log level
     * @return String representation
     */
    std::string level_to_string(LogLevel level) const;
    
    /**
     * @brief Get current timestamp as ISO string
     * @return ISO timestamp
     */
    std::string get_timestamp() const;
};

/**
 * @brief Global logger registry
 */
class LoggerRegistry {
public:
    /**
     * @brief Get or create a logger instance
     * @param name Logger name
     * @param log_file Path to log file (optional)
     * @return Shared pointer to logger
     */
    static std::shared_ptr<Logger> get_logger(const std::string& name, const std::string& log_file = "");
    
    /**
     * @brief Remove logger from registry
     * @param name Logger name
     */
    static void remove_logger(const std::string& name);
    
    /**
     * @brief Get list of all logger names
     * @return Vector of logger names
     */
    static std::vector<std::string> get_logger_names();
    
    /**
     * @brief Set global log level for all loggers
     * @param level Global log level
     */
    static void set_global_level(LogLevel level);
    
    /**
     * @brief Flush all loggers
     */
    static void flush_all();

private:
    static std::mutex mutex_;
    static std::unordered_map<std::string, std::shared_ptr<Logger>> loggers_;
};

/**
 * @brief Utility functions for logging
 */
class LogUtils {
public:
    /**
     * @brief Convert log level string to enum
     * @param level_str String representation
     * @return Log level enum
     */
    static LogLevel string_to_level(const std::string& level_str);
    
    /**
     * @brief Parse log level from environment variable
     * @param env_var Environment variable name
     * @param default_level Default level if not found
     * @return Parsed log level
     */
    static LogLevel parse_env_level(const std::string& env_var, LogLevel default_level = LogLevel::INFO);
    
    /**
     * @brief Create log entry with common fields
     * @param level Log level
     * @param message Message
     * @param logger_name Logger name
     * @param data Additional data
     * @return JSON log entry
     */
    static nlohmann::json create_log_entry(
        LogLevel level,
        const std::string& message,
        const std::string& logger_name,
        const nlohmann::json& data = nlohmann::json{}
    );
    
    /**
     * @brief Convert log level to string
     * @param level Log level
     * @return String representation
     */
    static std::string level_to_string(LogLevel level);
    
    /**
     * @brief Get current timestamp as ISO string
     * @return ISO timestamp
     */
    static std::string get_current_timestamp();
};

// Global convenience functions
void debug(const std::string& message, const nlohmann::json& data = nlohmann::json{});
void info(const std::string& message, const nlohmann::json& data = nlohmann::json{});
void warn(const std::string& message, const nlohmann::json& data = nlohmann::json{});
void error(const std::string& message, const nlohmann::json& data = nlohmann::json{});
void fatal(const std::string& message, const nlohmann::json& data = nlohmann::json{});
void trace(const std::string& message, const nlohmann::json& data = nlohmann::json{});

} // namespace logging
} // namespace aimux

// Global convenience functions in aimux namespace
namespace aimux {
void debug(const std::string& message, const nlohmann::json& data = nlohmann::json{});
void info(const std::string& message, const nlohmann::json& data = nlohmann::json{});
void warn(const std::string& message, const nlohmann::json& data = nlohmann::json{});
void error(const std::string& message, const nlohmann::json& data = nlohmann::json{});
void fatal(const std::string& message, const nlohmann::json& data = nlohmann::json{});
void trace(const std::string& message, const nlohmann::json& data = nlohmann::json{});
} // namespace aimux

