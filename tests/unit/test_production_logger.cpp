/**
 * Production Logger Test Suite
 * Tests the enhanced logging system with correlation IDs and structured logging
 * Target: >95% code coverage for ProductionLogger and related classes
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <thread>
#include <chrono>
#include <fstream>
#include <sstream>
#include <future>
#include <filesystem>
#include <regex>

#include "aimux/logging/production_logger.h"
#include "nlohmann/json.hpp"

using namespace aimux::logging;
using namespace ::testing;

// Test fixture for Production Logger tests
class ProductionLoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set up unique log directory for each test
        test_log_dir_ = std::filesystem::temp_directory_path() / "aimux_test_logs" /
                       "test_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        std::filesystem::create_directories(test_log_dir_);

        // Configure logger for testing
        ProductionLogger::Config config;
        config.level = LogLevel::DEBUG;
        config.enableConsoleLogging = false; // Disable console for clean test output
        config.enableFileLogging = true;
        config.logFile = (test_log_dir_ / "test.log").string();
        config.maxFileSize = 1024 * 1024; // 1MB
        config.maxFileCount = 5;
        config.jsonConsole = true;
        config.filterSensitiveData = true;
        config.async = false; // Synchronous for predictable test behavior

        ProductionLogger::getInstance().configure(config);
        ProductionLogger::getInstance().flush();
    }

    void TearDown() override {
        ProductionLogger::getInstance().shutdown();

        // Clean up test files
        if (std::filesystem::exists(test_log_dir_)) {
            std::filesystem::remove_all(test_log_dir_);
        }
    }

    std::string readLogFile() {
        std::ifstream file((test_log_dir_ / "test.log").string());
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    std::vector<nlohmann::json> parseLogFileEntries() {
        std::string content = readLogFile();
        std::vector<nlohmann::json> entries;
        std::istringstream stream(content);
        std::string line;

        while (std::getline(stream, line)) {
            if (!line.empty()) {
                try {
                    entries.push_back(nlohmann::json::parse(line));
                } catch (const nlohmann::json::parse_error&) {
                    // Skip invalid JSON lines
                }
            }
        }
        return entries;
    }

    std::filesystem::path test_log_dir_;
};

// Basic functionality tests
TEST_F(ProductionLoggerTest, BasicLogging) {
    Logger logger("test_logger");
    logger.info("Test message");

    auto entries = parseLogFileEntries();
    ASSERT_GE(entries.size(), 1);

    auto entry = entries.back();
    EXPECT_EQ(entry["logger_name"], "test_logger");
    EXPECT_EQ(entry["level"], "INFO");
    EXPECT_EQ(entry["message"], "Test message");
    EXPECT_TRUE(entry.contains("@timestamp"));
    EXPECT_TRUE(entry.contains("correlation_id"));
    EXPECT_TRUE(entry.contains("service"));
}

TEST_F(ProductionLoggerTest, LogLevelFiltering) {
    ProductionLogger::getInstance().setLevel(LogLevel::WARN);

    Logger logger("test_logger");
    logger.debug("Debug message"); // Should be filtered
    logger.info("Info message");   // Should be filtered
    logger.warn("Warn message");   // Should appear
    logger.error("Error message"); // Should appear

    auto entries = parseLogFileEntries();
    // Should only have WARN and ERROR messages
    EXPECT_EQ(entries.size(), 2);

    EXPECT_EQ(entries[0]["level"], "WARN");
    EXPECT_EQ(entries[1]["level"], "ERROR");
}

// Correlation ID tests
TEST_F(ProductionLoggerTest, CorrelationIdGeneration) {
    Logger logger("test_logger");

    std::string correlation_id1 = Logger::generateCorrelationId();
    std::string correlation_id2 = Logger::generateCorrelationId();

    EXPECT_NE(correlation_id1, correlation_id2);
    EXPECT_FALSE(correlation_id1.empty());
    EXPECT_FALSE(correlation_id2.empty());

    // Check format (should contain hex and timestamp)
    EXPECT_TRUE(correlation_id1.find("-") != std::string::npos);
    EXPECT_TRUE(correlation_id2.find("-") != std::string::npos);
}

TEST_F(ProductionLoggerTest, CorrelationIdPropagation) {
    std::string correlation_id = "test-correlation-123";
    Logger logger("test_logger");

    logger.info_with_correlation("Test message", correlation_id);

    auto entries = parseLogFileEntries();
    ASSERT_GE(entries.size(), 1);

    auto entry = entries.back();
    EXPECT_EQ(entry["correlation_id"], correlation_id);
}

TEST_F(ProductionLoggerTest, DefaultCorrelationIdGeneration) {
    Logger logger("test_logger");
    logger.info("Test message with auto-generated correlation ID");

    auto entries = parseLogFileEntries();
    ASSERT_GE(entries.size(), 1);

    auto entry = entries.back();
    EXPECT_TRUE(entry.contains("correlation_id"));
    EXPECT_FALSE(entry["correlation_id"].get<std::string>().empty());
}

// Structured data tests
TEST_F(ProductionLoggerTest, StructuredDataLogging) {
    Logger logger("test_logger");

    nlohmann::json structured_data = {
        {"user_id", "12345"},
        {"request_id", "req-abc-123"},
        {"response_time_ms", 250},
        {"cache_hit", true}
    };

    logger.info("Request completed", structured_data);

    auto entries = parseLogFileEntries();
    ASSERT_GE(entries.size(), 1);

    auto entry = entries.back();
    EXPECT_TRUE(entry.contains("structured_data"));
    EXPECT_EQ(entry["structured_data"]["user_id"], "12345");
    EXPECT_EQ(entry["structured_data"]["request_id"], "req-abc-123");
    EXPECT_EQ(entry["structured_data"]["response_time_ms"], 250);
    EXPECT_TRUE(entry["structured_data"]["cache_hit"]);
}

// Security filtering tests
TEST_F(ProductionLoggerTest, SensitiveDataFiltering) {
    Logger logger("test_logger");

    nlohmann::json sensitive_data = {
        {"api_key", "secret-api-key-12345"},
        {"password", "user-password"},
        {"token", "auth-token-xyz"},
        {"safe_data", "this is safe"},
        {"user_secret", "confidential-info"}
    };

    logger.info("Request with sensitive data", sensitive_data);

    auto entries = parseLogFileEntries();
    ASSERT_GE(entries.size(), 1);

    auto entry = entries.back();
    EXPECT_TRUE(entry.contains("structured_data"));

    auto structured = entry["structured_data"];
    EXPECT_EQ(structured["api_key"], "[REDACTED]");
    EXPECT_EQ(structured["password"], "[REDACTED]");
    EXPECT_EQ(structured["token"], "[REDACTED]");
    EXPECT_EQ(structured["user_secret"], "[REDACTED]");
    EXPECT_EQ(structured["safe_data"], "this is safe"); // Should not be redacted
}

// JSON structure tests
TEST_F(ProductionLoggerTest, JsonStructureValidation) {
    Logger logger("test_logger");
    logger.info("Structured message test");

    auto entries = parseLogFileEntries();
    ASSERT_GE(entries.size(), 1);

    auto entry = entries.back();

    // Required fields
    EXPECT_TRUE(entry.contains("@timestamp"));
    EXPECT_TRUE(entry.contains("level"));
    EXPECT_TRUE(entry.contains("logger_name"));
    EXPECT_TRUE(entry.contains("message"));
    EXPECT_TRUE(entry.contains("correlation_id"));
    EXPECT_TRUE(entry.contains("thread_id"));
    EXPECT_TRUE(entry.contains("source"));
    EXPECT_TRUE(entry.contains("service"));

    // Check source structure
    auto source = entry["source"];
    EXPECT_TRUE(source.contains("file"));
    EXPECT_TRUE(source.contains("line"));
    EXPECT_TRUE(source.contains("function"));

    // Check service structure
    auto service = entry["service"];
    EXPECT_EQ(service["name"], "aimux2");
    EXPECT_EQ(service["version"], "2.0.0");
}

// Thread safety tests
TEST_F(ProductionLoggerTest, ConcurrentLogging) {
    Logger logger1("logger1");
    Logger logger2("logger2");
    const int num_threads = 10;
    const int messages_per_thread = 50;

    std::vector<std::future<void>> futures;

    // Launch multiple threads logging concurrently
    for (int i = 0; i < num_threads; ++i) {
        futures.push_back(std::async(std::launch::async, [&, i]() {
            for (int j = 0; j < messages_per_thread; ++j) {
                if (i % 2 == 0) {
                    logger1.info("Message " + std::to_string(i) + "-" + std::to_string(j));
                } else {
                    logger2.debug("Debug " + std::to_string(i) + "-" + std::to_string(j));
                }
            }
        }));
    }

    // Wait for all threads to complete
    for (auto& future : futures) {
        future.get();
    }

    ProductionLogger::getInstance().flush();

    auto entries = parseLogFileEntries();
    EXPECT_EQ(entries.size(), num_threads * messages_per_thread);

    // Verify both logger names appear
    std::set<std::string> logger_names;
    for (const auto& entry : entries) {
        logger_names.insert(entry["logger_name"]);
    }
    EXPECT_EQ(logger_names.size(), 2);
    EXPECT_TRUE(logger_names.contains("logger1"));
    EXPECT_TRUE(logger_names.contains("logger2"));
}

// Log level tests
TEST_F(ProductionLoggerTest, AllLogLevels) {
    Logger logger("test_logger");

    logger.trace("Trace message");
    logger.debug("Debug message");
    logger.info("Info message");
    logger.warn("Warning message");
    logger.error("Error message");
    logger.fatal("Fatal message");

    ProductionLogger::getInstance().flush();

    auto entries = parseLogFileEntries();
    ASSERT_EQ(entries.size(), 6);

    // Verify correct log levels
    EXPECT_EQ(entries[0]["level"], "TRACE");
    EXPECT_EQ(entries[1]["level"], "DEBUG");
    EXPECT_EQ(entries[2]["level"], "INFO");
    EXPECT_EQ(entries[3]["level"], "WARN");
    EXPECT_EQ(entries[4]["level"], "ERROR");
    EXPECT_EQ(entries[5]["level"], "FATAL");
}

// Timestamp tests
TEST_F(ProductionLoggerTest, TimestampAccuracy) {
    Logger logger("test_logger");

    auto before = std::chrono::system_clock::now();
    logger.info("Timestamp test");
    auto after = std::chrono::system_clock::now();

    auto entries = parseLogFileEntries();
    ASSERT_GE(entries.size(), 1);

    auto entry = entries.back();

    // Convert timestamp back to time_point
    auto timestamp_ms = entry["@timestamp"].get<int64_t>();
    auto log_time = std::chrono::system_clock::time_point{
        std::chrono::milliseconds(timestamp_ms)
    };

    // Verify timestamp is within reasonable range
    EXPECT_GE(log_time, before);
    EXPECT_LE(log_time, after);
}

// Configuration tests
TEST_F(ProductionLoggerTest, ConfigurationValidation) {
    ProductionLogger::Config config;

    // Test default values
    EXPECT_EQ(config.level, LogLevel::INFO);
    EXPECT_TRUE(config.enableConsoleLogging);
    EXPECT_FALSE(config.enableFileLogging);
    EXPECT_EQ(config.logFile, "/var/log/aimux/aimux.log");
    EXPECT_TRUE(config.filterSensitiveData);
    EXPECT_FALSE(config.async);

    // Test JSON serialization
    auto json = config.to_json();
    EXPECT_TRUE(json.contains("level"));
    EXPECT_TRUE(json.contains("enableConsoleLogging"));
    EXPECT_TRUE(json.contains("enableFileLogging"));

    // Test configuration from JSON
    auto loaded_config = ProductionLogger::Config::from_json(json);
    EXPECT_EQ(loaded_config.level, config.level);
    EXPECT_EQ(loaded_config.enableConsoleLogging, config.enableConsoleLogging);
}

// Error handling tests
TEST_F(ProductionLoggerTest, InvalidConfigurationHandling) {
    ProductionLogger::Config config;
    config.logFile = "/invalid/path/that/does/not/exist/test.log";

    // Should handle invalid file path gracefully
    EXPECT_THROW({
        ProductionLogger::getInstance().configure(config);
    }, std::runtime_error);
}

// Performance tests
TEST_F(ProductionLoggerTest, LoggingPerformance) {
    Logger logger("performance_test");
    const int num_messages = 1000;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_messages; ++i) {
        logger.info("Performance test message " + std::to_string(i));
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Should complete within reasonable time (<100 microseconds per message)
    EXPECT_LT(duration.count(), num_messages * 100);
}

TEST_F(ProductionLoggerTest, StructuredLoggingPerformance) {
    Logger logger("performance_test");
    nlohmann::json data = {
        {"user_id", "12345"},
        {"request_id", "req-abc"},
        {"response_time_ms", 250},
        {"cache_hit", true}
    };

    const int num_messages = 1000;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_messages; ++i) {
        logger.info("Performance test", data);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Structured logging should still be reasonably fast
    EXPECT_LT(duration.count(), num_messages * 200);
}

// Log rotation tests
TEST_F(ProductionLoggerTest, LogRotation) {
    ProductionLogger::Config small_config;
    small_config.level = LogLevel::DEBUG;
    small_config.enableConsoleLogging = false;
    small_config.enableFileLogging = true;
    small_config.logFile = (test_log_dir_ / "small.log").string();
    small_config.maxFileSize = 1024; // Very small for testing
    small_config.maxFileCount = 3;

    ProductionLogger::getInstance().configure(small_config);

    Logger logger("rotation_test");

    // Generate enough logs to trigger rotation
    std::string large_message(500, 'x');
    for (int i = 0; i < 10; ++i) {
        logger.info("Rotate test message " + std::to_string(i) + " " + large_message);
    }

    ProductionLogger::getInstance().flush();

    // Check that log files exist
    EXPECT_TRUE(std::filesystem::exists(test_log_dir_ / "small.log"));

    // The exact behavior depends on implementation, but main log should exist
    std::ifstream main_file((test_log_dir_ / "small.log").string());
    EXPECT_TRUE(main_file.good());
}

// Utility tests
TEST_F(ProductionLoggerTest, LogLevelStringConversion) {
    EXPECT_EQ(LogEntry::levelToString(LogLevel::TRACE), "TRACE");
    EXPECT_EQ(LogEntry::levelToString(LogLevel::DEBUG), "DEBUG");
    EXPECT_EQ(LogEntry::levelToString(LogLevel::INFO), "INFO");
    EXPECT_EQ(LogEntry::levelToString(LogLevel::WARN), "WARN");
    EXPECT_EQ(LogEntry::levelToString(LogLevel::ERROR), "ERROR");
    EXPECT_EQ(LogEntry::levelToString(LogLevel::FATAL), "FATAL");

    EXPECT_EQ(LogEntry::stringToLevel("TRACE"), LogLevel::TRACE);
    EXPECT_EQ(LogEntry::stringToLevel("DEBUG"), LogLevel::DEBUG);
    EXPECT_EQ(LogEntry::stringToLevel("INFO"), LogLevel::INFO);
    EXPECT_EQ(LogEntry::stringToLevel("WARN"), LogLevel::WARN);
    EXPECT_EQ(LogEntry::stringToLevel("ERROR"), LogLevel::ERROR);
    EXPECT_EQ(LogEntry::stringToLevel("FATAL"), LogLevel::FATAL);

    // Default to INFO for unknown levels
    EXPECT_EQ(LogEntry::stringToLevel("UNKNOWN"), LogLevel::INFO);
}

// Singleton tests
TEST_F(ProductionLoggerTest, SingletonBehavior) {
    auto& logger1 = ProductionLogger::getInstance();
    auto& logger2 = ProductionLogger::getInstance();

    EXPECT_EQ(&logger1, &logger2);
}

// Edge case tests
TEST_F(ProductionLoggerTest, EmptyMessageHandling) {
    Logger logger("test_logger");

    logger.info(""); // Empty message
    logger.debug(""); // Empty message with different level

    ProductionLogger::getInstance().flush();

    auto entries = parseLogFileEntries();
    ASSERT_EQ(entries.size(), 2);

    EXPECT_EQ(entries[0]["message"], "");
    EXPECT_EQ(entries[1]["message"], "");
}

TEST_F(ProductionLoggerTest, SpecialCharacterHandling) {
    Logger logger("test_logger");

    std::string special_message = "Special chars: äöü ñ 中文 🚀 emojis\n\ttabs and newlines";
    logger.info(special_message);

    ProductionLogger::getInstance().flush();

    auto entries = parseLogFileEntries();
    ASSERT_GE(entries.size(), 1);

    // Message should be preserved correctly in JSON
    EXPECT_EQ(entries[0]["message"], special_message);
}