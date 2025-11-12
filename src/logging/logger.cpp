#include "aimux/logging/logger.hpp"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <unordered_map>
#include <algorithm>
#include <ctime>
#include <sstream>

namespace aimux {
namespace logging {

// Logger implementation
struct Logger::Impl {
    std::string name;
    std::ofstream file_stream;
    LogLevel min_level = LogLevel::INFO;
    bool console_enabled = true;
    std::mutex mutex;
    std::unordered_map<std::string, nlohmann::json> default_fields;
    nlohmann::json stats;
    
    Impl(const std::string& logger_name, const std::string& log_file) 
        : name(logger_name) {
        stats["name"] = name;
        stats["total_logs"] = 0;
        stats["logs_by_level"] = nlohmann::json::object();
        
        if (!log_file.empty()) {
            file_stream.open(log_file, std::ios::app);
            if (!file_stream.is_open()) {
                std::cerr << "Warning: Could not open log file: " << log_file << std::endl;
            }
        }
    }
    
    void write_to_console(const nlohmann::json& entry) {
        std::cout << entry.dump(-1) << std::endl;
    }
    
    void write_to_file(const nlohmann::json& entry) {
        if (file_stream.is_open()) {
            file_stream << entry.dump(-1) << std::endl;
            file_stream.flush();
        }
    }
    
    void update_stats(LogLevel level) {
        stats["total_logs"] = stats["total_logs"].get<int>() + 1;
        std::string level_str = LogUtils::level_to_string(level);
        if (!stats["logs_by_level"].contains(level_str)) {
            stats["logs_by_level"][level_str] = 0;
        }
        stats["logs_by_level"][level_str] = stats["logs_by_level"][level_str].get<int>() + 1;
    }
};

Logger::Logger(const std::string& name, const std::string& log_file) 
    : pImpl(std::make_unique<Impl>(name, log_file)) {}

Logger::~Logger() = default;

void Logger::set_level(LogLevel level) {
    std::lock_guard<std::mutex> lock(pImpl->mutex);
    pImpl->min_level = level;
}

void Logger::set_console_enabled(bool enabled) {
    std::lock_guard<std::mutex> lock(pImpl->mutex);
    pImpl->console_enabled = enabled;
}

void Logger::add_default_field(const std::string& key, const nlohmann::json& value) {
    std::lock_guard<std::mutex> lock(pImpl->mutex);
    pImpl->default_fields[key] = value;
}

void Logger::remove_default_field(const std::string& key) {
    std::lock_guard<std::mutex> lock(pImpl->mutex);
    pImpl->default_fields.erase(key);
}

void Logger::log(LogLevel level, const std::string& message, const nlohmann::json& data) {
    if (level < pImpl->min_level) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(pImpl->mutex);
    
    nlohmann::json entry = LogUtils::create_log_entry(level, message, pImpl->name, data);
    
    // Add default fields
    for (const auto& field : pImpl->default_fields) {
        entry[field.first] = field.second;
    }
    
    // Write outputs
    if (pImpl->console_enabled) {
        pImpl->write_to_console(entry);
    }
    
    pImpl->write_to_file(entry);
    
    // Update statistics
    pImpl->update_stats(level);
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
    std::lock_guard<std::mutex> lock(pImpl->mutex);
    return pImpl->stats;
}

void Logger::flush() {
    std::lock_guard<std::mutex> lock(pImpl->mutex);
    if (pImpl->file_stream.is_open()) {
        pImpl->file_stream.flush();
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
    
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count() << "Z";
    return oss.str();
}

// LoggerRegistry implementation
std::mutex LoggerRegistry::mutex_;
std::unordered_map<std::string, std::shared_ptr<Logger>> LoggerRegistry::loggers_;

std::shared_ptr<Logger> LoggerRegistry::get_logger(const std::string& name, const std::string& log_file) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = loggers_.find(name);
    if (it != loggers_.end()) {
        return it->second;
    }
    
    auto logger = std::make_shared<Logger>(name, log_file);
    loggers_[name] = logger;
    return logger;
}

void LoggerRegistry::remove_logger(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    loggers_.erase(name);
}

std::vector<std::string> LoggerRegistry::get_logger_names() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> names;
    for (const auto& pair : loggers_) {
        names.push_back(pair.first);
    }
    return names;
}

void LoggerRegistry::set_global_level(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (auto& pair : loggers_) {
        pair.second->set_level(level);
    }
}

void LoggerRegistry::flush_all() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (auto& pair : loggers_) {
        pair.second->flush();
    }
}

// LogUtils implementation
LogLevel LogUtils::string_to_level(const std::string& level_str) {
    std::string upper_str = level_str;
    std::transform(upper_str.begin(), upper_str.end(), upper_str.begin(), ::toupper);
    
    if (upper_str == "TRACE") return LogLevel::TRACE;
    if (upper_str == "DEBUG") return LogLevel::DEBUG;
    if (upper_str == "INFO") return LogLevel::INFO;
    if (upper_str == "WARN" || upper_str == "WARNING") return LogLevel::WARN;
    if (upper_str == "ERROR") return LogLevel::ERROR;
    if (upper_str == "FATAL") return LogLevel::FATAL;
    
    return LogLevel::INFO; // Default
}

LogLevel LogUtils::parse_env_level(const std::string& env_var, LogLevel default_level) {
    const char* env_value = std::getenv(env_var.c_str());
    if (!env_value) {
        return default_level;
    }
    
    return string_to_level(std::string(env_value));
}

nlohmann::json LogUtils::create_log_entry(
    LogLevel level,
    const std::string& message,
    const std::string& logger_name,
    const nlohmann::json& data) {
    
    nlohmann::json entry;
    entry["timestamp"] = get_current_timestamp();
    entry["level"] = level_to_string(level);
    entry["logger"] = logger_name;
    entry["message"] = message;
    
    if (!data.empty()) {
        entry["data"] = data;
    }
    
    return entry;
}

std::string LogUtils::level_to_string(LogLevel level) {
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

std::string LogUtils::get_current_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count() << "Z";
    return oss.str();
}

} // namespace logging
} // namespace aimux