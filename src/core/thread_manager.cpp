/**
 * @file thread_manager.cpp
 * @brief Production-ready thread resource management implementation
 *
 * Implementation of comprehensive thread lifecycle management with proper
 * cleanup, monitoring, and resource leak prevention.
 */

#include "aimux/core/thread_manager.hpp"
#include "aimux/core/error_handler.hpp"
#include <iostream>
#include <algorithm>
#include <cstring>

#ifdef __linux__
#include <sys/prctl.h>
#include <pthread.h>
#include <sys/resource.h>
#elif defined(__APPLE__)
#include <pthread.h>
#endif

namespace aimux {
namespace core {

// ManagedThread implementation
ManagedThread::ManagedThread(const std::string& name, const std::string& description)
    : info_(name, description) {
    AIMUX_DEBUG("Creating managed thread: " + name);
}

ManagedThread::~ManagedThread() {
    cleanup_resources();
}

ManagedThread::ManagedThread(ManagedThread&& other) noexcept
    : thread_(std::move(other.thread_))
    , info_(other.info_) {
}

ManagedThread& ManagedThread::operator=(ManagedThread&& other) noexcept {
    if (this != &other) {
        cleanup_resources();
        thread_ = std::move(other.thread_);
        info_ = other.info_;
    }
    return *this;
}

void ManagedThread::set_os_name(const std::string& name) {
    std::lock_guard<std::mutex> lock(info_mutex_);

#ifdef __linux__
    if (thread_.joinable()) {
        prctl(PR_SET_NAME, name.c_str(), 0, 0, 0);
    }
#elif defined(__APPLE__)
    if (thread_.joinable()) {
        pthread_setname_np(name.c_str());
    }
#endif
}

bool ManagedThread::set_priority(int priority) {
    if (!thread_.joinable()) {
        return false;
    }

#ifdef __linux__
    sched_param param;
    param.sched_priority = priority;
    int policy = SCHED_RR;
    if (pthread_setschedparam(thread_.native_handle(), policy, &param) != 0) {
        AIMUX_WARNING("Failed to set thread priority for " + info_.name);
        return false;
    }
    return true;
#elif defined(__APPLE__)
    sched_param param;
    param.sched_priority = priority;
    if (pthread_setschedparam(thread_.native_handle(), SCHED_FIFO, &param) != 0) {
        AIMUX_WARNING("Failed to set thread priority for " + info_.name);
        return false;
    }
    return true;
#else
    static_cast<void>(priority);
    return false;
#endif
}

void ManagedThread::cleanup_resources() {
    AIMUX_DEBUG("Cleaning up managed thread: " + info_.name);

    // Graceful shutdown
    stop();

    if (force_stop(5000)) {
        AIMUX_DEBUG("Thread " + info_.name + " cleaned up successfully");
    } else {
        AIMUX_WARNING("Thread " + info_.name + " failed to clean up gracefully");
    }

    std::lock_guard<std::mutex> lock(info_mutex_);
    info_.status = ThreadStatus::STOPPED;
    info_.memory_usage_bytes = 0;
}

// ThreadManager implementation
ThreadManager& ThreadManager::instance() {
    static ThreadManager instance;
    return instance;
}

ThreadManager::~ThreadManager() {
    force_shutdown_all();
}

ManagedThread& ThreadManager::create_thread(const std::string& name, const std::string& description) {
    std::unique_lock<std::shared_mutex> lock(threads_mutex_);

    if (threads_.find(name) != threads_.end()) {
        AIMUX_THROW(ErrorCode::SYSTEM_THREAD_CREATION_FAILED,
                   "Thread with name '" + name + "' already exists");
    }

    auto thread = std::make_unique<ManagedThread>(name, description);
    auto& thread_ref = *thread;

    threads_[name] = std::move(thread);

    // Add lifecycle callback
    add_lifecycle_callback([this, name](const ThreadInfo& info, ThreadStatus status) {
        if (status == ThreadStatus::ERROR || status == ThreadStatus::TIMEOUT) {
            AIMUX_ERROR("Thread " + name + " entered error state: " + info.last_error);
        }
    });

    AIMUX_INFO("Created managed thread: " + name);
    return thread_ref;
}

ManagedThread* ThreadManager::get_thread(const std::string& name) {
    std::shared_lock<std::shared_mutex> lock(threads_mutex_);
    auto it = threads_.find(name);
    return (it != threads_.end()) ? it->second.get() : nullptr;
}

size_t ThreadManager::shutdown_all(int timeout_ms) {
    std::shared_lock<std::shared_mutex> lock(threads_mutex_);

    AIMUX_INFO("Shutting down all managed threads (" + std::to_string(threads_.size()) + " threads)");

    size_t failed_count = 0;
    std::vector<std::future<bool>> shutdown_futures;

    // Signal all threads to stop
    for (auto& [name, thread] : threads_) {
        if (thread->is_running()) {
            thread->stop();
            notify_lifecycle_change(thread->get_info(), ThreadStatus::STOPPING);
        }
    }

    // Wait for all threads to stop
    for (auto& [name, thread] : threads_) {
        auto future = std::async(std::launch::async, [&thread, timeout_ms]() {
            return thread->force_stop(timeout_ms);
        });
        shutdown_futures.push_back(std::move(future));
    }

    // Check results
    for (auto& future : shutdown_futures) {
        try {
            if (future.wait_for(std::chrono::milliseconds(timeout_ms + 1000)) == std::future_status::timeout) {
                failed_count++;
            } else if (!future.get()) {
                failed_count++;
            }
        } catch (const std::exception& e) {
            failed_count++;
            AIMUX_ERROR("Thread shutdown error: " + std::string(e.what()));
        }
    }

    if (failed_count > 0) {
        AIMUX_WARNING(std::to_string(failed_count) + "/" + std::to_string(threads_.size()) +
                     " threads failed to shutdown cleanly");
    } else {
        AIMUX_INFO("All threads shutdown successfully");
    }

    return failed_count;
}

void ThreadManager::force_shutdown_all() {
    std::unique_lock<std::shared_mutex> lock(threads_mutex_);

    // Disable health monitoring first
    health_monitoring_enabled_ = false;
    if (health_monitor_thread_.joinable()) {
        health_monitor_thread_.join();
    }

    // Force termination of all threads
    for (auto& [name, thread] : threads_) {
        if (thread->is_running()) {
            try {
                thread->force_stop(1000); // Very short timeout for force shutdown
            } catch (...) {
                // Ignore errors during force shutdown
            }
        }
    }

    threads_.clear();
}

std::vector<ThreadInfo> ThreadManager::get_all_thread_info() const {
    std::shared_lock<std::shared_mutex> lock(threads_mutex_);

    std::vector<ThreadInfo> info;
    info.reserve(threads_.size());

    for (const auto& [name, thread] : threads_) {
        info.push_back(thread->get_info());
    }

    return info;
}

std::unordered_map<ThreadStatus, size_t> ThreadManager::get_status_counts() const {
    std::shared_lock<std::shared_mutex> lock(threads_mutex_);

    std::unordered_map<ThreadStatus, size_t> counts;

    for (const auto& [name, thread] : threads_) {
        auto info = thread->get_info();
        counts[info.status]++;
    }

    return counts;
}

std::vector<std::string> ThreadManager::health_check() const {
    std::shared_lock<std::shared_mutex> lock(threads_mutex_);

    std::vector<std::string> issues;
    auto now = std::chrono::steady_clock::now();

    for (const auto& [name, thread] : threads_) {
        auto info = thread->get_info();

        // Check for stuck threads
        if (info.status == ThreadStatus::RUNNING) {
            auto inactive_duration = std::chrono::duration_cast<std::chrono::minutes>(
                now - info.last_activity);

            if (inactive_duration.count() > 30) { // 30 minutes
                issues.push_back("Thread '" + name + "' appears stuck (inactive for " +
                               std::to_string(inactive_duration.count()) + " minutes)");
            }
        }

        // Check for error states
        if (info.status == ThreadStatus::ERROR) {
            issues.push_back("Thread '" + name + "' is in error state: " + info.last_error);
        }

        // Check for timeout
        if (info.status == ThreadStatus::TIMEOUT) {
            issues.push_back("Thread '" + name + "' failed to shutdown within timeout");
        }

        // Check for resource exhaustion
        if (info.memory_usage_bytes > 100 * 1024 * 1024) { // 100MB threshold
            issues.push_back("Thread '" + name + "' is using excessive memory: " +
                           std::to_string(info.memory_usage_bytes / 1024 / 1024) + "MB");
        }
    }

    return issues;
}

size_t ThreadManager::cleanup_terminated_threads() {
    std::unique_lock<std::shared_mutex> lock(threads_mutex_);

    size_t cleaned_count = 0;
    auto it = threads_.begin();

    while (it != threads_.end()) {
        if (!it->second->is_running() && !it->second->joinable()) {
            auto info = it->second->get_info();
            notify_lifecycle_change(info, ThreadStatus::STOPPED);
            it = threads_.erase(it);
            cleaned_count++;
        } else {
            ++it;
        }
    }

    if (cleaned_count > 0) {
        AIMUX_DEBUG("Cleaned up " + std::to_string(cleaned_count) + " terminated threads");
    }

    return cleaned_count;
}

size_t ThreadManager::get_total_memory_usage() const {
    std::shared_lock<std::shared_mutex> lock(threads_mutex_);

    size_t total = 0;
    for (const auto& [name, thread] : threads_) {
        total += thread->get_info().memory_usage_bytes;
    }
    return total;
}

void ThreadManager::enable_health_monitoring(bool enable, std::chrono::seconds interval) {
    if (health_monitoring_enabled_.load() == enable) {
        return; // Already in desired state
    }

    health_monitoring_enabled_.store(enable);

    if (enable) {
        health_monitor_thread_ = std::thread(&ThreadManager::health_monitor_loop, this);
        AIMUX_INFO("Thread health monitoring enabled (interval: " +
                  std::to_string(interval.count()) + "s)");
    } else {
        if (health_monitor_thread_.joinable()) {
            health_monitor_thread_.join();
        }
        AIMUX_INFO("Thread health monitoring disabled");
    }
}

void ThreadManager::add_lifecycle_callback(std::function<void(const ThreadInfo&, ThreadStatus)> callback) {
    lifecycle_callbacks_.push_back(std::move(callback));
}

bool ThreadManager::has_resource_issues() const {
    auto issues = health_check();
    auto status_counts = get_status_counts();

    return !issues.empty() ||
           status_counts[ThreadStatus::ERROR] > 0 ||
           status_counts[ThreadStatus::TIMEOUT] > 0;
}

void ThreadManager::health_monitor_loop() {
    while (health_monitoring_enabled_) {
        try {
            auto issues = health_check();
            if (!issues.empty()) {
                for (const auto& issue : issues) {
                    AIMUX_WARNING("Thread health issue: " + issue);
                }
            }

            // Cleanup terminated threads
            cleanup_terminated_threads();

        } catch (const std::exception& e) {
            AIMUX_ERROR("Health monitor error: " + std::string(e.what()));
        }

        std::this_thread::sleep_for(std::chrono::seconds(30));
    }
}

void ThreadManager::notify_lifecycle_change(const ThreadInfo& info, ThreadStatus new_status) {
    try {
        for (auto& callback : lifecycle_callbacks_) {
            callback(info, new_status);
        }
    } catch (const std::exception& e) {
        AIMUX_WARNING("Thread lifecycle callback error: " + std::string(e.what()));
    }
}

// ThreadPool implementation
ThreadPool::ThreadPool(size_t thread_count, const std::string& pool_name)
    : pool_name_(pool_name) {

    workers_.reserve(thread_count);
    for (size_t i = 0; i < thread_count; ++i) {
        auto worker = std::make_unique<ManagedThread>(
            pool_name + "_worker_" + std::to_string(i),
            "Thread pool worker thread " + std::to_string(i));

        // Store raw pointer before moving
        ManagedThread* worker_ptr = worker.get();

        workers_.push_back(std::move(worker));

        // Start worker thread
        worker_ptr->start([this, worker_ptr]() {
            worker_loop(worker_ptr);
        });
    }

    AIMUX_INFO("Thread pool '" + pool_name + "' created with " +
              std::to_string(thread_count) + " workers");
}

ThreadPool::~ThreadPool() {
    shutdown();
}

ThreadPool::PoolStats ThreadPool::get_stats() const {
    PoolStats stats;
    stats.total_threads = workers_.size();
    stats.active_threads = active_workers_.load();

    std::lock_guard<std::mutex> lock(queue_mutex_);
    stats.queued_tasks = task_queue_.size();
    stats.completed_tasks = completed_tasks_.load();

    return stats;
}

void ThreadPool::shutdown() {
    shutdown_flag_.store(true);
    queue_cv_.notify_all();

    // Wait for all workers to finish
    size_t failed_shutdowns = 0;
    for (auto& worker : workers_) {
        if (!worker->force_stop(5000)) {
            failed_shutdowns++;
        }
    }

    if (failed_shutdowns > 0) {
        AIMUX_WARNING(std::to_string(failed_shutdowns) + " thread pool workers failed to shutdown cleanly");
    }

    AIMUX_INFO("Thread pool '" + pool_name_ + "' shutdown completed");
}

void ThreadPool::worker_loop(ManagedThread* worker) {
    while (!shutdown_flag_.load()) {
        std::function<void()> task;

        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_cv_.wait(lock, [this]() {
                return !task_queue_.empty() || shutdown_flag_.load();
            });

            if (shutdown_flag_.load()) {
                break;
            }

            task = std::move(task_queue_.front());
            task_queue_.pop();
        }

        active_workers_++;
        worker->update_activity();

        try {
            task();
            completed_tasks_++;
        } catch (const std::exception& e) {
            AIMUX_ERROR("Thread pool task error: " + std::string(e.what()));
        }

        active_workers_--;
    }

    worker->update_activity();
}

} // namespace core
} // namespace aimux