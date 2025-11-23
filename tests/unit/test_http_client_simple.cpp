/**
 * Simple HTTP Client Test Suite - Focused testing for Aimux v2.0.0 HTTP client
 * Tests basic requests without complex dependencies
 */

#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <future>

// Include core C++ standard library features
#include <string>
#include <string_view>
#include <memory>
#include <unordered_map>
#include <expected>
#include <atomic>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>

// External dependencies
#include <curl/curl.h>

// Aimux HTTP client interface (minimal version for testing)
namespace aimux {

// Enums for HTTP operations
enum class HttpMethod {
    GET, POST, PUT, DELETE, PATCH, HEAD, OPTIONS
};

enum class HttpError {
    Timeout, ConnectionFailure, SslError, DnsError,
    NetworkError, ProtocolError, InvalidUrl, RateLimited,
    ServerError, ClientError, Unknown
};

// Structures for HTTP operations
using Headers = std::unordered_map<std::string, std::string>;

struct HttpRequest {
    std::string url;
    HttpMethod method;
    std::string body;
    Headers headers;
    std::chrono::milliseconds timeout{30000};

    HttpRequest() = default;
    HttpRequest(std::string_view url, HttpMethod method = HttpMethod::GET)
        : url(url), method(method) {}
};

struct HttpResponse {
    int status_code{0};
    std::string body;
    Headers headers;
    std::chrono::milliseconds elapsed_ms{0};
    std::chrono::milliseconds connect_time_ms{0};
    std::chrono::milliseconds name_lookup_time_ms{0};
};

struct HttpClientConfig {
    std::chrono::milliseconds default_timeout{30000};
    std::chrono::milliseconds connect_timeout{10000};
    size_t max_redirects{5};
    bool follow_redirects{true};
    bool verify_ssl{true};
    std::string user_agent{"Aimux-Test/2.0.0"};
    size_t connection_pool_size{5};
};

struct HttpMetrics {
    std::atomic<uint64_t> total_requests{0};
    std::atomic<uint64_t> successful_requests{0};
    std::atomic<uint64_t> failed_requests{0};
    std::atomic<uint64_t> total_response_time_ms{0};
    std::atomic<uint64_t> bytes_sent{0};
    std::atomic<uint64_t> bytes_received{0};

    uint64_t get_total_requests() const { return total_requests.load(); }
    uint64_t get_successful_requests() const { return successful_requests.load(); }
    uint64_t get_failed_requests() const { return failed_requests.load(); }
    std::chrono::milliseconds get_total_response_time() const {
        return std::chrono::milliseconds(total_response_time_ms.load());
    }
    uint64_t get_bytes_sent() const { return bytes_sent.load(); }
    uint64_t get_bytes_received() const { return bytes_received.load(); }
};

using HttpResult = std::expected<HttpResponse, HttpError>;

// Simplified HTTP client implementation for testing
class HttpClient {
public:
    explicit HttpClient(HttpClientConfig config = {});
    ~HttpClient();

    // Non-copyable, movable
    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;
    HttpClient(HttpClient&&) noexcept;
    HttpClient& operator=(HttpClient&&) noexcept;

    HttpResult send(const HttpRequest& request);
    HttpResult send(std::string_view url, HttpMethod method = HttpMethod::GET,
                   std::string_view body = {}, const Headers& headers = {});
    HttpResult get(std::string_view url, const Headers& headers = {});
    HttpResult post(std::string_view url, std::string_view body, const Headers& headers = {});

    const HttpClientConfig& getConfig() const noexcept { return config_; }
    void setConfig(HttpClientConfig config) { config_ = std::move(config); }
    const HttpMetrics& getMetrics() const noexcept { return metrics_; }
    void resetMetrics() noexcept;

    std::string_view methodToString(HttpMethod method) const noexcept;
    std::string errorToString(HttpError error) const noexcept;

private:
    HttpResult executeRequest(CURL* curl, const HttpRequest& request);
    CURL* acquireConnection();
    void releaseConnection(CURL* connection);
    void setupCurlOptions(CURL* curl, const HttpRequest& request);
    HttpResponse parseResponse(CURL* curl, CURLcode result, std::string& response_data);
    HttpError curlCodeToError(CURLcode code) const;
    void updateMetrics(bool success, std::chrono::milliseconds response_time,
                      size_t bytes_in, size_t bytes_out);

    HttpClientConfig config_;
    HttpMetrics metrics_;
    std::vector<CURL*> connection_pool_;
    std::queue<CURL*> available_connections_;
    mutable std::mutex pool_mutex_;
    std::condition_variable connection_available_;
};

// Callback functions for curl
extern "C" {
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
        auto* response_data = static_cast<std::string*>(userp);
        response_data->append(static_cast<char*>(contents), size * nmemb);
        return size * nmemb;
    }

    static size_t HeaderCallback(char* buffer, size_t size, size_t nitems, void* userp) {
        auto* headers = static_cast<Headers*>(userp);
        const std::string_view header(buffer, size * nitems);

        const size_t colon_pos = header.find(':');
        if (colon_pos != std::string_view::npos && colon_pos + 1 < header.size()) {
            std::string key{header.substr(0, colon_pos)};
            std::string value{header.substr(colon_pos + 1)};

            key.erase(0, key.find_first_not_of(" \t\r\n"));
            key.erase(key.find_last_not_of(" \t\r\n") + 1);

            value.erase(0, value.find_first_not_of(" \t\r\n"));
            value.erase(value.find_last_not_of(" \t\r\n") + 1);

            if (!key.empty()) {
                (*headers)[std::move(key)] = std::move(value);
            }
        }

        return size * nitems;
    }
}

// RAII Curl initialization guard
class CurlGuard {
public:
    CurlGuard() {
        if (!initialized) {
            curl_global_init(CURL_GLOBAL_DEFAULT);
            initialized = true;
        }
    }
    ~CurlGuard() {}
private:
    static bool initialized;
};

bool CurlGuard::initialized = false;

HttpClient::HttpClient(HttpClientConfig config)
    : config_(std::move(config)) {

    static CurlGuard curl_guard;

    connection_pool_.reserve(config_.connection_pool_size);
    for (size_t i = 0; i < config_.connection_pool_size; ++i) {
        if (auto curl = curl_easy_init()) {
            connection_pool_.push_back(curl);
            std::lock_guard<std::mutex> lock(pool_mutex_);
            available_connections_.push(curl);
        }
    }
}

HttpClient::~HttpClient() {
    std::lock_guard<std::mutex> lock(pool_mutex_);

    while (!available_connections_.empty()) {
        available_connections_.pop();
    }

    for (auto* curl : connection_pool_) {
        if (curl) {
            curl_easy_cleanup(curl);
        }
    }
    connection_pool_.clear();
}

HttpClient::HttpClient(HttpClient&& other) noexcept
    : config_(std::move(other.config_)), connection_pool_(std::move(other.connection_pool_)) {

    std::lock_guard<std::mutex> lock(pool_mutex_);
    std::lock_guard<std::mutex> other_lock(other.pool_mutex_);
    std::swap(available_connections_, other.available_connections_);
}

HttpClient& HttpClient::operator=(HttpClient&& other) noexcept {
    if (this != &other) {
        std::lock_guard<std::mutex> lock1(pool_mutex_);
        std::lock_guard<std::mutex> lock2(other.pool_mutex_);

        config_ = std::move(other.config_);
        connection_pool_ = std::move(other.connection_pool_);
        available_connections_ = std::move(other.available_connections_);
    }
    return *this;
}

HttpResult HttpClient::send(const HttpRequest& request) {
    const auto start_time = std::chrono::steady_clock::now();
    auto* curl = acquireConnection();

    HttpResult result = executeRequest(curl, request);

    const auto end_time = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    const size_t bytes_in = request.body.size();
    const size_t bytes_out = result ? result->body.size() : 0;

    updateMetrics(result.has_value(), elapsed, bytes_in, bytes_out);
    releaseConnection(curl);
    return result;
}

HttpResult HttpClient::send(std::string_view url, HttpMethod method,
                           std::string_view body, const Headers& headers) {
    HttpRequest request;
    request.url = url;
    request.method = method;
    request.body = body;
    request.headers = headers;
    request.timeout = config_.default_timeout;

    return send(request);
}

HttpResult HttpClient::get(std::string_view url, const Headers& headers) {
    return send(url, HttpMethod::GET, {}, headers);
}

HttpResult HttpClient::post(std::string_view url, std::string_view body, const Headers& headers) {
    return send(url, HttpMethod::POST, body, headers);
}

void HttpClient::resetMetrics() noexcept {
    metrics_.total_requests.store(0);
    metrics_.successful_requests.store(0);
    metrics_.failed_requests.store(0);
    metrics_.total_response_time_ms.store(0);
    metrics_.bytes_sent.store(0);
    metrics_.bytes_received.store(0);
}

std::string_view HttpClient::methodToString(HttpMethod method) const noexcept {
    switch (method) {
        case HttpMethod::GET: return "GET";
        case HttpMethod::POST: return "POST";
        case HttpMethod::PUT: return "PUT";
        case HttpMethod::DELETE: return "DELETE";
        case HttpMethod::PATCH: return "PATCH";
        case HttpMethod::HEAD: return "HEAD";
        case HttpMethod::OPTIONS: return "OPTIONS";
    }
    return "UNKNOWN";
}

std::string HttpClient::errorToString(HttpError error) const noexcept {
    switch (error) {
        case HttpError::Timeout: return "Request timeout";
        case HttpError::ConnectionFailure: return "Connection failed";
        case HttpError::SslError: return "SSL/TLS error";
        case HttpError::DnsError: return "DNS resolution failed";
        case HttpError::NetworkError: return "Network error";
        case HttpError::ProtocolError: return "HTTP protocol error";
        case HttpError::InvalidUrl: return "Invalid URL";
        case HttpError::RateLimited: return "Rate limited";
        case HttpError::ServerError: return "Server error";
        case HttpError::ClientError: return "Client error";
        case HttpError::Unknown: return "Unknown error";
    }
    return "Unrecognized error";
}

HttpResult HttpClient::executeRequest(CURL* curl, const HttpRequest& request) {
    std::string response_data;
    Headers response_headers;

    setupCurlOptions(curl, request);

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, HeaderCallback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response_headers);

    const CURLcode result = curl_easy_perform(curl);

    if (result == CURLE_OK) {
        return std::expected<HttpResponse, HttpError>{parseResponse(curl, result, response_data)};
    } else {
        return std::unexpected{curlCodeToError(result)};
    }
}

CURL* HttpClient::acquireConnection() {
    std::unique_lock<std::mutex> lock(pool_mutex_);

    const auto timeout = std::chrono::milliseconds(config_.default_timeout);

    if (connection_available_.wait_for(lock, timeout, [this] {
        return !available_connections_.empty();
    })) {
        auto* connection = available_connections_.front();
        available_connections_.pop();
        return connection;
    }

    return curl_easy_init();
}

void HttpClient::releaseConnection(CURL* connection) {
    if (!connection) return;

    std::lock_guard<std::mutex> lock(pool_mutex_);

    curl_easy_reset(connection);

    const bool is_permanent = std::find(
        connection_pool_.begin(),
        connection_pool_.end(),
        connection) != connection_pool_.end();

    if (is_permanent) {
        available_connections_.push(connection);
        connection_available_.notify_one();
    } else {
        curl_easy_cleanup(connection);
    }
}

void HttpClient::setupCurlOptions(CURL* curl, const HttpRequest& request) {
    curl_easy_setopt(curl, CURLOPT_URL, request.url.c_str());

    switch (request.method) {
        case HttpMethod::GET:
            curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
            break;
        case HttpMethod::POST:
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            if (!request.body.empty()) {
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.body.c_str());
                curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(request.body.size()));
            }
            break;
        case HttpMethod::PUT:
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
            if (!request.body.empty()) {
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.body.c_str());
                curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(request.body.size()));
            }
            break;
        case HttpMethod::DELETE:
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
            if (!request.body.empty()) {
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.body.c_str());
                curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(request.body.size()));
            }
            break;
        case HttpMethod::PATCH:
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
            if (!request.body.empty()) {
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.body.c_str());
                curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(request.body.size()));
            }
            break;
        case HttpMethod::HEAD:
            curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
            break;
        case HttpMethod::OPTIONS:
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "OPTIONS");
            break;
    }

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, ("User-Agent: " + config_.user_agent).c_str());
    headers = curl_slist_append(headers, "Accept: application/json");

    if (!request.headers.contains("Content-Type") && !request.headers.contains("Content-Length")) {
        headers = curl_slist_append(headers, "Content-Type: application/json");
    }

    for (const auto& [key, value] : request.headers) {
        std::string header = key + ": " + value;
        headers = curl_slist_append(headers, header.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, static_cast<long>(request.timeout.count()));
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, static_cast<long>(config_.connect_timeout.count()));

    if (config_.follow_redirects) {
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_MAXREDIRS, static_cast<long>(config_.max_redirects));
    }

    if (config_.verify_ssl) {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    } else {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    }

    curl_slist_free_all(headers);
}

HttpResponse HttpClient::parseResponse(CURL* curl, CURLcode, std::string& response_data) {
    HttpResponse response;

    long response_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    response.status_code = static_cast<int>(response_code);
    response.body = std::move(response_data);

    double total_time, connect_time, name_lookup_time;
    curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &total_time);
    curl_easy_getinfo(curl, CURLINFO_CONNECT_TIME, &connect_time);
    curl_easy_getinfo(curl, CURLINFO_NAMELOOKUP_TIME, &name_lookup_time);

    response.elapsed_ms = std::chrono::milliseconds(static_cast<long>(total_time * 1000.0));
    response.connect_time_ms = std::chrono::milliseconds(static_cast<long>(connect_time * 1000.0));
    response.name_lookup_time_ms = std::chrono::milliseconds(static_cast<long>(name_lookup_time * 1000.0));

    return response;
}

HttpError HttpClient::curlCodeToError(CURLcode code) const {
    switch (code) {
        case CURLE_OPERATION_TIMEDOUT:
            return HttpError::Timeout;
        case CURLE_COULDNT_CONNECT:
            return HttpError::ConnectionFailure;
        case CURLE_COULDNT_RESOLVE_HOST:
        case CURLE_COULDNT_RESOLVE_PROXY:
            return HttpError::DnsError;
        case CURLE_SSL_CONNECT_ERROR:
        case CURLE_PEER_FAILED_VERIFICATION:
            return HttpError::SslError;
        case CURLE_URL_MALFORMAT:
            return HttpError::InvalidUrl;
        case CURLE_HTTP_RETURNED_ERROR:
            return HttpError::ServerError;
        case CURLE_RECV_ERROR:
        case CURLE_SEND_ERROR:
            return HttpError::NetworkError;
        default:
            return HttpError::Unknown;
    }
}

void HttpClient::updateMetrics(bool success, std::chrono::milliseconds response_time,
                              size_t bytes_in, size_t bytes_out) {
    metrics_.total_requests.fetch_add(1);

    if (success) {
        metrics_.successful_requests.fetch_add(1);
    } else {
        metrics_.failed_requests.fetch_add(1);
    }

    metrics_.total_response_time_ms.fetch_add(static_cast<uint64_t>(response_time.count()));
    metrics_.bytes_sent.fetch_add(bytes_in);
    metrics_.bytes_received.fetch_add(bytes_out);
}

} // namespace aimux

// Test cases
using namespace aimux;
using namespace std::chrono_literals;

class HttpClientTest : public ::testing::Test {
protected:
    void SetUp() override {
        HttpClientConfig config;
        config.default_timeout = 30000ms;
        config.connect_timeout = 10000ms;
        config.user_agent = "Aimux-Test/2.0.0";
        config.connection_pool_size = 3;

        client = std::make_unique<HttpClient>(config);
    }

    void TearDown() override {
        client.reset();
    }

    std::unique_ptr<HttpClient> client;
};

TEST_F(HttpClientTest, BasicGetRequest) {
    HttpResult result = client->get("https://httpbin.org/get");

    ASSERT_TRUE(result.has_value()) << "GET request should succeed. Error: " << client->errorToString(result.error());
    EXPECT_EQ(result->status_code, 200);
    EXPECT_FALSE(result->body.empty());
    EXPECT_GT(result->elapsed_ms.count(), 0);

    EXPECT_TRUE(result->body.find("\"url\":") != std::string::npos);
    EXPECT_TRUE(result->body.find("httpbin.org/get") != std::string::npos);

    std::cout << "GET request completed in " << result->elapsed_ms.count() << "ms\n";
}

TEST_F(HttpClientTest, BasicPostRequest) {
    const std::string json_body = R"({"test": "value", "number": 42})";
    const Headers headers = {{"Content-Type", "application/json"}};

    HttpResult result = client->post("https://httpbin.org/post", json_body, headers);

    ASSERT_TRUE(result.has_value()) << "POST request should succeed. Error: " << client->errorToString(result.error());
    EXPECT_EQ(result->status_code, 200);
    EXPECT_FALSE(result->body.empty());

    EXPECT_TRUE(result->body.find("\"test\": \"value\"") != std::string::npos);
    EXPECT_TRUE(result->body.find("\"number\": 42") != std::string::npos);

    std::cout << "POST request completed in " << result->elapsed_ms.count() << "ms\n";
}

TEST_F(HttpClientTest, TimeoutTest) {
    HttpClientConfig config;
    config.default_timeout = 1000ms; // 1 second timeout
    config.connect_timeout = 500ms;

    HttpClient timeout_client(config);

    HttpResult result = timeout_client.get("https://httpbin.org/delay/5");

    EXPECT_FALSE(result.has_value()) << "Request should timeout";
    EXPECT_EQ(result.error(), HttpError::Timeout);

    std::cout << "Request correctly timed out\n";
}

TEST_F(HttpClientTest, ErrorHandling) {
    HttpResult result = client->get("not-a-valid-url");

    EXPECT_FALSE(result.has_value()) << "Invalid URL should fail";
    EXPECT_EQ(result.error(), HttpError::InvalidUrl);

    std::cout << "Invalid URL properly rejected\n";
}

TEST_F(HttpClientTest, MetricsTracking) {
    client->resetMetrics();

    client->get("https://httpbin.org/get");
    client->get("https://httpbin.org/delay/1");

    const auto& metrics = client->getMetrics();

    EXPECT_EQ(metrics.get_total_requests(), 2);
    EXPECT_EQ(metrics.get_successful_requests(), 2);
    EXPECT_EQ(metrics.get_failed_requests(), 0);
    EXPECT_GT(metrics.get_bytes_sent(), 0);
    EXPECT_GT(metrics.get_bytes_received(), 0);
    EXPECT_GT(metrics.get_total_response_time().count(), 0);

    std::cout << "Metrics after 2 requests: "
              << metrics.get_total_requests() << " total, "
              << metrics.get_successful_requests() << " successful\n";
}