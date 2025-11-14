#pragma once

#include <iostream>
#include <fstream>
#include <mutex>
#include <thread>
#include <queue>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <vector>
#include <memory>
#include <algorithm>
#include <cctype>
#include <nlohmann/json.hpp>

namespace aimux {
namespace logging {

// Log levels
enum class LogLevel {
    TRACE = 0,
    DEBUG = 1,
    INFO = 2,
    WARN = 3,
    ERROR = 4,
    FATAL = 5
};

// Log entry structure
struct LogEntry {
    LogLevel level;
    std::string message;
    std::string category;
    std::string file;
    int line;
    std::string function;
    std::string correlationId;
    std::chrono::system_clock::time_point timestamp;
    nlohmann::json extra;

    LogEntry(LogLevel l, const std::string& msg, const std::string& cat,
             const std::string& f, int ln, const std::string& func,
             const std::string& corrId = "")
        : level(l), message(msg), category(cat), file(f), line(ln),
          function(func), correlationId(corrId),
          timestamp(std::chrono::system_clock::now()) {}

    static std::string generateCorrelationId();
    nlohmann::json toJson() const;
    std::string formatTimestamp() const;
};

// Forward declarations
class LogHandler;
class LogFormatter;

// Production logger configuration
struct Config {
    bool async = true;
    size_t queueSize = 10000;
    size_t batchSize = 100;
    std::chrono::milliseconds flushInterval{1000};
    bool enableConsoleLogging = true;
    bool enableFileLogging = true;
    std::string logFile = "aimux.log";
    size_t maxFileSize = 10 * 1024 * 1024; // 10MB
    size_t maxFileCount = 5;
    bool jsonConsole = false;
    bool filterSensitiveData = true;
    std::vector<std::string> sensitivePatterns = {"key", "token", "password", "secret"};
};

// Handler base class
class LogHandler {
public:
    explicit LogHandler(std::unique_ptr<LogFormatter> formatter = nullptr);
    virtual ~LogHandler() = default;
    virtual void handle(const LogEntry& entry) = 0;
    virtual void flush() = 0;

protected:
    std::unique_ptr<LogFormatter> formatter_;
};

// Formatter base class
class LogFormatter {
public:
    virtual ~LogFormatter() = default;
    virtual std::string format(const LogEntry& entry) = 0;
};

// Simple text formatter
class SimpleFormatter : public LogFormatter {
public:
    std::string format(const LogEntry& entry) override;
};

// JSON formatter
class JsonFormatter : public LogFormatter {
public:
    std::string format(const LogEntry& entry) override;
};

// Console handler
class ConsoleHandler : public LogHandler {
public:
    explicit ConsoleHandler(std::unique_ptr<LogFormatter> formatter = nullptr);
    void handle(const LogEntry& entry) override;
    void flush() override;
};

// File handler
class FileHandler : public LogHandler {
public:
    FileHandler(const std::string& filename, size_t maxFileSize = 10*1024*1024, size_t maxFileCount = 5);
    ~FileHandler();
    void handle(const LogEntry& entry) override;
    void flush() override;

private:
    std::ofstream file_;
    std::string filename_;
    size_t maxFileSize_;
    size_t maxFileCount_;
    size_t currentFileSize_;
    std::mutex fileMutex_;

    void rotateIfNeeded();
    void openFile();
};

// Correlation context for request tracing
class CorrelationContext {
public:
    static CorrelationContext& getInstance();

    void setCorrelationId(const std::string& id);
    std::string getCurrentCorrelationId() const;
    void clear();
    nlohmann::json toJson() const;

private:
    std::string correlationId_;
    mutable std::mutex mutex_;
};

// Main production logger (singleton)
class ProductionLogger {
public:
    static ProductionLogger& getInstance();

    // Configuration
    void configure(const Config& config);
    void setLevel(LogLevel level);
    LogLevel getLevel() const;

    // Handler management
    void addHandler(std::unique_ptr<LogHandler> handler);
    void removeHandler(const std::string& type);

    // Logging interface
    void log(LogLevel level, const std::string& message,
             const std::string& file, int line, const std::string& function,
             const std::string& correlationId = "",
             const nlohmann::json& extra = nlohmann::json{});

    // Control operations
    void flush();
    void shutdown();

private:
    ProductionLogger();
    ~ProductionLogger();

    // Non-copyable
    ProductionLogger(const ProductionLogger&) = delete;
    ProductionLogger& operator=(const ProductionLogger&) = delete;

    // Implementation details
    void start();
    void workerThread();
    void processQueue();
    nlohmann::json filterSensitiveData(const nlohmann::json& data) const;

    // Configuration and state
    Config config_;
    LogLevel level_;
    std::atomic<bool> running_;
    std::atomic<bool> flushScheduled_;

    // Handlers
    std::vector<std::unique_ptr<LogHandler>> handlers_;
    std::mutex handlersMutex_;

    // Async processing
    std::queue<LogEntry> logQueue_;
    std::mutex queueMutex_;
    std::condition_variable queueCondition_;
    std::thread workerThread_;
    std::thread flushThread_;
    std::vector<LogEntry> batch_;
};

// Logger wrapper class for easy usage
class Logger {
public:
    explicit Logger(const std::string& category = "aimux", const std::string& correlationId = "");

    // Set correlation ID for this logger instance
    void setCorrelationId(const std::string& id);

    // Logging methods
    void trace(const std::string& message, const nlohmann::json& extra = {});
    void debug(const std::string& message, const nlohmann::json& extra = {});
    void info(const std::string& message, const nlohmann::json& extra = {});
    void warn(const std::string& message, const nlohmann::json& extra = {});
    void error(const std::string& message, const nlohmann::json& extra = {});
    void fatal(const std::string& message, const nlohmann::json& extra = {});

    // Internal logging method
    void log(LogLevel level, const std::string& message, const nlohmann::json& extra = {},
             const std::string& explicitCorrelationId = "");

private:
    std::string category_;
    std::string correlationId_;

    std::string getEffectiveCorrelationId(const std::string& explicitCorrelationId) const;
};

} // namespace logging
} // namespace aimux