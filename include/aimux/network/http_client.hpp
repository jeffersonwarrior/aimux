#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <thread>
#include <future>
#include <nlohmann/json.hpp>

namespace aimux {
namespace network {

/**
 * @brief HTTP response structure
 */
struct HttpResponse {
    int status_code = 0;
    std::string body;
    std::vector<std::pair<std::string, std::string>> headers;
    double response_time_ms = 0.0;
    std::string error_message;
    
    bool is_success() const { return status_code >= 200 && status_code < 300; }
    
    nlohmann::json to_json() const;
};

/**
 * @brief HTTP request structure
 */
struct HttpRequest {
    std::string url;
    std::string method = "GET";
    std::vector<std::pair<std::string, std::string>> headers;
    std::string body;
    int timeout_ms = 30000;
    
    nlohmann::json to_json() const;
};

/**
 * @brief Asynchronous HTTP client with connection pooling
 */
class HttpClient {
public:
    /**
     * @brief HTTP response callback type
     */
    using ResponseCallback = std::function<void(const HttpResponse&)>;
    
    /**
     * @brief Constructor
     * @param max_connections Maximum concurrent connections
     * @param connection_timeout_ms Connection timeout in milliseconds
     */
    explicit HttpClient(int max_connections = 10, int connection_timeout_ms = 30000);
    
    /**
     * @brief Destructor
     */
    ~HttpClient();
    
    /**
     * @brief Send HTTP request (synchronous)
     * @param request HTTP request
     * @return HTTP response
     */
    HttpResponse send_request(const HttpRequest& request);
    
    /**
     * @brief Send HTTP request (asynchronous)
     * @param request HTTP request
     * @param callback Response callback
     */
    void send_request_async(const HttpRequest& request, ResponseCallback callback);
    
    /**
     * @brief Send HTTP request (future-based)
     * @param request HTTP request
     * @return Future containing HTTP response
     */
    std::future<HttpResponse> send_request_future(const HttpRequest& request);
    
    /**
     * @brief Get client statistics
     * @return JSON with client metrics
     */
    nlohmann::json get_statistics() const;
    
    /**
     * @brief Set timeout for all requests
     * @param timeout_ms Timeout in milliseconds
     */
    void set_timeout(int timeout_ms);
    
    /**
     * @brief Add default header for all requests
     * @param name Header name
     * @param value Header value
     */
    void add_default_header(const std::string& name, const std::string& value);
    
    /**
     * @brief Remove default header
     * @param name Header name
     */
    void remove_default_header(const std::string& name);
    
    /**
     * @brief Check if client is available for requests
     * @return true if available
     */
    bool is_available() const;
    
    /**
     * @brief Reset client state
     */
    void reset();

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl;
};


/**
 * @brief Factory for creating HTTP clients
 */
class HttpClientFactory {
public:
    /**
     * @brief Create HTTP client with default settings
     * @return Unique pointer to HTTP client
     */
    static std::unique_ptr<HttpClient> create_client();
    
    /**
     * @brief Create HTTP client with custom settings
     * @param max_connections Maximum concurrent connections
     * @param timeout_ms Request timeout in milliseconds
     * @return Unique pointer to HTTP client
     */
    static std::unique_ptr<HttpClient> create_client(int max_connections, int timeout_ms);
    
    /**
     * @brief Configure global SSL settings
     * @param ca_cert_path Path to CA certificate file
     * @param verify_peer Whether to verify peer certificate
     */
    static void configure_ssl(const std::string& ca_cert_path = "", bool verify_peer = true);
};

/**
 * @brief Utility functions for HTTP operations
 */
class HttpUtils {
public:
    /**
     * @brief Parse URL into components
     * @param url URL to parse
     * @param host Output host
     * @param port Output port
     * @param path Output path
     * @param is_ssl Output whether HTTPS
     * @return true if parsing successful
     */
    static bool parse_url(
        const std::string& url,
        std::string& host,
        int& port,
        std::string& path,
        bool& is_ssl
    );
    
    /**
     * @brief URL encode a string
     * @param str String to encode
     * @return Encoded string
     */
    static std::string url_encode(const std::string& str);
    
    /**
     * @brief URL decode a string
     * @param str String to decode
     * @return Decoded string
     */
    static std::string url_decode(const std::string& str);
    
    /**
     * @ Build query string from parameters
     * @param params Parameter map
     * @return Query string
     */
    static std::string build_query_string(
        const std::vector<std::pair<std::string, std::string>>& params
    );
    
    /**
     * @brief Get MIME type from file extension
     * @param extension File extension
     * @return MIME type
     */
    static std::string get_mime_type(const std::string& extension);
};

} // namespace network
} // namespace aimux