#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <chrono>
#include <thread>
#include <memory>
#include <future>

#include "aimux/prettifier/streaming_processor.hpp"
#include "aimux/prettifier/synthetic_formatter.hpp"

using namespace aimux::prettifier;
using ::testing::Gt;
using ::testing::Lt;

class StreamingProcessorTest : public ::testing::Test {
protected:
    void SetUp() override {
        processor_ = std::make_shared<StreamingProcessor>();
        formatter_ = std::make_shared<SyntheticFormatter>();

        test_context_.provider_name = "test";
        test_context_.model_name = "test-model";
        test_context_.original_format = "json";
        test_context_.processing_start = std::chrono::system_clock::now();

        formatter_->configure({
            {"simulation_mode", "synthetic"},
            {"enable_detailed_logging", false} // Reduce log noise in tests
        });
    }

    std::shared_ptr<StreamingProcessor> processor_;
    std::shared_ptr<SyntheticFormatter> formatter_;
    ProcessingContext test_context_;
};

TEST_F(StreamingProcessorTest, BasicFunctionality_StreamLifecycle) {
    // Create stream
    auto stream_id = processor_->create_stream(test_context_, formatter_);
    EXPECT_FALSE(stream_id.empty());
    EXPECT_TRUE(processor_->is_stream_active(stream_id));

    // Process chunks
    auto future1 = processor_->process_chunk(stream_id, R"({"delta":{"content":"Chunk 1"}})");
    auto future2 = processor_->process_chunk(stream_id, R"({"delta":{"content":"Chunk 2"}})", true);

    EXPECT_TRUE(future1.get());
    EXPECT_TRUE(future2.get());

    // Get result
    auto result = processor_->get_result(stream_id);
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.streaming_mode);
    EXPECT_FALSE(result.processed_content.empty());

    // Stream should be completed
    EXPECT_FALSE(processor_->is_stream_active(stream_id));
}

TEST_F(StreamingProcessorTest, Performance_ThroughputTest) {
    const int num_chunks = 100;
    const int chunk_size = 1024;
    std::string test_chunk(chunk_size, 'x');

    auto stream_id = processor_->create_stream(test_context_, formatter_);

    auto start_time = std::chrono::high_resolution_clock::now();

    std::vector<std::future<bool>> futures;
    for (int i = 0; i < num_chunks; ++i) {
        bool is_final = (i == num_chunks - 1);
        auto future = processor_->process_chunk(stream_id, test_chunk, is_final);
        futures.push_back(std::move(future));
    }

    // Wait for all chunks
    for (auto& future : futures) {
        EXPECT_TRUE(future.get());
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto total_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();

    auto stats = processor_->get_statistics();
    EXPECT_EQ(stats.total_chunks_processed, num_chunks);
    EXPECT_GT(stats.average_chunks_per_second, 100.0); // Should handle >100 chunks/second
    EXPECT_LT(total_time_ms, 5000); // Should complete in <5 seconds
}

TEST_F(StreamingProcessorTest, ConcurrentProcessing_MultipleStreams) {
    const int num_streams = 10;
    const int chunks_per_stream = 20;

    std::vector<std::string> stream_ids;
    std::vector<std::future<ProcessingResult>> results;

    // Create multiple streams
    for (int i = 0; i < num_streams; ++i) {
        test_context_.model_name = "test-model-" + std::to_string(i);
        auto stream_id = processor_->create_stream(test_context_, formatter_);
        stream_ids.push_back(stream_id);

        // Process chunks asynchronously
        results.push_back(std::async(std::launch::async, [&]() {
            for (int j = 0; j < chunks_per_stream; ++j) {
                bool is_final = (j == chunks_per_stream - 1);
                std::string chunk = R"({"delta":{"content":"Stream )" + std::to_string(i) + " chunk " + std::to_string(j) + "\"}})";
                auto future = processor_->process_chunk(stream_id, chunk, is_final);
                EXPECT_TRUE(future.get());
            }
            return processor_->get_result(stream_id);
        }));
    }

    // Wait for all streams to complete
    for (auto& result : results) {
        auto stream_result = result.get();
        EXPECT_TRUE(stream_result.success);
        EXPECT_TRUE(stream_result.streaming_mode);
    }

    auto stats = processor_->get_statistics();
    EXPECT_EQ(stats.total_chunks_processed, num_streams * chunks_per_stream);
    EXPECT_GT(stats.completed_streams, 0);
}

TEST_F(StreamingProcessorTest, MemoryEfficiency_LargeResponseHandling) {
    // Test with large chunks to ensure memory efficiency
    const int large_chunk_size = 100 * 1024; // 100KB chunks
    std::string large_chunk(large_chunk_size, 'x');

    processor_->configure({
        {"buffer_size_mb", 8}, // Small buffer to test efficiency
        {"enable_compression", true}
    });

    auto stream_id = processor_->create_stream(test_context_, formatter_);
    auto initial_memory = processor_->get_statistics().current_memory_usage;

    // Process multiple large chunks
    for (int i = 0; i < 10; ++i) {
        bool is_final = (i == 9);
        auto future = processor_->process_chunk(stream_id, large_chunk, is_final);
        EXPECT_TRUE(future.get());
    }

    auto result = processor_->get_result(stream_id);
    EXPECT_TRUE(result.success);

    auto final_memory = processor_->get_statistics().current_memory_usage;
    auto memory_increase = final_memory - initial_memory;

    // Memory usage should be reasonable (not just accumulating all data)
    EXPECT_LT(memory_increase, 50 * 1024 * 1024); // Should use less than 50MB
}

TEST_F(StreamingProcessorTest, ErrorHandling_InvalidStreamId) {
    // Test operations on non-existent stream
    auto future = processor_->process_chunk("invalid_stream_id", "test chunk");
    EXPECT_FALSE(future.get());

    auto result = processor_->get_result("invalid_stream_id");
    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.error_message.find("Stream not found") != std::string::npos);

    EXPECT_FALSE(processor_->cancel_stream("invalid_stream_id"));
    EXPECT_FALSE(processor_->is_stream_active("invalid_stream_id"));
}

TEST_F(StreamingProcessorTest, ErrorHandling_StreamTimeout) {
    // Configure short timeout for testing
    processor_->configure({
        {"stream_timeout_ms", 100}, // 100ms timeout
        {"chunk_timeout_ms", 50}
    });

    auto stream_id = processor_->create_stream(test_context_, formatter_);

    // Process a chunk normally
    auto future1 = processor_->process_chunk(stream_id, "normal chunk", false);
    EXPECT_TRUE(future1.get());

    // Wait for timeout
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    // Try to get result - should fail due to timeout
    auto result = processor_->get_result(stream_id);
    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.error_message.empty() || result.error_message.find("timeout") != std::string::npos);
}

TEST_F(StreamingProcessorTest, Configuration_CustomSettings) {
    nlohmann::json config = {
        {"thread_pool_size", 2},
        {"buffer_size_mb", 32},
        {"backpressure_threshold", 500},
        {"max_concurrent_streams", 100},
        {"chunk_timeout_ms", 2000},
        {"enable_metrics", true}
    };

    EXPECT_TRUE(processor_->configure(config));

    auto applied_config = processor_->get_configuration();
    EXPECT_EQ(applied_config["thread_pool_size"], 2);
    EXPECT_EQ(applied_config["buffer_size_mb"], 32);
    EXPECT_EQ(applied_config["backpressure_threshold"], 500);
    EXPECT_EQ(applied_config["max_concurrent_streams"], 100);
}

TEST_F(StreamingProcessorTest, Optimization_PerformanceModes) {
    // Test throughput optimization
    processor_->optimize_for_throughput();
    auto throughput_config = processor_->get_configuration();
    EXPECT_GE(throughput_config["thread_pool_size"], 4); // Should increase threads
    EXPECT_FALSE(throughput_config["enable_compression"]); // Should disable compression

    // Test latency optimization
    processor_->optimize_for_latency();
    auto latency_config = processor_->get_configuration();
    EXPECT_LE(latency_config["buffer_size_mb"], 16); // Should reduce buffer size
    EXPECT_LE(latency_config["backpressure_threshold"], 500); // Should reduce threshold

    // Test memory optimization
    processor_->optimize_for_memory();
    auto memory_config = processor_->get_configuration();
    EXPECT_EQ(memory_config["thread_pool_size"], 2); // Should minimize threads
    EXPECT_TRUE(memory_config["enable_compression"]); // Should enable compression
}

TEST_F(StreamingProcessorTest, Backpressure_Management) {
    // Configure low backpressure threshold
    processor_->configure({
        {"backpressure_threshold", 5} // Very low threshold
    });

    auto stream_id = processor_->create_stream(test_context_, formatter_);

    // Process chunks up to threshold
    for (int i = 0; i < 5; ++i) {
        std::string chunk = R"({"delta":{"content":"Chunk )" + std::to_string(i) + "\"}})";
        auto future = processor_->process_chunk(stream_id, chunk, false);
        EXPECT_TRUE(future.get());
    }

    // Next chunk should trigger backpressure
    std::string overflow_chunk = R"({"delta":{"content":"Overflow chunk"}})";
    auto future = processor_->process_chunk(stream_id, overflow_chunk, false);
    EXPECT_FALSE(future.get()); // Should fail due to backpressure

    // Verify backpressure was triggered
    auto stats = processor_->get_statistics();
    EXPECT_GT(stats.backpressure_events, 0);
}

TEST_F(StreamingProcessorTest, HealthCheck_ComprehensiveValidation) {
    // Process some activity to generate statistics
    auto stream_id = processor_->create_stream(test_context_, formatter_);
    processor_->process_chunk(stream_id, "test chunk", true);
    processor_->get_result(stream_id);

    auto health = processor_->health_check();

    EXPECT_EQ(health["status"], "healthy");
    EXPECT_TRUE(health["thread_pool_responsive"]);
    EXPECT_TRUE(health["memory_within_limits"]);
    EXPECT_TRUE(health["acceptable_success_rate"]);
    EXPECT_TRUE(health["overall_healthy"]);

    EXPECT_TRUE(health.contains("performance_metrics"));
    EXPECT_TRUE(health["performance_metrics"].contains("average_chunks_per_second"));
    EXPECT_TRUE(health["performance_metrics"].contains("success_rate"));
}

TEST_F(StreamingProcessorTest, Statistics_TrackingAndReset) {
    // Reset initial statistics
    processor_->reset_statistics();

    auto stream_id1 = processor_->create_stream(test_context_, formatter_);

    // Process some chunks
    processor_->process_chunk(stream_id1, "chunk1", false);
    processor_->process_chunk(stream_id1, "chunk2", false);
    processor_->process_chunk(stream_id1, "chunk3", true);

    processor_->get_result(stream_id1);

    // Create another stream but cancel it
    auto stream_id2 = processor_->create_stream(test_context_, formatter_);
    processor_->cancel_stream(stream_id2);

    auto stats = processor_->get_statistics();
    EXPECT_GT(stats.total_streams, 0);
    EXPECT_GT(stats.completed_streams, 0);
    EXPECT_GT(stats.failed_streams, 0);
    EXPECT_GT(stats.total_chunks_processed, 0);
    EXPECT_GT(stats.total_bytes_processed, 0);

    // Reset statistics
    processor_->reset_statistics();
    auto reset_stats = processor_->get_statistics();
    EXPECT_EQ(reset_stats.total_streams, 0);
    EXPECT_EQ(reset_stats.completed_streams, 0);
    EXPECT_EQ(reset_stats.failed_streams, 0);
    EXPECT_EQ(reset_stats.total_chunks_processed, 0);
}

TEST_F(StreamingProcessorTest, Diagnostics_DetailedInformation) {
    // Generate some activity
    auto stream_id = processor_->create_stream(test_context_, formatter_);
    processor_->process_chunk(stream_id, "diagnostic test chunk", true);

    auto diagnostics = processor_->get_diagnostics();

    EXPECT_TRUE(diagnostics.contains("statistics"));
    EXPECT_TRUE(diagnostics.contains("configuration"));
    EXPECT_TRUE(diagnostics.contains("thread_pool"));
    EXPECT_TRUE(diagnostics.contains("buffer_pool"));

    auto stats = diagnostics["statistics"];
    EXPECT_TRUE(stats.contains("total_streams"));
    EXPECT_TRUE(stats.contains("active_streams"));
    EXPECT_TRUE(stats.contains("success_rate"));

    auto buffer_pool = diagnostics["buffer_pool"];
    EXPECT_TRUE(buffer_pool.contains("total_buffers"));
    EXPECT_TRUE(buffer_pool.contains("available_buffers"));
    EXPECT_TRUE(buffer_pool.contains("buffer_size_bytes"));
}

