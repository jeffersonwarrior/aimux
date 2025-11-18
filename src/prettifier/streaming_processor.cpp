#include "aimux/prettifier/streaming_processor.hpp"
#include <sstream>
#include <algorithm>
#include <cstring>
#include <random>

// TODO: Replace with proper logging when available
#define LOG_ERROR(msg, ...) do { printf("[STREAMING ERROR] " msg "\n", ##__VA_ARGS__); } while(0)
#define LOG_DEBUG(msg, ...) do { printf("[STREAMING DEBUG] " msg "\n", ##__VA_ARGS__); } while(0)
#define LOG_DIAGNOSTIC(msg, ...) do { printf("[STREAMING DIAG] " msg "\n", ##__VA_ARGS__); } while(0)

namespace aimux {
namespace prettifier {

// StreamingProcessor implementation
StreamingProcessor::StreamingProcessor() : start_time_(std::chrono::steady_clock::now()) {
    LOG_DEBUG("Initializing StreamingProcessor with thread pool size: %d", thread_pool_size_);

    try {
        initialize_buffer_pool();
        initialize_thread_pool();
        LOG_DEBUG("StreamingProcessor initialized successfully");
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to initialize StreamingProcessor: %s", e.what());
        throw;
    }
}

StreamingProcessor::~StreamingProcessor() {
    LOG_DEBUG("Shutting down StreamingProcessor");

    // Signal shutdown to worker threads
    shutdown_requested_.store(true);
    queue_cv_.notify_all();

    // Wait for all worker threads to complete
    for (auto& thread : worker_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    // Clean up remaining streams
    {
        std::unique_lock<std::shared_mutex> lock(streams_mutex_);
        active_streams_.clear();
    }

    LOG_DEBUG("StreamingProcessor shutdown completed");
}

std::string StreamingProcessor::create_stream(
    const ProcessingContext& context,
    std::shared_ptr<PrettifierPlugin> formatter) {

    auto stream_id = generate_stream_id();

    try {
        auto stream_context = std::make_unique<StreamContext>();
        stream_context->stream_id = stream_id;
        stream_context->process_context = context;
        stream_context->start_time = std::chrono::steady_clock::now();
        stream_context->formatter = formatter;

        // Initialize TOON builder
        stream_context->toon_builder = {
            {"format", "toon"},
            {"version", "1.0.0"},
            {"provider", context.provider_name},
            {"model", context.model_name},
            {"streaming", true},
            {"metadata", {
                {"stream_id", stream_id},
                {"created_at", std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count()},
                {"streaming_processor", "StreamingProcessor-v1.0.0"}
            }}
        };

        // Pre-allocate chunk buffer
        stream_context->chunk_buffer.reserve(100); // Reserve space for 100 chunks
        stream_context->content_accumulator.reserve(8192); // 8KB initial content buffer

        {
            std::unique_lock<std::shared_mutex> lock(streams_mutex_);
            if (active_streams_.size() >= max_concurrent_streams_) {
                throw std::runtime_error("Maximum concurrent streams exceeded");
            }
            active_streams_[stream_id] = std::move(stream_context);
        }

        total_streams_.fetch_add(1);

        LOG_DEBUG("Created stream %s for provider %s", stream_id.c_str(), context.provider_name.c_str());
        return stream_id;

    } catch (const std::exception& e) {
        LOG_ERROR("Failed to create stream %s: %s", stream_id.c_str(), e.what());
        failed_streams_.fetch_add(1);
        throw;
    }
}

std::future<bool> StreamingProcessor::process_chunk(
    const std::string& stream_id,
    const std::string& chunk,
    bool is_final) {

    auto stream_context = get_stream_context(stream_id);
    if (!stream_context || !stream_context->is_active) {
        std::promise<bool> promise;
        promise.set_value(false);
        return promise.get_future();
    }

    // Apply backpressure if necessary
    if (!apply_backpressure(stream_id)) {
        std::promise<bool> promise;
        promise.set_value(false);
        backpressure_events_.fetch_add(1);
        return promise.get_future();
    }

    // Create processing task
    ProcessingTask task;
    task.stream_id = stream_id;
    task.chunk_data = chunk;
    task.is_final = is_final;
    task.timestamp = std::chrono::steady_clock::now();

    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        task_queue_.push(std::move(task));
    }

    queue_cv_.notify_one();

    // Return a future that will be completed when the task is processed
    auto future = task_queue_.back().completion_promise.get_future();
    return future;
}

ProcessingResult StreamingProcessor::get_result(const std::string& stream_id) {
    auto stream_context = get_stream_context(stream_id);
    if (!stream_context) {
        ProcessingResult result;
        result.success = false;
        result.error_message = "Stream not found: " + stream_id;
        return result;
    }

    // Wait for stream to be finalized
    std::unique_lock<std::shared_mutex> lock(streams_mutex_);
    auto condition = [&]() { return !stream_context->is_active || stream_context->is_finalized; };

    if (!condition()) {
        // Wait with timeout
        constexpr int wait_timeout_ms = 100;
        auto timeout = std::chrono::steady_clock::now() + std::chrono::milliseconds(wait_timeout_ms);

        streams_mutex_.unlock_shared();
        streams_mutex_.lock_shared(); // Upgrade lock

        streams_mutex_.unlock_shared();
        streams_mutex_.lock_shared();

        if (!condition() && std::chrono::steady_clock::now() > timeout) {
            ProcessingResult result;
            result.success = false;
            result.error_message = "Stream result retrieval timeout: " + stream_id;
            return result;
        }
    }

    if (!stream_context->error_message.empty()) {
        ProcessingResult result;
        result.success = false;
        result.error_message = stream_context->error_message;
        return result;
    }

    // Generate final TOON format
    std::string final_toon = generate_streaming_toon(*stream_context);

    ProcessingResult result;
    result.success = true;
    result.processed_content = final_toon;
    result.extracted_tool_calls = stream_context->accumulated_tool_calls;
    result.streaming_mode = true;

    auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - stream_context->start_time).count();

    result.processing_time = std::chrono::milliseconds(total_time);
    result.tokens_processed = stream_context->content_accumulator.length() / 4;

    result.metadata = {
        {"stream_id", stream_id},
        {"total_chunks", stream_context->total_chunks},
        {"total_bytes", stream_context->total_bytes},
        {"streaming_complete", true},
        {"processor_stats", {
            {"chunks_per_second", stream_context->total_chunks * 1000.0 / total_time},
            {"throughput_mbps", (stream_context->total_bytes * 8.0) / (total_time * 1000000.0)}
        }}
    };

    // Update statistics
    completed_streams_.fetch_add(1);

    LOG_DEBUG("Retrieved result for stream %s: %zu chunks in %ld ms",
              stream_id.c_str(), stream_context->total_chunks, total_time);

    return result;
}

bool StreamingProcessor::cancel_stream(const std::string& stream_id) {
    auto stream_context = get_stream_context(stream_id);
    if (!stream_context) {
        return false;
    }

    stream_context->is_active = false;
    stream_context->error_message = "Stream cancelled";

    // Update statistics
    failed_streams_.fetch_add(1);

    LOG_DEBUG("Cancelled stream %s", stream_id.c_str());
    return true;
}

bool StreamingProcessor::is_stream_active(const std::string& stream_id) const {
    auto stream_context = get_stream_context(stream_id);
    return stream_context && stream_context->is_active;
}

bool StreamingProcessor::configure(const nlohmann::json& config) {
    try {
        if (config.contains("thread_pool_size")) {
            int new_size = config["thread_pool_size"].get<int>();
            if (new_size > 0 && new_size <= 32) {
                thread_pool_size_ = new_size;
            }
        }

        if (config.contains("buffer_size_mb")) {
            buffer_size_mb_ = config["buffer_size_mb"].get<size_t>();
        }

        if (config.contains("backpressure_threshold")) {
            backpressure_threshold_ = config["backpressure_threshold"].get<size_t>();
        }

        if (config.contains("enable_compression")) {
            enable_compression_ = config["enable_compression"].get<bool>();
        }

        if (config.contains("max_concurrent_streams")) {
            max_concurrent_streams_ = config["max_concurrent_streams"].get<size_t>();
        }

        if (config.contains("chunk_timeout_ms")) {
            chunk_timeout_ms_ = config["chunk_timeout_ms"].get<int>();
        }

        if (config.contains("stream_timeout_ms")) {
            stream_timeout_ms_ = config["stream_timeout_ms"].get<int>();
        }

        if (config.contains("enable_metrics")) {
            enable_metrics_ = config["enable_metrics"].get<bool>();
        }

        LOG_DEBUG("StreamingProcessor configuration updated");
        return true;

    } catch (const std::exception& e) {
        LOG_ERROR("Configuration failed: %s", e.what());
        return false;
    }
}

nlohmann::json StreamingProcessor::get_configuration() const {
    nlohmann::json config;
    config["thread_pool_size"] = thread_pool_size_;
    config["buffer_size_mb"] = buffer_size_mb_;
    config["backpressure_threshold"] = backpressure_threshold_;
    config["enable_compression"] = enable_compression_;
    config["max_concurrent_streams"] = max_concurrent_streams_;
    config["chunk_timeout_ms"] = chunk_timeout_ms_;
    config["stream_timeout_ms"] = stream_timeout_ms_;
    config["enable_metrics"] = enable_metrics_;
    return config;
}

StreamingProcessor::ProcessorStats StreamingProcessor::get_statistics() const {
    ProcessorStats stats;

    auto now = std::chrono::steady_clock::now();
    auto total_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time_).count();

    stats.total_streams = total_streams_.load();
    stats.completed_streams = completed_streams_.load();
    stats.failed_streams = failed_streams_.load();
    stats.total_chunks_processed = total_chunks_processed_.load();
    stats.total_bytes_processed = total_bytes_processed_.load();
    stats.current_memory_usage = current_memory_usage_.load();
    stats.backpressure_events = backpressure_events_.load();

    // Calculate derived metrics
    if (total_time_ms > 0) {
        stats.average_chunks_per_second = static_cast<double>(total_chunks_processed_.load()) / (total_time_ms / 1000.0);
        stats.average_throughput_mbps = (static_cast<double>(total_bytes_processed_.load()) * 8.0) / (total_time_ms * 1000000.0);
    }

    // Count active streams
    {
        std::shared_lock<std::shared_mutex> lock(streams_mutex_);
        stats.active_streams = active_streams_.size();
    }

    return stats;
}

nlohmann::json StreamingProcessor::get_diagnostics() const {
    nlohmann::json diagnostics;

    auto stats = get_statistics();
    diagnostics["statistics"] = {
        {"total_streams", stats.total_streams},
        {"active_streams", stats.active_streams},
        {"completed_streams", stats.completed_streams},
        {"failed_streams", stats.failed_streams},
        {"success_rate", stats.total_streams > 0 ?
            static_cast<double>(stats.completed_streams) / stats.total_streams : 1.0},
        {"total_chunks", stats.total_chunks_processed},
        {"total_bytes", stats.total_bytes_processed},
        {"average_chunks_per_second", stats.average_chunks_per_second},
        {"average_throughput_mbps", stats.average_throughput_mbps},
        {"current_memory_bytes", stats.current_memory_usage},
        {"backpressure_events", stats.backpressure_events}
    };

    diagnostics["configuration"] = get_configuration();

    // Thread pool status
    diagnostics["thread_pool"] = {
        {"worker_threads", worker_threads_.size()},
        {"shutdown_requested", shutdown_requested_.load()}
    };

    // Buffer pool status
    diagnostics["buffer_pool"] = {
        {"total_buffers", buffer_pool_.total_buffers},
        {"available_buffers", buffer_pool_.available_buffers.size()},
        {"buffer_size_bytes", buffer_pool_.buffer_size}
    };

    // Active streams details
    nlohmann::json streams_info = nlohmann::json::array();
    {
        std::shared_lock<std::shared_mutex> lock(streams_mutex_);
        for (const auto& [stream_id, stream] : active_streams_) {
            auto stream_age_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - stream->start_time).count();

            streams_info.push_back({
                {"stream_id", stream_id},
                {"provider", stream->process_context.provider_name},
                {"model", stream->process_context.model_name},
                {"chunks_received", stream->total_chunks},
                {"bytes_received", stream->total_bytes},
                {"age_ms", stream_age_ms},
                {"is_active", stream->is_active},
                {"is_finalized", stream->is_finalized}
            });
        }
    }
    diagnostics["active_streams"] = streams_info;

    return diagnostics;
}

nlohmann::json StreamingProcessor::health_check() const {
    nlohmann::json health;

    try {
        health["status"] = "healthy";
        health["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        auto stats = get_statistics();

        // Check thread pool responsiveness
        bool thread_pool_responsive = !shutdown_requested_.load() && !worker_threads_.empty();
        health["thread_pool_responsive"] = thread_pool_responsive;

        // Check memory usage
        size_t max_memory_mb = buffer_size_mb_ * 2; // Allow some overhead
        bool memory_within_limits = stats.current_memory_usage < max_memory_mb * 1024 * 1024;
        health["memory_within_limits"] = memory_within_limits;

        // Check success rate
        double success_rate = stats.total_streams > 0 ?
            static_cast<double>(stats.completed_streams) / stats.total_streams : 1.0;
        bool acceptable_success_rate = success_rate >= 0.95; // 95% success rate
        health["acceptable_success_rate"] = acceptable_success_rate;

        // Check backpressure frequency
        double backpressure_rate = stats.total_chunks_processed > 0 ?
            static_cast<double>(stats.backpressure_events) / stats.total_chunks_processed : 0.0;
        bool acceptable_backpressure = backpressure_rate <= 0.1; // 10% backpressure rate
        health["acceptable_backpressure"] = acceptable_backpressure;

        // Overall health assessment
        health["overall_healthy"] = thread_pool_responsive &&
                                   memory_within_limits &&
                                   acceptable_success_rate &&
                                   acceptable_backpressure;

        // Performance metrics
        health["performance_metrics"] = {
            {"average_chunks_per_second", stats.average_chunks_per_second},
            {"average_throughput_mbps", stats.average_throughput_mbps},
            {"success_rate", success_rate},
            {"backpressure_rate", backpressure_rate},
            {"active_streams", stats.active_streams},
            {"memory_usage_mb", stats.current_memory_usage / (1024.0 * 1024.0)}
        };

    } catch (const std::exception& e) {
        health["status"] = "unhealthy";
        health["error"] = e.what();
    }

    return health;
}

void StreamingProcessor::reset_statistics() {
    total_streams_.store(0);
    completed_streams_.store(0);
    failed_streams_.store(0);
    total_chunks_processed_.store(0);
    total_bytes_processed_.store(0);
    backpressure_events_.store(0);
    current_memory_usage_.store(0);

    start_time_ = std::chrono::steady_clock::now();

    LOG_DEBUG("StreamingProcessor statistics reset");
}

void StreamingProcessor::optimize_for_throughput() {
    LOG_DEBUG("Optimizing for throughput");

    nlohmann::json config;
    config["thread_pool_size"] = std::min(thread_pool_size_ * 2, 16);
    config["buffer_size_mb"] = buffer_size_mb_ * 2;
    config["backpressure_threshold"] = backpressure_threshold_ * 2;
    config["enable_compression"] = false;
    config["chunk_timeout_ms"] = chunk_timeout_ms_ * 2;

    configure(config);

    LOG_DEBUG("Throughput optimization applied");
}

void StreamingProcessor::optimize_for_latency() {
    LOG_DEBUG("Optimizing for latency");

    nlohmann::json config;
    config["thread_pool_size"] = std::max(thread_pool_size_ / 2, 2);
    config["buffer_size_mb"] = std::max(buffer_size_mb_ / 2, static_cast<size_t>(16));
    config["backpressure_threshold"] = std::max(backpressure_threshold_ / 2, static_cast<size_t>(100));
    config["chunk_timeout_ms"] = std::max(chunk_timeout_ms_ / 2, 1000);

    configure(config);

    LOG_DEBUG("Latency optimization applied");
}

void StreamingProcessor::optimize_for_memory() {
    LOG_DEBUG("Optimizing for memory efficiency");

    nlohmann::json config;
    config["thread_pool_size"] = 2;
    config["buffer_size_mb"] = std::max(buffer_size_mb_ / 4, static_cast<size_t>(8));
    config["backpressure_threshold"] = std::max(backpressure_threshold_ / 4, static_cast<size_t>(50));
    config["enable_compression"] = true;
    config["max_concurrent_streams"] = std::max(max_concurrent_streams_ / 2, static_cast<size_t>(100));

    configure(config);

    LOG_DEBUG("Memory optimization applied");
}

// Private methods implementation

void StreamingProcessor::initialize_thread_pool() {
    worker_threads_.reserve(static_cast<size_t>(thread_pool_size_));

    for (int i = 0; i < thread_pool_size_; ++i) {
        worker_threads_.emplace_back(&StreamingProcessor::worker_thread_main, this);
    }

    LOG_DEBUG("Thread pool initialized with %d threads", thread_pool_size_);
}

void StreamingProcessor::worker_thread_main() {
    thread_local std::string thread_id = "worker_" + std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id()));

    while (!shutdown_requested_.load()) {
        std::unique_lock<std::mutex> lock(queue_mutex_);

        // Wait for task or shutdown
        queue_cv_.wait(lock, [this]() {
            return !task_queue_.empty() || shutdown_requested_.load();
        });

        if (shutdown_requested_.load()) {
            break;
        }

        if (task_queue_.empty()) {
            continue;
        }

        ProcessingTask task = std::move(task_queue_.front());
        task_queue_.pop();

        lock.unlock(); // Release queue lock while processing

        auto start_time = std::chrono::high_resolution_clock::now();

        try {
            bool success = process_task(task);
            task.completion_promise.set_value(success);

            auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::high_resolution_clock::now() - start_time).count();

            if (enable_metrics_) {
                update_metrics(task.chunk_data.length(), static_cast<size_t>(elapsed_us));
            }

        } catch (const std::exception& e) {
            LOG_ERROR("Task processing failed in %s: %s", thread_id.c_str(), e.what());
            task.completion_promise.set_exception(std::current_exception());
        }
    }

    LOG_DEBUG("Worker thread %s exiting", thread_id.c_str());
}

bool StreamingProcessor::process_task(const ProcessingTask& task) {
    auto stream_context = get_stream_context(task.stream_id);
    if (!stream_context || !stream_context->is_active) {
        return false;
    }

    try {
        // Check for timeout
        auto now = std::chrono::steady_clock::now();
        auto stream_age_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - stream_context->start_time).count();

        if (stream_age_ms > stream_timeout_ms_) {
            stream_context->error_message = "Stream timeout";
            stream_context->is_active = false;
            failed_streams_.fetch_add(1);
            return false;
        }

        // Check chunk timeout
        auto chunk_age_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - task.timestamp).count();
        if (chunk_age_ms > chunk_timeout_ms_) {
            LOG_DEBUG("Chunk timeout for stream %s, processing anyway", task.stream_id.c_str());
        }

        // Add chunk to buffer
        stream_context->chunk_buffer.push_back(task.chunk_data);
        stream_context->total_bytes += task.chunk_data.length();
        stream_context->total_chunks++;
        total_chunks_processed_.fetch_add(task.chunk_data.length());

        // Process chunk with formatter
        ProcessingResult chunk_result = stream_context->formatter->process_streaming_chunk(
            task.chunk_data,
            task.is_final,
            stream_context->process_context);

        if (task.is_final) {
            finalize_stream(*stream_context);
        }

        // Extract tool calls if any
        if (!chunk_result.extracted_tool_calls.empty()) {
            stream_context->accumulated_tool_calls.insert(
                stream_context->accumulated_tool_calls.end(),
                chunk_result.extracted_tool_calls.begin(),
                chunk_result.extracted_tool_calls.end());
        }

        // Accumulate content
        if (!chunk_result.processed_content.empty()) {
            stream_context->content_accumulator += chunk_result.processed_content;
        }

        return true;

    } catch (const std::exception& e) {
        LOG_ERROR("Failed to process task for stream %s: %s", task.stream_id.c_str(), e.what());
        stream_context->error_message = e.what();
        stream_context->is_active = false;
        failed_streams_.fetch_add(1);
        return false;
    }
}

void StreamingProcessor::finalize_stream(StreamContext& stream) {
    if (stream.is_finalized) {
        return;
    }

    LOG_DEBUG("Finalizing stream %s", stream.stream_id.c_str());

    // Call formatter end_streaming
    try {
        auto result = stream.formatter->end_streaming(stream.process_context);
        if (!result.success) {
            stream.error_message = result.error_message;
        }
    } catch (const std::exception& e) {
        LOG_ERROR("End streaming failed for stream %s: %s", stream.stream_id.c_str(), e.what());
        stream.error_message = e.what();
    }

    stream.is_finalized = true;
    stream.is_active = false;
}

std::string StreamingProcessor::generate_streaming_toon(StreamContext& stream) {
    nlohmann::json toon = stream.toon_builder;

    // Add accumulated content
    toon["content"] = stream.content_accumulator;

    // Add accumulated tool calls
    if (!stream.accumulated_tool_calls.empty()) {
        nlohmann::json tool_array = nlohmann::json::array();
        for (const auto& tool : stream.accumulated_tool_calls) {
            tool_array.push_back(tool.to_json());
        }
        toon["tool_calls"] = tool_array;
    }

    // Add streaming metadata
    auto total_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - stream.start_time).count();

    toon["metadata"]["streaming_stats"] = {
        {"total_chunks", stream.total_chunks},
        {"total_bytes", stream.total_bytes},
        {"processing_time_ms", total_time_ms},
        {"chunks_per_second", static_cast<double>(stream.total_chunks) / (total_time_ms / 1000.0)},
        {"throughput_mbps", (static_cast<double>(stream.total_bytes) * 8.0) / (total_time_ms * 1000000.0)},
        {"finalized", stream.is_finalized}
    };

    return toon.dump();
}

bool StreamingProcessor::apply_backpressure(const std::string& stream_id) {
    auto stream_context = get_stream_context(stream_id);
    if (!stream_context) {
        return false;
    }

    // Check per-stream threshold
    if (stream_context->total_chunks >= backpressure_threshold_) {
        LOG_DEBUG("Backpressure applied to stream %s: %zu chunks received",
                  stream_id.c_str(), stream_context->total_chunks);
        return false;
    }

    // Check global threshold
    {
        std::shared_lock<std::shared_mutex> lock(streams_mutex_);
        if (active_streams_.size() >= max_concurrent_streams_) {
            LOG_DEBUG("Global backpressure: %zu active streams", active_streams_.size());
            return false;
        }
    }

    return true;
}

char* StreamingProcessor::get_buffer_from_pool() {
    std::lock_guard<std::mutex> lock(buffer_pool_.pool_mutex);

    if (buffer_pool_.available_buffers.empty()) {
        // Allocate new buffer if pool is empty
        auto new_buffer = std::make_unique<char[]>(buffer_pool_.buffer_size);
        char* buffer_ptr = new_buffer.get();
        buffer_pool_.buffers.push_back(std::move(new_buffer));
        buffer_pool_.total_buffers++;
        return buffer_ptr;
    }

    char* buffer = buffer_pool_.available_buffers.front();
    buffer_pool_.available_buffers.pop();
    return buffer;
}

void StreamingProcessor::return_buffer_to_pool(char* buffer) {
    std::lock_guard<std::mutex> lock(buffer_pool_.pool_mutex);
    buffer_pool_.available_buffers.push(buffer);
}

void StreamingProcessor::initialize_buffer_pool() {
    size_t buffer_size = 1024 * 1024; // 1MB buffers
    size_t num_buffers = (buffer_size_mb_ * 1024 * 1024) / buffer_size;

    buffer_pool_.buffer_size = buffer_size;
    buffer_pool_.total_buffers = num_buffers;

    // Allocate initial buffers
    for (size_t i = 0; i < num_buffers; ++i) {
        auto buffer = std::make_unique<char[]>(buffer_size);
        buffer_pool_.available_buffers.push(buffer.get());
        buffer_pool_.buffers.push_back(std::move(buffer));
    }

    LOG_DEBUG("Buffer pool initialized: %zu buffers of %zu bytes each", num_buffers, buffer_size);
}

void StreamingProcessor::update_metrics(size_t chunk_size, size_t /*processing_time_us*/) {
    if (enable_metrics_) {
        total_bytes_processed_.fetch_add(chunk_size);
        current_memory_usage_.fetch_add(chunk_size);
    }
}

void StreamingProcessor::cleanup_expired_streams() {
    auto now = std::chrono::steady_clock::now();

    std::unique_lock<std::shared_mutex> lock(streams_mutex_);
    auto it = active_streams_.begin();

    while (it != active_streams_.end()) {
        auto age_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - it->second->start_time).count();

        if (age_ms > stream_timeout_ms_ * 2 && !it->second->is_finalized) {
            LOG_DEBUG("Cleaning up expired stream %s", it->first.c_str());
            it->second->is_active = false;
            it->second->error_message = "Stream expired";
            failed_streams_.fetch_add(1);
            it = active_streams_.erase(it);
        } else {
            ++it;
        }
    }
}

std::shared_ptr<StreamingProcessor::StreamContext> StreamingProcessor::get_stream_context(const std::string& stream_id) const {
    std::shared_lock<std::shared_mutex> lock(streams_mutex_);
    auto it = active_streams_.find(stream_id);
    if (it != active_streams_.end()) {
        // Convert unique_ptr to shared_ptr by creating a copy
        auto& context_ptr = it->second;
        return std::shared_ptr<StreamContext>(context_ptr.get(), [](StreamContext*){}); // Non-owning shared_ptr
    }
    return nullptr;
}

std::string StreamingProcessor::generate_stream_id() const {
    static std::atomic<uint64_t> counter{0};
    auto now = std::chrono::steady_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    auto count = counter.fetch_add(1);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1000, 9999);

    return "stream_" + std::to_string(timestamp) + "_" + std::to_string(count) + "_" + std::to_string(dis(gen));
}

bool StreamingProcessor::validate_stream_state(const StreamContext& stream) const {
    if (stream.stream_id.empty()) return false;
    if (!stream.formatter) return false;
    if (stream.total_chunks > backpressure_threshold_ * 10) return false; // Sanity check

    return true;
}

} // namespace prettifier
} // namespace aimux