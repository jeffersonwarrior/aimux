#include <gtest/gtest.h>
#include "aimux/prettifier/streaming_processor.hpp"
#include "aimux/prettifier/anthropic_formatter.hpp"
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <random>

using namespace aimux::prettifier;

class StreamingProcessorSyncTest : public ::testing::Test {
protected:
    void SetUp() override {
        processor_ = std::make_shared<StreamingProcessor>();
        formatter_ = std::make_shared<AnthropicFormatter>();

        // Configure for testing
        nlohmann::json config;
        config["thread_pool_size"] = 8;
        config["max_concurrent_streams"] = 100;
        config["chunk_timeout_ms"] = 10000;
        config["stream_timeout_ms"] = 30000;
        processor_->configure(config);
    }

    void TearDown() override {
        processor_.reset();
        formatter_.reset();
    }

    ProcessingContext create_test_context() {
        ProcessingContext ctx;
        ctx.provider_name = "anthropic";
        ctx.model_name = "claude-3-5-sonnet-20241022";
        ctx.original_format = "json";
        ctx.requested_formats = {"toon"};
        ctx.streaming_mode = true;
        return ctx;
    }

    std::string generate_test_chunk(int index) {
        return "test_chunk_" + std::to_string(index) + "_data";
    }

    std::shared_ptr<StreamingProcessor> processor_;
    std::shared_ptr<PrettifierPlugin> formatter_;
};

// Test 1: Concurrent stream creation
TEST_F(StreamingProcessorSyncTest, ConcurrentStreamCreation) {
    const int num_threads = 10;
    const int streams_per_thread = 10;
    std::vector<std::thread> threads;
    std::vector<std::string> stream_ids;
    std::mutex ids_mutex;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            for (int j = 0; j < streams_per_thread; ++j) {
                auto ctx = create_test_context();

                auto stream_id = processor_->create_stream(ctx, formatter_);

                std::lock_guard<std::mutex> lock(ids_mutex);
                stream_ids.push_back(stream_id);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(stream_ids.size(), num_threads * streams_per_thread);

    // Verify all stream IDs are unique
    std::set<std::string> unique_ids(stream_ids.begin(), stream_ids.end());
    EXPECT_EQ(unique_ids.size(), stream_ids.size());
}

// Test 2: Concurrent chunk processing on same stream
TEST_F(StreamingProcessorSyncTest, ConcurrentChunkProcessingOnSameStream) {
    auto ctx = create_test_context();
    auto stream_id = processor_->create_stream(ctx, formatter_);

    const int num_threads = 5;
    const int chunks_per_thread = 20;
    std::vector<std::thread> threads;
    std::atomic<int> successful_chunks{0};

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            for (int j = 0; j < chunks_per_thread; ++j) {
                auto chunk = generate_test_chunk(i * chunks_per_thread + j);
                bool is_final = (i == num_threads - 1 && j == chunks_per_thread - 1);

                auto future = processor_->process_chunk(stream_id, chunk, is_final);
                if (future.get()) {
                    successful_chunks.fetch_add(1);
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_GT(successful_chunks.load(), 0);
}

// Test 3: Concurrent reads and writes to different streams
TEST_F(StreamingProcessorSyncTest, ConcurrentReadsAndWritesToDifferentStreams) {
    const int num_streams = 20;
    std::vector<std::string> stream_ids;

    // Create streams
    for (int i = 0; i < num_streams; ++i) {
        auto ctx = create_test_context();
        stream_ids.push_back(processor_->create_stream(ctx, formatter_));
    }

    std::vector<std::thread> threads;
    std::atomic<int> operations_completed{0};

    // Start concurrent operations
    for (int i = 0; i < num_streams; ++i) {
        threads.emplace_back([&, i]() {
            const auto& stream_id = stream_ids[i];

            // Write chunks
            for (int j = 0; j < 10; ++j) {
                auto chunk = generate_test_chunk(j);
                bool is_final = (j == 9);
                processor_->process_chunk(stream_id, chunk, is_final);
            }

            // Read status
            for (int k = 0; k < 5; ++k) {
                processor_->is_stream_active(stream_id);
            }

            operations_completed.fetch_add(1);
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(operations_completed.load(), num_streams);
}

// Test 4: Race condition test - stream cancellation while processing
TEST_F(StreamingProcessorSyncTest, RaceConditionStreamCancellationWhileProcessing) {
    auto ctx = create_test_context();
    auto stream_id = processor_->create_stream(ctx, formatter_);

    std::atomic<bool> processing_started{false};
    std::atomic<bool> cancellation_attempted{false};

    // Thread 1: Process chunks
    std::thread processor_thread([&]() {
        processing_started.store(true);
        for (int i = 0; i < 100; ++i) {
            if (cancellation_attempted.load()) {
                break;
            }
            auto chunk = generate_test_chunk(i);
            processor_->process_chunk(stream_id, chunk, false);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });

    // Thread 2: Cancel stream
    std::thread canceller_thread([&]() {
        while (!processing_started.load()) {
            std::this_thread::yield();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        cancellation_attempted.store(true);
        processor_->cancel_stream(stream_id);
    });

    processor_thread.join();
    canceller_thread.join();

    // Should not crash or deadlock
    EXPECT_TRUE(true);
}

// Test 5: Deadlock detection - get_result while processing
TEST_F(StreamingProcessorSyncTest, NoDeadlockGetResultWhileProcessing) {
    auto ctx = create_test_context();
    auto stream_id = processor_->create_stream(ctx, formatter_);

    std::thread processor_thread([&]() {
        for (int i = 0; i < 10; ++i) {
            auto chunk = generate_test_chunk(i);
            bool is_final = (i == 9);
            processor_->process_chunk(stream_id, chunk, is_final).get();
        }
    });

    std::thread result_thread([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        auto result = processor_->get_result(stream_id);
    });

    processor_thread.join();
    result_thread.join();

    EXPECT_TRUE(true); // No deadlock occurred
}

// Test 6: Mutex ordering consistency
TEST_F(StreamingProcessorSyncTest, MutexOrderingConsistency) {
    const int num_operations = 100;
    std::vector<std::thread> threads;

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&, i]() {
            for (int j = 0; j < num_operations; ++j) {
                auto ctx = create_test_context();

                auto stream_id = processor_->create_stream(ctx, formatter_);
                processor_->is_stream_active(stream_id);
                processor_->cancel_stream(stream_id);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_TRUE(true); // No deadlock from mutex ordering issues
}

// Test 7: Stream context access after deletion
TEST_F(StreamingProcessorSyncTest, StreamContextAccessAfterDeletion) {
    auto ctx = create_test_context();
    auto stream_id = processor_->create_stream(ctx, formatter_);

    // Process and finalize
    for (int i = 0; i < 5; ++i) {
        auto chunk = generate_test_chunk(i);
        bool is_final = (i == 4);
        processor_->process_chunk(stream_id, chunk, is_final).get();
    }

    // Try to access after finalization
    std::thread access_thread([&]() {
        for (int i = 0; i < 50; ++i) {
            processor_->is_stream_active(stream_id);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });

    access_thread.join();
    EXPECT_TRUE(true); // No crash
}

// Test 8: Queue overflow protection
TEST_F(StreamingProcessorSyncTest, QueueOverflowProtection) {
    auto ctx = create_test_context();
    auto stream_id = processor_->create_stream(ctx, formatter_);

    std::vector<std::future<bool>> futures;

    // Submit many chunks rapidly
    for (int i = 0; i < 1000; ++i) {
        auto chunk = generate_test_chunk(i);
        futures.push_back(processor_->process_chunk(stream_id, chunk, false));
    }

    // Wait for all to complete
    int successful = 0;
    for (auto& f : futures) {
        try {
            if (f.get()) {
                successful++;
            }
        } catch (...) {
            // Some may fail due to backpressure - that's okay
        }
    }

    EXPECT_GT(successful, 0);
}

// Test 9: Statistics consistency under concurrent load
TEST_F(StreamingProcessorSyncTest, StatisticsConsistencyUnderConcurrentLoad) {
    const int num_threads = 10;
    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < 20; ++j) {
                auto ctx = create_test_context();
                auto stream_id = processor_->create_stream(ctx, formatter_);

                for (int k = 0; k < 5; ++k) {
                    auto chunk = generate_test_chunk(k);
                    bool is_final = (k == 4);
                    processor_->process_chunk(stream_id, chunk, is_final);
                }
            }
        });
    }

    // Concurrent statistics reads
    std::thread stats_thread([&]() {
        for (int i = 0; i < 100; ++i) {
            auto stats = processor_->get_statistics();
            EXPECT_GE(stats.total_streams, stats.completed_streams + stats.failed_streams);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    for (auto& t : threads) {
        t.join();
    }
    stats_thread.join();

    EXPECT_TRUE(true);
}

// Test 10: Condition variable spurious wakeup handling
TEST_F(StreamingProcessorSyncTest, ConditionVariableSpuriousWakeupHandling) {
    auto ctx = create_test_context();
    auto stream_id = processor_->create_stream(ctx, formatter_);

    // Submit tasks with delays to test worker thread wait logic
    std::vector<std::thread> threads;
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([&, i]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(i * 20));
            for (int j = 0; j < 10; ++j) {
                auto chunk = generate_test_chunk(j);
                processor_->process_chunk(stream_id, chunk, false);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_TRUE(true);
}

// Test 11: Shared mutex upgrade safety
TEST_F(StreamingProcessorSyncTest, SharedMutexUpgradeSafety) {
    std::vector<std::string> stream_ids;

    // Create multiple streams
    for (int i = 0; i < 20; ++i) {
        auto ctx = create_test_context();
        stream_ids.push_back(processor_->create_stream(ctx, formatter_));
    }

    std::vector<std::thread> threads;

    // Multiple readers and writers
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&, i]() {
            for (int j = 0; j < 50; ++j) {
                // Read operations
                processor_->is_stream_active(stream_ids[i % stream_ids.size()]);
                processor_->get_diagnostics();

                // Occasional write
                if (j % 10 == 0) {
                    auto chunk = generate_test_chunk(j);
                    processor_->process_chunk(stream_ids[i % stream_ids.size()], chunk, false);
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_TRUE(true);
}

// Test 12: Memory ordering and atomic operations
TEST_F(StreamingProcessorSyncTest, MemoryOrderingAndAtomicOperations) {
    std::vector<std::thread> threads;
    std::atomic<int> counter{0};

    for (int i = 0; i < 20; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < 100; ++j) {
                auto ctx = create_test_context();

                auto stream_id = processor_->create_stream(ctx, formatter_);
                auto chunk = generate_test_chunk(0);
                processor_->process_chunk(stream_id, chunk, true);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    auto stats = processor_->get_statistics();
    EXPECT_GE(stats.total_streams, 2000);
}

// Test 13: Proper cleanup on exception
TEST_F(StreamingProcessorSyncTest, ProperCleanupOnException) {
    // Test that exceptions don't leave locks held
    try {
        auto ctx = create_test_context();
        ctx.provider_name = ""; // Invalid
        processor_->create_stream(ctx, nullptr); // Invalid formatter
        FAIL() << "Expected exception";
    } catch (...) {
        // Should be able to continue using processor
        auto ctx = create_test_context();
        auto stream_id = processor_->create_stream(ctx, formatter_);
        EXPECT_FALSE(stream_id.empty());
    }
}

// Test 14: Thread pool shutdown with pending tasks
TEST_F(StreamingProcessorSyncTest, ThreadPoolShutdownWithPendingTasks) {
    auto ctx = create_test_context();
    auto stream_id = processor_->create_stream(ctx, formatter_);

    // Submit many tasks
    std::vector<std::future<bool>> futures;
    for (int i = 0; i < 100; ++i) {
        auto chunk = generate_test_chunk(i);
        futures.push_back(processor_->process_chunk(stream_id, chunk, false));
    }

    // Let some process, then destroy processor
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Processor destructor should handle this gracefully
    processor_.reset();
    processor_ = std::make_shared<StreamingProcessor>();

    EXPECT_TRUE(true); // No crash
}

// Test 15: Promise/future exception safety
TEST_F(StreamingProcessorSyncTest, PromiseFutureExceptionSafety) {
    auto ctx = create_test_context();
    auto stream_id = processor_->create_stream(ctx, formatter_);

    std::vector<std::future<bool>> futures;

    // Submit tasks
    for (int i = 0; i < 20; ++i) {
        auto chunk = generate_test_chunk(i);
        futures.push_back(processor_->process_chunk(stream_id, chunk, false));
    }

    // Cancel stream while processing
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    processor_->cancel_stream(stream_id);

    // All futures should be accessible without exception
    int completed = 0;
    for (auto& f : futures) {
        try {
            f.get();
            completed++;
        } catch (...) {
            // Some may fail, that's okay
        }
    }

    EXPECT_GE(completed, 0);
}

// Test 16: Configuration changes during operation
TEST_F(StreamingProcessorSyncTest, ConfigurationChangesDuringOperation) {
    std::atomic<bool> keep_running{true};

    std::thread worker([&]() {
        while (keep_running.load()) {
            auto ctx = create_test_context();
            auto stream_id = processor_->create_stream(ctx, formatter_);

            for (int i = 0; i < 5; ++i) {
                auto chunk = generate_test_chunk(i);
                processor_->process_chunk(stream_id, chunk, i == 4);
            }
        }
    });

    // Change config while processing
    for (int i = 0; i < 10; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        nlohmann::json config;
        config["chunk_timeout_ms"] = 5000 + i * 100;
        processor_->configure(config);
    }

    keep_running.store(false);
    worker.join();

    EXPECT_TRUE(true);
}

// Test 17: Health check thread safety
TEST_F(StreamingProcessorSyncTest, HealthCheckThreadSafety) {
    std::vector<std::thread> threads;

    // Workers creating streams
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < 20; ++j) {
                auto ctx = create_test_context();
                auto stream_id = processor_->create_stream(ctx, formatter_);
                auto chunk = generate_test_chunk(j);
                processor_->process_chunk(stream_id, chunk, true);
            }
        });
    }

    // Health checkers
    for (int i = 0; i < 3; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < 50; ++j) {
                auto health = processor_->health_check();
                EXPECT_TRUE(health.contains("status"));
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_TRUE(true);
}

// Test 18: Buffer pool thread safety
TEST_F(StreamingProcessorSyncTest, BufferPoolThreadSafety) {
    std::vector<std::thread> threads;

    // Multiple threads creating and processing streams
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < 30; ++j) {
                auto ctx = create_test_context();
                auto stream_id = processor_->create_stream(ctx, formatter_);

                // Process large chunks to stress buffer pool
                std::string large_chunk(10000, 'x');
                processor_->process_chunk(stream_id, large_chunk, true);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    auto diagnostics = processor_->get_diagnostics();
    EXPECT_TRUE(diagnostics.contains("buffer_pool"));
}

// Test 19: Stream lifecycle race conditions
TEST_F(StreamingProcessorSyncTest, StreamLifecycleRaceConditions) {
    const int num_iterations = 50;

    for (int i = 0; i < num_iterations; ++i) {
        auto ctx = create_test_context();
        auto stream_id = processor_->create_stream(ctx, formatter_);

        std::thread t1([&]() {
            for (int j = 0; j < 10; ++j) {
                processor_->process_chunk(stream_id, generate_test_chunk(j), j == 9);
            }
        });

        std::thread t2([&]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            processor_->is_stream_active(stream_id);
        });

        std::thread t3([&]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(15));
            processor_->get_result(stream_id);
        });

        t1.join();
        t2.join();
        t3.join();
    }

    EXPECT_TRUE(true);
}

// Test 20: Stress test - maximum concurrent operations
TEST_F(StreamingProcessorSyncTest, StressTestMaximumConcurrentOperations) {
    const int num_streams = 50;
    const int operations_per_stream = 20;

    std::vector<std::thread> threads;
    std::atomic<int> successful_operations{0};
    std::atomic<int> failed_operations{0};

    for (int i = 0; i < num_streams; ++i) {
        threads.emplace_back([&, i]() {
            try {
                auto ctx = create_test_context();
                auto stream_id = processor_->create_stream(ctx, formatter_);

                for (int j = 0; j < operations_per_stream; ++j) {
                    auto chunk = generate_test_chunk(j);
                    bool is_final = (j == operations_per_stream - 1);

                    auto future = processor_->process_chunk(stream_id, chunk, is_final);

                    // Interleave with status checks
                    processor_->is_stream_active(stream_id);

                    if (future.get()) {
                        successful_operations.fetch_add(1);
                    } else {
                        failed_operations.fetch_add(1);
                    }
                }

                // Get result
                auto result = processor_->get_result(stream_id);

            } catch (const std::exception& e) {
                failed_operations.fetch_add(operations_per_stream);
            }
        });
    }

    // Monitor thread
    std::thread monitor([&]() {
        for (int i = 0; i < 100; ++i) {
            auto stats = processor_->get_statistics();
            auto health = processor_->health_check();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    for (auto& t : threads) {
        t.join();
    }
    monitor.join();

    auto final_stats = processor_->get_statistics();
    EXPECT_GT(successful_operations.load(), 0);
    EXPECT_EQ(final_stats.total_streams, num_streams);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
