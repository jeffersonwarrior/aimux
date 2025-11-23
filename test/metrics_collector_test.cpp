#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <thread>
#include <chrono>
#include <memory>
#include <random>
#include "aimux/metrics/metrics_collector.hpp"
#include "aimux/metrics/time_series_db.hpp"

namespace aimux {
namespace metrics {
namespace test {

class MockTimeSeriesDB : public TimeSeriesDB {
public:
    MockTimeSeriesDB() : TimeSeriesDB(TSDBConfig{}) {
        connected_ = true;
    }

    MOCK_METHOD(bool, connect, (), (override));
    MOCK_METHOD(bool, disconnect, (), (override));
    MOCK_METHOD(bool, is_connected, (), (const, override));
    MOCK_METHOD(bool, ping, (), (override));

    MOCK_METHOD(bool, create_database, (const std::string&), (override));
    MOCK_METHOD(bool, drop_database, (const std::string&), (override));
    MOCK_METHOD(std::vector<std::string>, list_databases, (), (override));

    bool write_metrics_sync(const std::vector<MetricPoint>& metrics) override {
        stored_metrics_.insert(stored_metrics_.end(), metrics.begin(), metrics.end());
        return true;
    }

    bool write_events_sync(const std::vector<PrettificationEvent>& events) override {
        stored_events_.insert(stored_events_.end(), events.begin(), events.end());
        return true;
    }

    MOCK_METHOD(std::vector<MetricPoint>, query_metrics, (const TSDBQueryBuilder&), (override));
    MOCK_METHOD(std::vector<PrettificationEvent>, query_events, (const TSDBQueryBuilder&), (override));
    MOCK_METHOD(std::vector<MetricStatistics>, query_aggregations, (const TSDBQueryBuilder&, const std::vector<std::string>&), (override));

    MOCK_METHOD(bool, create_retention_policy, (const std::string&, const std::chrono::hours&, int, bool), (override));
    MOCK_METHOD(bool, drop_retention_policy, (const std::string&), (override));
    MOCK_METHOD(std::vector<std::string>, list_retention_policies, (), (override));

    MOCK_METHOD(bool, create_continuous_query, (const std::string&, const std::string&), (override));
    MOCK_METHOD(bool, drop_continuous_query, (const std::string&), (override));
    MOCK_METHOD(std::vector<std::string>, list_continuous_queries, (), (override));

    MOCK_METHOD(nlohmann::json, get_status, (), (const, override));
    MOCK_METHOD(double, get_query_performance_ms, (), (const, override));

    // Test access methods
    const std::vector<MetricPoint>& get_metrics() const { return stored_metrics_; }
    const std::vector<PrettificationEvent>& get_events() const { return stored_events_; }
    void clear_stored_data() {
        stored_metrics_.clear();
        stored_events_.clear();
    }

private:
    std::vector<MetricPoint> stored_metrics_;
    std::vector<PrettificationEvent> stored_events_;
};

class MetricsCollectorTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto mock_db = std::make_shared<MockTimeSeriesDB>();
        collector_ = std::make_unique<InMemoryMetricsCollector>(MetricsCollector::Config{});
        mock_db_ = mock_db;

        // Set up expectations
        EXPECT_CALL(*mock_db, connect())
            .WillRepeatedly(testing::Return(true));
        EXPECT_CALL(*mock_db, is_connected())
            .WillRepeatedly(testing::Return(true));
    }

    std::unique_ptr<InMemoryMetricsCollector> collector_;
    std::shared_ptr<MockTimeSeriesDB> mock_db_;

    PrettificationEvent create_test_event(const std::string& plugin = "test-plugin") {
        PrettificationEvent event;
        event.plugin_name = plugin;
        event.provider = "test-provider";
        event.model = "test-model";
        event.input_format = "raw";
        event.output_format = "formatted";
        event.processing_time_ms = 15.5;
        event.input_size_bytes = 1024;
        event.output_size_bytes = 950;
        event.success = true;
        event.tokens_processed = 100;
        event.capabilities_used = {"formatting", "validation"};
        event.timestamp = std::chrono::system_clock::now();
        event.metadata["test"] = "true";
        return event;
    }

    MetricPoint create_test_metric(const std::string& name, double value) {
        MetricPoint point;
        point.name = name;
        point.type = MetricType::GAUGE;
        point.value = value;
        point.timestamp = std::chrono::system_clock::now();
        point.tags["test"] = "true";
        return point;
    }
};

TEST_F(MetricsCollectorTest, BasicMetricRecording) {
    collector_->record_counter("test_counter", 1.0, {{"plugin", "test"}});
    collector_->record_gauge("test_gauge", 42.5, {{"plugin", "test"}});
    collector_->record_histogram("test_histogram", 15.7, {{"plugin", "test"}});
    collector_->record_timer("test_timer", std::chrono::milliseconds(100), {{"plugin", "test"}});

    // Force flush to capture metrics
    collector_->flush();

    auto stored_metrics = collector_->get_stored_metrics();
    EXPECT_EQ(stored_metrics.size(), 4);

    // Verify counter
    auto counter_it = std::find_if(stored_metrics.begin(), stored_metrics.end(),
        [](const MetricPoint& p) { return p.name == "test_counter"; });
    ASSERT_NE(counter_it, stored_metrics.end());
    EXPECT_EQ(counter_it->type, MetricType::COUNTER);
    EXPECT_EQ(counter_it->value, 1.0);

    // Verify gauge
    auto gauge_it = std::find_if(stored_metrics.begin(), stored_metrics.end(),
        [](const MetricPoint& p) { return p.name == "test_gauge"; });
    ASSERT_NE(gauge_it, stored_metrics.end());
    EXPECT_EQ(gauge_it->type, MetricType::GAUGE);
    EXPECT_EQ(gauge_it->value, 42.5);

    // Verify histogram
    auto histogram_it = std::find_if(stored_metrics.begin(), stored_metrics.end(),
        [](const MetricPoint& p) { return p.name == "test_histogram"; });
    ASSERT_NE(histogram_it, stored_metrics.end());
    EXPECT_EQ(histogram_it->type, MetricType::HISTOGRAM);
    EXPECT_EQ(histogram_it->value, 15.7);

    // Verify timer
    auto timer_it = std::find_if(stored_metrics.begin(), stored_metrics.end(),
        [](const MetricPoint& p) { return p.name == "test_timer"; });
    ASSERT_NE(timer_it, stored_metrics.end());
    EXPECT_EQ(timer_it->type, MetricType::TIMER);
    EXPECT_NEAR(timer_it->value, 100.0, 1.0);
}

TEST_F(MetricsCollectorTest, PrettificationEventRecording) {
    auto event = create_test_event();
    collector_->record_prettification_event(event);

    collector_->flush();

    auto stored_events = collector_->get_stored_events();
    ASSERT_EQ(stored_events.size(), 1);

    const auto& stored_event = stored_events[0];
    EXPECT_EQ(stored_event.plugin_name, event.plugin_name);
    EXPECT_EQ(stored_event.provider, event.provider);
    EXPECT_EQ(stored_event.model, event.model);
    EXPECT_EQ(stored_event.processing_time_ms, event.processing_time_ms);
    EXPECT_EQ(stored_event.input_size_bytes, event.input_size_bytes);
    EXPECT_EQ(stored_event.output_size_bytes, event.output_size_bytes);
    EXPECT_EQ(stored_event.success, event.success);
    EXPECT_EQ(stored_event.tokens_processed, event.tokens_processed);
    EXPECT_EQ(stored_event.capabilities_used, event.capabilities_used);
}

TEST_F(MetricsCollectorTest, BatchRecording) {
    std::vector<MetricPoint> metrics;
    for (int i = 0; i < 100; ++i) {
        metrics.push_back(create_test_metric("batch_test", i));
    }

    collector_->record_batch(metrics);
    collector_->flush();

    auto stored_metrics = collector_->get_stored_metrics();
    EXPECT_EQ(stored_metrics.size(), 100);

    // Verify all values are present
    std::vector<double> values;
    for (const auto& metric : stored_metrics) {
        if (metric.name == "batch_test") {
            values.push_back(metric.value);
        }
    }
    EXPECT_EQ(values.size(), 100);

    std::sort(values.begin(), values.end());
    for (int i = 0; i < 100; ++i) {
        EXPECT_EQ(values[i], i);
    }
}

TEST_F(MetricsCollectorTest, RealTimeStatistics) {
    // Generate test data
    std::vector<double> values;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<> dist(50.0, 10.0);

    for (int i = 0; i < 1000; ++i) {
        double value = dist(gen);
        values.push_back(value);
        collector_->record_histogram("realtime_test", value);
    }

    // Get real-time statistics
    auto stats = collector_->get_real_time_stats({"realtime_test"});
    ASSERT_EQ(stats.size(), 1);

    const auto& stat = stats[0];
    EXPECT_EQ(stat.name, "realtime_test");
    EXPECT_EQ(stat.type, MetricType::HISTOGRAM);
    EXPECT_EQ(stat.count, 1000);

    // Verify mean is close to expected
    double expected_mean = 50.0;
    EXPECT_NEAR(stat.mean, expected_mean, 2.0);

    // Verify percentiles are reasonable
    EXPECT_GT(stat.p95, stat.median);
    EXPECT_GT(stat.p99, stat.p95);
    EXPECT_GT(stat.median, stat.min);
    EXPECT_GT(stat.max, stat.p99);
}

TEST_F(MetricsCollectorTest, SamplingRate) {
    MetricsCollector::Config config;
    config.sampling_rate = 0.1; // 10% sampling
    auto sampling_collector = std::make_unique<InMemoryMetricsCollector>(config);

    // Record many metrics
    for (int i = 0; i < 1000; ++i) {
        sampling_collector->record_counter("sampled_metric", 1.0);
    }

    sampling_collector->flush();

    auto stored_metrics = sampling_collector->get_stored_metrics();

    // Should have approximately 10% of the metrics (with some tolerance)
    EXPECT_GT(stored_metrics.size(), 50);
    EXPECT_LT(stored_metrics.size(), 150);
}

TEST_F(MetricsCollectorTest, StressTestHighThroughput) {
    const int num_threads = 10;
    const int metrics_per_thread = 10000;
    std::vector<std::thread> threads;

    auto start_time = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([this, i, metrics_per_thread]() {
            for (int j = 0; j < metrics_per_thread; ++j) {
                collector_->record_timer("stress_test",
                    std::chrono::nanoseconds(j * 1000),
                    {{"thread", std::to_string(i)}});
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    collector_->flush();
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    auto stored_metrics = collector_->get_stored_metrics();
    EXPECT_EQ(stored_metrics.size(), num_threads * metrics_per_thread);

    // Verify performance requirement: should finish within reasonable time
    EXPECT_LT(duration.count(), 5000); // Less than 5 seconds for 100k metrics

    // Verify all thread values are present
    std::unordered_set<int> thread_ids;
    for (const auto& metric : stored_metrics) {
        if (metric.name == "stress_test") {
            auto thread_it = metric.tags.find("thread");
            if (thread_it != metric.tags.end()) {
                thread_ids.insert(std::stoi(thread_it->second));
            }
        }
    }
    EXPECT_EQ(thread_ids.size(), num_threads);
}

TEST_F(MetricsCollectorTest, MemoryUsage) {
    // Get initial memory baseline
    collector_->clear_stored_data();
    size_t initial_count = collector_->get_stored_metrics().size();

    // Add a large number of metrics
    const int large_count = 50000;
    for (int i = 0; i < large_count; ++i) {
        collector_->record_gauge("memory_test", static_cast<double>(i),
            {{"index", std::to_string(i)}});
    }

    collector_->flush();
    auto stored_metrics = collector_->get_stored_metrics();
    EXPECT_EQ(stored_metrics.size(), large_count);

    // Clear and verify memory is freed
    collector_->clear_stored_data();
    auto final_count = collector_->get_stored_metrics().size();
    EXPECT_EQ(final_count, 0);
}

TEST_F(MetricsCollectorTest, ConcurrencySafety) {
    const int num_threads = 20;
    const int operations_per_thread = 1000;
    std::vector<std::thread> threads;

    // Mix of different operations
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([this, i, operations_per_thread]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> op_dist(0, 4);

            for (int j = 0; j < operations_per_thread; ++j) {
                int operation = op_dist(gen);
                switch (operation) {
                    case 0:
                        collector_->record_counter("concurrent_counter", 1.0);
                        break;
                    case 1:
                        collector_->record_gauge("concurrent_gauge", j);
                        break;
                    case 2:
                        collector_->record_histogram("concurrent_histogram", j * 0.1);
                        break;
                    case 3:
                        collector_->record_timer("concurrent_timer",
                            std::chrono::microseconds(j));
                        break;
                    case 4:
                        collector_->record_prettification_event(create_test_event());
                        break;
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    collector_->flush();

    // Should have recorded all metrics without corruption or crashes
    auto metrics = collector_->get_stored_metrics();
    auto events = collector_->get_stored_events();

    EXPECT_GT(metrics.size(), 0);
    EXPECT_GT(events.size(), 0);

    // Verify no duplicate timestamps for the same metric (within tolerance)
    std::map<std::string, std::vector<std::chrono::system_clock::time_point>> metric_timestamps;
    for (const auto& metric : metrics) {
        metric_timestamps[metric.name].push_back(metric.timestamp);
    }

    for (const auto& [name, timestamps] : metric_timestamps) {
        if (timestamps.size() > 1) {
            // Check that timestamps are reasonable (not all exactly the same)
            auto min_time = *std::min_element(timestamps.begin(), timestamps.end());
            auto max_time = *std::max_element(timestamps.begin(), timestamps.end());
            EXPECT_GT(max_time - min_time, std::chrono::milliseconds(0));
        }
    }
}

TEST_F(MetricsCollectorTest, StatusAndConfiguration) {
    auto status = collector_->get_status();
    EXPECT_TRUE(status.contains("collecting"));
    EXPECT_TRUE(status.contains("buffer_size"));
    EXPECT_TRUE(status.contains("sampling_rate"));
    EXPECT_TRUE(status.contains("metrics_buffer_size"));
    EXPECT_TRUE(status.contains("real_time_metrics_count"));

    // Test configuration update
    MetricsCollector::Config new_config;
    new_config.sampling_rate = 0.5;
    new_config.buffer_size = 5000;
    new_config.flush_interval = std::chrono::milliseconds(50);

    collector_->update_config(new_config);

    auto updated_status = collector_->get_status();
    EXPECT_EQ(updated_status["sampling_rate"], 0.5);
}

class TimeSeriesDBTest : public ::testing::Test {
protected:
    void SetUp() override {
        tsdb_.reset(new MockTimeSeriesDB());
    }

    std::unique_ptr<MockTimeSeriesDB> tsdb_;
};

TEST_F(TimeSeriesDBTest, BasicConnection) {
    EXPECT_CALL(*tsdb_, connect())
        .WillOnce(testing::Return(true));
    EXPECT_CALL(*tsdb_, is_connected())
        .WillRepeatedly(testing::Return(true));

    EXPECT_TRUE(tsdb_->connect());
    EXPECT_TRUE(tsdb_->is_connected());
}

TEST_F(TimeSeriesDBTest, MetricOperations) {
    std::vector<MetricPoint> metrics;
    for (int i = 0; i < 10; ++i) {
        MetricPoint point;
        point.name = "test_metric";
        point.type = MetricType::GAUGE;
        point.value = i * 1.5;
        point.timestamp = std::chrono::system_clock::now();
        point.tags["index"] = std::to_string(i);
        metrics.push_back(point);
    }

    EXPECT_TRUE(tsdb_->write_metrics(metrics));

    auto stored_metrics = tsdb_->get_metrics();
    EXPECT_EQ(stored_metrics.size(), 10);

    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(stored_metrics[i].name, "test_metric");
        EXPECT_EQ(stored_metrics[i].value, i * 1.5);
        EXPECT_EQ(stored_metrics[i].tags["index"], std::to_string(i));
    }
}

TEST_F(TimeSeriesDBTest, AsyncWriteOperations) {
    std::vector<MetricPoint> metrics;
    for (int i = 0; i < 100; ++i) {
        MetricPoint point;
        point.name = "async_test";
        point.type = MetricType::COUNTER;
        point.value = 1.0;
        point.timestamp = std::chrono::system_clock::now();
        metrics.push_back(point);
    }

    // Test async write
    tsdb_->write_metrics_async(metrics);

    // Allow some time for async processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto stored_metrics = tsdb_->get_metrics();
    EXPECT_EQ(stored_metrics.size(), 100);
}

class TSDBQueryBuilderTest : public ::testing::Test {
protected:
    TSDBQueryBuilder builder_{"test_measurement"};
};

TEST_F(TSDBQueryBuilderTest, BasicQuery) {
    std::string query = builder_.build_query();
    EXPECT_EQ(query, "SELECT * FROM test_measurement");
}

TEST_F(TSDBQueryBuilderTest, QueryWithTimeRange) {
    auto start = std::chrono::system_clock::now();
    auto end = start + std::chrono::hours(1);

    builder_.time_range(start, end);
    std::string query = builder_.build_query();

    EXPECT_TRUE(query.find("SELECT * FROM test_measurement") == 0);
    EXPECT_TRUE(query.find("WHERE time >= ") != std::string::npos);
    EXPECT_TRUE(query.find("AND time <= ") != std::string::npos);
}

TEST_F(TSDBQueryBuilderTest, QueryWithTags) {
    builder_.tag("plugin", "test-plugin");
    builder_.tag("provider", "test-provider");

    std::string query = builder_.build_query();
    EXPECT_TRUE(query.find("WHERE \"plugin\" = 'test-plugin'") != std::string::npos);
    EXPECT_TRUE(query.find("AND \"provider\" = 'test-provider'") != std::string::npos);
}

TEST_F(TSDBQueryBuilderTest, QueryWithFieldsAndGroupBy) {
    builder_.field("value");
    builder_.field("timestamp");
    builder_.fields({"field1", "field2"});
    builder_.group_by({"plugin", "provider"});

    std::string query = builder_.build_query();
    EXPECT_TRUE(query.find("SELECT value, timestamp, field1, field2") != std::string::npos);
    EXPECT_TRUE(query.find("GROUP BY \"plugin\", \"provider\"") != std::string::npos);
}

} // namespace test
} // namespace metrics
} // namespace aimux