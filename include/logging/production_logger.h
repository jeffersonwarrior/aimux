#ifndef AIMUX_PRODUCTION_LOGGER_H
#define AIMUX_PRODUCTION_LOGGER_H

#include <string>
#include <memory>
#include <fstream>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <chrono>
#include <nlohmann/json.hpp>
#include <sstream>
#include <filesystem>

#include "aimux/logging/correlation_context.h"

namespace aimux {
namespace logging {

/**
 * Production-grade logger with structured JSON logging
 * Features: async logging, log rotation, multiple outputs, security filtering
 */

enum class LogLevel {
    TRACE = 0,
    DEBUG = 1,
    INFO = 2,
    WARN = 3,
    ERROR = 4,
    FATAL = 5
};

enum class LogOutput {
    CONSOLE,
    FILE,
    SYSLOG,
    JOURNALD,
    NETWORK
};

// Log entry structure
struct LogEntry {
    std::chrono::system_clock::time_point timestamp;
    LogLevel level;
    std::string logger;
    std::string message;
    std::string threadId;
    std::string fileId;
    int line;
    std::string function;
    std::string correlationId;
    nlohmann::json extra;

    LogEntry(LogLevel lvl, const std::string& msg, const std::string& lg,
             const std::string& file, int ln, const std::string& func,
             const std::string& correlationId = "")
        : timestamp(std::chrono::system_clock::now()), level(lvl), logger(lg),
          message(msg), fileId(file), line(ln), function(func),
          correlationId(correlationId) {

        // Extract filename from path
        std::filesystem::path filePath(file);
        fileId = filePath.filename().string();

        // Generate thread ID
        std::ostringstream oss;
        oss << std::this_thread::get_id();
        threadId = oss.str();

        // Generate correlation ID if not provided
        if (this->correlationId.empty()) {
            this->correlationId = generateCorrelationId();
        }
    }

    // Generate correlation ID for request tracing
    static std::string generateCorrelationId() {
        static std::atomic<uint64_t> counter{0};
        auto now = std::chrono::high_resolution_clock::now();
        auto timestamp = now.time_since_epoch().count();
        auto count = counter++;

        std::ostringstream oss;
        oss << std::hex << timestamp << "-" << std::hex << count;
        return oss.str();
    }

    nlohmann::json toJson() const {
        nlohmann::json j;

        // Standardized field names following RFC 5424 and best practices
        // Timestamp in milliseconds since epoch for consistency
        j["@timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            timestamp.time_since_epoch()).count();

        // Event metadata
        j["level"] = levelToString(level);
        j["logger_name"] = logger;
        j["message"] = message;

        // Request tracing
        j["correlation_id"] = correlationId;

        // Thread context
        j["thread_id"] = threadId;

        // Source location information
        j["source"] = nlohmann::json{
            {"file", fileId},
            {"line", line},
            {"function", function}
        };

        // Service context for distributed tracing
        j["service"] = nlohmann::json{
            {"name", "aimux2"},
            {"version", "2.0.0"}
        };

        // Add extra fields under structured_data for consistency
        if (!extra.empty()) {
            j["structured_data"] = extra;
        }

        return j;
    }

    static std::string levelToString(LogLevel level) {
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

    static LogLevel stringToLevel(const std::string& level) {
        if (level == "TRACE") return LogLevel::TRACE;
        if (level == "DEBUG") return LogLevel::DEBUG;
        if (level == "INFO") return LogLevel::INFO;
        if (level == "WARN") return LogLevel::WARN;
        if (level == "ERROR") return LogLevel::ERROR;
        if (level == "FATAL") return LogLevel::FATAL;
        return LogLevel::INFO;
    }
};

// Log formatter interface
class LogFormatter {
public:
    virtual ~LogFormatter() = default;
    virtual std::string format(const LogEntry& entry) = 0;
};

// JSON formatter
class JsonFormatter : public LogFormatter {
public:
    std::string format(const LogEntry& entry) override {
        nlohmann::json json = entry.toJson();
        json["service"] = "aimux2";
        return json.dump();
    }
};

// Pretty console formatter
class ConsoleFormatter : public LogFormatter {
public:
    std::string format(const LogEntry& entry) override {
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            entry.timestamp.time_since_epoch()).count();
        auto secs = ms / 1000;
        auto millis = ms % 1000;

        std::time_t time_t = secs;
        std::tm tm = *std::localtime(&time_t);

        std::ostringstream oss;
        // Timestamp
        oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
        oss << '.' << std::setfill('0') << std::setw(3) << millis << ' ';

        // Level (colored for console output)
        oss << '[' << std::setw(5) << LogEntry::levelToString(entry.level) << "] ";

        // Correlation ID (shortened for console)
        std::string shortCorrelationId = entry.correlationId.length() > 12
            ? entry.correlationId.substr(0, 12) + "..."
            : entry.correlationId;
        oss << '[' << shortCorrelationId << "] ";

        // Thread
        oss << '[' << entry.threadId << "] ";

        // Source location
        oss << '[' << entry.fileId << ':' << entry.line << "] ";

        // Message
        oss << entry.message;

        return oss.str();
    }
};

// Log handler interface
class LogHandler {
public:
    virtual ~LogHandler() = default;
    virtual void handle(const LogEntry& entry) = 0;
    virtual void flush() {}
};

// Console handler
class ConsoleHandler : public LogHandler {
public:
    ConsoleHandler(std::unique_ptr<LogFormatter> formatter = std::make_unique<ConsoleFormatter>())
        : formatter_(std::move(formatter)) {}

    void handle(const LogEntry& entry) override {
        std::string formatted = formatter_->format(entry);
        if (entry.level >= LogLevel::ERROR) {
            std::cerr << formatted << std::endl;
        } else {
            std::cout << formatted << std::endl;
        }
    }

private:
    std::unique_ptr<LogFormatter> formatter_;
};

// File handler with rotation
class FileHandler : public LogHandler {
public:
    FileHandler(const std::string& filename, size_t maxSize = 10 * 1024 * 1024, int maxFiles = 10)
        : filename_(filename), maxSize_(maxSize), maxFiles_(maxFiles),
          formatter_(std::make_unique<JsonFormatter>()) {
        openFile();
    }

    void handle(const LogEntry& entry) override {
        std::string formatted = formatter_->format(entry) + '\n';

        std::lock_guard<std::mutex> lock(mutex_);

        if (!file_.is_open()) {
            openFile();
        }

        if (file_.tellp() > maxSize_) {
            rotateLog();
        }

        file_ << formatted;

        // Force flush for errors and higher
        if (entry.level >= LogLevel::ERROR) {
            file_.flush();
        }
    }

    void flush() override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (file_.is_open()) {
            file_.flush();
        }
    }

private:
    void openFile() {
        if (file_.is_open()) {
            file_.close();
        }

        // Ensure directory exists
        std::filesystem::path filePath(filename_);
        std::filesystem::path dir = filePath.parent_path();
        if (!dir.empty() && !std::filesystem::exists(dir)) {
            std::filesystem::create_directories(dir);
        }

        file_.open(filename_, std::ios::app);
    }

    void rotateLog() {
        file_.close();

        // Remove oldest file if it exists
        std::string oldest = filename_ + "." + std::to_string(maxFiles_);
        std::filesystem::remove(oldest);

        // Rotate existing files
        for (int i = maxFiles_ - 1; i >= 1; --i) {
            std::string current = filename_ + "." + std::to_string(i);
            std::string next = filename_ + "." + std::to_string(i + 1);
            std::filesystem::rename(current, next);
        }

        // Move current file to .1
        std::string rotated = filename_ + ".1";
        std::filesystem::rename(filename_, rotated);

        // Open new file
        openFile();
    }

    std::string filename_;
    size_t maxSize_;
    int maxFiles_;
    std::unique_ptr<LogFormatter> formatter_;
    std::ofstream file_;
    std::mutex mutex_;
};

// Async logger with queue
class ProductionLogger {
public:
    static ProductionLogger& getInstance();

    void setLevel(LogLevel level);
    LogLevel getLevel() const;

    void addHandler(std::unique_ptr<LogHandler> handler);
    void removeHandler(const std::string& type);

    void log(LogLevel level, const std::string& message,
             const std::string& file, int line, const std::string& function,
             const std::string& correlationId = "",
             const nlohmann::json& extra = {});

    void flush();
    void shutdown();

    // Configuration
    struct Config {
        LogLevel level = LogLevel::INFO;
        bool async = true;
        size_t queueSize = 10000;
        std::chrono::milliseconds flushInterval{100};
        size_t batchSize = 100;

        // File logging
        bool enableFileLogging = false;
        std::string logFile = "/var/log/aimux/aimux.log";
        size_t maxFileSize = 10 * 1024 * 1024; // 10MB
        int maxFileCount = 10;

        // Console logging
        bool enableConsoleLogging = true;
        bool jsonConsole = false; // false = human readable, true = JSON

        // Security
        bool filterSensitiveData = true;
        std::vector<std::string> sensitivePatterns = {
            "api_key", "password", "token", "secret"
        };
    };

    void configure(const Config& config);

private:
    ProductionLogger();
    ~ProductionLogger();

    ProductionLogger(const ProductionLogger&) = delete;
    ProductionLogger& operator=(const ProductionLogger&) = delete;

    void workerThread();
    void processQueue();
    void flushQueuedLogs();
    nlohmann::json filterSensitiveData(const nlohmann::json& data) const;

    LogLevel level_;
    Config config_;

    std::vector<std::unique_ptr<LogHandler>> handlers_;
    mutable std::mutex handlersMutex_;

    // Async processing
    std::atomic<bool> running_;
    std::queue<LogEntry> logQueue_;
    mutable std::mutex queueMutex_;
    std::condition_variable queueCondition_;
    std::thread workerThread_;

    // Batch processing
    std::vector<LogEntry> batch_;
    std::thread flushThread_;
    std::atomic<bool> flushScheduled_;
};

// Logger wrapper for easy usage with correlation ID support
class Logger {
public:
    Logger(const std::string& name) : name_(name), correlationId_("") {}

    Logger(const std::string& name, const std::string& correlationId)
        : name_(name), correlationId_(correlationId) {}

    void trace(const std::string& message, const nlohmann::json& extra = {}) {
        log(LogLevel::TRACE, message, extra);
    }

    void debug(const std::string& message, const nlohmann::json& extra = {}) {
        log(LogLevel::DEBUG, message, extra);
    }

    void info(const std::string& message, const nlohmann::json& extra = {}) {
        log(LogLevel::INFO, message, extra);
    }

    void warn(const std::string& message, const nlohmann::json& extra = {}) {
        log(LogLevel::WARN, message, extra);
    }

    void error(const std::string& message, const nlohmann::json& extra = {}) {
        log(LogLevel::ERROR, message, extra);
    }

    void fatal(const std::string& message, const nlohmann::json& extra = {}) {
        log(LogLevel::FATAL, message, extra);
    }

    // Overloads with explicit correlation ID support
    void trace_with_correlation(const std::string& message, const std::string& correlationId,
                                const nlohmann::json& extra = {}) {
        log(LogLevel::TRACE, message, extra, correlationId);
    }

    void debug_with_correlation(const std::string& message, const std::string& correlationId,
                                const nlohmann::json& extra = {}) {
        log(LogLevel::DEBUG, message, extra, correlationId);
    }

    void info_with_correlation(const std::string& message, const std::string& correlationId,
                               const nlohmann::json& extra = {}) {
        log(LogLevel::INFO, message, extra, correlationId);
    }

    void warn_with_correlation(const std::string& message, const std::string& correlationId,
                               const nlohmann::json& extra = {}) {
        log(LogLevel::WARN, message, extra, correlationId);
    }

    void error_with_correlation(const std::string& message, const std::string& correlationId,
                                const nlohmann::json& extra = {}) {
        log(LogLevel::ERROR, message, extra, correlationId);
    }

    void fatal_with_correlation(const std::string& message, const std::string& correlationId,
                                const nlohmann::json& extra = {}) {
        log(LogLevel::FATAL, message, extra, correlationId);
    }

    // Thread-local correlation ID setter
    void setCurrentCorrelationId(const std::string& correlationId) {
        correlationId_ = correlationId;
    }

    const std::string& getCurrentCorrelationId() const {
        return correlationId_;
    }

    const std::string& getName() const { return name_; }

    // Static method to generate correlation ID
    static std::string generateCorrelationId() {
        return LogEntry::generateCorrelationId();
    }

private:
    void log(LogLevel level, const std::string& message, const nlohmann::json& extra = {},
             const std::string& explicitCorrelationId = "");

    std::string getEffectiveCorrelationId(const std::string& explicitCorrelationId) const;

    std::string name_;
    std::string correlationId_;
};

// Utility macros for logging
#define AIMUX_LOGGER(logger) \
    aimux::logging::Logger(logger)

#define AIMUX_LOGGER_WITH_CORRELATION(logger, correlationId) \
    aimux::logging::Logger(logger, correlationId)

// Auto-correlation logging macros (uses thread-local context)
#define AIMUX_LOG_AUTO_TRACE(message) \
    aimux::logging::CorrelationContext::getInstance().getCurrentCorrelationId().empty() ? \
        AIMUX_LOGGER("auto").trace(message) : \
        AIMUX_LOGGER_WITH_CORRELATION("auto", AIMUX_CURRENT_CORRELATION_ID()).trace(message)

#define AIMUX_LOG_AUTO_DEBUG(message) \
    aimux::logging::CorrelationContext::getInstance().getCurrentCorrelationId().empty() ? \
        AIMUX_LOGGER("auto").debug(message) : \
        AIMUX_LOGGER_WITH_CORRELATION("auto", AIMUX_CURRENT_CORRELATION_ID()).debug(message)

#define AIMUX_LOG_AUTO_INFO(message) \
    aimux::logging::CorrelationContext::getInstance().getCurrentCorrelationId().empty() ? \
        AIMUX_LOGGER("auto").info(message) : \
        AIMUX_LOGGER_WITH_CORRELATION("auto", AIMUX_CURRENT_CORRELATION_ID()).info(message)

#define AIMUX_LOG_AUTO_WARN(message) \
    aimux::logging::CorrelationContext::getInstance().getCurrentCorrelationId().empty() ? \
        AIMUX_LOGGER("auto").warn(message) : \
        AIMUX_LOGGER_WITH_CORRELATION("auto", AIMUX_CURRENT_CORRELATION_ID()).warn(message)

#define AIMUX_LOG_AUTO_ERROR(message) \
    aimux::logging::CorrelationContext::getInstance().getCurrentCorrelationId().empty() ? \
        AIMUX_LOGGER("auto").error(message) : \
        AIMUX_LOGGER_WITH_CORRELATION("auto", AIMUX_CURRENT_CORRELATION_ID()).error(message)

#define AIMUX_LOG_AUTO_FATAL(message) \
    aimux::logging::CorrelationContext::getInstance().getCurrentCorrelationId().empty() ? \
        AIMUX_LOGGER("auto").fatal(message) : \
        AIMUX_LOGGER_WITH_CORRELATION("auto", AIMUX_CURRENT_CORRELATION_ID()).fatal(message)

// Context-aware structured logging macros
#define AIMUX_LOG_TRACE_EX(message, extra) \
    aimux::logging::CorrelationContext::getInstance().getCurrentCorrelationId().empty() ? \
        AIMUX_LOGGER("auto").trace(message, extra) : \
        AIMUX_LOGGER_WITH_CORRELATION("auto", AIMUX_CURRENT_CORRELATION_ID()).trace(message, extra)

#define AIMUX_LOG_DEBUG_EX(message, extra) \
    aimux::logging::CorrelationContext::getInstance().getCurrentCorrelationId().empty() ? \
        AIMUX_LOGGER("auto").debug(message, extra) : \
        AIMUX_LOGGER_WITH_CORRELATION("auto", AIMUX_CURRENT_CORRELATION_ID()).debug(message, extra)

#define AIMUX_LOG_INFO_EX(message, extra) \
    aimux::logging::CorrelationContext::getInstance().getCurrentCorrelationId().empty() ? \
        AIMUX_LOGGER("auto").info(message, extra) : \
        AIMUX_LOGGER_WITH_CORRELATION("auto", AIMUX_CURRENT_CORRELATION_ID()).info(message, extra)

#define AIMUX_LOG_WARN_EX(message, extra) \
    aimux::logging::CorrelationContext::getInstance().getCurrentCorrelationId().empty() ? \
        AIMUX_LOGGER("auto").warn(message, extra) : \
        AIMUX_LOGGER_WITH_CORRELATION("auto", AIMUX_CURRENT_CORRELATION_ID()).warn(message, extra)

#define AIMUX_LOG_ERROR_EX(message, extra) \
    aimux::logging::CorrelationContext::getInstance().getCurrentCorrelationId().empty() ? \
        AIMUX_LOGGER("auto").error(message, extra) : \
        AIMUX_LOGGER_WITH_CORRELATION("auto", AIMUX_CURRENT_CORRELATION_ID()).error(message, extra)

#define AIMUX_LOG_FATAL_EX(message, extra) \
    aimux::logging::CorrelationContext::getInstance().getCurrentCorrelationId().empty() ? \
        AIMUX_LOGGER("auto").fatal(message, extra) : \
        AIMUX_LOGGER_WITH_CORRELATION("auto", AIMUX_CURRENT_CORRELATION_ID()).fatal(message, extra)

// Standard logging macros
#define AIMUX_LOG_TRACE(message) \
    AIMUX_LOGGER("default").trace(message)

#define AIMUX_LOG_DEBUG(message) \
    AIMUX_LOGGER("default").debug(message)

#define AIMUX_LOG_INFO(message) \
    AIMUX_LOGGER("default").info(message)

#define AIMUX_LOG_WARN(message) \
    AIMUX_LOGGER("default").warn(message)

#define AIMUX_LOG_ERROR(message) \
    AIMUX_LOGGER("default").error(message)

#define AIMUX_LOG_FATAL(message) \
    AIMUX_LOGGER("default").fatal(message)

// Logger-specific macros
#define AIMUX_LOG_TRACE_WITH(logger, message) \
    AIMUX_LOGGER(logger).trace(message)

#define AIMUX_LOG_DEBUG_WITH(logger, message) \
    AIMUX_LOGGER(logger).debug(message)

#define AIMUX_LOG_INFO_WITH(logger, message) \
    AIMUX_LOGGER(logger).info(message)

#define AIMUX_LOG_WARN_WITH(logger, message) \
    AIMUX_LOGGER(logger).warn(message)

#define AIMUX_LOG_ERROR_WITH(logger, message) \
    AIMUX_LOGGER(logger).error(message)

#define AIMUX_LOG_FATAL_WITH(logger, message) \
    AIMUX_LOGGER(logger).fatal(message)

// Correlation ID logging macros
#define AIMUX_LOG_TRACE_CORR(message, correlationId) \
    AIMUX_LOGGER("default").trace_with_correlation(message, correlationId)

#define AIMUX_LOG_DEBUG_CORR(message, correlationId) \
    AIMUX_LOGGER("default").debug_with_correlation(message, correlationId)

#define AIMUX_LOG_INFO_CORR(message, correlationId) \
    AIMUX_LOGGER("default").info_with_correlation(message, correlationId)

#define AIMUX_LOG_WARN_CORR(message, correlationId) \
    AIMUX_LOGGER("default").warn_with_correlation(message, correlationId)

#define AIMUX_LOG_ERROR_CORR(message, correlationId) \
    AIMUX_LOGGER("default").error_with_correlation(message, correlationId)

#define AIMUX_LOG_FATAL_CORR(message, correlationId) \
    AIMUX_LOGGER("default").fatal_with_correlation(message, correlationId)

// Logger and correlation ID specific macros
#define AIMUX_LOG_TRACE_WITH_CORR(logger, message, correlationId) \
    AIMUX_LOGGER(logger).trace_with_correlation(message, correlationId)

#define AIMUX_LOG_DEBUG_WITH_CORR(logger, message, correlationId) \
    AIMUX_LOGGER(logger).debug_with_correlation(message, correlationId)

#define AIMUX_LOG_INFO_WITH_CORR(logger, message, correlationId) \
    AIMUX_LOGGER(logger).info_with_correlation(message, correlationId)

#define AIMUX_LOG_WARN_WITH_CORR(logger, message, correlationId) \
    AIMUX_LOGGER(logger).warn_with_correlation(message, correlationId)

#define AIMUX_LOG_ERROR_WITH_CORR(logger, message, correlationId) \
    AIMUX_LOGGER(logger).error_with_correlation(message, correlationId)

#define AIMUX_LOG_FATAL_WITH_CORR(logger, message, correlationId) \
    AIMUX_LOGGER(logger).fatal_with_correlation(message, correlationId)

// Structured data logging macros
#define AIMUX_LOG_INFO_STRUCTURED(message, structuredData) \
    AIMUX_LOGGER("default").info(message, structuredData)

#define AIMUX_LOG_ERROR_STRUCTURED(message, structuredData) \
    AIMUX_LOGGER("default").error(message, structuredData)

#define AIMUX_LOG_INFO_WITH_STRUCTURED(logger, message, structuredData) \
    AIMUX_LOGGER(logger).info(message, structuredData)

#define AIMUX_LOG_ERROR_WITH_STRUCTURED(logger, message, structuredData) \
    AIMUX_LOGGER(logger).error(message, structuredData)

} // namespace logging
} // namespace aimux

#endif // AIMUX_PRODUCTION_LOGGER_H