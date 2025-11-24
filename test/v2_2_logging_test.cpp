/**
 * Test suite for v2.2 Global Logging Functions
 * Tests the global aimux:: namespace logging functions implementation
 */

#include <gtest/gtest.h>
#include <memory>
#include <string>
#include "aimux/logging/logger.hpp"

using namespace aimux::logging;

class GlobalLoggingTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a test logger to ensure registry is initialized
        test_logger_ = LoggerRegistry::get_logger("test_logger");
    }

    void TearDown() override {
        LoggerRegistry::remove_logger("test_logger");
        LoggerRegistry::remove_logger("aimux_default");
    }

    std::shared_ptr<Logger> test_logger_;
};

// Test: Global debug function can be called
TEST_F(GlobalLoggingTest, GlobalDebugFunction) {
    ASSERT_NO_THROW({
        aimux::debug("Test debug message");
    });
}

// Test: Global info function can be called
TEST_F(GlobalLoggingTest, GlobalInfoFunction) {
    ASSERT_NO_THROW({
        aimux::info("Test info message");
    });
}

// Test: Global warn function can be called
TEST_F(GlobalLoggingTest, GlobalWarnFunction) {
    ASSERT_NO_THROW({
        aimux::warn("Test warning message");
    });
}

// Test: Global error function can be called
TEST_F(GlobalLoggingTest, GlobalErrorFunction) {
    ASSERT_NO_THROW({
        aimux::error("Test error message");
    });
}

// Test: Global fatal function can be called
TEST_F(GlobalLoggingTest, GlobalFatalFunction) {
    ASSERT_NO_THROW({
        aimux::fatal("Test fatal message");
    });
}

// Test: Global trace function can be called
TEST_F(GlobalLoggingTest, GlobalTraceFunction) {
    ASSERT_NO_THROW({
        aimux::trace("Test trace message");
    });
}

// Test: Global functions with JSON data
TEST_F(GlobalLoggingTest, GlobalFunctionsWithData) {
    nlohmann::json test_data = {
        {"key1", "value1"},
        {"key2", 42},
        {"key3", true}
    };

    ASSERT_NO_THROW({
        aimux::debug("Debug with data", test_data);
        aimux::info("Info with data", test_data);
        aimux::warn("Warn with data", test_data);
        aimux::error("Error with data", test_data);
        aimux::fatal("Fatal with data", test_data);
        aimux::trace("Trace with data", test_data);
    });
}

// Test: Global functions with empty JSON data (default parameter)
TEST_F(GlobalLoggingTest, GlobalFunctionsDefaultData) {
    ASSERT_NO_THROW({
        aimux::debug("Message without explicit data");
        aimux::info("Message without explicit data");
        aimux::warn("Message without explicit data");
        aimux::error("Message without explicit data");
        aimux::fatal("Message without explicit data");
        aimux::trace("Message without explicit data");
    });
}

// Test: aimux::logging:: namespace functions (forwarding)
TEST_F(GlobalLoggingTest, LoggingNamespaceFunctions) {
    ASSERT_NO_THROW({
        aimux::logging::debug("Debug via logging namespace");
        aimux::logging::info("Info via logging namespace");
        aimux::logging::warn("Warn via logging namespace");
        aimux::logging::error("Error via logging namespace");
        aimux::logging::fatal("Fatal via logging namespace");
        aimux::logging::trace("Trace via logging namespace");
    });
}

// Test: Concurrent access to global logging functions
TEST_F(GlobalLoggingTest, ConcurrentAccess) {
    const int num_threads = 10;
    const int messages_per_thread = 100;

    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([i, messages_per_thread]() {
            for (int j = 0; j < messages_per_thread; ++j) {
                aimux::info("Concurrent message from thread " + std::to_string(i));
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Test passes if no crashes occur
    SUCCEED();
}

// Test: Multiple rapid calls don't cause issues
TEST_F(GlobalLoggingTest, RapidFireLogging) {
    const int num_messages = 1000;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_messages; ++i) {
        aimux::info("Rapid fire message " + std::to_string(i));
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete in reasonable time (< 5 seconds for 1000 messages)
    EXPECT_LT(duration.count(), 5000);
}

// Test: Logger is created and reused
TEST_F(GlobalLoggingTest, LoggerCreationAndReuse) {
    // First call creates the logger
    aimux::info("First message");

    // Get the default logger
    auto logger1 = LoggerRegistry::get_logger("aimux_default");
    ASSERT_NE(logger1, nullptr);

    // Second call should reuse the same logger
    aimux::info("Second message");

    auto logger2 = LoggerRegistry::get_logger("aimux_default");
    ASSERT_NE(logger2, nullptr);

    // Should be the same instance
    EXPECT_EQ(logger1, logger2);
}

// Test: Different log levels work correctly
TEST_F(GlobalLoggingTest, DifferentLogLevels) {
    // Set the default logger to TRACE level to ensure all messages are processed
    auto default_logger = LoggerRegistry::get_logger("aimux_default");
    if (default_logger) {
        default_logger->set_level(LogLevel::TRACE);
    }

    ASSERT_NO_THROW({
        aimux::trace("Trace level message");
        aimux::debug("Debug level message");
        aimux::info("Info level message");
        aimux::warn("Warn level message");
        aimux::error("Error level message");
        aimux::fatal("Fatal level message");
    });
}

// Test: Long messages don't cause issues
TEST_F(GlobalLoggingTest, LongMessages) {
    std::string long_message(10000, 'x');

    ASSERT_NO_THROW({
        aimux::info("Long message: " + long_message);
    });
}

// Test: Special characters in messages
TEST_F(GlobalLoggingTest, SpecialCharactersInMessages) {
    ASSERT_NO_THROW({
        aimux::info("Message with special chars: \n\r\t\\\"\'");
        aimux::info("Unicode: 你好世界 🚀");
        aimux::info("Symbols: !@#$%^&*()_+-=[]{}|;:,.<>?/~`");
    });
}

// Test: Complex JSON data structures
TEST_F(GlobalLoggingTest, ComplexJSONData) {
    nlohmann::json complex_data = {
        {"string", "value"},
        {"number", 42},
        {"float", 3.14159},
        {"boolean", true},
        {"null_value", nullptr},
        {"array", {1, 2, 3, 4, 5}},
        {"nested", {
            {"level1", {
                {"level2", "deep value"}
            }}
        }}
    };

    ASSERT_NO_THROW({
        aimux::info("Complex data structure", complex_data);
    });
}

// Main function for running tests
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
