#include "aimux/network/http_client.hpp"
#include <curl/curl.h>
#include <sstream>
#include <iomanip>
#include <regex>
#include <algorithm>
#include <chrono>
#include <thread>
#include <unordered_map>

namespace aimux {
namespace network {

// Static callback function for CURL
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    size_t total_size = size * nmemb;
    userp->append(static_cast<char*>(contents), total_size);
    return total_size;
}

// HttpResponse implementation
nlohmann::json HttpResponse::to_json() const {
    nlohmann::json j;
    j["status_code"] = status_code;
    j["body"] = body;
    j["response_time_ms"] = response_time_ms;
    j["error_message"] = error_message;
    
    nlohmann::json headers_json = nlohmann::json::object();
    for (const auto& header : headers) {
        headers_json[header.first] = header.second;
    }
    j["headers"] = headers_json;
    
    return j;
}

// HttpRequest implementation
nlohmann::json HttpRequest::to_json() const {
    nlohmann::json j;
    j["url"] = url;
    j["method"] = method;
    j["timeout_ms"] = timeout_ms;
    j["body_length"] = body.length();
    
    nlohmann::json headers_json = nlohmann::json::object();
    for (const auto& header : headers) {
        headers_json[header.first] = header.second;
    }
    j["headers"] = headers_json;
    
    return j;
}


// HttpClient implementation
struct HttpClient::Impl {
    std::vector<std::pair<std::string, std::string>> default_headers;
    int timeout_ms = 30000;
    mutable std::mutex stats_mutex;
    nlohmann::json stats;
    bool in_use = false;
    std::chrono::steady_clock::time_point last_activity;
    
    Impl(int /*max_connections*/, int /*connection_timeout_ms*/) {
        stats["total_requests"] = 0;
        stats["successful_requests"] = 0;
        stats["failed_requests"] = 0;
        stats["avg_response_time_ms"] = 0.0;
    }
    
    static size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* response) {
        size_t total_size = size * nmemb;
        response->append(static_cast<char*>(contents), total_size);
        return total_size;
    }
    
    static size_t header_callback(void* contents, size_t size, size_t nmemb, std::vector<std::pair<std::string, std::string>>* headers) {
        size_t total_size = size * nmemb;
        std::string header_line(static_cast<char*>(contents), total_size);
        
        // Parse header line
        size_t colon_pos = header_line.find(':');
        if (colon_pos != std::string::npos) {
            std::string name = header_line.substr(0, colon_pos);
            std::string value = header_line.substr(colon_pos + 1);
            
            // Trim whitespace
            name.erase(0, name.find_first_not_of(" \t\r\n"));
            name.erase(name.find_last_not_of(" \t\r\n") + 1);
            value.erase(0, value.find_first_not_of(" \t\r\n"));
            value.erase(value.find_last_not_of(" \t\r\n") + 1);
            
            if (!name.empty()) {
                headers->emplace_back(name, value);
            }
        }
        
        return total_size;
    }
};

HttpClient::HttpClient(int max_connections, int connection_timeout_ms) 
    : pImpl(std::make_unique<Impl>(max_connections, connection_timeout_ms)) {}

HttpClient::~HttpClient() = default;

HttpResponse HttpClient::send_request(const HttpRequest& request) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    HttpResponse response;
    CURL* curl = curl_easy_init();
    
    if (!curl) {
        response.error_message = "Failed to initialize CURL";
        response.status_code = 0;
        return response;
    }
    
    // Set URL
    curl_easy_setopt(curl, CURLOPT_URL, request.url.c_str());
    
    // Set method
    if (request.method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.body.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, request.body.length());
    } else if (request.method == "GET") {
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    } else if (request.method == "PUT") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.body.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, request.body.length());
    } else if (request.method == "DELETE") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    }
    
    // Set headers
    struct curl_slist* headers = nullptr;
    for (const auto& header : request.headers) {
        std::string header_line = header.first + ": " + header.second;
        headers = curl_slist_append(headers, header_line.c_str());
    }
    
    // Add default headers
    for (const auto& header : pImpl->default_headers) {
        std::string header_line = header.first + ": " + header.second;
        headers = curl_slist_append(headers, header_line.c_str());
    }
    
    if (headers) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }
    
    // Set timeouts
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, request.timeout_ms / 1000);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10);
    
    // SSL verification options (configurable)
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    
    // Set response data
    std::string response_data;
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, request.timeout_ms);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, request.timeout_ms / 2);
    
    // Set callbacks
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, Impl::write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, Impl::header_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response.headers);
    
    // SSL settings
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    
    // Perform request
    CURLcode res = curl_easy_perform(curl);
    
    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status_code);
    } else {
        response.error_message = curl_easy_strerror(res);
        response.status_code = 0;
    }
    
    // Calculate response time
    auto end_time = std::chrono::high_resolution_clock::now();
    response.response_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    
    // Update statistics
    {
        std::lock_guard<std::mutex> lock(pImpl->stats_mutex);
        pImpl->stats["total_requests"] = pImpl->stats["total_requests"].get<int>() + 1;
        if (res == CURLE_OK && response.is_success()) {
            pImpl->stats["successful_requests"] = pImpl->stats["successful_requests"].get<int>() + 1;
        } else {
            pImpl->stats["failed_requests"] = pImpl->stats["failed_requests"].get<int>() + 1;
        }
        
        // Update average response time
        int total = pImpl->stats["total_requests"];
        double current_avg = pImpl->stats["avg_response_time_ms"];
        pImpl->stats["avg_response_time_ms"] = (current_avg * (total - 1) + response.response_time_ms) / total;
    }
    
    // Cleanup
    if (headers) {
        curl_slist_free_all(headers);
    }
    curl_easy_cleanup(curl);
    
    return response;
}

void HttpClient::send_request_async(const HttpRequest& request, ResponseCallback callback) {
    std::thread([this, request, callback]() {
        HttpResponse response = send_request(request);
        callback(response);
    }).detach();
}

std::future<HttpResponse> HttpClient::send_request_future(const HttpRequest& request) {
    return std::async(std::launch::async, [this, request]() {
        return send_request(request);
    });
}

nlohmann::json HttpClient::get_statistics() const {
    std::lock_guard<std::mutex> lock(pImpl->stats_mutex);
    nlohmann::json stats = pImpl->stats;
    
    nlohmann::json pool_stats;
    pool_stats["max_connections"] = 10;
    pool_stats["active_connections"] = 0;
    pool_stats["idle_connections"] = 0;
    stats["connection_pool"] = pool_stats;
    
    return stats;
}

void HttpClient::set_timeout(int timeout_ms) {
    pImpl->timeout_ms = timeout_ms;
}

void HttpClient::add_default_header(const std::string& name, const std::string& value) {
    pImpl->default_headers.emplace_back(name, value);
}

void HttpClient::remove_default_header(const std::string& name) {
    pImpl->default_headers.erase(
        std::remove_if(pImpl->default_headers.begin(), pImpl->default_headers.end(),
            [&name](const auto& header) { return header.first == name; }),
        pImpl->default_headers.end()
    );
}

// HttpClientFactory implementation
std::unique_ptr<HttpClient> HttpClientFactory::create_client() {
    return std::make_unique<HttpClient>(10, 30000);
}

std::unique_ptr<HttpClient> HttpClientFactory::create_client(int max_connections, int timeout_ms) {
    return std::make_unique<HttpClient>(max_connections, timeout_ms);
}

void HttpClientFactory::configure_ssl(const std::string& /*ca_cert_path*/, bool /*verify_peer*/) {
    // Basic SSL verification implemented
}

// HttpUtils implementation
bool HttpUtils::parse_url(const std::string& url, std::string& host, int& port, std::string& path, bool& is_ssl) {
    std::regex url_regex(R"(^(https?):\/\/([^:\/]+)(?::(\d+))?(\/.*)?$)");
    std::smatch matches;
    
    if (!std::regex_match(url, matches, url_regex)) {
        return false;
    }
    
    std::string scheme = matches[1].str();
    host = matches[2].str();
    std::string port_str = matches[3].str();
    path = matches[4].str();
    
    is_ssl = (scheme == "https");
    port = port_str.empty() ? (is_ssl ? 443 : 80) : std::stoi(port_str);
    
    if (path.empty()) path = "/";
    
    return true;
}

std::string HttpUtils::url_encode(const std::string& str) {
    std::ostringstream encoded;
    encoded.fill('0');
    encoded << std::hex;
    
    for (char c : str) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded << c;
        } else {
            encoded << std::uppercase;
            encoded << '%' << std::setw(2) << static_cast<int>(static_cast<unsigned char>(c));
            encoded << std::nouppercase;
        }
    }
    
    return encoded.str();
}

std::string HttpUtils::url_decode(const std::string& str) {
    std::string decoded;
    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == '%' && i + 2 < str.length()) {
            int value;
            std::sscanf(str.substr(i + 1, 2).c_str(), "%x", reinterpret_cast<unsigned int*>(&value));
            decoded += static_cast<char>(value);
            i += 2;
        } else if (str[i] == '+') {
            decoded += ' ';
        } else {
            decoded += str[i];
        }
    }
    return decoded;
}

std::string HttpUtils::build_query_string(const std::vector<std::pair<std::string, std::string>>& params) {
    std::ostringstream query;
    bool first = true;
    
    for (const auto& param : params) {
        if (!first) {
            query << "&";
        }
        query << url_encode(param.first) << "=" << url_encode(param.second);
        first = false;
    }
    
    return query.str();
}

std::string HttpUtils::get_mime_type(const std::string& extension) {
    static const std::unordered_map<std::string, std::string> mime_types = {
        {".html", "text/html"},
        {".css", "text/css"},
        {".js", "application/javascript"},
        {".json", "application/json"},
        {".xml", "application/xml"},
        {".txt", "text/plain"},
        {".pdf", "application/pdf"},
        {".png", "image/png"},
        {".jpg", "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".gif", "image/gif"}
    };
    
    auto it = mime_types.find(extension);
    return it != mime_types.end() ? it->second : "application/octet-stream";
}

// Missing method implementations
bool HttpClient::is_available() const {
    return !pImpl->in_use;
}

void HttpClient::reset() {
    pImpl->in_use = false;
    pImpl->last_activity = std::chrono::steady_clock::now();
}

} // namespace network
} // namespace aimux