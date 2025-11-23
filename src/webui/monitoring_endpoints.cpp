#include "monitoring/metrics.h"
#include "logging/production_logger.h"
#include <nlohmann/json.hpp>
#include <chrono>

namespace aimux {
namespace webui {

/**
 * WebUI endpoints for monitoring and metrics
 * Provides REST API endpoints for health checks, metrics, and logs
 */

// Health check endpoint
nlohmann::json handleHealthCheck() {
    std::string correlationId = aimux::logging::Logger::generateCorrelationId();
    auto logger = aimux::logging::Logger("health_check", correlationId);

    logger.info("Health check requested");

    try {
        auto& healthChecker = monitoring::HealthChecker::getInstance();
        auto healthStatus = healthChecker.checkHealth();

        // Add system metrics
        auto& systemCollector = getSystemMetricsCollector();
        auto systemMetrics = systemCollector.getCurrentMetrics();

        nlohmann::json response = healthStatus.toJson();
        response["system"] = {
            {"cpu_usage_percent", systemMetrics.cpu_usage_percent},
            {"memory_usage_mb", systemMetrics.memory_usage_mb},
            {"memory_usage_percent", systemMetrics.memory_usage_percent},
            {"disk_used_mb", systemMetrics.disk_used_mb},
            {"disk_total_mb", systemMetrics.disk_total_mb},
            {"process_count", systemMetrics.process_count},
            {"thread_count", systemMetrics.thread_count},
            {"uptime_seconds", getUptimeSeconds()}
        };

        // Service information
        response["service"] = {
            {"name", "aimux2"},
            {"version", getVersion()},
            {"build_date", getBuildDate()},
            {"environment", getEnvironment()},
            {"uptime_seconds", getUptimeSeconds()}
        };

        logger.info("Health check completed successfully", {
            {"healthy", response["healthy"]},
            {"cpu_percent", systemMetrics.cpu_usage_percent},
            {"memory_percent", systemMetrics.memory_usage_percent}
        });

        return response;

    } catch (const std::exception& e) {
        logger.error("Health check failed", {
            {"error_type", typeid(e).name()},
            {"error_message", e.what()},
            {"endpoint", "/health"}
        });

        return {
            {"healthy", false},
            {"status", "Health check failed"},
            {"error", e.what()},
            {"correlation_id", correlationId},
            {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()}
        };
    }
}

// Metrics endpoint (Prometheus format)
std::string handleMetricsPrometheus() {
    std::string correlationId = aimux::logging::Logger::generateCorrelationId();
    auto logger = aimux::logging::Logger("metrics_export", correlationId);

    logger.debug("Prometheus metrics export requested");

    try {
        auto& registry = monitoring::MetricsRegistry::getInstance();
        auto result = registry.exportToPrometheus();

        logger.debug("Prometheus metrics export completed", {
            {"format", "prometheus"},
            {"response_size_bytes", result.length()}
        });

        return result;
    } catch (const std::exception& e) {
        logger.error("Prometheus metrics export failed", {
            {"error_type", typeid(e).name()},
            {"error_message", e.what()},
            {"endpoint", "/metrics/prometheus"},
            {"format", "prometheus"}
        });

        return "# Error exporting metrics\n";
    }
}

// Metrics endpoint (JSON format)
nlohmann::json handleMetricsJson() {
    std::string correlationId = aimux::logging::Logger::generateCorrelationId();
    auto logger = aimux::logging::Logger("metrics_export", correlationId);

    logger.debug("JSON metrics export requested");

    try {
        auto& registry = monitoring::MetricsRegistry::getInstance();
        auto metricsJson = registry.exportToJson();

        // Add system metrics
        auto& systemCollector = getSystemMetricsCollector();
        auto systemMetrics = systemCollector.getCurrentMetrics();

        metricsJson["system"] = {
            {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
                systemMetrics.timestamp.time_since_epoch()).count()},
            {"cpu", {
                {"usage_percent", systemMetrics.cpu_usage_percent},
                {"load_average", {
                    {"1m", systemMetrics.load_average_1m},
                    {"5m", systemMetrics.load_average_5m},
                    {"15m", systemMetrics.load_average_15m}
                }}
            }},
            {"memory", {
                {"used_mb", systemMetrics.memory_usage_mb},
                {"usage_percent", systemMetrics.memory_usage_percent}
            }},
            {"disk", {
                {"used_mb", systemMetrics.disk_used_mb},
                {"total_mb", systemMetrics.disk_total_mb}
            }},
            {"network", {
                {"bytes_sent", systemMetrics.network_bytes_sent},
                {"bytes_received", systemMetrics.network_bytes_received}
            }},
            {"processes", {
                {"count", systemMetrics.process_count},
                {"threads", systemMetrics.thread_count}
            }}
        };

        metricsJson["correlation_id"] = correlationId;

        logger.debug("JSON metrics export completed", {
            {"format", "json"},
            {"system_cpu_percent", systemMetrics.cpu_usage_percent},
            {"system_memory_percent", systemMetrics.memory_usage_percent}
        });

        return metricsJson;

    } catch (const std::exception& e) {
        logger.error("JSON metrics export failed", {
            {"error_type", typeid(e).name()},
            {"error_message", e.what()},
            {"endpoint", "/metrics"},
            {"format", "json"}
        });

        return {
            {"error", e.what()},
            {"correlation_id", correlationId},
            {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()}
        };
    }
}

// Logs endpoint
nlohmann::json handleLogs(int limit = 100, const std::string& level = "",
                         const std::string& since = "") {
    std::string correlationId = aimux::logging::Logger::generateCorrelationId();
    auto logger = aimux::logging::Logger("logs_retrieval", correlationId);

    logger.info("Log retrieval requested", {
        {"requested_limit", limit},
        {"requested_level", level},
        {"has_since_filter", !since.empty()}
    });

    try {
        std::string logFile = getLogFile();
        std::ifstream file(logFile);
        if (!file.is_open()) {
            return {
                {"error", "Unable to open log file"},
                {"log_file", logFile}
            };
        }

        std::vector<nlohmann::json> logs;
        std::string line;

        // Read log file from bottom (recent logs first)
        std::vector<std::string> lines;
        while (std::getline(file, line)) {
            if (line.empty()) continue;

            try {
                auto jsonLog = nlohmann::json::parse(line);

                // Filter by level if specified
                if (!level.empty()) {
                    if (jsonLog.contains("level") && jsonLog["level"] != level) {
                        continue;
                    }
                }

                // Filter by time if since is specified
                if (!since.empty()) {
                    auto logTime = std::chrono::milliseconds(jsonLog["timestamp"].get<int64_t>());
                    auto sinceTime = std::chrono::milliseconds(std::stoll(since));
                    if (logTime < sinceTime) {
                        continue;
                    }
                }

                lines.push_back(line);
            } catch (const nlohmann::json::parse_error&) {
                // Skip invalid JSON lines
                continue;
            }
        }

        // Return last 'limit' entries
        int startIdx = std::max(0, static_cast<int>(lines.size()) - limit);
        for (int i = startIdx; i < lines.size(); ++i) {
            try {
                logs.push_back(nlohmann::json::parse(lines[i]));
            } catch (const nlohmann::json::parse_error&) {
                // Skip invalid JSON
            }
        }

        return {
            {"logs", logs},
            {"total", lines.size()},
            {"returned", logs.size()},
            {"filters", {
                {"level", level},
                {"limit", limit}
            }}
        };

    } catch (const std::exception& e) {
            logger.error("Log retrieval failed", {
            {"error_type", typeid(e).name()},
            {"error_message", e.what()},
            {"endpoint", "/logs"},
            {"requested_limit", limit},
            {"requested_level", level}
        });
        return {
            {"error", e.what()},
            {"logs", nlohmann::json::array()}
        };
    }
}

// Alerts endpoint
nlohmann::json handleAlerts() {
    try {
        auto& alertManager = monitoring::AlertManager::getInstance();
        auto activeAlerts = alertManager.getActiveAlerts();

        nlohmann::json response;
        response["alerts"] = nlohmann::json::array();
        response["alert_count"] = activeAlerts.size();
        response["severity_counts"] = {
            {"critical", 0},
            {"warning", 0},
            {"info", 0}
        };

        for (const auto& alert : activeAlerts) {
            response["alerts"].push_back(alert.toJson());
            response["severity_counts"][alert.severity]++;
        }

        return response;

    } catch (const std::exception& e) {
        AIMUX_LOG_ERROR("Alert retrieval failed: " + std::string(e.what()));
        return {
            {"error", e.what()},
            {"alerts", nlohmann::json::array()}
        };
    }
}

// Performance profiling endpoint
nlohmann::json handleProfiling() {
    try {
        auto& registry = monitoring::MetricsRegistry::getInstance();
        nlohmann::json response;

        // Request metrics
        auto requestCounter = registry.getMetric("http_requests_total");
        auto requestDuration = registry.getMetric("http_request_duration_ms");
        auto activeConnections = registry.getMetric("active_connections");

        if (requestCounter) {
            response["requests"] = {
                {"total", requestCounter->toJson()["value"]},
                {"rate_per_minute", calculateRequestRatePerMinute()}
            };
        }

        if (requestDuration) {
            auto durationJson = requestDuration->toJson();
            response["performance"] = {
                {"average_duration_ms", durationJson["sum"] / durationJson["count"]},
                {"max_duration_ms", durationJson["max"]},
                {"min_duration_ms", durationJson["min"]},
                {"request_count", durationJson["count"]}
            };
        }

        if (activeConnections) {
            response["connections"] = {
                {"active", activeConnections->toJson()["value"]}
            };
        }

        // Memory usage
        response["memory"] = {
            {"heap_used_mb", getHeapUsageMB()},
            {"rss_mb", getRSSUsageMB()},
            {"virtual_memory_mb", getVirtualMemoryMB()}
        };

        return response;

    } catch (const std::exception& e) {
        AIMUX_LOG_ERROR("Profiling data retrieval failed: " + std::string(e.what()));
        return {
            {"error", e.what()}
        };
    }
}

// Configuration endpoint (read-only for security)
nlohmann::json handleConfiguration() {
    try {
        auto config = getConfigReader().readConfig();

        // Remove sensitive information
        nlohmann::json sanitized = config;
        if (sanitized.contains("providers")) {
            for (auto& provider : sanitized["providers"]) {
                if (provider.contains("api_key")) {
                    provider["api_key"] = "***REDACTED***";
                }
            }
        }

        return {
            {"config", sanitized},
            {"config_file", getConfigFile()},
            {"last_modified", getConfigFileModifiedTime()}
        };

    } catch (const std::exception& e) {
        AIMUX_LOG_ERROR("Configuration retrieval failed: " + std::string(e.what()));
        return {
            {"error", e.what()},
            {"config", nlohmann::json::object()}
        };
    }
}

// Service status endpoint
nlohmann::json handleServiceStatus() {
    try {
        nlohmann::json response;

        // Basic service info
        response["service"] = {
            {"name", "aimux2"},
            {"status", "running"},
            {"version", getVersion()},
            {"build_info", getBuildInfo()},
            {"uptime_seconds", getUptimeSeconds()},
            {"pid", getpid()}
        };

        // Provider status
        response["providers"] = getProviderStatus();

        // WebUI status
        response["webui"] = {
            {"enabled", isWebUIEnabled()},
            {"port", getWebUIPort()},
            {"ssl_enabled", isWebUISSLEnabled()}
        };

        // System resources
        auto metrics = getSystemMetricsCollector().getCurrentMetrics();
        response["resources"] = {
            {"cpu_percent", metrics.cpu_usage_percent},
            {"memory_percent", metrics.memory_usage_percent},
            {"disk_percent", (static_cast<double>(metrics.disk_used_mb) /
                           static_cast<double>(metrics.disk_total_mb)) * 100}
        };

        return response;

    } catch (const std::exception& e) {
        AIMUX_LOG_ERROR("Service status retrieval failed: " + std::string(e.what()));
        return {
            {"error", e.what()},
            {"service", {{"status", "error"}}}
        };
    }
}

// Register monitoring endpoints
void registerMonitoringEndpoints() {
    registerEndpoint("/health", HTTPMethod::GET, handleHealthCheck);
    registerEndpoint("/metrics", HTTPMethod::GET, handleMetricsJson);
    registerEndpoint("/metrics/prometheus", HTTPMethod::GET, handleMetricsPrometheus);
    registerEndpoint("/logs", HTTPMethod::GET, handleLogs);
    registerEndpoint("/alerts", HTTPMethod::GET, handleAlerts);
    registerEndpoint("/profiling", HTTPMethod::GET, handleProfiling);
    registerEndpoint("/config", HTTPMethod::GET, handleConfiguration);
    registerEndpoint("/status", HTTPMethod::GET, handleServiceStatus);

    aimux::logging::Logger monitorLogger("endpoint_registration");
    monitorLogger.info("Monitoring endpoints registered", {
        {"endpoints", {
            "/health", "/metrics", "/metrics/prometheus",
            "/logs", "/alerts", "/profiling",
            "/config", "/status"
        }},
        {"total_count", 8}
    });
}

} // namespace webui
} // namespace aimux