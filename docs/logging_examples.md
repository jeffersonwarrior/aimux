# Enhanced Logging Examples

This document demonstrates the improved logging system with correlation IDs and structured data support in Aimux v2.0.

## Basic Logging Usage

### Simple Logging
```cpp
#include "aimux/logging/production_logger.h"

// Basic logging
AIMUX_LOG_INFO("Application started");
AIMUX_LOG_ERROR("Connection failed");

// With specific logger
AIMUX_LOG_INFO_WITH("webui", "Web server started on port 8080");
AIMUX_LOG_ERROR_WITH("database", "Database connection lost");
```

### Correlation ID Logging
```cpp
// Generate correlation ID for a request
std::string correlationId = Logger::generateCorrelationId();

// Log with correlation ID
AIMUX_LOG_INFO_CORR("Processing user request", correlationId);
AIMUX_LOG_ERROR_CORR("Request processing failed", correlationId);

// Using logger with correlation ID
auto logger = AIMUX_LOGGER_WITH_CORRELATION("api_handler", correlationId);
logger.info("Authenticated user");
logger.error("Authentication failed", {{"error_code", "AUTH_001"}});
```

### Structured Data Logging
```cpp
// Log with structured data
nlohmann::json requestData = {
    {"user_id", "12345"},
    {"endpoint", "/api/v1/chat"},
    {"method", "POST"},
    {"request_size", 1024}
};

AIMUX_LOG_INFO_STRUCTURED("API request received", requestData);

// Error with structured data
nlohmann::json errorInfo = {
    {"error_type", "validation_error"},
    {"field", "api_key"},
    {"message", "Missing required field"},
    {"request_id", correlationId}
};

AIMUX_LOG_ERROR_STRUCTURED("Request validation failed", errorInfo);
```

## Advanced Usage Patterns

### Request Tracing
```cpp
class RequestHandler {
public:
    void handleRequest(const HttpRequest& request) {
        // Generate correlation ID for this request
        std::string correlationId = Logger::generateCorrelationId();

        // Create logger with correlation ID
        auto logger = AIMUX_LOGGER_WITH_CORRELATION("request_handler", correlationId);

        logger.info("Request started", {
            {"method", request.method},
            {"path", request.path},
            {"remote_addr", request.remoteAddr},
            {"user_agent", request.userAgent}
        });

        try {
            // Process request
            processRequestInternal(request, correlationId);

            logger.info("Request completed successfully", {
                {"response_time_ms", responseTime},
                {"status_code", 200}
            });

        } catch (const std::exception& e) {
            logger.error("Request failed", {
                {"error_type", typeid(e).name()},
                {"error_message", e.what()},
                {"status_code", 500}
            });
        }
    }

private:
    void processRequestInternal(const HttpRequest& request, const std::string& correlationId) {
        // All logging in this method will inherit the correlation ID
        auto logger = AIMUX_LOGGER_WITH_CORRELATION("request_processor", correlationId);

        logger.debug("Validating request parameters");
        logger.info("Calling provider API", {
            {"provider", "cerebras"},
            {"model", "llama3.1-70b"},
            {"timeout_ms", 30000}
        });
        // ... more processing
    }
};
```

### Performance Monitoring
```cpp
class PerformanceMonitor {
public:
    void logPerformanceMetrics(const std::string& operation,
                              const std::string& correlationId = "") {
        auto metrics = collectMetrics();

        if (correlationId.empty()) {
            AIMUX_LOG_INFO_STRUCTURED("Performance metrics", {
                {"operation", operation},
                {"response_time_ms", metrics.responseTime},
                {"memory_usage_mb", metrics.memoryUsage},
                {"cpu_usage_percent", metrics.cpuUsage},
                {"success", metrics.success}
            });
        } else {
            AIMUX_LOG_INFO_WITH_STRUCTURED("performance", "Performance metrics", {
                {"operation", operation},
                {"response_time_ms", metrics.responseTime},
                {"memory_usage_mb", metrics.memoryUsage},
                {"cpu_usage_percent", metrics.cpuUsage},
                {"success", metrics.success},
                {"correlation_id", correlationId}
            });
        }
    }
};
```

### Error Handling with Context
```cpp
class ProviderManager {
public:
    void handleProviderError(const std::string& provider,
                           const std::exception& e,
                           const std::string& correlationId = "") {
        nlohmann::json errorContext = {
            {"provider", provider},
            {"error_type", typeid(e).name()},
            {"error_message", e.what()},
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()},
            {"recovery_attempts", getAttemptCount(provider)},
            {"last_success_time", getLastSuccessTime(provider)}
        };

        if (correlationId.empty()) {
            AIMUX_LOG_ERROR_STRUCTURED("Provider error occurred", errorContext);
        } else {
            AIMUX_LOG_ERROR_WITH_CORR("error_handler", "Provider error occurred", correlationId);
        }

        // Initiate failover
        initiateFailover(provider, correlationId);
    }

private:
    void initiateFailover(const std::string& failedProvider, const std::string& correlationId) {
        auto logger = AIMUX_LOGGER_WITH_CORRELATION("failover_handler", correlationId);

        logger.warn("Initiating provider failover", {
            {"failed_provider", failedProvider},
            {"available_providers", getAvailableProviders()},
            {"failover_strategy", "round_robin"}
        });

        // ... failover logic

        logger.info("Failover completed successfully", {
            {"new_provider", selectedProvider},
            {"failover_time_ms", failoverTime}
        });
    }
};
```

## Configuration Examples

### Basic Configuration
```cpp
aimux::logging::ProductionLogger::Config config;
config.level = aimux::logging::LogLevel::INFO;
config.enableConsoleLogging = true;
config.enableFileLogging = true;
config.logFile = "/var/log/aimux/aimux.log";
config.jsonConsole = false; // Human readable console output

aimux::logging::ProductionLogger::getInstance().configure(config);
```

### Production Configuration
```cpp
aimux::logging::ProductionLogger::Config config;
config.level = aimux::logging::LogLevel::WARN; // Higher level for production
config.enableConsoleLogging = false;
config.enableFileLogging = true;
config.logFile = "/var/log/aimux/aimux-production.log";
config.maxFileSize = 50 * 1024 * 1024; // 50MB
config.maxFileCount = 30; // Keep 30 days of logs
config.jsonConsole = true; // JSON format for log aggregation
config.filterSensitiveData = true;

aimux::logging::ProductionLogger::getInstance().configure(config);
```

### Development Configuration
```cpp
aimux::logging::ProductionLogger::Config config;
config.level = aimux::logging::LogLevel::DEBUG;
config.enableConsoleLogging = true;
config.enableFileLogging = true;
config.logFile = "./logs/aimux-dev.log";
config.jsonConsole = false; // Human readable for development
config.filterSensitiveData = true;

aimux::logging::ProductionLogger::getInstance().configure(config);
```

## Log Output Examples

### Console Output (Human Readable)
```
2025-11-14 15:30:45.123 [INFO] [1a2b3c4d...] [140234567890] [request_handler.cpp:23] Request started
2025-11-14 15:30:45.156 [INFO] [1a2b3c4d...] [140234567890] [request_processor.cpp:45] Validating request parameters
2025-11-14 15:30:45.234 [ERROR] [1a2b3c4d...] [140234567890] [provider_manager.cpp:67] Provider error occurred
2025-11-14 15:30:45.235 [WARN] [1a2b3c4d...] [140234567890] [failover_handler.cpp:89] Initiating provider failover
2025-11-14 15:30:45.345 [INFO] [1a2b3c4d...] [140234567890] [failover_handler.cpp:112] Failover completed successfully
```

### JSON Output (Structured)
```json
{
  "@timestamp": 1736892645123,
  "level": "INFO",
  "logger_name": "request_handler",
  "message": "Request started",
  "correlation_id": "1a2b3c4d5e6f-7890",
  "thread_id": "140234567890",
  "source": {
    "file": "request_handler.cpp",
    "line": 23,
    "function": "handleRequest"
  },
  "service": {
    "name": "aimux2",
    "version": "2.0.0"
  },
  "structured_data": {
    "method": "POST",
    "path": "/api/v1/chat",
    "remote_addr": "192.168.1.100",
    "user_agent": "curl/7.78.0"
  }
}
```

## Best Practices

1. **Always use correlation IDs for user requests** - Generate at request start and pass through all related operations
2. **Use structured data for important context** - Include request IDs, user IDs, performance metrics
3. **Choose appropriate log levels** - DEBUG for development, INFO for normal operations, WARN+ for production
4. **Filter sensitive data** - Enable sensitive data filtering in production
5. **Use specific logger names** - Use meaningful logger names like "webui", "database", "api_handler"
6. **Include error context** - For errors, include error types, stack traces, and recovery actions
7. **Monitor log patterns** - Set up alerts for error rates and unusual patterns

## Integration with Monitoring Systems

The JSON output format is compatible with:
- ELK Stack (Elasticsearch, Logstash, Kibana)
- Fluentd
- Splunk
- Datadog
- New Relic
- Loki

The correlation IDs enable distributed tracing across different components and services.