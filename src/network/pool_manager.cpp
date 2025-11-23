#include "aimux/network/pool_manager.hpp"
#include <curl/curl.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <regex>
#include <future>
#include <algorithm>

namespace aimux {
namespace network {

// PooledConnection implementation
PooledConnection::PooledConnection(const std::string& host)
    : curl_(nullptr), host_(host), last_used(std::chrono::steady_clock::now()),
      created_at(std::chrono::steady_clock::now()) {

    error_buffer_ = std::string(CURL_ERROR_SIZE, '\0');
    curl_ = curl_easy_init();
    if (curl_) {
        curl_easy_setopt(curl_, CURLOPT_ERRORBUFFER, error_buffer_.data());
        curl_easy_setopt(curl_, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl_, CURLOPT_MAXREDIRS, 5L);

        // SSL settings
        curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYHOST, 2L);
    }
}

PooledConnection::~PooledConnection() {
    disconnect();
    if (curl_) {
        curl_easy_cleanup(curl_);
    }
}

bool PooledConnection::connect() {
    if (!curl_ || isConnected()) {
        return isConnected();
    }

    auto now = std::chrono::steady_clock::now();
    auto age = std::chrono::duration_cast<std::chrono::seconds>(now - created_at);

    // Connection is too old, recreate
    if (age.count() > 300) { // 5 minutes max age
        resetConnection();
    }

    is_healthy = true;
    return true;
}

void PooledConnection::disconnect() {
    if (curl_) {
        curl_easy_reset(curl_);
        // Re-apply basic settings
        curl_easy_setopt(curl_, CURLOPT_ERRORBUFFER, error_buffer_.data());
        curl_easy_setopt(curl_, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYHOST, 2L);
    }
    is_healthy = false;
}

bool PooledConnection::isConnected() const {
    return curl_ && is_healthy;
}

PooledConnection::Response PooledConnection::performRequest(
    const std::string& method,
    const std::string& url,
    const std::string& body,
    const std::unordered_map<std::string, std::string>& headers
) {
    Response response;
    auto start_time = std::chrono::steady_clock::now();

    if (!connect()) {
        response.success = false;
        return response;
    }

    // Set method
    if (method == "POST") {
        curl_easy_setopt(curl_, CURLOPT_POST, 1L);
        curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, body.c_str());
    } else if (method == "PUT") {
        curl_easy_setopt(curl_, CURLOPT_CUSTOMREQUEST, "PUT");
        curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, body.c_str());
    } else {
        curl_easy_setopt(curl_, CURLOPT_HTTPGET, 1L);
    }

    // Set URL
    curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());

    // Set headers
    struct curl_slist* header_list = nullptr;
    for (const auto& [key, value] : headers) {
        std::string header = key + ": " + value;
        header_list = curl_slist_append(header_list, header.c_str());
    }

    // Default headers
    header_list = curl_slist_append(header_list, "User-Agent: aimux/2.0");
    header_list = curl_slist_append(header_list, "Accept: application/json");
    header_list = curl_slist_append(header_list, "Content-Type: application/json");

    curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, header_list);

    // Set up response capture
    std::string response_body;
    std::unordered_map<std::string, std::string> response_headers;

    curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &response_body);
    curl_easy_setopt(curl_, CURLOPT_HEADERFUNCTION, HeaderCallback);
    curl_easy_setopt(curl_, CURLOPT_HEADERDATA, &response_headers);

    // Perform request
    CURLcode res = curl_easy_perform(curl_);
    auto end_time = std::chrono::steady_clock::now();

    response.response_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time
    );

    // Get status code
    long status_code;
    curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, &status_code);
    response.status_code = static_cast<int>(status_code);

    response.body = response_body;
    response.headers = response_headers;
    response.success = (res == CURLE_OK && status_code >= 200 && status_code < 300);

    // Update connection metadata
    last_used = end_time;
    request_count++;

    // Check connection health
    if (!response.success || res != CURLE_OK) {
        is_healthy = false;
    }

    // Cleanup
    curl_slist_free_all(header_list);

    return response;
}

void PooledConnection::resetConnection() {
    if (curl_) {
        curl_easy_reset(curl_);
        curl_easy_setopt(curl_, CURLOPT_ERRORBUFFER, error_buffer_.data());
        curl_easy_setopt(curl_, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYHOST, 2L);
    }
    created_at = std::chrono::steady_clock::now();
    request_count = 0;
}

size_t PooledConnection::WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total_size = size * nmemb;
    std::string* response = static_cast<std::string*>(userp);
    response->append(static_cast<char*>(contents), total_size);
    return total_size;
}

size_t PooledConnection::HeaderCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total_size = size * nmemb;
    std::string header(static_cast<char*>(contents), total_size);

    // Parse header
    auto colon_pos = header.find(':');
    if (colon_pos != std::string::npos) {
        std::string key = header.substr(0, colon_pos);
        std::string value = header.substr(colon_pos + 1);

        // Trim whitespace
        key.erase(0, key.find_first_not_of(" \t\r\n"));
        key.erase(key.find_last_not_of(" \t\r\n") + 1);
        value.erase(0, value.find_first_not_of(" \t\r\n"));
        value.erase(value.find_last_not_of(" \t\r\n") + 1);

        auto* headers = static_cast<std::unordered_map<std::string, std::string>*>(userp);
        (*headers)[key] = value;
    }

    return total_size;
}

// PoolManager implementation
PoolManager::PoolManager(const PoolConfig& config)
    : config_(config), start_time_(std::chrono::steady_clock::now()) {
}

PoolManager::~PoolManager() {
    stop();
}

void PoolManager::start() {
    if (running_) return;

    running_ = true;
    start_time_ = std::chrono::steady_clock::now();

    // Start background tasks
    cleanup_thread_ = std::thread(&PoolManager::cleanupTask, this);
    health_check_thread_ = std::thread(&PoolManager::healthCheckTask, this);
}

void PoolManager::stop() {
    if (!running_) return;

    running_ = false;

    // Wake up background threads
    if (cleanup_thread_.joinable()) {
        cleanup_thread_.join();
    }
    if (health_check_thread_.joinable()) {
        health_check_thread_.join();
    }

    cleanup();
}

std::unique_ptr<PooledConnection> PoolManager::getConnection(const std::string& host) {
    auto& pool = getOrCreatePool(host);
    std::lock_guard<std::mutex> lock(pool.mutex);

    // Try to get existing connection
    while (!pool.available.empty()) {
        auto conn = std::move(pool.available.front());
        pool.available.pop();

        if (conn && conn->isConnected() &&
            chrono::duration_cast<chrono::seconds>(
                chrono::steady_clock::now() - conn->last_used
            ).count() < config_.idle_timeout.count()) {

            pool.in_use.push(std::move(conn));
            global_requests_++;
            pool.request_count_++;
            return std::move(pool.in_use.back());
        }
    }

    // Create new connection if under limit
    if (pool.available.size() + pool.in_use.size() < config_.max_connections) {
        auto conn = createConnection(host);
        if (conn) {
            pool.in_use.push(std::move(conn));
            global_requests_++;
            pool.request_count_++;
            return std::move(pool.in_use.back());
        }
    }

    return nullptr;
}

void PoolManager::returnConnection(const std::string& host, std::unique_ptr<PooledConnection> conn) {
    if (!conn) return;

    auto& pool = getOrCreatePool(host);
    std::lock_guard<std::mutex> lock(pool.mutex);

    // Remove from in_use
    std::queue<std::unique_ptr<PooledConnection>> new_in_use;
    bool found = false;

    while (!pool.in_use.empty()) {
        auto current = std::move(pool.in_use.front());
        pool.in_use.pop();

        if (current.get() == conn.get() && !found) {
            new_in_use.push(std::move(current));
            found = true;
        } else {
            new_in_use.push(std::move(current));
        }
    }
    pool.in_use = std::move(new_in_use);

    // Return to available if healthy
    if (conn->isHealthy() &&
        conn->request_count < config_.max_request_count_per_connection) {
        pool.available.push(std::move(conn));
    }
}

PooledConnection::Response PoolManager::executeRequest(
    const std::string& method,
    const std::string& url,
    const std::string& body,
    const std::unordered_map<std::string, std::string>& headers
) {
    std::string host = extractHost(url);
    auto conn = getConnection(host);

    if (!conn) {
        Response response;
        response.success = false;
        return response;
    }

    auto response = conn->performRequest(method, url, body, headers);

    if (response.success) {
        global_successes_++;
        auto& pool = getOrCreatePool(host);
        pool.success_count_++;
    }

    returnConnection(host, std::move(conn));
    return response;
}

PoolManager::PoolStats PoolManager::getStats(const std::string& host) const {
    PoolStats stats;

    if (host.empty()) {
        // Global stats
        stats.active_connections = 0;
        stats.idle_connections = 0;

        for (const auto& [h, pool_ptr] : pools_) {
            const auto& pool = *pool_ptr;
            std::lock_guard<std::mutex> lock(pool.mutex);
            stats.active_connections += pool.in_use.size();
            stats.idle_connections += pool.available.size();
        }

        stats.total_connections = stats.active_connections + stats.idle_connections;
        stats.total_requests = global_requests_.load();
        stats.success_rate = global_requests_ > 0 ?
            static_cast<double>(global_successes_.load()) / global_requests_.load() : 0.0;

    } else {
        // Pool-specific stats
        auto it = pools_.find(host);
        if (it != pools_.end()) {
            const auto& pool = *it->second;
            std::lock_guard<std::mutex> lock(pool.mutex);
            stats.active_connections = pool.in_use.size();
            stats.idle_connections = pool.available.size();
            stats.total_connections = stats.active_connections + stats.idle_connections;
            stats.total_requests = pool.request_count_.load();
            stats.success_rate = pool.request_count_ > 0 ?
                static_cast<double>(pool.success_count_.load()) / pool.request_count_.load() : 0.0;
            stats.uptime = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start_time_);
        }
    }

    return stats;
}

void PoolManager::cleanup() {
    std::lock_guard<std::mutex> lock(pools_mutex_);

    for (auto& [host, pool_ptr] : pools_) {
        auto& pool = *pool_ptr;
        std::lock_guard<std::mutex> pool_lock(pool.mutex);

        auto now = std::chrono::steady_clock::now();

        // Cleanup old connections
        std::queue<std::unique_ptr<PooledConnection>> new_available;
        while (!pool.available.empty()) {
            auto conn = std::move(pool.available.front());
            pool.available.pop();

            auto age = std::chrono::duration_cast<std::chrono::seconds>(
                now - conn->last_used
            );

            if (age.count() < config_.idle_timeout.count() && conn->isConnected()) {
                new_available.push(std::move(conn));
            }
        }
        pool.available = std::move(new_available);
    }
}

void PoolManager::healthCheck() {
    std::lock_guard<std::mutex> lock(pools_mutex_);

    for (auto& [host, pool_ptr] : pools_) {
        auto& pool = *pool_ptr;
        performPoolHealthCheck(pool, host);
    }
}

void PoolManager::cleanupTask() {
    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(30));
        if (running_) {
            cleanup();
        }
    }
}

void PoolManager::healthCheckTask() {
    while (running_) {
        std::this_thread::sleep_for(config_.health_check_interval);
        if (running_) {
            healthCheck();
        }
    }
}

PoolManager::ConnectionPool& PoolManager::getOrCreatePool(const std::string& host) {
    std::lock_guard<std::mutex> lock(pools_mutex_);

    auto it = pools_.find(host);
    if (it == pools_.end()) {
        auto pool = std::make_unique<ConnectionPool>();
        pool->last_health_check = std::chrono::steady_clock::now();
        pools_[host] = std::move(pool);
    }

    return *pools_[host];
}

std::unique_ptr<PooledConnection> PoolManager::createConnection(const std::string& host) {
    return ConnectionFactory::create(host, config_);
}

void PoolManager::performPoolHealthCheck(ConnectionPool& pool, const std::string& host) {
    std::lock_guard<std::mutex> pool_lock(pool.mutex);

    auto now = std::chrono::steady_clock::now();
    auto time_since_check = std::chrono::duration_cast<std::chrono::seconds>(
        now - pool.last_health_check
    );

    if (time_since_check < config_.health_check_interval) {
        return;
    }

    pool.last_health_check = now;

    // Test a connection from the pool
    bool has_healthy_connection = false;

    // Check available connections
    std::queue<std::unique_ptr<PooledConnection>> new_available;
    while (!pool.available.empty()) {
        auto conn = std::move(pool.available.front());
        pool.available.pop();

        if (conn && conn->isConnected()) {
            has_healthy_connection = true;
            new_available.push(std::move(conn));
        }
    }
    pool.available = std::move(new_available);

    // Check in-use connections indirectly through success rate
    if (pool.request_count_ > 10) {
        double success_rate = static_cast<double>(pool.success_count_.load()) / pool.request_count_.load();
        pool.is_healthy_ = (success_rate >= config_.health_check_failure_threshold);
    } else {
        pool.is_healthy_ = has_healthy_connection;
    }
}

std::string PoolManager::extractHost(const std::string& url) const {
    std::regex url_regex(R"(^(https?://)([^/]+))");
    std::smatch match;

    if (std::regex_search(url, match, url_regex) && match.size() > 2) {
        return match[2].str();
    }

    return url; // Fallback
}

// RequestBuilder implementation
RequestBuilder& RequestBuilder::method(const std::string& method) {
    method_ = method;
    return *this;
}

RequestBuilder& RequestBuilder::url(const std::string& url) {
    url_ = url;
    return *this;
}

RequestBuilder& RequestBuilder::body(const std::string& body) {
    body_ = body;
    return *this;
}

RequestBuilder& RequestBuilder::body(const nlohmann::json& json) {
    body_ = json.dump();
    return *this;
}

RequestBuilder& RequestBuilder::header(const std::string& key, const std::string& value) {
    headers_[key] = value;
    return *this;
}

RequestBuilder& RequestBuilder::timeout(std::chrono::milliseconds timeout) {
    timeout_ = timeout;
    return *this;
}

PooledConnection::Response RequestBuilder::execute() {
    return manager_.executeRequest(method_, url_, body_, headers_);
}

std::future<PooledConnection::Response> RequestBuilder::executeAsync() {
    return std::async(std::launch::async, [this]() {
        return execute();
    });
}

// CircuitBreaker implementation
CircuitBreaker::CircuitBreaker(const Config& config) : config_(config) {}

bool CircuitBreaker::canExecute() const {
    std::lock_guard<std::mutex> lock(mutex_);

    switch (state_) {
        case State::CLOSED:
            return true;
        case State::OPEN:
            return std::chrono::steady_clock::now() >
                   (last_failure_time_ + config_.recovery_timeout);
        case State::HALF_OPEN:
            return true;
    }
    return false;
}

void CircuitBreaker::recordSuccess() {
    std::lock_guard<std::mutex> lock(mutex_);

    switch (state_) {
        case State::CLOSED:
            failure_count_ = 0;
            break;
        case State::OPEN:
            setState(State::HALF_OPEN);
            success_count_ = 1;
            break;
        case State::HALF_OPEN:
            success_count_++;
            if (success_count_ >= config_.success_threshold) {
                setState(State::CLOSED);
            }
            break;
    }
}

void CircuitBreaker::recordFailure() {
    std::lock_guard<std::mutex> lock(mutex_);

    last_failure_time_ = std::chrono::steady_clock::now();

    switch (state_) {
        case State::CLOSED:
            failure_count_++;
            if (failure_count_ >= config_.failure_threshold) {
                setState(State::OPEN);
            }
            break;
        case State::OPEN:
            // Already open, stay open
            break;
        case State::HALF_OPEN:
            setState(State::OPEN);
            break;
    }
}

void CircuitBreaker::setState(State new_state) {
    state_ = new_state;
    success_count_ = 0;
    if (new_state == State::CLOSED) {
        failure_count_ = 0;
    }
}

// ConnectionFactory implementation
std::unique_ptr<PooledConnection> ConnectionFactory::create(
    const std::string& host,
    const PoolConfig& config
) {
    return std::make_unique<PooledConnection>(host);
}

std::unordered_map<std::string, std::string> ConnectionFactory::getDefaultHeaders() {
    return {
        {"User-Agent", "aimux/2.0"},
        {"Accept", "application/json"},
        {"Accept-Encoding", "gzip, deflate"},
        {"Connection", "keep-alive"}
    };
}

} // namespace network
} // namespace aimux