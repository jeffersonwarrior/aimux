/**
 * @file error_handler.cpp
 * @brief Comprehensive error handling implementation for production deployment
 *
 * This file implements a unified error handling framework that provides:
 * - Consistent error classification and reporting
 * - Structured logging with correlation IDs
 * - Thread-safe error management
 * - Configurable severity filtering
 * - Callback-based error notification system
 */

#include "aimux/core/error_handler.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <mutex>
#include <algorithm>
#include <format>

namespace aimux {
namespace core {

// AimuxException implementation
AimuxException::AimuxException(ErrorCode error_code,
                               const std::string& message,
                               const std::string& component,
                               int line,
                               const std::string& file)
    : error_code_(error_code)
    , message_(message)
    , component_(component)
    , line_(line)
    , file_(file) {
}

const char* AimuxException::what() const noexcept {
    if (full_message_.empty()) {
        std::ostringstream oss;
        oss << "[" << ErrorHandler::get_error_code_string(error_code_) << "] "
            << ErrorHandler::get_error_category_string(ErrorHandler::get_error_category(error_code_)) << " "
            << message_;

        if (!component_.empty()) {
            oss << " (component: " << component_ << ")";
        }

        full_message_ = oss.str();
    }
    return full_message_.c_str();
}

ErrorCode AimuxException::get_error_code() const noexcept {
    return error_code_;
}

ErrorCategory AimuxException::get_category() const noexcept {
    return ErrorHandler::get_error_category(error_code_);
}

ErrorSeverity AimuxException::get_severity() const noexcept {
    return ErrorHandler::get_error_severity(error_code_);
}

const std::string& AimuxException::get_component() const noexcept {
    return component_;
}

int AimuxException::get_line() const noexcept {
    return line_;
}

const std::string& AimuxException::get_file() const noexcept {
    return file_;
}

// ErrorHandler implementation
ErrorHandler& ErrorHandler::instance() {
    static ErrorHandler instance;
    return instance;
}

void ErrorHandler::report_error(const ErrorInfo& error) {
    std::lock_guard<std::mutex> lock(error_mutex_);

    // Filter by minimum severity
    if (error.severity < minimum_severity_) {
        return;
    }

    // Add to history
    error_history_.push_back(error);

    // Trim history if it gets too large
    if (error_history_.size() > MAX_ERROR_HISTORY) {
        error_history_.erase(error_history_.begin());
    }

    // Log the error
    log_error(error);

    // Notify callbacks
    notify_callbacks(error);

    // Special handling for fatal errors
    if (error.severity >= ErrorSeverity::FATAL) {
        std::cerr << "FATAL ERROR in " << error.component << "::" << error.function
                  << " (" << error.file_name << ":" << error.line_number << "): "
                  << error.message << std::endl;
        if (!error.details.empty()) {
            std::cerr << "Details: " << error.details << std::endl;
        }
        std::terminate();
    }
}

void ErrorHandler::report_error(ErrorCode code,
                               const std::string& component,
                               const std::string& function,
                               const std::string& message,
                               const std::string& details,
                               int line,
                               const std::string& file) {
    ErrorInfo error;
    error.timestamp = std::chrono::system_clock::now();
    error.component = component;
    error.function = function;
    error.error_code = code;
    error.category = get_error_category(code);
    error.severity = get_error_severity(code);
    error.message = message;
    error.details = details;
    error.line_number = line;
    error.file_name = file;

    report_error(error);
}

void ErrorHandler::debug(const std::string& component,
                        const std::string& function,
                        const std::string& message) {
    ErrorInfo error;
    error.timestamp = std::chrono::system_clock::now();
    error.component = component;
    error.function = function;
    error.error_code = ErrorCode::UNKNOWN_ERROR;
    error.category = ErrorCategory::INTERNAL;
    error.severity = ErrorSeverity::DEBUG;
    error.message = message;

    report_error(error);
}

void ErrorHandler::info(const std::string& component,
                       const std::string& function,
                       const std::string& message) {
    ErrorInfo error;
    error.timestamp = std::chrono::system_clock::now();
    error.component = component;
    error.function = function;
    error.error_code = ErrorCode::UNKNOWN_ERROR;
    error.category = ErrorCategory::INTERNAL;
    error.severity = ErrorSeverity::INFO;
    error.message = message;

    report_error(error);
}

void ErrorHandler::warning(const std::string& component,
                          const std::string& function,
                          const std::string& message) {
    ErrorInfo error;
    error.timestamp = std::chrono::system_clock::now();
    error.component = component;
    error.function = function;
    error.error_code = ErrorCode::UNKNOWN_ERROR;
    error.category = ErrorCategory::INTERNAL;
    error.severity = ErrorSeverity::WARNING;
    error.message = message;

    report_error(error);
}

void ErrorHandler::error(const std::string& component,
                        const std::string& function,
                        const std::string& message) {
    ErrorInfo error;
    error.timestamp = std::chrono::system_clock::now();
    error.component = component;
    error.function = function;
    error.error_code = ErrorCode::UNKNOWN_ERROR;
    error.category = ErrorCategory::INTERNAL;
    error.severity = ErrorSeverity::ERROR;
    error.message = message;

    report_error(error);
}

void ErrorHandler::critical(const std::string& component,
                           const std::string& function,
                           const std::string& message) {
    ErrorInfo error;
    error.timestamp = std::chrono::system_clock::now();
    error.component = component;
    error.function = function;
    error.error_code = ErrorCode::UNKNOWN_ERROR;
    error.category = ErrorCategory::INTERNAL;
    error.severity = ErrorSeverity::CRITICAL;
    error.message = message;

    report_error(error);
}

void ErrorHandler::fatal(const std::string& component,
                        const std::string& function,
                        const std::string& message) {
    ErrorInfo error;
    error.timestamp = std::chrono::system_clock::now();
    error.component = component;
    error.function = function;
    error.error_code = ErrorCode::UNKNOWN_ERROR;
    error.category = ErrorCategory::INTERNAL;
    error.severity = ErrorSeverity::FATAL;
    error.message = message;

    report_error(error);
}

void ErrorHandler::set_log_file(const std::string& file_path) {
    std::lock_guard<std::mutex> lock(error_mutex_);
    log_file_path_ = file_path;
}

void ErrorHandler::set_minimum_severity(ErrorSeverity severity) {
    std::lock_guard<std::mutex> lock(error_mutex_);
    minimum_severity_ = severity;
}

void ErrorHandler::add_callback(std::function<void(const ErrorInfo&)> callback) {
    std::lock_guard<std::mutex> lock(error_mutex_);
    callbacks_.push_back(std::move(callback));
}

std::vector<ErrorInfo> ErrorHandler::get_recent_errors(size_t count) const {
    std::lock_guard<std::mutex> lock(error_mutex_);

    std::vector<ErrorInfo> recent;
    size_t start = error_history_.size() > count ? error_history_.size() - count : 0;

    for (size_t i = start; i < error_history_.size(); ++i) {
        recent.push_back(error_history_[i]);
    }

    return recent;
}

size_t ErrorHandler::get_error_count(ErrorSeverity severity) const {
    std::lock_guard<std::mutex> lock(error_mutex_);

    return static_cast<size_t>(std::count_if(error_history_.begin(), error_history_.end(),
                        [severity](const ErrorInfo& error) {
                            return error.severity >= severity;
                        }));
}

ErrorCategory ErrorHandler::get_error_category(ErrorCode code) {
    switch (code) {
        case ErrorCode::NETWORK_TIMEOUT:
        case ErrorCode::NETWORK_CONNECTION_FAILED:
        case ErrorCode::NETWORK_DNS_RESOLUTION_FAILED:
        case ErrorCode::NETWORK_SSL_HANDSHAKE_FAILED:
            return ErrorCategory::NETWORK;

        case ErrorCode::CONFIG_FILE_NOT_FOUND:
        case ErrorCode::CONFIG_PARSE_ERROR:
        case ErrorCode::CONFIG_MISSING_REQUIRED_FIELD:
        case ErrorCode::CONFIG_INVALID_VALUE:
        case ErrorCode::CONFIG_VALIDATION_FAILED:
            return ErrorCategory::CONFIGURATION;

        case ErrorCode::AUTH_INVALID_API_KEY:
        case ErrorCode::AUTH_TOKEN_EXPIRED:
        case ErrorCode::AUTH_INSUFFICIENT_PERMISSIONS:
        case ErrorCode::AUTH_PROVIDER_REJECTED:
            return ErrorCategory::AUTHENTICATION;

        case ErrorCode::PROVIDER_NOT_FOUND:
        case ErrorCode::PROVIDER_UNAVAILABLE:
        case ErrorCode::PROVIDER_RATE_LIMIT_EXCEEDED:
        case ErrorCode::PROVIDER_RESPONSE_ERROR:
        case ErrorCode::PROVIDER_TIMEOUT:
            return ErrorCategory::PROVIDER;

        case ErrorCode::SYSTEM_MEMORY_ALLOCATION_FAILED:
        case ErrorCode::SYSTEM_THREAD_CREATION_FAILED:
        case ErrorCode::SYSTEM_FILE_OPERATION_FAILED:
        case ErrorCode::SYSTEM_PERMISSION_DENIED:
            return ErrorCategory::SYSTEM;

        case ErrorCode::USER_INVALID_REQUEST_FORMAT:
        case ErrorCode::USER_MISSING_REQUIRED_PARAMETER:
        case ErrorCode::USER_INVALID_PARAMETER_VALUE:
        case ErrorCode::USER_REQUEST_TOO_LARGE:
            return ErrorCategory::USER_INPUT;

        case ErrorCode::INTERNAL_LOGIC_ERROR:
        case ErrorCode::INTERNAL_STATE_CORRUPTION:
        case ErrorCode::INTERNAL_UNEXPECTED_NULLPTR:
        case ErrorCode::INTERNAL_INDEX_OUT_OF_BOUNDS:
            return ErrorCategory::INTERNAL;

        case ErrorCode::RESOURCE_EXHAUSTED:
        case ErrorCode::RESOURCE_NOT_AVAILABLE:
        case ErrorCode::RESOURCE_QUOTA_EXCEEDED:
            return ErrorCategory::RESOURCE;

        default:
            return ErrorCategory::INTERNAL;
    }
}

ErrorSeverity ErrorHandler::get_error_severity(ErrorCode code) {
    switch (code) {
        case ErrorCode::SYSTEM_MEMORY_ALLOCATION_FAILED:
        case ErrorCode::SYSTEM_PERMISSION_DENIED:
        case ErrorCode::INTERNAL_STATE_CORRUPTION:
        case ErrorCode::INTERNAL_UNEXPECTED_NULLPTR:
        case ErrorCode::RESOURCE_EXHAUSTED:
            return ErrorSeverity::CRITICAL;

        case ErrorCode::NETWORK_TIMEOUT:
        case ErrorCode::NETWORK_CONNECTION_FAILED:
        case ErrorCode::CONFIG_PARSE_ERROR:
        case ErrorCode::CONFIG_MISSING_REQUIRED_FIELD:
        case ErrorCode::AUTH_INVALID_API_KEY:
        case ErrorCode::PROVIDER_UNAVAILABLE:
        case ErrorCode::SYSTEM_THREAD_CREATION_FAILED:
        case ErrorCode::SYSTEM_FILE_OPERATION_FAILED:
            return ErrorSeverity::ERROR;

        case ErrorCode::NETWORK_DNS_RESOLUTION_FAILED:
        case ErrorCode::NETWORK_SSL_HANDSHAKE_FAILED:
        case ErrorCode::CONFIG_FILE_NOT_FOUND:
        case ErrorCode::CONFIG_INVALID_VALUE:
        case ErrorCode::AUTH_TOKEN_EXPIRED:
        case ErrorCode::PROVIDER_RATE_LIMIT_EXCEEDED:
        case ErrorCode::PROVIDER_RESPONSE_ERROR:
        case ErrorCode::USER_INVALID_REQUEST_FORMAT:
        case ErrorCode::USER_MISSING_REQUIRED_PARAMETER:
            return ErrorSeverity::WARNING;

        default:
            return ErrorSeverity::INFO;
    }
}

std::string ErrorHandler::get_error_code_string(ErrorCode code) {
    switch (code) {
        case ErrorCode::NETWORK_TIMEOUT: return "NETWORK_TIMEOUT";
        case ErrorCode::NETWORK_CONNECTION_FAILED: return "NETWORK_CONNECTION_FAILED";
        case ErrorCode::NETWORK_DNS_RESOLUTION_FAILED: return "NETWORK_DNS_RESOLUTION_FAILED";
        case ErrorCode::NETWORK_SSL_HANDSHAKE_FAILED: return "NETWORK_SSL_HANDSHAKE_FAILED";
        case ErrorCode::CONFIG_FILE_NOT_FOUND: return "CONFIG_FILE_NOT_FOUND";
        case ErrorCode::CONFIG_PARSE_ERROR: return "CONFIG_PARSE_ERROR";
        case ErrorCode::CONFIG_MISSING_REQUIRED_FIELD: return "CONFIG_MISSING_REQUIRED_FIELD";
        case ErrorCode::CONFIG_INVALID_VALUE: return "CONFIG_INVALID_VALUE";
        case ErrorCode::CONFIG_VALIDATION_FAILED: return "CONFIG_VALIDATION_FAILED";
        case ErrorCode::AUTH_INVALID_API_KEY: return "AUTH_INVALID_API_KEY";
        case ErrorCode::AUTH_TOKEN_EXPIRED: return "AUTH_TOKEN_EXPIRED";
        case ErrorCode::AUTH_INSUFFICIENT_PERMISSIONS: return "AUTH_INSUFFICIENT_PERMISSIONS";
        case ErrorCode::AUTH_PROVIDER_REJECTED: return "AUTH_PROVIDER_REJECTED";
        case ErrorCode::PROVIDER_NOT_FOUND: return "PROVIDER_NOT_FOUND";
        case ErrorCode::PROVIDER_UNAVAILABLE: return "PROVIDER_UNAVAILABLE";
        case ErrorCode::PROVIDER_RATE_LIMIT_EXCEEDED: return "PROVIDER_RATE_LIMIT_EXCEEDED";
        case ErrorCode::PROVIDER_RESPONSE_ERROR: return "PROVIDER_RESPONSE_ERROR";
        case ErrorCode::PROVIDER_TIMEOUT: return "PROVIDER_TIMEOUT";
        case ErrorCode::SYSTEM_MEMORY_ALLOCATION_FAILED: return "SYSTEM_MEMORY_ALLOCATION_FAILED";
        case ErrorCode::SYSTEM_THREAD_CREATION_FAILED: return "SYSTEM_THREAD_CREATION_FAILED";
        case ErrorCode::SYSTEM_FILE_OPERATION_FAILED: return "SYSTEM_FILE_OPERATION_FAILED";
        case ErrorCode::SYSTEM_PERMISSION_DENIED: return "SYSTEM_PERMISSION_DENIED";
        case ErrorCode::USER_INVALID_REQUEST_FORMAT: return "USER_INVALID_REQUEST_FORMAT";
        case ErrorCode::USER_MISSING_REQUIRED_PARAMETER: return "USER_MISSING_REQUIRED_PARAMETER";
        case ErrorCode::USER_INVALID_PARAMETER_VALUE: return "USER_INVALID_PARAMETER_VALUE";
        case ErrorCode::USER_REQUEST_TOO_LARGE: return "USER_REQUEST_TOO_LARGE";
        case ErrorCode::INTERNAL_LOGIC_ERROR: return "INTERNAL_LOGIC_ERROR";
        case ErrorCode::INTERNAL_STATE_CORRUPTION: return "INTERNAL_STATE_CORRUPTION";
        case ErrorCode::INTERNAL_UNEXPECTED_NULLPTR: return "INTERNAL_UNEXPECTED_NULLPTR";
        case ErrorCode::INTERNAL_INDEX_OUT_OF_BOUNDS: return "INTERNAL_INDEX_OUT_OF_BOUNDS";
        case ErrorCode::RESOURCE_EXHAUSTED: return "RESOURCE_EXHAUSTED";
        case ErrorCode::RESOURCE_NOT_AVAILABLE: return "RESOURCE_NOT_AVAILABLE";
        case ErrorCode::RESOURCE_QUOTA_EXCEEDED: return "RESOURCE_QUOTA_EXCEEDED";
        default: return "UNKNOWN_ERROR";
    }
}

std::string ErrorHandler::get_error_category_string(ErrorCategory category) {
    switch (category) {
        case ErrorCategory::NETWORK: return "NETWORK";
        case ErrorCategory::CONFIGURATION: return "CONFIGURATION";
        case ErrorCategory::AUTHENTICATION: return "AUTHENTICATION";
        case ErrorCategory::PROVIDER: return "PROVIDER";
        case ErrorCategory::SYSTEM: return "SYSTEM";
        case ErrorCategory::USER_INPUT: return "USER_INPUT";
        case ErrorCategory::INTERNAL: return "INTERNAL";
        case ErrorCategory::RESOURCE: return "RESOURCE";
        default: return "UNKNOWN";
    }
}

std::string ErrorHandler::get_error_severity_string(ErrorSeverity severity) {
    switch (severity) {
        case ErrorSeverity::DEBUG: return "DEBUG";
        case ErrorSeverity::INFO: return "INFO";
        case ErrorSeverity::WARNING: return "WARNING";
        case ErrorSeverity::ERROR: return "ERROR";
        case ErrorSeverity::CRITICAL: return "CRITICAL";
        case ErrorSeverity::FATAL: return "FATAL";
        default: return "UNKNOWN";
    }
}

void ErrorHandler::log_error(const ErrorInfo& error) {
    std::ostringstream log_message;

    // Format timestamp
    auto time_t = std::chrono::system_clock::to_time_t(error.timestamp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        error.timestamp.time_since_epoch()) % 1000;

    log_message << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    log_message << '.' << std::setfill('0') << std::setw(3) << ms.count();

    // Structured log format
    log_message << " [" << get_error_severity_string(error.severity) << "]"
                << " [" << get_error_category_string(error.category) << "]"
                << " [" << get_error_code_string(error.error_code) << "]"
                << " [" << error.component << "]";

    if (!error.function.empty()) {
        log_message << " [" << error.function << "]";
    }

    log_message << " " << error.message;

    if (!error.details.empty()) {
        log_message << " | " << error.details;
    }

    if (error.line_number > 0 && !error.file_name.empty()) {
        log_message << " | " << error.file_name << ":" << error.line_number;
    }

    std::string log_line = log_message.str();

    // Simple error logging to stderr
    std::cerr << "ERROR: " << log_line << std::endl;

    // Maintain backward compatibility - also write to traditional log files if configured
    if (!log_file_path_.empty()) {
        std::ofstream log_file(log_file_path_, std::ios::app);
        if (log_file.is_open()) {
            log_file << log_line << std::endl;
        }
    }

    if (error.severity >= ErrorSeverity::ERROR) {
        std::ofstream error_log("aimux_errors.log", std::ios::app);
        if (error_log.is_open()) {
            error_log << log_line << std::endl;
        }
    }
}

void ErrorHandler::notify_callbacks(const ErrorInfo& error) {
    for (const auto& callback : callbacks_) {
        try {
            callback(error);
        } catch (...) {
            // Don't let callback errors affect error handling
            std::cerr << "Warning: Error in error callback - ignoring" << std::endl;
        }
    }
}

// ErrorContext implementation
ErrorContext::ErrorContext(const std::string& component,
                           const std::string& function,
                           int line,
                           const std::string& file)
    : component_(component)
    , function_(function)
    , line_(line)
    , file_(file) {
}

ErrorContext::~ErrorContext() noexcept {
    try {
        // Report context completion automatically if no errors were reported
        if (context_.empty()) {
            ErrorHandler::instance().debug(component_, function_, "Operation completed successfully");
        }
    } catch (...) {
        // Never throw from destructor
    }
}

void ErrorContext::add_context(const std::string& key, const std::string& value) {
    context_.push_back(key + "=" + value);
}

} // namespace core
} // namespace aimux
