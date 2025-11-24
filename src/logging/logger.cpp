#include "aimux/logging/logger.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <mutex>
#include <unordered_map>

namespace aimux {
namespace logging {

// Logger::Impl structure definition
struct Logger::Impl {
    std::string name;
    std::string log_file;
    LogLevel level;
    bool console_enabled;
    nlohmann::json default_fields;

    Impl(const std::string& name, const std::string& log_file)
        : name(name), level(LogLevel::INFO), console_enabled(true) {
        if (!log_file.empty()) {
            this->log_file = log_file;
        }
    }
};

// LoggerRegistry implementation according to header
std::shared_ptr<Logger> LoggerRegistry::get_logger(const std::string& name, const std::string& log_file) {
    static std::unordered_map<std::string, std::shared_ptr<Logger>> loggers;
    static std::mutex mutex;

    std::lock_guard<std::mutex> lock(mutex);

    auto it = loggers.find(name);
    if (it != loggers.end()) {
        return it->second;
    }

    // Create new logger if not found
    auto logger = std::make_shared<Logger>(name, log_file);
    loggers[name] = logger;
    return logger;
}

void LoggerRegistry::remove_logger(const std::string& name) {
    static std::unordered_map<std::string, std::shared_ptr<Logger>> loggers;
    static std::mutex mutex;

    std::lock_guard<std::mutex> lock(mutex);
    loggers.erase(name);
}

// LogUtils implementation
LogLevel LogUtils::string_to_level(const std::string& level_str) {
    if (level_str == "TRACE") return LogLevel::TRACE;
    if (level_str == "DEBUG") return LogLevel::DEBUG;
    if (level_str == "INFO") return LogLevel::INFO;
    if (level_str == "WARN") return LogLevel::WARN;
    if (level_str == "ERROR") return LogLevel::ERROR;
    if (level_str == "FATAL") return LogLevel::FATAL;
    return LogLevel::INFO;
}

// Logger implementation
Logger::Logger(const std::string& name, const std::string& log_file)
    : pImpl(std::make_unique<Impl>(name, log_file)) {
}

Logger::~Logger() = default;

void Logger::set_level(LogLevel level) {
    pImpl->level = level;
}

void Logger::set_console_enabled(bool enabled) {
    pImpl->console_enabled = enabled;
}

void Logger::add_default_field(const std::string& key, const nlohmann::json& value) {
    pImpl->default_fields[key] = value;
}

void Logger::trace(const std::string& message, const nlohmann::json& data) {
    log(LogLevel::TRACE, message, data);
}

void Logger::debug(const std::string& message, const nlohmann::json& data) {
    log(LogLevel::DEBUG, message, data);
}

void Logger::info(const std::string& message, const nlohmann::json& data) {
    log(LogLevel::INFO, message, data);
}

void Logger::warn(const std::string& message, const nlohmann::json& data) {
    log(LogLevel::WARN, message, data);
}

void Logger::error(const std::string& message, const nlohmann::json& data) {
    log(LogLevel::ERROR, message, data);
}

void Logger::fatal(const std::string& message, const nlohmann::json& data) {
    log(LogLevel::FATAL, message, data);
}

nlohmann::json Logger::get_statistics() const {
    nlohmann::json stats;
    stats["logger_name"] = pImpl->name;
    stats["log_file"] = pImpl->log_file;
    stats["level"] = static_cast<int>(pImpl->level);
    stats["console_enabled"] = pImpl->console_enabled;
    return stats;
}

void Logger::flush() {
    // For simple implementation, just flush stdout
    if (pImpl->console_enabled) {
        std::cout.flush();
    }
}

void Logger::log(LogLevel level, const std::string& message, const nlohmann::json& data) {
    if (level < pImpl->level) {
        return;
    }

    // Create log entry
    nlohmann::json log_entry;
    log_entry["timestamp"] = get_timestamp();
    log_entry["level"] = static_cast<int>(level);
    log_entry["level_name"] = level_to_string(level);
    log_entry["message"] = message;
    log_entry["logger"] = pImpl->name;

    // Merge default fields
    for (const auto& [key, value] : pImpl->default_fields.items()) {
        log_entry[key] = value;
    }

    // Merge user data
    if (!data.empty()) {
        log_entry["data"] = data;
    }

    // Output log entry
    std::string log_line = log_entry.dump();

    if (pImpl->console_enabled) {
        std::cout << log_line << std::endl;
    }

    if (!pImpl->log_file.empty()) {
        std::ofstream file(pImpl->log_file, std::ios::app);
        if (file.is_open()) {
            file << log_line << std::endl;
            file.close();
        }
    }
}

std::string Logger::level_to_string(LogLevel level) const {
    switch (level) {
        case LogLevel::TRACE: return "TRACE";
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARN: return "WARN";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::FATAL: return "FATAL";
        default: return "UNKNOWN";
    }
}

std::string Logger::get_timestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z';
    return ss.str();
}

} // namespace logging
} // namespace aimux

// ============================================================================
// Global Logging Functions (aimux namespace)
// ============================================================================

namespace aimux {

// Get or create default logger
static std::shared_ptr<logging::Logger> get_default_logger() {
    static auto logger = logging::LoggerRegistry::get_logger("aimux_default");
    return logger;
}

void debug(const std::string& message, const nlohmann::json& data) {
    auto logger = get_default_logger();
    if (logger) {
        logger->debug(message, data);
    }
}

void info(const std::string& message, const nlohmann::json& data) {
    auto logger = get_default_logger();
    if (logger) {
        logger->info(message, data);
    }
}

void warn(const std::string& message, const nlohmann::json& data) {
    auto logger = get_default_logger();
    if (logger) {
        logger->warn(message, data);
    }
}

void error(const std::string& message, const nlohmann::json& data) {
    auto logger = get_default_logger();
    if (logger) {
        logger->error(message, data);
    }
}

void fatal(const std::string& message, const nlohmann::json& data) {
    auto logger = get_default_logger();
    if (logger) {
        logger->fatal(message, data);
    }
}

void trace(const std::string& message, const nlohmann::json& data) {
    auto logger = get_default_logger();
    if (logger) {
        logger->trace(message, data);
    }
}

} // namespace aimux

// ============================================================================
// Global Logging Functions (aimux::logging namespace)
// ============================================================================

namespace aimux {
namespace logging {

void debug(const std::string& message, const nlohmann::json& data) {
    aimux::debug(message, data);
}

void info(const std::string& message, const nlohmann::json& data) {
    aimux::info(message, data);
}

void warn(const std::string& message, const nlohmann::json& data) {
    aimux::warn(message, data);
}

void error(const std::string& message, const nlohmann::json& data) {
    aimux::error(message, data);
}

void fatal(const std::string& message, const nlohmann::json& data) {
    aimux::fatal(message, data);
}

void trace(const std::string& message, const nlohmann::json& data) {
    aimux::trace(message, data);
}

} // namespace logging
} // namespace aimux