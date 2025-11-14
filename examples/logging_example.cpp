// Demonstration of Enhanced Logging with Correlation ID Tracking
// Shows Fix #6: Log Message Consistency implementation

#include "aimux/logging/correlation_context.h"
#include "aimux/logging/production_logger.h"
#include <iostream>
#include <thread>
#include <chrono>

using namespace aimux::logging;

void demonstrateBasicLogging() {
    std::cout << "=== Basic Structured Logging ===\n";

    // Standard logging with correlation ID
    std::string correlationId = Logger::generateCorrelationId();
    auto logger = Logger("demo", correlationId);

    logger.info("Application starting", {
        {"component", "demo_app"},
        {"version", "2.0.0"},
        {"feature", "logging_consistency"}
    });

    logger.debug("Debug information", {
        {"debug_level", "detailed"},
        {"trace_enabled", true}
    });
}

void demonstrateCorrelationContext() {
    std::cout << "\n=== Correlation Context Management ===\n";

    // Set up correlation context for the thread
    CorrelationScope scope1("req-001");

    AIMUX_LOG_AUTO_INFO("Processing request start", {
        {"request_type", "api_call"},
        {"endpoint", "/v1/completions"}
    });

    // Nested operation - automatic correlation ID propagation
    {
        CorrelationScope scope2("req-001-op-001");

        AIMUX_LOG_AUTO_INFO("Sub-operation started", {
            {"sub_operation", "provider_validation"},
            {"provider", "claude"}
        });

        // Even deeper nesting
        {
            CorrelationScope scope3; // Auto-generates ID

            AIMUX_LOG_AUTO_DEBUG("Deep validation check", {
                {"validation_type", "token_balance"},
                {"balance_check", true}
            });
        }

        AIMUX_LOG_AUTO_INFO("Sub-operation completed");
    }

    AIMUX_LOG_AUTO_INFO("Request processing completed");

    std::cout << "Final correlation ID: " << AIMUX_CURRENT_CORRELATION_ID() << "\n";
}

void demonstrateAsyncLogging() {
    std::cout << "\n=== Multi-threaded Logging ===\n";

    std::vector<std::thread> threads;

    for (int i = 0; i < 3; ++i) {
        threads.emplace_back([i]() {
            // Each thread gets its own correlation context
            CorrelationScope scope;
            AIMUX_SET_CORRELATION_ID("thread-" + std::to_string(i));

            for (int j = 0; j < 3; ++j) {
                CorrelationScope operationScope;

                AIMUX_LOG_AUTO_INFO("Thread operation", {
                    {"thread_id", i},
                    {"operation", j},
                    {"message_type", "work_item"}
                });

                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }
}

void demonstrateErrorHandling() {
    std::cout << "\n=== Error Logging with Context ===\n";

    CorrelationScope errorScope("error-demo");

    auto logger = Logger("error_handler", AIMUX_CURRENT_CORRELATION_ID());

    try {
        // Simulate an error
        throw std::runtime_error("Simulated internal error");
    } catch (const std::exception& e) {
        logger.error("Exception caught", {
            {"error_type", "runtime_error"},
            {"error_message", e.what()},
            {"component", "demo_module"},
            {"recovery_action", "graceful_degradation"}
        });

        AIMUX_LOG_AUTO_WARN("Continuing after error", {
            {"fallback_enabled", true},
            {"error_handled", true}
        });
    }
}

void demonstratePerformanceLogging() {
    std::cout << "\n=== Performance and Metrics Logging ===\n";

    auto start = std::chrono::high_resolution_clock::now();

    CorrelationScope perfScope("perf-test");

    AIMUX_LOG_AUTO_INFO("Performance test started", {
        {"test_type", "throughput_measurement"},
        {"iterations", 1000}
    });

    // Simulate work
    for (int i = 0; i < 1000; ++i) {
        // Dummy computation
        volatile int x = i * i;
        (void)x;
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    AIMUX_LOG_AUTO_INFO("Performance test completed", {
        {"iterations_completed", 1000},
        {"duration_us", duration.count()},
        {"throughput_ops_per_sec", (1000.0 * 1000000.0) / duration.count()},
        {"test_status", "passed"}
    });
}

int main() {
    std::cout << "Aimux v2.0.0 - Enhanced Logging demonstration\n";
    std::cout << "Fix #6: Log Message Consistency with correlation IDs\n";
    std::cout << "==================================================\n";

    try {
        // Initialize production logger
        ProductionLogger::getInstance();

        demonstrateBasicLogging();
        demonstrateCorrelationContext();
        demonstrateAsyncLogging();
        demonstrateErrorHandling();
        demonstratePerformanceLogging();

        std::cout << "\n=== Demo completed successfully! ===\n";
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Demo failed: " << e.what() << std::endl;
        return 1;
    }
}