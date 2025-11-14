#include "aimux/logging/production_logger.h"
#include <iomanip>
#include <sstream>
#include <random>
#include <iomanip>
#include <ctime>
#include <filesystem>

namespace aimux {
namespace logging {

// LogEntry implementations
std::string LogEntry::generateCorrelationId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);

    std::stringstream ss;
    for (int i = 0; i < 8; ++i) {
        ss << std::hex << dis(gen);
    }
    return ss.str();
}

std::string LogEntry::formatTimestamp() const {
    auto time_t = std::chrono::system_clock::to_time_t(timestamp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        timestamp.time_since_epoch()) % 1000;

    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

nlohmann::json LogEntry::toJson() const {
    nlohmann::json json;
    json["timestamp"] = formatTimestamp();
    json["level"] = static_cast<int>(level);
    json["level_name"] = level == LogLevel::TRACE ? "TRACE" :
                         level == LogLevel::DEBUG ? "DEBUG" :
                         level == LogLevel::INFO ? "INFO" :
                         level == LogLevel::WARN ? "WARN" :
                         level == LogLevel::ERROR ? "ERROR" : "FATAL";
    json["message"] = message;
    json["category"] = category;
    json["file"] = file;
    json["line"] = line;
    json["function"] = function;
    if (!correlationId.empty()) {
        json["correlation_id"] = correlationId;
    }
    if (!extra.empty()) {
        json["extra"] = extra;
    }
    return json;
}

// LogHandler implementation
LogHandler::LogHandler(std::unique_ptr<LogFormatter> formatter)
    : formatter_(std::move(formatter)) {}

// SimpleFormatter implementation
std::string SimpleFormatter::format(const LogEntry& entry) {
    std::stringstream ss;
    ss << entry.formatTimestamp() << " ";

    const char* level_str = "UNKNOWN";
    switch (entry.level) {
        case LogLevel::TRACE: level_str = "TRACE"; break;
        case LogLevel::DEBUG: level_str = "DEBUG"; break;
        case LogLevel::INFO: level_str = "INFO"; break;
        case LogLevel::WARN: level_str = "WARN"; break;
        case LogLevel::ERROR: level_str = "ERROR"; break;
        case LogLevel::FATAL: level_str = "FATAL"; break;
    }

    ss << '[' << level_str << "] ";
    ss << '(' << entry.function << ':' << entry.line << ") ";
    ss << entry.message;

    if (!entry.correlationId.empty()) {
        ss << " [cid:" << entry.correlationId << "]";
    }

    ss << std::endl;
    return ss.str();
}

// JsonFormatter implementation
std::string JsonFormatter::format(const LogEntry& entry) {
    return entry.toJson().dump() + "\n";
}

// ConsoleHandler implementation
ConsoleHandler::ConsoleHandler(std::unique_ptr<LogFormatter> formatter)
    : LogHandler(std::move(formatter)) {}

void ConsoleHandler::handle(const LogEntry& entry) {
    std::string formatted = formatter_ ? formatter_->format(entry) : entry.toJson().dump();
    std::cout << formatted;
}

void ConsoleHandler::flush() {
    std::cout.flush();
}

// FileHandler implementation
FileHandler::FileHandler(const std::string& filename, size_t maxFileSize, size_t maxFileCount)
    : filename_(filename), maxFileSize_(maxFileSize), maxFileCount_(maxFileCount), currentFileSize_(0) {
    openFile();
}

FileHandler::~FileHandler() {
    if (file_.is_open()) {
        file_.close();
    }
}

void FileHandler::openFile() {
    std::lock_guard<std::mutex> lock(fileMutex_);
    file_.open(filename_, std::ios::app);
    if (file_.is_open()) {
        file_.seekp(0, std::ios::end);
        currentFileSize_ = static_cast<size_t>(file_.tellp());
    }
}

void FileHandler::rotateIfNeeded() {
    if (currentFileSize_ >= maxFileSize_) {
        file_.close();

        // Rotate files
        for (size_t i = static_cast<size_t>(maxFileCount_) - 1; i > 0; --i) {
            std::string old_file = filename_ + "." + std::to_string(i);
            std::string new_file = filename_ + "." + std::to_string(i + 1);

            if (i == static_cast<size_t>(maxFileCount_) - 1) {
                std::filesystem::remove(new_file);
            }

            if (std::filesystem::exists(old_file)) {
                std::filesystem::rename(old_file, new_file);
            }
        }

        // Move current file to .1
        std::string backup_file = filename_ + ".1";
        if (std::filesystem::exists(backup_file)) {
            std::filesystem::remove(backup_file);
        }
        std::filesystem::rename(filename_, backup_file);

        openFile();
    }
}

void FileHandler::handle(const LogEntry& entry) {
    std::lock_guard<std::mutex> lock(fileMutex_);
    if (!file_.is_open()) {
        return;
    }

    std::string formatted = formatter_ ? formatter_->format(entry) : entry.toJson().dump();
    file_ << formatted;
    currentFileSize_ += formatted.length();

    rotateIfNeeded();
}

void FileHandler::flush() {
    std::lock_guard<std::mutex> lock(fileMutex_);
    if (file_.is_open()) {
        file_.flush();
    }
}

// CorrelationContext implementation
CorrelationContext& CorrelationContext::getInstance() {
    static CorrelationContext instance;
    return instance;
}

void CorrelationContext::setCorrelationId(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    correlationId_ = id;
}

std::string CorrelationContext::getCurrentCorrelationId() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return correlationId_;
}

void CorrelationContext::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    correlationId_.clear();
}

nlohmann::json CorrelationContext::toJson() const {
    std::lock_guard<std::mutex> lock(mutex_);
    nlohmann::json json;
    if (!correlationId_.empty()) {
        json["correlation_id"] = correlationId_;
    }
    return json;
}

// Logger wrapper implementation

// ProductionLogger singleton implementation
ProductionLogger& ProductionLogger::getInstance() {
    static ProductionLogger instance;
    return instance;
}

ProductionLogger::ProductionLogger()
    : level_(LogLevel::INFO), running_(false), flushScheduled_(false) {

    // Add default console handler
    addHandler(std::make_unique<ConsoleHandler>());

    if (config_.async) {
        start();
    }
}

ProductionLogger::~ProductionLogger() {
    shutdown();
}

void ProductionLogger::setLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(handlersMutex_);
    level_ = level;
}

LogLevel ProductionLogger::getLevel() const {
    return level_;
}

void ProductionLogger::addHandler(std::unique_ptr<LogHandler> handler) {
    std::lock_guard<std::mutex> lock(handlersMutex_);
    handlers_.push_back(std::move(handler));
}

void ProductionLogger::removeHandler(const std::string& type) {
    std::lock_guard<std::mutex> lock(handlersMutex_);
    handlers_.erase(
        std::remove_if(handlers_.begin(), handlers_.end(),
                      [&type](const std::unique_ptr<LogHandler>& handler) {
                          return handler && dynamic_cast<ConsoleHandler*>(handler.get()) != nullptr;
                      }),
        handlers_.end());
}

void ProductionLogger::log(LogLevel level, const std::string& message,
                          const std::string& file, int line, const std::string& function,
                          const std::string& correlationId,
                          const nlohmann::json& extra) {
    if (level < level_) {
        return;
    }

    // Create log entry
    LogEntry entry(level, message, "aimux", file, line, function, correlationId);

    // Add extra data
    if (!extra.empty()) {
        entry.extra = extra;
    }

    // Apply security filtering
    if (config_.filterSensitiveData) {
        entry.extra = filterSensitiveData(entry.extra);
    }

    if (config_.async) {
        std::lock_guard<std::mutex> lock(queueMutex_);
        if (logQueue_.size() < config_.queueSize) {
            logQueue_.push(entry);
            queueCondition_.notify_one();
        }
    } else {
        // Synchronous logging
        std::lock_guard<std::mutex> lock(handlersMutex_);
        for (auto& handler : handlers_) {
            handler->handle(entry);
        }
    }
}

void ProductionLogger::flush() {
    if (config_.async) {
        flushScheduled_ = true;
        queueCondition_.notify_all();
    } else {
        std::lock_guard<std::mutex> lock(handlersMutex_);
        for (auto& handler : handlers_) {
            handler->flush();
        }
    }
}

void ProductionLogger::shutdown() {
    if (running_) {
        running_ = false;
        queueCondition_.notify_all();

        if (workerThread_.joinable()) {
            workerThread_.join();
        }

        if (flushThread_.joinable()) {
            flushThread_.join();
        }

        // Process remaining logs synchronously
        processQueue();

        // Flush all handlers
        std::lock_guard<std::mutex> lock(handlersMutex_);
        for (auto& handler : handlers_) {
            handler->flush();
        }
    }
}

void ProductionLogger::configure(const Config& config) {
    config_ = config;

    // Clear existing handlers
    {
        std::lock_guard<std::mutex> lock(handlersMutex_);
        handlers_.clear();
    }

    // Add console handler if enabled
    if (config.enableConsoleLogging) {
        if (config.jsonConsole) {
            addHandler(std::make_unique<ConsoleHandler>(std::make_unique<JsonFormatter>()));
        } else {
            addHandler(std::make_unique<ConsoleHandler>());
        }
    }

    // Add file handler if enabled
    if (config.enableFileLogging) {
        addHandler(std::make_unique<FileHandler>(config.logFile, config.maxFileSize, config.maxFileCount));
    }

    // Restart async processing if needed
    if (config.async && !running_) {
        start();
    } else if (!config.async && running_) {
        // Stop async processing
        running_ = false;
        queueCondition_.notify_all();
        if (workerThread_.joinable()) {
            workerThread_.join();
        }
    }
}

void ProductionLogger::start() {
    if (!running_) {
        running_ = true;
        workerThread_ = std::thread(&ProductionLogger::workerThread, this);
        flushThread_ = std::thread(&ProductionLogger::processQueue, this);
    }
}

void ProductionLogger::workerThread() {
    while (running_) {
        std::unique_lock<std::mutex> lock(queueMutex_);
        queueCondition_.wait(lock, [this] { return !logQueue_.empty() || !running_; });

        // Process batch of logs
        batch_.clear();
        while (!logQueue_.empty() && batch_.size() < config_.batchSize) {
            batch_.push_back(logQueue_.front());
            logQueue_.pop();
        }
        lock.unlock();

        // Process the batch
        if (!batch_.empty()) {
            std::lock_guard<std::mutex> handlersLock(handlersMutex_);
            for (const auto& entry : batch_) {
                for (auto& handler : handlers_) {
                    handler->handle(entry);
                }
            }
        }
    }
}

void ProductionLogger::processQueue() {
    while (running_ || flushScheduled_) {
        std::this_thread::sleep_for(config_.flushInterval);

        bool shouldFlush = (flushScheduled_.exchange(false) || !running_);

        std::lock_guard<std::mutex> lock(queueMutex_);
        if (!logQueue_.empty()) {
            batch_.clear();
            while (!logQueue_.empty() && batch_.size() < config_.batchSize) {
                batch_.push_back(logQueue_.front());
                logQueue_.pop();
            }

            std::lock_guard<std::mutex> handlersLock(handlersMutex_);
            for (const auto& entry : batch_) {
                for (auto& handler : handlers_) {
                    handler->handle(entry);
                }
            }
        }

        if (shouldFlush) {
            std::lock_guard<std::mutex> handlersLock(handlersMutex_);
            for (auto& handler : handlers_) {
                handler->flush();
            }
        }
    }
}

nlohmann::json ProductionLogger::filterSensitiveData(const nlohmann::json& data) const {
    if (!data.is_object()) {
        return data;
    }

    nlohmann::json filtered = data;

    for (const auto& pattern : config_.sensitivePatterns) {
        for (auto it = filtered.begin(); it != filtered.end(); ++it) {
            std::string key = it.key();
            std::transform(key.begin(), key.end(), key.begin(), ::tolower);

            if (key.find(pattern) != std::string::npos) {
                if (filtered[it.key()].is_string()) {
                    filtered[it.key()] = "[REDACTED]";
                }
            }
        }
    }

    return filtered;
}

// Logger wrapper implementation
std::string Logger::getEffectiveCorrelationId(const std::string& explicitCorrelationId) const {
    // 1. Use explicit correlation ID if provided
    if (!explicitCorrelationId.empty()) {
        return explicitCorrelationId;
    }

    // 2. Use logger's correlation ID if set
    if (!correlationId_.empty()) {
        return correlationId_;
    }

    // 3. Use thread-local correlation context if available
    const std::string& contextCorrelationId = CorrelationContext::getInstance().getCurrentCorrelationId();
    if (!contextCorrelationId.empty()) {
        return contextCorrelationId;
    }

    // 4. Generate new correlation ID as last resort
    return LogEntry::generateCorrelationId();
}

void Logger::log(LogLevel level, const std::string& message, const nlohmann::json& extra,
                 const std::string& explicitCorrelationId) {
    const std::string& correlationIdToUse = getEffectiveCorrelationId(explicitCorrelationId);

    // Add correlation context information to extra data
    nlohmann::json enhancedExtra = extra;
    if (!extra.contains("correlation_context")) {
        enhancedExtra["correlation_context"] = CorrelationContext::getInstance().toJson();
    }

    ProductionLogger::getInstance().log(level, message, __FILE__, __LINE__, __FUNCTION__, correlationIdToUse, enhancedExtra);
}

// Logger wrapper implementation
Logger::Logger(const std::string& category, const std::string& correlationId)
    : category_(category), correlationId_(correlationId) {}

void Logger::setCorrelationId(const std::string& id) {
    correlationId_ = id;
}

void Logger::trace(const std::string& message, const nlohmann::json& extra) {
    log(LogLevel::TRACE, message, extra);
}

void Logger::debug(const std::string& message, const nlohmann::json& extra) {
    log(LogLevel::DEBUG, message, extra);
}

void Logger::info(const std::string& message, const nlohmann::json& extra) {
    log(LogLevel::INFO, message, extra);
}

void Logger::warn(const std::string& message, const nlohmann::json& extra) {
    log(LogLevel::WARN, message, extra);
}

void Logger::error(const std::string& message, const nlohmann::json& extra) {
    log(LogLevel::ERROR, message, extra);
}

void Logger::fatal(const std::string& message, const nlohmann::json& extra) {
    log(LogLevel::FATAL, message, extra);
}

} // namespace logging
} // namespace aimux