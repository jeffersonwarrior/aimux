/**
 * @file thread_manager.hpp
 * @brief Production-ready background thread resource management
 *
 * This module provides comprehensive thread lifecycle management including:
 * - Proper thread cleanup and resource release
 * - Thread monitoring and health checking
 * - Graceful shutdown with join operations
 * - Memory leak prevention in long-running threads
 * - Thread-safe resource access patterns
 */

#pragma once

#include <thread>
#include <atomic>
#include <vector>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <functional>
#include <string>
#include <unordered_map>
#include <future>
#include <exception>
#include <shared_mutex>
#include <queue>

namespace aimux {
namespace core {

/**
 * @brief Thread health status for monitoring
 */
enum class ThreadStatus {
    STOPPED,
    STARTING,
    RUNNING,
    STOPPING,
    ERROR,
    TIMEOUT,
    RESOURCE_EXHAUSTED
};

/**
 * @brief Thread metadata for monitoring and management
 */
struct ThreadInfo {
    std::string name;
    std::string description;
    std::thread::id thread_id;
    ThreadStatus status{ThreadStatus::STOPPED};
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point last_activity;
    size_t memory_usage_bytes{0};
    uint64_t operations_completed{0};
    std::string last_error;
    std::atomic<bool> should_stop{false};

    ThreadInfo(const std::string& thread_name, const std::string& thread_desc)
        : name(thread_name)
        , description(thread_desc)
        , thread_id(0)
        , last_activity(std::chrono::steady_clock::now()) {}

    ThreadInfo(const ThreadInfo& other)
        : name(other.name)
        , description(other.description)
        , thread_id(other.thread_id)
        , status(other.status)
        , start_time(other.start_time)
        , last_activity(other.last_activity)
        , memory_usage_bytes(other.memory_usage_bytes)
        , operations_completed(other.operations_completed)
        , last_error(other.last_error)
        , should_stop(other.should_stop.load()) {}

    ThreadInfo& operator=(const ThreadInfo& other) {
        if (this != &other) {
            name = other.name;
            description = other.description;
            thread_id = other.thread_id;
            status = other.status;
            start_time = other.start_time;
            last_activity = other.last_activity;
            memory_usage_bytes = other.memory_usage_bytes;
            operations_completed = other.operations_completed;
            last_error = other.last_error;
            should_stop.store(other.should_stop.load());
        }
        return *this;
    }

    ThreadInfo(ThreadInfo&&) = delete;
    ThreadInfo& operator=(ThreadInfo&&) = delete;
};

/**
 * @brief RAII thread wrapper with automatic cleanup
 */
class ManagedThread {
public:
    explicit ManagedThread(const std::string& name, const std::string& description = "");
    ~ManagedThread();

    // Delete copy operations
    ManagedThread(const ManagedThread&) = delete;
    ManagedThread& operator=(const ManagedThread&) = delete;

    // Allow move operations
    ManagedThread(ManagedThread&& other) noexcept;
    ManagedThread& operator=(ManagedThread&& other) noexcept;

    /**
     * @brief Start a thread with the given function
     *
     * @tparam Function Callable type
     * @tparam Args Arguments types
     * @param func Function to execute
     * @param args Arguments to pass to function
     * @return true if thread started successfully
     */
    template<typename Function, typename... Args>
    bool start(Function&& func, Args&&... args) {
        std::lock_guard<std::mutex> lock(info_mutex_);

        if (thread_.joinable()) {
            return false; // Already running
        }

        try {
            // Set thread info
            info_.status = ThreadStatus::STARTING;
            info_.start_time = std::chrono::steady_clock::now();
            info_.last_activity = info_.start_time;
            info_.should_stop.store(false);

            // Start the thread with wrapper function
            thread_ = std::thread([this, func = std::forward<Function>(func), args...]() mutable {
                info_.thread_id = std::this_thread::get_id();

                try {
                    info_.status = ThreadStatus::RUNNING;

                    // Execute the user function
                    if constexpr (std::is_invocable_v<decltype(func), decltype(args)..., std::atomic<bool>&>) {
                        func(args..., info_.should_stop);
                    } else {
                        func(args...);
                    }

                    info_.status = ThreadStatus::STOPPED;

                } catch (const std::exception& e) {
                    info_.status = ThreadStatus::ERROR;
                    info_.last_error = e.what();
                    // Don't re-throw - let the thread exit gracefully
                } catch (...) {
                    info_.status = ThreadStatus::ERROR;
                    info_.last_error = "Unknown exception";
                }
            });

            return true;

        } catch (const std::exception& e) {
            info_.status = ThreadStatus::ERROR;
            info_.last_error = e.what();
            return false;
        }
    }

    /**
     * @brief Signal thread to stop gracefully
     */
    void stop() {
        info_.should_stop.store(true);
        if (info_.status == ThreadStatus::RUNNING) {
            info_.status = ThreadStatus::STOPPING;
        }
    }

    /**
     * @brief Force stop thread (use only as last resort)
     *
     * @param timeout_ms Maximum time to wait for graceful shutdown
     * @return true if thread terminated cleanly
     */
    bool force_stop(int timeout_ms = 5000) {
        stop();

        if (!thread_.joinable()) {
            return true;
        }

        // Wait for thread to finish
        auto future = std::async(std::launch::async, [this]() {
            if (thread_.joinable()) {
                thread_.join();
            }
        });

        if (future.wait_for(std::chrono::milliseconds(timeout_ms)) == std::future_status::timeout) {
            info_.status = ThreadStatus::TIMEOUT;
            return false; // Thread didn't stop in time
        }

        info_.status = ThreadStatus::STOPPED;
        return true;
    }

    /**
     * @brief Check if thread is currently running
     */
    bool is_running() const {
        std::lock_guard<std::mutex> lock(info_mutex_);
        return info_.status == ThreadStatus::RUNNING;
    }

    /**
     * @brief Check if thread needs cleanup
     */
    bool needs_cleanup() const {
        std::lock_guard<std::mutex> lock(info_mutex_);
        return !thread_.joinable() && info_.status != ThreadStatus::STOPPED;
    }

    /**
     * @brief Get thread information
     */
    const ThreadInfo& get_info() const {
        std::lock_guard<std::mutex> lock(info_mutex_);
        return info_;
    }

    /**
     * @brief Update thread activity timestamp
     */
    void update_activity() {
        std::lock_guard<std::mutex> lock(info_mutex_);
        info_.last_activity = std::chrono::steady_clock::now();
        info_.operations_completed++;
    }

    /**
     * @brief Update memory usage (for monitoring)
     */
    void update_memory_usage(size_t bytes) {
        std::lock_guard<std::mutex> lock(info_mutex_);
        info_.memory_usage_bytes = bytes;
    }

    /**
     * @brief Get thread uptime
     */
    std::chrono::milliseconds get_uptime() const {
        std::lock_guard<std::mutex> lock(info_mutex_);
        if (info_.status == ThreadStatus::STOPPED) {
            return std::chrono::milliseconds(0);
        }
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now - info_.start_time);
    }

    /**
     * @brief Join thread (blocking)
     */
    void join() {
        if (thread_.joinable()) {
            thread_.join();
        }
    }

    /**
     * @brief Check if thread is joinable
     */
    bool joinable() const {
        return thread_.joinable();
    }

    /**
     * @brief Get native thread handle
     */
    std::thread::native_handle_type native_handle() {
        return thread_.native_handle();
    }

    /**
     * @brief Set thread name (Linux/macOS)
     */
    void set_os_name(const std::string& name);

    /**
     * @brief Set thread priority (platform-specific)
     */
    bool set_priority(int priority);

private:
    std::thread thread_;
    mutable ThreadInfo info_;
    mutable std::mutex info_mutex_;

    void cleanup_resources();
};

/**
 * @brief Comprehensive thread manager for production systems
 *
 * Provides centralized management of all background threads with
 * proper resource cleanup and monitoring capabilities.
 */
class ThreadManager {
public:
    static ThreadManager& instance();

    /**
     * @brief Create and manage a new thread
     *
     * @param name Thread identifier
     * @param description Thread description for monitoring
     * @return ManagedThread reference
     */
    ManagedThread& create_thread(const std::string& name, const std::string& description = "");

    /**
     * @brief Get existing thread by name
     *
     * @param name Thread identifier
     * @return ManagedThread pointer or nullptr if not found
     */
    ManagedThread* get_thread(const std::string& name);

    /**
     * @brief Gracefully shutdown all threads
     *
     * @param timeout_ms Maximum time to wait per thread
     * @return Number of threads that failed to shutdown cleanly
     */
    size_t shutdown_all(int timeout_ms = 10000);

    /**
     * @brief Force shutdown all threads (resource cleanup only)
     */
    void force_shutdown_all();

    /**
     * @brief Get status of all managed threads
     */
    std::vector<ThreadInfo> get_all_thread_info() const;

    /**
     * @brief Get count of threads by status
     */
    std::unordered_map<ThreadStatus, size_t> get_status_counts() const;

    /**
     * @brief Check for thread health issues
     */
    std::vector<std::string> health_check() const;

    /**
     * @brief Cleanup terminated threads
     *
     * @return Number of threads cleaned up
     */
    size_t cleanup_terminated_threads();

    /**
     * @brief Get memory usage statistics for all threads
     */
    size_t get_total_memory_usage() const;

    /**
     * @brief Enable/disable automatic health monitoring
     */
    void enable_health_monitoring(bool enable, std::chrono::seconds interval = std::chrono::seconds(30));

    /**
     * @brief Add thread lifecycle callback
     */
    void add_lifecycle_callback(std::function<void(const ThreadInfo&, ThreadStatus)> callback);

    /**
     * @brief Check if any threads have resource issues
     */
    bool has_resource_issues() const;

private:
    ThreadManager() = default;
    ~ThreadManager();

    std::unordered_map<std::string, std::unique_ptr<ManagedThread>> threads_;
    mutable std::shared_mutex threads_mutex_;
    std::atomic<bool> health_monitoring_enabled_{false};
    std::thread health_monitor_thread_;
    std::vector<std::function<void(const ThreadInfo&, ThreadStatus)>> lifecycle_callbacks_;

    void health_monitor_loop();
    void notify_lifecycle_change(const ThreadInfo& info, ThreadStatus new_status);
};

/**
 * @brief RAII thread pool with automatic resource management
 */
class ThreadPool {
public:
    explicit ThreadPool(size_t thread_count, const std::string& pool_name = "ThreadPool");
    ~ThreadPool();

    // Delete copy/move operations
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    /**
     * @brief Submit task to thread pool
     *
     * @tparam Function Callable type
     * @tparam Args Arguments types
     * @param func Function to execute
     * @param args Arguments to pass to function
     * @return Future for the result
     */
    template<typename Function, typename... Args>
    auto submit(Function&& func, Args&&... args) -> std::future<decltype(func(args...))> {
        using ReturnType = decltype(func(args...));

        auto promise = std::make_shared<std::promise<ReturnType>>();
        auto future = promise->get_future();

        task_queue_.push([=, p = std::move(promise)]() mutable {
            try {
                if constexpr (std::is_void_v<ReturnType>) {
                    func(args...);
                    p->set_value();
                } else {
                    p->set_value(func(args...));
                }
            } catch (...) {
                p->set_exception(std::current_exception());
            }
        });

        return future;
    }

    /**
     * @brief Get pool statistics
     */
    struct PoolStats {
        size_t total_threads;
        size_t active_threads;
        size_t queued_tasks;
        size_t completed_tasks;
    };

    PoolStats get_stats() const;

    /**
     * @brief Shutdown pool and wait for all tasks to complete
     */
    void shutdown();

private:
    std::vector<std::unique_ptr<ManagedThread>> workers_;
    std::queue<std::function<void()>> task_queue_;
    mutable std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::atomic<bool> shutdown_flag_{false};
    std::atomic<size_t> active_workers_{0};
    std::atomic<size_t> completed_tasks_{0};
    const std::string pool_name_;

    void worker_loop(ManagedThread* worker);
};

/**
 * @brief Utility macros for common thread operations
 */
#define AIMUX_MANAGED_THREAD(name, ...) \
    aimux::core::ThreadManager::instance().create_thread(name, __VA_ARGS__)

#define AIMUX_THREAD_ACTIVITY_UPDATE() \
    // Call within thread to update activity timestamp

#define AIMUX_THREAD_SHUTDOWN_ALL() \
    aimux::core::ThreadManager::instance().shutdown_all()

} // namespace core
} // namespace aimux