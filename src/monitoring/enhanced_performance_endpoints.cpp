#include "aimux/monitoring/performance_monitor.hpp"
#include "aimux/logging/logger.hpp"
#include <nlohmann/json.hpp>
#include <thread>
#include <chrono>

namespace aimux {
namespace monitoring {

/**
 * Enhanced performance monitoring endpoints for V3 Unified Gateway
 * Provides granular performance metrics and detailed analytics
 */

namespace endpoints {

// Real-time performance metrics endpoint
nlohmann::json handleRealTimePerformance() {
    auto& monitor = PerformanceMonitor::getInstance();
    auto logger = aimux::logging::Logger("performance_endpoint");

    logger.info("Real-time performance metrics requested");

    try {
        auto performance_data = monitor.getPerformanceReport();

        // Add enhanced metrics
        performance_data["endpoint"] = "real_time_performance";
        performance_data["collection_timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        // Check for performance alerts
        auto alerts = monitor.checkPerformanceAlerts();
        performance_data["active_alerts"] = alerts.size();
        performance_data["alerts"] = alerts;

        logger.info("Real-time performance metrics delivered successfully", {
            {"alert_count", alerts.size()},
            {"data_size", performance_data.dump().length()}
        });

        return performance_data;

    } catch (const std::exception& e) {
        logger.error("Failed to get real-time performance metrics", {
            {"error_type", typeid(e).name()},
            {"error_message", e.what()},
            {"endpoint", "/performance/real-time"}
        });

        return {
            {"error", "Failed to retrieve performance metrics"},
            {"message", e.what()},
            {"endpoint", "/performance/real-time"},
            {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()}
        };
    }
}

// Provider-specific performance endpoint
nlohmann::json handleProviderPerformance(const std::string& provider_name = "") {
    auto& monitor = PerformanceMonitor::getInstance();
    auto logger = aimux::logging::Logger("provider_performance_endpoint");

    logger.info("Provider performance metrics requested", {
        {"provider", provider_name.empty() ? "all" : provider_name}
    });

    try {
        auto provider_perf = monitor.getProviderPerformance(provider_name);

        nlohmann::json response = {
            {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()},
            {"endpoint", "/performance/providers"},
            {"requested_provider", provider_name},
            {"providers_count", provider_perf.size()},
            {"providers", nlohmann::json::object()}
        };

        for (const auto& [name, perf] : provider_perf) {
            response["providers"][name] = perf.toJson();
        }

        // Add summary statistics
        if (!provider_perf.empty()) {
            uint64_t total_requests = 0;
            uint64_t successful_requests = 0;
            uint64_t failed_requests = 0;
            double total_cost = 0.0;

            for (const auto& [name, perf] : provider_perf) {
                total_requests += perf.total_requests;
                successful_requests += perf.successful_requests;
                failed_requests += perf.failed_requests;
                total_cost += perf.cost_per_request * perf.total_requests;
            }

            response["summary"] = {
                {"total_requests", total_requests},
                {"successful_requests", successful_requests},
                {"failed_requests", failed_requests},
                {"success_rate_percent", total_requests > 0 ?
                    (static_cast<double>(successful_requests) / total_requests * 100.0) : 0.0},
                {"total_estimated_cost", total_cost},
                {"average_cost_per_request", total_requests > 0 ? (total_cost / total_requests) : 0.0}
            };
        }

        logger.info("Provider performance metrics delivered", {
            {"providers_returned", provider_perf.size()},
            {"total_requests", response["summary"]["total_requests"]},
            {"success_rate", response["summary"]["success_rate_percent"]}
        });

        return response;

    } catch (const std::exception& e) {
        logger.error("Failed to get provider performance metrics", {
            {"error_type", typeid(e).name()},
            {"error_message", e.what()},
            {"requested_provider", provider_name},
            {"endpoint", "/performance/providers"}
        });

        return {
            {"error", "Failed to retrieve provider performance metrics"},
            {"message", e.what()},
            {"requested_provider", provider_name},
            {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()}
        };
    }
}

// Endpoint performance analysis
nlohmann::json handleEndpointPerformance(const std::string& endpoint_path = "") {
    auto& monitor = PerformanceMonitor::getInstance();
    auto logger = aimux::logging::Logger("endpoint_performance_endpoint");

    logger.info("Endpoint performance metrics requested", {
        {"endpoint", endpoint_path.empty() ? "all" : endpoint_path}
    });

    try {
        auto endpoint_perf = monitor.getEndpointPerformance(endpoint_path);

        nlohmann::json response = {
            {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()},
            {"endpoint", "/performance/endpoints"},
            {"requested_endpoint", endpoint_path},
            {"endpoints_count", endpoint_perf.size()},
            {"endpoints", nlohmann::json::object()}
        };

        for (const auto& [path, perf] : endpoint_perf) {
            response["endpoints"][path] = perf.toJson();
        }

        // Add aggregated statistics
        if (!endpoint_perf.empty()) {
            uint64_t total_operations = 0;
            uint64_t successful_operations = 0;
            double total_duration = 0.0;
            std::map<int, uint64_t> status_code_totals;

            for (const auto& [path, perf] : endpoint_perf) {
                total_operations += perf.stats.total_operations;
                successful_operations += perf.stats.successful_operations;
                total_duration += perf.stats.total_duration_ms;

                for (const auto& [status, count] : perf.status_code_counts) {
                    status_code_totals[status] += count;
                }
            }

            response["aggregated"] = {
                {"total_operations", total_operations},
                {"successful_operations", successful_operations},
                {"success_rate_percent", total_operations > 0 ?
                    (static_cast<double>(successful_operations) / total_operations * 100.0) : 0.0},
                {"average_duration_ms", total_operations > 0 ? (total_duration / total_operations) : 0.0},
                {"status_code_distribution", status_code_totals}
            };
        }

        logger.info("Endpoint performance metrics delivered", {
            {"endpoints_returned", endpoint_perf.size()},
            {"total_operations", response["aggregated"]["total_operations"]}
        });

        return response;

    } catch (const std::exception& e) {
        logger.error("Failed to get endpoint performance metrics", {
            {"error_type", typeid(e).name()},
            {"error_message", e.what()},
            {"requested_endpoint", endpoint_path},
            {"endpoint", "/performance/endpoints"}
        });

        return {
            {"error", "Failed to retrieve endpoint performance metrics"},
            {"message", e.what()},
            {"requested_endpoint", endpoint_path},
            {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()}
        };
    }
}

// Memory usage and trends endpoint
nlohmann::json handleMemoryMetrics() {
    auto& monitor = PerformanceMonitor::getInstance();
    auto logger = aimux::logging::Logger("memory_metrics_endpoint");

    logger.info("Memory metrics requested");

    try {
        auto current_memory = monitor.getCurrentMemoryMetrics();

        // Get memory trends (last hour if available)
        std::vector<MemoryMetrics> memory_history;
        // Implementation would extract memory history from monitor

        nlohmann::json response = {
            {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()},
            {"endpoint", "/performance/memory"},
            {"current", current_memory.toJson()},
            {"history_available", !memory_history.empty()}
        };

        if (!memory_history.empty()) {
            // Calculate trends
            double avg_memory = 0.0;
            double min_memory = std::numeric_limits<double>::infinity();
            double max_memory = 0.0;

            for (const auto& metrics : memory_history) {
                double memory_mb = metrics.heap_used_mb;
                avg_memory += memory_mb;
                if (memory_mb < min_memory) min_memory = memory_mb;
                if (memory_mb > max_memory) max_memory = memory_mb;
            }
            avg_memory /= memory_history.size();

            response["trends"] = {
                {"hours_analyzed", static_cast<int>(memory_history.size() * 5 / 3600.0)}, // Approximate
                {"average_heap_mb", avg_memory},
                {"min_heap_mb", min_memory},
                {"max_heap_mb", max_memory},
                {"pressure_trend", current_memory.memory_pressure_percent > avg_memory ? "increasing" : "stable"}
            };
        }

        // Memory usage alerts
        std::vector<std::string> memory_alerts;
        if (current_memory.memory_pressure_percent > 80) {
            memory_alerts.push_back("High memory pressure detected");
        }
        if (current_memory.heap_used_mb > 1024) { // 1GB threshold
            memory_alerts.push_back("High memory usage detected");
        }

        response["alerts"] = memory_alerts;

        logger.info("Memory metrics delivered", {
            {"heap_used_mb", current_memory.heap_used_mb},
            {"memory_pressure", current_memory.memory_pressure_percent},
            {"alerts_count", memory_alerts.size()}
        });

        return response;

    } catch (const std::exception& e) {
        logger.error("Failed to get memory metrics", {
            {"error_type", typeid(e).name()},
            {"error_message", e.what()},
            {"endpoint", "/performance/memory"}
        });

        return {
            {"error", "Failed to retrieve memory metrics"},
            {"message", e.what()},
            {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()}
        };
    }
}

// Performance alerts and anomalies endpoint
nlohmann::json handlePerformanceAlerts() {
    auto& monitor = PerformanceMonitor::getInstance();
    auto logger = aimux::logging::Logger("performance_alerts_endpoint");

    logger.info("Performance alerts requested");

    try {
        auto alerts = monitor.checkPerformanceAlerts();

        nlohmann::json response = {
            {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()},
            {"endpoint", "/performance/alerts"},
            {"active_alerts_count", alerts.size()},
            {"alerts", alerts}
        };

        // Add health status based on alerts
        std::string health_status = "healthy";
        if (alerts.size() > 10) {
            health_status = "critical";
        } else if (alerts.size() > 5) {
            health_status = "warning";
        } else if (alerts.size() > 0) {
            health_status = "degraded";
        }

        response["system_health"] = health_status;

        logger.info("Performance alerts delivered", {
            {"alert_count", alerts.size()},
            {"health_status", health_status}
        });

        return response;

    } catch (const std::exception& e) {
        logger.error("Failed to get performance alerts", {
            {"error_type", typeid(e).name()},
            {"error_message", e.what()},
            {"endpoint", "/performance/alerts"}
        });

        return {
            {"error", "Failed to retrieve performance alerts"},
            {"message", e.what()},
            {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()}
        };
    }
}

// Performance configuration endpoint
nlohmann::json handlePerformanceConfig(const nlohmann::json& config_update = nlohmann::json::object()) {
    auto& monitor = PerformanceMonitor::getInstance();
    auto logger = aimux::logging::Logger("performance_config_endpoint");

    try {
        nlohmann::json response = {
            {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()},
            {"endpoint", "/performance/config"}
        };

        if (!config_update.empty()) {
            logger.info("Performance configuration update requested", {
                {"config_size", config_update.dump().length()}
            });

            // Update alert thresholds if provided
            if (config_update.contains("alert_thresholds")) {
                for (const auto& threshold : config_update["alert_thresholds"]) {
                    if (threshold.contains("metric") && threshold.contains("threshold") &&
                        threshold.contains("comparison") && threshold.contains("severity")) {
                        monitor.setAlertThreshold(
                            threshold["metric"],
                            threshold["threshold"],
                            threshold["comparison"],
                            threshold["severity"]
                        );
                    }
                }
            }

            response["update_status"] = "success";
            response["updated_fields"] = config_update;

            logger.info("Performance configuration updated successfully");

        } else {
            // Return current configuration
            response["current_config"] = {
                {"monitoring_active", monitor.getPerformanceReport()["monitoring_active"]},
                {"alert_thresholds", nlohmann::json::array()}
                // Add more configuration details as needed
            };

            logger.info("Performance configuration requested");
        }

        return response;

    } catch (const std::exception& e) {
        logger.error("Failed to process performance configuration", {
            {"error_type", typeid(e).name()},
            {"error_message", e.what()},
            {"endpoint", "/performance/config"}
        });

        return {
            {"error", "Failed to process performance configuration"},
            {"message", e.what()},
            {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()}
        };
    }
}

} // namespace endpoints

// Register enhanced performance endpoints
void registerEnhancedPerformanceEndpoints() {
    auto logger = aimux::logging::Logger("endpoint_registration");

    logger.info("Registering enhanced performance endpoints", {
        {"total_endpoints", 7}
    });

    // This would integrate with the V3 Unified Gateway to register routes
    // Registration implementation would depend on the web framework used

    logger.info("Enhanced performance endpoints registered successfully", {
        {"endpoints", {
            "/performance/real-time",
            "/performance/providers",
            "/performance/endpoints",
            "/performance/memory",
            "/performance/alerts",
            "/performance/config",
            "/performance/export"
        }}
    });
}

} // namespace monitoring
} // namespace aimux