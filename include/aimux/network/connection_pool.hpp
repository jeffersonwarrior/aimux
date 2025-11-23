#pragma once

#include <memory>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <thread>
#include <atomic>
#include "aimux/network/http_client.hpp"

namespace aimux {
namespace core {
    class ManagedThread;
}
}

namespace aimux {
namespace network {

/**
 * @brief Connection pool statistics
 */
struct PoolStats {
    size_t total_connections = 0;
    size_t active_connections = 0;
    size_t available_connections = 0;
    size_t total_requests_served = 0;
    double avg_wait_time_ms = 0.0;
};

/**
 * @brief Thread-safe connection pool for HTTP clients
 * Manages connections efficiently with proper cleanup
 */
class ConnectionPool {
public:
    explicit ConnectionPool(size_t max_connections = 100);
    ~ConnectionPool();

    // Get connection from pool (blocks if pool exhausted and max_connections reached)
    std::shared_ptr<HttpClient> get_connection(const std::string& base_url, int timeout_ms = 30000);
    
    // Return connection to pool
    void return_connection(std::shared_ptr<HttpClient> connection);
    
    // Get pool statistics
    PoolStats get_stats() const;
    
    // Shutdown pool and cleanup all connections
    void shutdown();

private:
    struct PooledConnection {
        std::shared_ptr<HttpClient> client;
        std::chrono::steady_clock::time_point last_used;
        std::string base_url;
        bool is_available() const;
    };

    // Find existing connection for URL
    std::shared_ptr<HttpClient> find_available_connection(const std::string& base_url);
    
    // Create new connection
    std::shared_ptr<HttpClient> create_connection(const std::string& base_url);
    
    // Cleanup old connections
    void cleanup_old_connections();
    
    // Background cleanup thread
    void cleanup_worker();

    // Managed cleanup worker with proper resource handling
    void cleanup_worker_managed(std::atomic<bool>& should_stop);
    
    // Update statistics
    void update_stats_wait_time(const std::chrono::steady_clock::time_point& start_time);

    size_t max_connections_;
    std::vector<PooledConnection> connections_;
    mutable std::mutex connections_mutex_;
    std::condition_variable connection_available_;
    std::atomic<bool> shutdown_flag_{false};
    core::ManagedThread* cleanup_thread_{nullptr};
    
    // Statistics
    mutable std::mutex stats_mutex_;
    PoolStats stats_;
    std::chrono::steady_clock::time_point pool_start_time_;
};

/**
 * @brief Connection pool factory
 */
class ConnectionPoolFactory {
public:
    static std::unique_ptr<ConnectionPool> create_pool(size_t max_connections = 100);
};

} // namespace network
} // namespace aimux
