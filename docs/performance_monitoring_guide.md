# Aimux v2.0.0 - Performance Monitoring Guide

This guide provides comprehensive documentation for the enhanced performance monitoring system implemented in Aimux v2.0.0.

## 📊 Overview

The enhanced performance monitoring system provides granular insights into every aspect of the Aimux system:

- **Real-time Performance Metrics**: Live monitoring of system and component performance
- **Provider-specific Analytics**: Detailed performance tracking per AI provider
- **Endpoint Performance**: Granular metrics per API endpoint
- **Memory Usage Monitoring**: Comprehensive memory tracking and analysis
- **Performance Alerting**: Automated alerts for performance degradation
- **Historical Analysis**: Trend analysis and performance history

## 🚀 Quick Start

### Basic Integration

```cpp
#include "aimux/monitoring/performance_monitor.h"

// Start performance monitoring
auto& monitor = aimux::monitoring::PerformanceMonitor::getInstance();
monitor.startMonitoring(
    std::chrono::seconds(5),    // Collection interval
    std::chrono::hours(24)      // Retention period
);

// Track a component operation
{
    AIMUX_TRACK_COMPONENT("router");

    // Your code here...

} // Automatically records duration on scope exit
```

### Provider Request Tracking

```cpp
// Record provider request
auto& monitor = aimux::monitoring::PerformanceMonitor::getInstance();

monitor.recordProviderRequest(
    "cerebras",      // Provider name
    "llama3.1-70b",  // Model
    850.5,           // Duration in ms
    true,            // Success
    "",              // Error type
    0.0042           // Cost
);
```

### Request Lifecycle Tracking

```cpp
// Start tracking a request
std::string request_id = "req_12345";
std::string event_id = monitor.recordRequestStart(
    request_id,
    "/anthropic",
    "POST",
    "cerebras",
    "llama3.1-70b"
);

// ... process request ...

// End tracking
monitor.recordRequestEnd(event_id, true, 200, 2048, "", 0.0042);
```

## 🔧 Core Components

### PerformanceMonitor

The central component managing all performance monitoring activities.

**Key Methods:**

- `startMonitoring()`: Begin collecting metrics
- `recordEvent()`: Record custom performance events
- `recordRequestStart/End()`: Track request lifecycle
- `recordProviderRequest()`: Track provider-specific requests
- `getPerformanceReport()`: Get comprehensive performance data
- `setAlertThreshold()`: Configure performance alerts

### PerformanceEvent

Represents individual performance events with rich metadata.

**Event Types:**
- `REQUEST_START/END`: Request lifecycle
- `PROVIDER_START/END`: Provider operation
- `CACHE_HIT/MISS`: Cache performance
- `ERROR/TIMEOUT`: Error tracking
- `RATE_LIMIT_HIT`: Rate limiting
- `FAILOVER_INITIATED`: Failover events

### ProviderPerformance

Tracks detailed performance metrics for each AI provider.

**Metrics Tracked:**
- Request count and success rates
- Response time distributions (p50, p95, p99)
- Model-specific performance
- Cost tracking and analysis
- Health monitoring

### EndpointPerformance

Provides granular metrics per API endpoint.

**Metrics Tracked:**
- Request/response times
- Status code distributions
- Request/response sizes
- Error rates and patterns
- Traffic patterns

### MemoryMetrics

Comprehensive memory usage monitoring.

**Metrics Tracked:**
- Heap usage breakdown
- Stack and anonymous memory
- File cache and shared memory
- Memory pressure indicators
- Page fault tracking

## 📈 Monitoring Endpoints

The system provides REST API endpoints for accessing performance data:

### `/performance/real-time`

**Purpose**: Get real-time performance metrics

**Response**:
```json
{
  "timestamp": 1699999999,
  "monitoring_active": true,
  "memory_metrics": {
    "heap_used_mb": 245,
    "memory_pressure_percent": 15.2,
    "total_process_mb": 512
  },
  "provider_performance": {
    "cerebras": { /* provider-specific data */ },
    "zai": { /* provider-specific data */ }
  },
  "active_alerts": 2,
  "alerts": ["High memory pressure detected", "Response time degradation"]
}
```

### `/performance/providers`

**Purpose**: Get provider-specific performance data

**Parameters**: `?provider=provider_name` (optional, for specific provider)

**Response**:
```json
{
  "timestamp": 1699999999,
  "providers_count": 2,
  "providers": {
    "cerebras": {
      "provider_name": "cerebras",
      "total_requests": 1250,
      "successful_requests": 1185,
      "success_rate_percent": 94.8,
      "average_response_time_ms": 850.5,
      "model_stats": {
        "llama3.1-70b": {
          "total_operations": 800,
          "avg_duration_ms": 820.3,
          "p95_duration_ms": 1200.0
        }
      }
    }
  },
  "summary": {
    "total_requests": 1250,
    "success_rate_percent": 94.8,
    "total_estimated_cost": 5.25
  }
}
```

### `/performance/endpoints`

**Purpose**: Get endpoint-specific performance data

**Parameters**: `?endpoint=/api/v1/chat` (optional, for specific endpoint)

**Response**:
```json
{
  "timestamp": 1699999999,
  "endpoints_count": 5,
  "endpoints": {
    "/anthropic": {
      "endpoint_path": "/anthropic",
      "method": "POST",
      "stats": {
        "total_operations": 2000,
        "success_rate_percent": 96.5,
        "avg_duration_ms": 1250.3,
        "p95_duration_ms": 2000.0
      },
      "status_code_counts": {
        "200": 1930,
        "400": 45,
        "500": 25
      }
    }
  },
  "aggregated": {
    "total_operations": 2000,
    "success_rate_percent": 96.5,
    "average_duration_ms": 1250.3,
    "status_code_distribution": {
      "200": 1930, "400": 45, "500": 25
    }
  }
}
```

### `/performance/memory`

**Purpose**: Get memory usage and trends

**Response**:
```json
{
  "timestamp": 1699999999,
  "current": {
    "heap_used_mb": 245,
    "stack_used_mb": 32,
    "anonymous_mb": 180,
    "file_cache_mb": 45,
    "memory_pressure_percent": 15.2,
    "page_faults": 1250
  },
  "trends": {
    "hours_analyzed": 24,
    "average_heap_mb": 220.5,
    "min_heap_mb": 185.0,
    "max_heap_mb": 285.0,
    "pressure_trend": "stable"
  },
  "alerts": []
}
```

### `/performance/alerts`

**Purpose**: Get current performance alerts

**Response**:
```json
{
  "timestamp": 1699999999,
  "active_alerts_count": 2,
  "system_health": "degraded",
  "alerts": [
    "[warning] High memory pressure detected (85.5%)",
    "[warning] Response time degradation detected for provider cerebras"
  ]
}
```

### `/performance/config`

**Purpose**: Configure performance monitoring settings

**Method**: `GET` to view config, `POST` to update

**Update Request Body**:
```json
{
  "alert_thresholds": [
    {
      "metric": "memory_pressure",
      "threshold": 80.0,
      "comparison": ">",
      "severity": "warning"
    },
    {
      "metric": "response_time_ms",
      "threshold": 2000.0,
      "comparison": ">",
      "severity": "critical"
    }
  ]
}
```

## 🔍 Performance Tracking Code Examples

### Manual Event Tracking

```cpp
auto& monitor = aimux::monitoring::PerformanceMonitor::getInstance();

// Record custom performance event
aimux::monitoring::PerformanceEvent event(
    "custom_event_id",
    aimux::monitoring::PerformanceEventType::REQUEST_START,
    "database",
    "query"
);

event.metadata["query_type"] = "SELECT";
event.metadata["table"] = "users";
event.metadata["complexity"] = "high";
event.duration = std::chrono::duration<double, std::milli>(150.5);

monitor.recordEvent(event);
```

### Scoped Performance Tracking

```cpp
void processRequest(const std::string& request) {
    AIMUX_TRACK_OPERATION("request_processor", "parse");

    // Parsing logic...
    {
        AIMUX_TRACK_COMPONENT("validation");
        // Validation logic...
    }

    {
        AIMUX_TRACK_OPERATION_WITH_METADATA("provider_call", "execute", {
            {"provider", "cerebras"},
            {"model", "llama3.1-70b"}
        });

        // Provider call logic...
    }
}
```

### Batch Performance Tracking

```cpp
void batchRequestProcessing(const std::vector<Request>& requests) {
    auto& monitor = aimux::monitoring::PerformanceMonitor::getInstance();

    std::string batch_id = "batch_" + std::to_string(std::time(nullptr));

    for (const auto& request : requests) {
        auto event_id = monitor.recordRequestStart(
            request.id,
            request.endpoint,
            request.method,
            request.provider,
            request.model
        );

        try {
            // Process request...
            monitor.recordRequestEnd(event_id, true, 200, response.size, "", cost);
        } catch (const std::exception& e) {
            monitor.recordRequestEnd(event_id, false, 500, 0, e.what(), 0.0);
        }
    }
}
```

## ⚡ Performance Optimization

### Monitoring Overhead

The performance monitoring system is designed for minimal overhead:

- **Collection Interval**: Default 5 seconds (configurable)
- **Memory Usage**: <5MB overhead
- **CPU Impact**: <1% additional CPU usage
- **I/O Impact**: Minimal, mostly in-memory operations

### Best Practices for Optimal Performance

1. **Use Scoped Tracking** for automatic duration measurement
2. **Batch Events** when possible to reduce overhead
3. **Configure Appropriate Intervals** based on your monitoring needs
4. **Set Reasonable Retention** to limit memory usage
5. **Filter Events** to avoid unnecessary noise

### Tuning Recommendations

```cpp
// High-performance configuration
monitor.startMonitoring(
    std::chrono::seconds(10),  // Less frequent for lower overhead
    std::chrono::hours(6)      // Shorter retention for less memory
);

// Comprehensive monitoring configuration
monitor.startMonitoring(
    std::chrono::seconds(1),   // High-frequency for detailed analysis
    std::chrono::hours(48)     // Longer retention for trend analysis
);
```

## 🚨 Alert Configuration

### Setting Up Alert Thresholds

```cpp
// Memory pressure alert (warning at 80%, critical at 95%)
monitor.setAlertThreshold("memory_pressure", 80.0, ">", "warning");
monitor.setAlertThreshold("memory_pressure", 95.0, ">", "critical");

// Response time alerts
monitor.setAlertThreshold("response_time_ms", 2000.0, ">", "warning");
monitor.setAlertThreshold("response_time_ms", 5000.0, ">", "critical");

// Error rate alerts
monitor.setAlertThreshold("error_rate_percent", 5.0, ">", "warning");
monitor.setAlertThreshold("error_rate_percent", 10.0, ">", "critical");
```

### Custom Alert Logic

```cpp
// Custom alert condition - provider-specific performance issues
void checkProviderHealth() {
    auto provider_perf = monitor.getProviderPerformance();

    for (const auto& [name, perf] : provider_perf) {
        if (perf.success_rate_percent < 90.0 && perf.total_requests > 100) {
            // Trigger custom alert
            std::cout << "ALERT: Provider " << name
                     << " has low success rate: " << perf.success_rate_percent << "%\n";
        }

        if (perf.average_response_time_ms > 5000.0) {
            std::cout << "ALERT: Provider " << name
                     << " has high average response time: " << perf.average_response_time_ms << "ms\n";
        }
    }
}
```

## 📊 Data Export and Analysis

### Export Performance Data

```cpp
// Export to JSON
auto json_data = monitor.exportPerformanceData("json");

// Export to Prometheus format
auto prometheus_data = monitor.exportPerformanceData("prometheus");

// Export with time filter (last hour)
auto since_time = std::chrono::system_clock::now() - std::chrono::hours(1);
auto recent_data = monitor.exportPerformanceData("json", since_time);
```

### Integrating with External Systems

```cpp
// Integration with monitoring systems
void exportToPrometheus() {
    auto prometheus_data = monitor.exportPerformanceData("prometheus");

    // Send to Prometheus Push Gateway
    sendToPrometheusGateway(prometheus_data);
}

// Integration with ELK Stack
void exportToElasticsearch() {
    auto json_data = monitor.getPerformanceReport();

    // Send to Elasticsearch
    sendToElasticsearch(json_data);
}
```

## 🔧 Troubleshooting

### Common Issues and Solutions

**Issue**: High memory usage from performance monitoring

**Solution**:
```cpp
// Reduce collection frequency and retention
monitor.startMonitoring(
    std::chrono::seconds(10),  // Reduce frequency
    std::chrono::hours(12)      // Reduce retention
);
```

**Issue**: Too many alerts (alert fatigue)

**Solution**:
```cpp
// Adjust alert thresholds to be less sensitive
monitor.setAlertThreshold("memory_pressure", 90.0, ">", "warning"); // Was 80%
```

**Issue**: Missing performance data for some components

**Solution**:
```cpp
// Ensure proper tracking with scoped macros
void myFunction() {
    AIMUX_TRACK_COMPONENT("my_component");
    // Your code here
}
```

### Performance Debugging

Enable debug logging for performance monitoring:

```cpp
// Set debug log level
aimux::logging::ProductionLogger::Config config;
config.log_level = aimux::logging::LogLevel::DEBUG;
aimux::logging::ProductionLogger::getInstance().configure(config);
```

This will provide detailed logs about:
- Monitoring thread status
- Data collection activities
- Alert triggering events
- Memory usage statistics

## 📚 Advanced Usage

### Custom Performance Metrics

```cpp
// Track custom business metrics
class CustomMetrics {
public:
    void trackUserAction(const std::string& action, const std::string& user_id) {
        // Create a custom timer for this action
        auto timer = AIMUX_TIMER("user_actions_" + action);

        // Perform the action...
        auto duration = performAction(action);

        // Record the duration with user metadata
        timer->observe(duration.count());

        // Also track as an event
        aimux::monitoring::PerformanceEvent event(
            "user_action",
            aimux::monitoring::PerformanceEventType::REQUEST_START,
            "user_action",
            action
        );
        event.metadata["user_id"] = user_id;
        event.duration = duration;

        aimux::monitoring::PerformanceMonitor::getInstance().recordEvent(event);
    }
};
```

### A/B Testing Performance Analysis

```cpp
// Track A/B test variants
void trackABTestPerformance(const std::string& test_name, const std::string& variant,
                           double response_time, bool success) {
    auto& monitor = aimux::monitoring::PerformanceMonitor::getInstance();

    aimux::monitoring::PerformanceEvent event(
        test_name + "_" + variant,
        aimux::monitoring::PerformanceEventType::REQUEST_END,
        "abtest",
        test_name
    );
    event.duration = std::chrono::duration<double, std::milli>(response_time);
    event.metadata["variant"] = variant;
    event.metadata["success"] = std::to_string(success);

    monitor.recordEvent(event);
}
```

## 🎯 Performance Targets

### Recommended Performance Thresholds

| Metric | Target | Warning | Critical |
|--------|--------|---------|----------|
| Response Time (p95) | < 1000ms | > 2000ms | > 5000ms |
| Success Rate | > 95% | < 90% | < 80% |
| Memory Pressure | < 60% | > 80% | > 95% |
| Error Rate | < 1% | > 5% | > 10% |
| CPU Usage | < 70% | > 85% | > 95% |

### Monitoring KPIs

- **Availability**: System uptime and accessibility
- **Response Time**: Average and percentile response times
- **Throughput**: Requests per second handling capacity
- **Error Rate**: Percentage of failed requests
- **Resource Usage**: CPU, memory, and network utilization
- **Provider Performance**: Individual AI provider health metrics

This comprehensive performance monitoring system provides the insights needed to maintain optimal performance, quickly identify issues, and make data-driven decisions about scaling and optimization.