#include "aimux/webui/metrics_streamer.hpp"
#include <algorithm>
#include <random>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <atomic>
#include <cstring>

#ifdef __linux__
#include <sys/sysinfo.h>
#include <sys/resource.h>
#include <unistd.h>
#endif

namespace aimux {
namespace webui {

MetricsStreamer& MetricsStreamer::getInstance() {
    static MetricsStreamer instance;
    return instance;
}

bool MetricsStreamer::initialize(const MetricsStreamerConfig& config) {
    if (running_.load()) {
        return true; // Already initialized
    }

    config_ = config;

    // Initialize system metrics
    system_metrics_.start_time = std::chrono::steady_clock::now();

#ifdef __linux__
    struct sysinfo info;
    if (sysinfo(&info) == 0) {
        system_metrics_.cpu.cores = info.procs;
        system_metrics_.memory.total_mb = info.totalram * info.mem_unit / (1024 * 1024);
        system_metrics_.uptime_seconds = static_cast<uint64_t>(info.uptime);
    }
#endif

    // Start background threads
    running_.store(true);
    metrics_thread_ = std::thread(&MetricsStreamer::metrics_collection_loop, this);
    broadcast_thread_ = std::thread(&MetricsStreamer::websocket_broadcast_loop, this);

    std::cout << "MetricsStreamer initialized with " << config_.update_interval_ms
              << "ms update interval and " << config_.broadcast_interval_ms
              << "ms broadcast interval" << std::endl;

    return true;
}

void MetricsStreamer::shutdown() {
    if (!running_.load()) {
        return; // Already shutdown
    }

    running_.store(false);

    // Notify threads to stop
    {
        std::lock_guard<std::mutex> lock(stop_mutex_);
        stop_cv_.notify_all();
    }

    // Wait for threads to finish
    if (metrics_thread_.joinable()) {
        metrics_thread_.join();
    }

    if (broadcast_thread_.joinable()) {
        broadcast_thread_.join();
    }

    // Close all WebSocket connections
    {
        std::shared_lock<std::shared_mutex> lock(connections_mutex_);
        for (const auto& [id, conn] : connections_) {
            if (conn && conn->connection) {
                try {
                    conn->connection->close();
                } catch (const std::exception& e) {
                    std::cerr << "Error closing WebSocket connection " << id << ": " << e.what() << std::endl;
                }
            }
        }
    }

    std::cout << "MetricsStreamer shutdown completed" << std::endl;
}

std::string MetricsStreamer::register_connection(crow::websocket::connection* conn, const std::string& client_info) {
    if (!conn || connections_.size() >= config_.max_connections) {
        return "";
    }

    std::string connection_id = generate_connection_id();

    {
        std::unique_lock<std::shared_mutex> lock(connections_mutex_);
        auto ws_conn = std::make_unique<WebSocketConnection>(conn, connection_id);
        ws_conn->client_info = client_info;
        connections_[connection_id] = std::move(ws_conn);
    }

    // Update performance stats
    {
        std::lock_guard<std::mutex> lock(performance_mutex_);
        performance_stats_.current_connections = connections_.size();
        performance_stats_.peak_connections = std::max(performance_stats_.peak_connections,
                                                      performance_stats_.current_connections);
    }

    std::cout << "WebSocket connection registered: " << connection_id
              << " (client: " << client_info << ")" << std::endl;

    return connection_id;
}

void MetricsStreamer::unregister_connection(const std::string& connection_id) {
    {
        std::unique_lock<std::shared_mutex> lock(connections_mutex_);
        auto it = connections_.find(connection_id);
        if (it != connections_.end()) {
            connections_.erase(it);
        }
    }

    // Update performance stats
    {
        std::lock_guard<std::mutex> lock(performance_mutex_);
        performance_stats_.current_connections = connections_.size();
    }

    std::cout << "WebSocket connection unregistered: " << connection_id << std::endl;
}

void MetricsStreamer::handle_message(const std::string& connection_id, const std::string& message) {
    try {
        nlohmann::json msg = nlohmann::json::parse(message);
        std::string type = msg.value("type", "");

        {
            std::shared_lock<std::shared_mutex> lock(connections_mutex_);
            auto it = connections_.find(connection_id);
            if (it != connections_.end()) {
                it->second->messages_received++;
                it->second->last_ping = std::chrono::steady_clock::now();
            }
        }

        if (type == "request_update") {
            // Send immediate update
            auto data = get_comprehensive_metrics();
            send_to_connection(connection_id, data);

        } else if (type == "auth") {
            handle_connection_request(connection_id, msg);

        } else if (type == "ping") {
            handle_ping_pong(connection_id, msg);

        } else {
            // Send error response for unknown message types
            nlohmann::json error = {
                {"type", "error"},
                {"message", "Unknown message type: " + type},
                {"timestamp", get_current_timestamp()}
            };
            send_to_connection(connection_id, error);
        }

    } catch (const nlohmann::json::parse_error& e) {
        nlohmann::json error = {
            {"type", "error"},
            {"message", "Invalid JSON: " + std::string(e.what())},
            {"timestamp", get_current_timestamp()}
        };
        send_to_connection(connection_id, error);

    } catch (const std::exception& e) {
        std::cerr << "Error handling WebSocket message from " << connection_id
                  << ": " << e.what() << std::endl;
    }
}

void MetricsStreamer::update_provider_metrics(const std::string& provider_name,
                                             double response_time_ms,
                                             bool success,
                                             const std::string& error_type,
                                             uint64_t input_tokens,
                                             uint64_t output_tokens,
                                             double cost) {
    std::unique_lock<std::shared_mutex> lock(providers_mutex_);

    ProviderMetrics& metrics = provider_metrics_[provider_name];
    metrics.name = provider_name;

    // Update request counters
    metrics.total_requests++;
    if (success) {
        // This will be properly calculated in the metrics collection loop
    } else {
        if (!error_type.empty()) {
            metrics.error_breakdown[error_type]++;
        }
    }

    // Store response time for percentile calculations
    // In a production system, you'd want a more sophisticated windowed approach
    metrics.avg_response_time_ms = (metrics.avg_response_time_ms * (metrics.total_requests - 1) + response_time_ms) / metrics.total_requests;

    // Update token usage
    metrics.tokens_used.input += input_tokens;
    metrics.tokens_used.output += output_tokens;
    metrics.tokens_used.total += input_tokens + output_tokens;

    // Update cost
    metrics.total_cost += cost;

    // Update last request time
    metrics.last_request_time = std::chrono::steady_clock::now();

    // Update status
    if (!success) {
        if (error_type == "rate_limit") {
            metrics.status = "rate_limited";
            metrics.rate_limited = true;
            metrics.rate_limit_until = std::chrono::steady_clock::now() + std::chrono::minutes(1);
        } else if (error_type == "auth") {
            metrics.status = "unhealthy";
        } else {
            metrics.status = "degraded";
        }
    } else {
        metrics.status = "healthy";
        metrics.rate_limited = false;
    }
}

nlohmann::json MetricsStreamer::get_comprehensive_metrics() const {
    nlohmann::json data;
    data["timestamp"] = get_current_timestamp();
    data["seq"] = performance_stats_.total_updates;
    data["update_type"] = "comprehensive_metrics";

    // Provider metrics
    {
        std::shared_lock<std::shared_mutex> lock(providers_mutex_);
        data["providers"] = nlohmann::json::object();
        for (const auto& [name, metrics] : provider_metrics_) {
            data["providers"][name] = metrics.to_json();
        }
    }

    // System metrics
    {
        std::shared_lock<std::shared_mutex> lock(system_mutex_);
        data["system"] = system_metrics_.to_json();
    }

    // Historical data
    {
        std::shared_lock<std::shared_mutex> lock(history_mutex_);
        data["historical"] = historical_data_.to_json();
    }

    // Performance stats
    {
        std::lock_guard<std::mutex> lock(performance_mutex_);
        data["performance"] = {
            {"current_connections", performance_stats_.current_connections},
            {"peak_connections", performance_stats_.peak_connections},
            {"total_updates", performance_stats_.total_updates},
            {"total_broadcasts", performance_stats_.total_broadcasts},
            {"avg_update_time_ms", performance_stats_.avg_update_time_ms},
            {"avg_broadcast_time_ms", performance_stats_.avg_broadcast_time_ms}
        };
    }

    return data;
}

ProviderMetrics MetricsStreamer::get_provider_metrics(const std::string& provider_name) const {
    std::shared_lock<std::shared_mutex> lock(providers_mutex_);
    auto it = provider_metrics_.find(provider_name);
    if (it != provider_metrics_.end()) {
        return it->second;
    }
    return ProviderMetrics{}; // Return empty metrics if not found
}

SystemMetrics MetricsStreamer::get_system_metrics() const {
    std::shared_lock<std::shared_mutex> lock(system_mutex_);
    return system_metrics_;
}

HistoricalData MetricsStreamer::get_historical_data() const {
    std::shared_lock<std::shared_mutex> lock(history_mutex_);
    return historical_data_;
}

MetricsStreamer::PerformanceStats MetricsStreamer::get_performance_stats() const {
    std::lock_guard<std::mutex> lock(performance_mutex_);
    return performance_stats_;
}

void MetricsStreamer::metrics_collection_loop() {
    std::cout << "Metrics collection loop started" << std::endl;

    auto update_interval = std::chrono::milliseconds(config_.update_interval_ms);
    auto last_update = std::chrono::steady_clock::now();

    while (running_.load()) {
        try {
            auto start_time = std::chrono::steady_clock::now();

            // Update system metrics
            update_system_metrics();

            // Update provider metrics calculations
            {
                std::unique_lock<std::shared_mutex> lock(providers_mutex_);
                auto now = std::chrono::steady_clock::now();

                for (auto& [name, metrics] : provider_metrics_) {
                    // Calculate requests per second
                    auto time_diff = std::chrono::duration_cast<std::chrono::seconds>(now - metrics.last_request_time);
                    if (time_diff.count() < 60) {
                        metrics.requests_last_minute = metrics.total_requests;
                    } else {
                        metrics.requests_last_minute = 0;
                    }

                    // Reset rate limiting if expired
                    if (metrics.rate_limited && now > metrics.rate_limit_until) {
                        metrics.rate_limited = false;
                        metrics.status = "healthy";
                    }

                    // Calculate success rate
                    uint64_t total_errors = 0;
                    for (const auto& [error_type, count] : metrics.error_breakdown) {
                        total_errors += count;
                    }

                    if (metrics.total_requests > 0) {
                        metrics.success_rate = (static_cast<double>(metrics.total_requests - total_errors) / metrics.total_requests) * 100.0;
                    }

                    // Calculate cost per hour (based on recent activity)
                    if (metrics.total_cost > 0) {
                        auto uptime_hours = std::chrono::duration_cast<std::chrono::hours>(now - system_metrics_.start_time).count();
                        if (uptime_hours > 0) {
                            metrics.cost_per_hour = metrics.total_cost / uptime_hours;
                        }
                    }
                }
            }

            // Update historical data
            update_historical_data();

            // Update performance statistics
            auto end_time = std::chrono::steady_clock::now();
            auto update_duration = std::chrono::duration<double, std::milli>(end_time - start_time).count();

            {
                std::lock_guard<std::mutex> lock(performance_mutex_);
                performance_stats_.total_updates++;

                // Calculate rolling average
                performance_stats_.avg_update_time_ms =
                    (performance_stats_.avg_update_time_ms * (performance_stats_.total_updates - 1) + update_duration) /
                    performance_stats_.total_updates;
            }

        } catch (const std::exception& e) {
            std::cerr << "Error in metrics collection loop: " << e.what() << std::endl;
        }

        // Sleep until next update
        std::this_thread::sleep_until(last_update + update_interval);
        last_update = std::chrono::steady_clock::now();
    }

    std::cout << "Metrics collection loop stopped" << std::endl;
}

void MetricsStreamer::websocket_broadcast_loop() {
    std::cout << "WebSocket broadcast loop started" << std::endl;

    auto broadcast_interval = std::chrono::milliseconds(config_.broadcast_interval_ms);
    auto last_broadcast = std::chrono::steady_clock::now();

    while (running_.load()) {
        try {
            auto start_time = std::chrono::steady_clock::now();

            // Cleanup stale connections
            cleanup_stale_connections();

            // Broadcast to all connected clients
            uint64_t sequence_num = performance_stats_.total_broadcasts;
            auto data = create_comprehensive_message(sequence_num);
            broadcast_to_all_connections(data);

            // Update performance statistics
            auto end_time = std::chrono::steady_clock::now();
            auto broadcast_duration = std::chrono::duration<double, std::milli>(end_time - start_time).count();

            {
                std::lock_guard<std::mutex> lock(performance_mutex_);
                performance_stats_.total_broadcasts++;

                // Calculate rolling average
                performance_stats_.avg_broadcast_time_ms =
                    (performance_stats_.avg_broadcast_time_ms * (performance_stats_.total_broadcasts - 1) + broadcast_duration) /
                    performance_stats_.total_broadcasts;
            }

        } catch (const std::exception& e) {
            std::cerr << "Error in WebSocket broadcast loop: " << e.what() << std::endl;
        }

        // Sleep until next broadcast
        std::this_thread::sleep_until(last_broadcast + broadcast_interval);
        last_broadcast = std::chrono::steady_clock::now();
    }

    std::cout << "WebSocket broadcast loop stopped" << std::endl;
}

void MetricsStreamer::update_system_metrics() {
    std::unique_lock<std::shared_mutex> lock(system_mutex_);
    auto now = std::chrono::steady_clock::now();

    // Update uptime
    system_metrics_.uptime_seconds = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
        now - system_metrics_.start_time).count());

    // CPU usage (simplified - in production, use proper CPU monitoring)
    system_metrics_.cpu.current_percent = calculate_cpu_usage();
    if (system_metrics_.cpu.current_percent < 20) {
        system_metrics_.cpu.load = "light";
    } else if (system_metrics_.cpu.current_percent < 70) {
        system_metrics_.cpu.load = "moderate";
    } else {
        system_metrics_.cpu.load = "heavy";
    }

    // Memory usage
    system_metrics_.memory.used_mb = get_memory_usage();
    if (system_metrics_.memory.total_mb > 0) {
        system_metrics_.memory.percent = (static_cast<double>(system_metrics_.memory.used_mb) / system_metrics_.memory.total_mb) * 100.0;
        system_metrics_.memory.available_mb = system_metrics_.memory.total_mb - system_metrics_.memory.used_mb;
    }

    // Network metrics (simplified)
    system_metrics_.network.connections = connections_.size();
    system_metrics_.network.last_activity = now;

    // Calculate requests per second from provider metrics
    double total_rps = 0.0;
    {
        std::shared_lock<std::shared_mutex> providers_lock(providers_mutex_);
        for (const auto& [name, metrics] : provider_metrics_) {
            total_rps += metrics.requests_per_second;
        }
    }
    system_metrics_.requests_per_second = total_rps;
}

void MetricsStreamer::broadcast_to_all_connections(const nlohmann::json& data) {
    std::shared_lock<std::shared_mutex> lock(connections_mutex_);
    std::string data_str = data.dump();

    for (const auto& [id, conn] : connections_) {
        if (conn && conn->connection) {
            try {
                conn->connection->send_text(data_str);
                conn->messages_sent++;

                {
                    std::lock_guard<std::mutex> perf_lock(performance_mutex_);
                    performance_stats_.messages_sent++;
                }

            } catch (const std::exception& e) {
                std::cerr << "Failed to send data to connection " << id << ": " << e.what() << std::endl;

                {
                    std::lock_guard<std::mutex> perf_lock(performance_mutex_);
                    performance_stats_.messages_dropped++;
                }
            }
        }
    }
}

void MetricsStreamer::send_to_connection(const std::string& connection_id, const nlohmann::json& data) {
    std::shared_lock<std::shared_mutex> lock(connections_mutex_);
    auto it = connections_.find(connection_id);

    if (it != connections_.end() && it->second && it->second->connection) {
        try {
            it->second->connection->send_text(data.dump());
            it->second->messages_sent++;

            {
                std::lock_guard<std::mutex> perf_lock(performance_mutex_);
                performance_stats_.messages_sent++;
            }

        } catch (const std::exception& e) {
            std::cerr << "Failed to send data to connection " << connection_id << ": " << e.what() << std::endl;

            {
                std::lock_guard<std::mutex> perf_lock(performance_mutex_);
                performance_stats_.messages_dropped++;
            }
        }
    }
}

void MetricsStreamer::cleanup_stale_connections() {
    auto now = std::chrono::steady_clock::now();
    auto timeout = std::chrono::milliseconds(config_.connection_timeout_ms);

    std::unique_lock<std::shared_mutex> lock(connections_mutex_);
    std::vector<std::string> stale_connections;

    for (const auto& [id, conn] : connections_) {
        if (now - conn->last_ping > timeout) {
            stale_connections.push_back(id);
        }
    }

    for (const std::string& id : stale_connections) {
        if (auto it = connections_.find(id); it != connections_.end()) {
            try {
                if (it->second && it->second->connection) {
                    it->second->connection->close();
                }
            } catch (const std::exception& e) {
                std::cerr << "Error closing stale connection " << id << ": " << e.what() << std::endl;
            }
            connections_.erase(it);
        }
    }

    if (!stale_connections.empty()) {
        std::cout << "Cleaned up " << stale_connections.size() << " stale WebSocket connections" << std::endl;

        // Update performance stats
        {
            std::lock_guard<std::mutex> perf_lock(performance_mutex_);
            performance_stats_.current_connections = connections_.size();
            performance_stats_.failed_connections += stale_connections.size();
        }
    }
}

void MetricsStreamer::update_historical_data() {
    std::unique_lock<std::shared_mutex> lock(history_mutex_);

    // Calculate averages from current metrics
    double avg_response_time = 0.0;
    double avg_success_rate = 100.0;
    uint64_t total_requests_per_min = 0;
    double cpu_usage = system_metrics_.cpu.current_percent;
    double memory_usage = system_metrics_.memory.percent;

    {
        std::shared_lock<std::shared_mutex> providers_lock(providers_mutex_);
        for (const auto& [name, metrics] : provider_metrics_) {
            avg_response_time += metrics.avg_response_time_ms;
            avg_success_rate += metrics.success_rate;
            total_requests_per_min += metrics.requests_last_minute;
        }

        if (!provider_metrics_.empty()) {
            avg_response_time /= provider_metrics_.size();
            avg_success_rate /= provider_metrics_.size();
        }
    }

    // Add to historical data
    historical_data_.add_response_time(avg_response_time);
    historical_data_.add_success_rate(avg_success_rate);
    historical_data_.add_requests_per_min(total_requests_per_min);
    historical_data_.add_cpu_usage(cpu_usage);
    historical_data_.add_memory_usage(memory_usage);
}

nlohmann::json MetricsStreamer::create_comprehensive_message(uint64_t sequence_num) const {
    nlohmann::json message;
    message["timestamp"] = get_current_timestamp();
    message["seq"] = sequence_num;
    message["update_type"] = "comprehensive_metrics";

    // Provider metrics
    {
        std::shared_lock<std::shared_mutex> lock(providers_mutex_);
        message["providers"] = nlohmann::json::object();
        for (const auto& [name, metrics] : provider_metrics_) {
            message["providers"][name] = metrics.to_json();
        }
    }

    // System metrics
    {
        std::shared_lock<std::shared_mutex> lock(system_mutex_);
        message["system"] = system_metrics_.to_json();
    }

    // Historical data (last 30 points for bandwidth efficiency)
    {
        std::shared_lock<std::shared_mutex> lock(history_mutex_);
        auto hist_json = historical_data_.to_json();

        // Trim historical data for bandwidth efficiency
        auto trim_array = [](nlohmann::json& arr, size_t max_size) {
            if (arr.is_array() && arr.size() > max_size) {
                auto it = arr.begin();
                std::advance(it, arr.size() - max_size);
                arr = nlohmann::json(it, arr.end());
            }
        };

        trim_array(hist_json["response_times"], 30);
        trim_array(hist_json["success_rates"], 30);
        trim_array(hist_json["requests_per_min"], 30);
        trim_array(hist_json["cpu_usage"], 30);
        trim_array(hist_json["memory_usage"], 30);

        message["historical"] = hist_json;
    }

    return message;
}

std::string MetricsStreamer::generate_connection_id() const {
    static std::atomic<uint64_t> counter{0};
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1000, 9999);

    std::stringstream ss;
    ss << "ws_" << timestamp << "_" << counter.fetch_add(1) << "_" << dis(gen);
    return ss.str();
}

void MetricsStreamer::handle_connection_request(const std::string& connection_id, const nlohmann::json& message) {
    std::string auth_token = message.value("token", "");

    {
        std::shared_lock<std::shared_mutex> lock(connections_mutex_);
        auto it = connections_.find(connection_id);
        if (it != connections_.end()) {
            if (config_.enable_authentication && !authenticate_connection(connection_id, auth_token)) {
                nlohmann::json response = {
                    {"type", "auth_failed"},
                    {"message", "Invalid authentication token"},
                    {"timestamp", get_current_timestamp()}
                };
                send_to_connection(connection_id, response);
                return;
            }

            it->second->authenticated = true;

            nlohmann::json response = {
                {"type", "auth_success"},
                {"connection_id", connection_id},
                {"server_config", {
                    {"update_interval_ms", config_.update_interval_ms},
                    {"broadcast_interval_ms", config_.broadcast_interval_ms},
                    {"delta_compression", config_.enable_delta_compression}
                }},
                {"timestamp", get_current_timestamp()}
            };
            send_to_connection(connection_id, response);
        }
    }
}

void MetricsStreamer::handle_ping_pong(const std::string& connection_id, const nlohmann::json& /* message */) {
    nlohmann::json pong = {
        {"type", "pong"},
        {"timestamp", get_current_timestamp()},
        {"server_time", get_current_timestamp()}
    };
    send_to_connection(connection_id, pong);
}

bool MetricsStreamer::authenticate_connection(const std::string& /* connection_id */, const std::string& auth_token) const {
    if (!config_.enable_authentication) {
        return true;
    }
    return !auth_token.empty() && auth_token == config_.auth_token;
}

uint64_t MetricsStreamer::get_current_timestamp() const {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
}

double MetricsStreamer::calculate_cpu_usage() const {
#ifdef __linux__
    static uint64_t last_idle = 0, last_total = 0;

    std::ifstream file("/proc/stat");
    if (!file.is_open()) {
        return 0.0;
    }

    std::string line;
    if (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string cpu_label;
        uint64_t user, nice, system, idle, iowait, irq, softirq, steal;

        if (iss >> cpu_label >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal) {
            uint64_t total = user + nice + system + idle + iowait + irq + softirq + steal;
            uint64_t diff_idle = idle - last_idle;
            uint64_t diff_total = total - last_total;

            if (diff_total > 0) {
                double usage = 100.0 * (1.0 - static_cast<double>(diff_idle) / diff_total);
                last_idle = idle;
                last_total = total;
                return usage;
            }
        }
    }
#endif

    // Fallback: simulate some CPU usage
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<> dis(1.0, 15.0);
    return dis(gen);
}

uint64_t MetricsStreamer::get_memory_usage() const {
#ifdef __linux__
    std::ifstream file("/proc/self/status");
    if (file.is_open()) {
        std::string line;
        while (std::getline(file, line)) {
            if (line.substr(0, 6) == "VmRSS:") {
                std::istringstream iss(line);
                std::string label, value, unit;
                if (iss >> label >> value >> unit) {
                    return std::stoull(value) / 1024; // Convert KB to MB
                }
            }
        }
    }
#endif

    // Fallback: estimate based on allocation
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<uint64_t> dis(30, 80); // 30-80MB
    return dis(gen);
}

} // namespace webui
} // namespace aimux