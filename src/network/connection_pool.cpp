#include "aimux/network/connection_pool.hpp"
#include "aimux/core/thread_manager.hpp"
#include <algorithm>
#include <iostream>

namespace aimux {
namespace network {

bool ConnectionPool::PooledConnection::is_available() const {
    return client && client->is_available();
}

ConnectionPool::ConnectionPool(size_t max_connections)
    : max_connections_(max_connections), pool_start_time_(std::chrono::steady_clock::now()) {

    // Start cleanup worker thread with proper resource management
    auto& thread_manager = core::ThreadManager::instance();
    cleanup_thread_ = &thread_manager.create_thread(
        "connection-pool-cleanup",
        "Background thread for connection pool cleanup");

    if (!cleanup_thread_->start([this](std::atomic<bool>& should_stop) {
        cleanup_worker_managed(should_stop);
    })) {
        std::cerr << "Failed to start connection pool cleanup thread" << std::endl;
    }

    std::cout << "ConnectionPool initialized with max " << max_connections_ << " connections\n";
}

ConnectionPool::~ConnectionPool() {
    shutdown();
}

std::shared_ptr<HttpClient> ConnectionPool::get_connection(const std::string& base_url, int /* timeout_ms */) {
    std::unique_lock<std::mutex> lock(connections_mutex_);
    
    auto start_time = std::chrono::steady_clock::now();
    
    // Try to find available connection
    auto connection = find_available_connection(base_url);
    if (connection) {
        update_stats_wait_time(start_time);
        return connection;
    }
    
    // Create new connection if under limit
    if (connections_.size() < max_connections_) {
        connection = create_connection(base_url);
        if (connection) {
            update_stats_wait_time(start_time);
            return connection;
        }
    }
    
    // Wait for available connection
    connection_available_.wait(lock, [this, &base_url]() {
        // Check if we can create new connection or find available one
        return connections_.size() < max_connections_ || 
               find_available_connection(base_url) != nullptr;
    });
    
    // Try again after waiting
    connection = find_available_connection(base_url);
    if (!connection && connections_.size() < max_connections_) {
        connection = create_connection(base_url);
    }
    
    update_stats_wait_time(start_time);
    return connection;
}

void ConnectionPool::return_connection(std::shared_ptr<HttpClient> client) {
    if (!client) return;
    
    std::lock_guard<std::mutex> lock(connections_mutex_);
    
    // Find existing pooled connection
    auto it = std::find_if(connections_.begin(), connections_.end(),
        [&client](const PooledConnection& pooled) {
            return pooled.client.get() == client.get();
        });
    
    if (it != connections_.end()) {
        // Update last used time
        it->last_used = std::chrono::steady_clock::now();
        it->client->reset(); // Reset client state
    }
    
    connection_available_.notify_one();
}

PoolStats ConnectionPool::get_stats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    std::lock_guard<std::mutex> conn_lock(connections_mutex_);
    
    PoolStats stats = stats_;
    stats.total_connections = connections_.size();
    stats.active_connections = static_cast<size_t>(std::count_if(connections_.begin(), connections_.end(),
        [](const PooledConnection& conn) { return !conn.is_available(); }));
    stats.available_connections = connections_.size() - stats.active_connections;
    
    return stats;
}

void ConnectionPool::shutdown() {
    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        shutdown_flag_ = true;
    }

    connection_available_.notify_all();

    // Stop cleanup thread with proper resource management
    if (cleanup_thread_) {
        cleanup_thread_->stop();
        if (!cleanup_thread_->force_stop(5000)) {
            std::cerr << "Connection pool cleanup thread failed to stop gracefully" << std::endl;
        }
    }

    // Cleanup all connections
    std::lock_guard<std::mutex> lock(connections_mutex_);
    connections_.clear();
}

std::shared_ptr<HttpClient> ConnectionPool::find_available_connection(const std::string& base_url) {
    auto it = std::find_if(connections_.begin(), connections_.end(),
        [&base_url](const PooledConnection& conn) {
            return conn.is_available() && 
                   (base_url.empty() || conn.base_url == base_url);
        });
    
    if (it != connections_.end()) {
        auto client = it->client;
        it->last_used = std::chrono::steady_clock::now();
        return client;
    }
    
    return nullptr;
}

std::shared_ptr<HttpClient> ConnectionPool::create_connection(const std::string& base_url) {
    try {
        auto client = HttpClientFactory::create_client();
        if (client) {
            // Set client configuration
            client->set_timeout(30000);
            client->add_default_header("User-Agent", "Aimux2/2.0.0");
            client->add_default_header("Connection", "keep-alive");
            
            // Add to pool
            PooledConnection pooled;
            pooled.client = std::move(client);
            pooled.base_url = base_url;
            pooled.last_used = std::chrono::steady_clock::now();
            
            connections_.push_back(pooled);
            return client;
        }
    } catch (const std::exception& e) {
        std::cerr << "Failed to create connection: " << e.what() << "\n";
    }
    
    return nullptr;
}

void ConnectionPool::cleanup_old_connections() {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    
    auto now = std::chrono::steady_clock::now();
    auto cutoff = now - std::chrono::minutes(5); // Remove connections idle for 5 minutes
    
    auto original_size = connections_.size();
    
    connections_.erase(
        std::remove_if(connections_.begin(), connections_.end(),
            [this, cutoff](const PooledConnection& conn) {
                return !conn.is_available() || 
                       (conn.last_used < cutoff && connections_.size() > 10);
            }),
        connections_.end());
    
    if (connections_.size() != original_size) {
        std::cout << "Cleaned up " << (original_size - connections_.size()) 
                  << " old connections\n";
    }
}

void ConnectionPool::cleanup_worker() {
    // Legacy method - use managed version instead
    std::atomic<bool> dummy_stop{false};
    cleanup_worker_managed(dummy_stop);
}

void ConnectionPool::cleanup_worker_managed(std::atomic<bool>& should_stop) {
    // Set thread name for monitoring
    if (cleanup_thread_) {
        cleanup_thread_->set_os_name("conn-pool-cleaner");
    }

    while (!should_stop.load() && !shutdown_flag_.load()) {
        try {
            // Update thread activity for monitoring
            if (cleanup_thread_) {
                cleanup_thread_->update_activity();
            }

            cleanup_old_connections();

            // Sleep for 1 minute
            std::this_thread::sleep_for(std::chrono::minutes(1));

        } catch (const std::exception& e) {
            std::cerr << "Error in connection pool cleanup worker: " << e.what() << std::endl;
            // Continue running despite errors
        }
    }

    // Final cleanup
    if (cleanup_thread_) {
        cleanup_thread_->update_activity();
    }
}

void ConnectionPool::update_stats_wait_time(const std::chrono::steady_clock::time_point& start_time) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    auto now = std::chrono::steady_clock::now();
    auto wait_time = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time);
    
    stats_.total_requests_served++;
    
    // Update rolling average wait time
    double alpha = 0.1; // Smoothing factor
    stats_.avg_wait_time_ms = alpha * wait_time.count() + (1 - alpha) * stats_.avg_wait_time_ms;
}

std::unique_ptr<ConnectionPool> ConnectionPoolFactory::create_pool(size_t max_connections) {
    return std::make_unique<ConnectionPool>(max_connections);
}

} // namespace network
} // namespace aimux
