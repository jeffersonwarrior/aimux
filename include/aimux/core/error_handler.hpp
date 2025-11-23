#pragma once

#include <string>
#include <stdexcept>
#include <system_error>
#include <chrono>
#include <memory>
#include <vector>
#include <functional>
#include <mutex>

namespace aimux {
namespace core {

/**
 * @brief Error severity levels for categorizing issues
 */
enum class ErrorSeverity {
    DEBUG = 0,
    INFO = 1,
    WARNING = 2,
    ERROR = 3,
    CRITICAL = 4,
    FATAL = 5
};

/**
 * @brief Error categories for systematic classification
 */
enum class ErrorCategory {
    NETWORK,
    CONFIGURATION,
    AUTHENTICATION,
    PROVIDER,
    SYSTEM,
    USER_INPUT,
    INTERNAL,
    RESOURCE
};

/**
 * @brief Standardized error codes for consistent error handling
 */
enum class ErrorCode {
    // Network errors (1000-1099)
    NETWORK_TIMEOUT = 1000,
    NETWORK_CONNECTION_FAILED = 1001,
    NETWORK_DNS_RESOLUTION_FAILED = 1002,
    NETWORK_SSL_HANDSHAKE_FAILED = 1003,

    // Configuration errors (1100-1199)
    CONFIG_FILE_NOT_FOUND = 1100,
    CONFIG_PARSE_ERROR = 1101,
    CONFIG_MISSING_REQUIRED_FIELD = 1102,
    CONFIG_INVALID_VALUE = 1103,
    CONFIG_VALIDATION_FAILED = 1104,

    // Authentication errors (1200-1299)
    AUTH_INVALID_API_KEY = 1200,
    AUTH_TOKEN_EXPIRED = 1201,
    AUTH_INSUFFICIENT_PERMISSIONS = 1202,
    AUTH_PROVIDER_REJECTED = 1203,

    // Provider errors (1300-1399)
    PROVIDER_NOT_FOUND = 1300,
    PROVIDER_UNAVAILABLE = 1301,
    PROVIDER_RATE_LIMIT_EXCEEDED = 1302,
    PROVIDER_RESPONSE_ERROR = 1303,
    PROVIDER_TIMEOUT = 1304,

    // System errors (1400-1499)
    SYSTEM_MEMORY_ALLOCATION_FAILED = 1400,
    SYSTEM_THREAD_CREATION_FAILED = 1401,
    SYSTEM_FILE_OPERATION_FAILED = 1402,
    SYSTEM_PERMISSION_DENIED = 1403,

    // User input errors (1500-1599)
    USER_INVALID_REQUEST_FORMAT = 1500,
    USER_MISSING_REQUIRED_PARAMETER = 1501,
    USER_INVALID_PARAMETER_VALUE = 1502,
    USER_REQUEST_TOO_LARGE = 1503,

    // Internal errors (1600-1699)
    INTERNAL_LOGIC_ERROR = 1600,
    INTERNAL_STATE_CORRUPTION = 1601,
    INTERNAL_UNEXPECTED_NULLPTR = 1602,
    INTERNAL_INDEX_OUT_OF_BOUNDS = 1603,

    // Resource errors (1700-1799)
    RESOURCE_EXHAUSTED = 1700,
    RESOURCE_NOT_AVAILABLE = 1701,
    RESOURCE_QUOTA_EXCEEDED = 1702,

    // Generic errors
    UNKNOWN_ERROR = 9999
};

/**
 * @brief Structured error information for consistent error reporting
 */
struct ErrorInfo {
    std::chrono::system_clock::time_point timestamp;
    std::string component;
    std::string function;
    ErrorCode error_code;
    ErrorCategory category;
    ErrorSeverity severity;
    std::string message;
    std::string details;
    int line_number;
    std::string file_name;
    std::vector<std::string> context;
};

/**
 * @brief Custom exception class for aimux-specific errors
 */
class AimuxException : public std::exception {
public:
    explicit AimuxException(ErrorCode error_code,
                           const std::string& message,
                           const std::string& component = "",
                           int line = 0,
                           const std::string& file = "");

    const char* what() const noexcept override;
    ErrorCode get_error_code() const noexcept;
    ErrorCategory get_category() const noexcept;
    ErrorSeverity get_severity() const noexcept;
    const std::string& get_component() const noexcept;
    int get_line() const noexcept;
    const std::string& get_file() const noexcept;

private:
    ErrorCode error_code_;
    std::string message_;
    std::string component_;
    int line_;
    std::string file_;
    mutable std::string full_message_;
};

/**
 * @brief Main error handling class for unified error management
 */
class ErrorHandler {
public:
    static ErrorHandler& instance();

    // Error reporting methods
    void report_error(const ErrorInfo& error);
    void report_error(ErrorCode code,
                     const std::string& component,
                     const std::string& function,
                     const std::string& message,
                     const std::string& details = "",
                     int line = 0,
                     const std::string& file = "");

    // Convenience methods for different severity levels
    void debug(const std::string& component,
              const std::string& function,
              const std::string& message);
    void info(const std::string& component,
             const std::string& function,
             const std::string& message);
    void warning(const std::string& component,
                const std::string& function,
                const std::string& message);
    void error(const std::string& component,
              const std::string& function,
              const std::string& message);
    void critical(const std::string& component,
                 const std::string& function,
                 const std::string& message);
    void fatal(const std::string& component,
              const std::string& function,
              const std::string& message);

    // Configuration
    void set_log_file(const std::string& file_path);
    void set_minimum_severity(ErrorSeverity severity);
    void add_callback(std::function<void(const ErrorInfo&)> callback);

    // Query methods
    std::vector<ErrorInfo> get_recent_errors(size_t count = 100) const;
    size_t get_error_count(ErrorSeverity severity = ErrorSeverity::ERROR) const;

    // Utility methods
    static ErrorCategory get_error_category(ErrorCode code);
    static ErrorSeverity get_error_severity(ErrorCode code);
    static std::string get_error_code_string(ErrorCode code);
    static std::string get_error_category_string(ErrorCategory category);
    static std::string get_error_severity_string(ErrorSeverity severity);

private:
    ErrorHandler() = default;
    void log_error(const ErrorInfo& error);
    void notify_callbacks(const ErrorInfo& error);

    std::string log_file_path_;
    std::string component_{"error_handler"};
    ErrorSeverity minimum_severity_{ErrorSeverity::INFO};
    std::vector<std::function<void(const ErrorInfo&)>> callbacks_;
    std::vector<ErrorInfo> error_history_;
    mutable std::mutex error_mutex_;
    static constexpr size_t MAX_ERROR_HISTORY = 10000;
};

/**
 * @brief RAII error context manager for automatic error reporting
 */
class ErrorContext {
public:
    explicit ErrorContext(const std::string& component,
                         const std::string& function,
                         int line = 0,
                         const std::string& file = "");
    ~ErrorContext() noexcept;

    void add_context(const std::string& key, const std::string& value);

private:
    std::string component_;
    std::string function_;
    int line_;
    std::string file_;
    std::vector<std::string> context_;
};

// Utility macros for consistent error reporting
#define AIMUX_REPORT_ERROR(code, message) \
    aimux::core::ErrorHandler::instance().report_error( \
        code, __FUNCTION__, __FUNCTION__, message, "", __LINE__, __FILE__)

#define AIMUX_ERROR(message) \
    aimux::core::ErrorHandler::instance().error(__FUNCTION__, __FUNCTION__, message)

#define AIMUX_WARNING(message) \
    aimux::core::ErrorHandler::instance().warning(__FUNCTION__, __FUNCTION__, message)

#define AIMUX_INFO(message) \
    aimux::core::ErrorHandler::instance().info(__FUNCTION__, __FUNCTION__, message)

#define AIMUX_DEBUG(message) \
    aimux::core::ErrorHandler::instance().debug(__FUNCTION__, __FUNCTION__, message)

#define AIMUX_CRITICAL(message) \
    aimux::core::ErrorHandler::instance().critical(__FUNCTION__, __FUNCTION__, message)

#define AIMUX_FATAL(message) \
    aimux::core::ErrorHandler::instance().fatal(__FUNCTION__, __FUNCTION__, message)

#define AIMUX_THROW(code, message) \
    throw aimux::core::AimuxException(code, message, __FUNCTION__, __LINE__, __FILE__)

#define AIMUX_THROW_IF(condition, code, message) \
    do { if (condition) AIMUX_THROW(code, message); } while(0)

#define AIMUX_ERROR_CONTEXT(component, function) \
    aimux::core::ErrorContext _error_ctx(component, function, __LINE__, __FILE__)

} // namespace core
} // namespace aimux