#include "aimux/cache/response_cache.hpp"
#include <openssl/sha.h>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <cmath>

namespace aimux {
namespace cache {

// ResponseCache implementation
ResponseCache::ResponseCache(const Config& config) : config_(config) {
    lru_list_.reserve(config_.max_entries);
}

std::string ResponseCache::generateKey(const std::string& model, const nlohmann::json& request) const {
    return KeyGenerator::hashingStrategy(model, request);
}

std::optional<nlohmann::json> ResponseCache::get(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = cache_.find(key);
    if (it == cache_.end()) {
        misses_++;
        return std::nullopt;
    }

    auto& entry = it->second;
    if (entry.isExpired()) {
        cache_.erase(it);
        misses_++;
        return std::nullopt;
    }

    // Update LRU and hit count
    updateLRU(key);
    entry.hit_count++;
    hits_++;

    return entry.response;
}

void ResponseCache::put(const std::string& key, const nlohmann::json& response,
                       std::optional<std::chrono::milliseconds> ttl) {
    std::lock_guard<std::mutex> lock(mutex_);

    CacheEntry entry;
    entry.response = response;
    entry.timestamp = std::chrono::steady_clock::now();
    entry.ttl = ttl ? *ttl : config_.default_ttl;
    entry.response_size = response.dump().length();

    // Apply adaptive TTL if enabled
    if (config_.enable_smart_ttl) {
        entry.ttl = std::chrono::milliseconds(
            static_cast<long long>(entry.ttl.count() * ttl_multiplier_)
        );
    }

    // Enforce memory limits before insertion
    enforceMemoryLimit();

    // If cache is full, evict LRU
    if (cache_.size() >= config_.max_entries) {
        evictLRU();
    }

    cache_[key] = std::move(entry);
    updateLRU(key);
}

void ResponseCache::remove(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    cache_.erase(key);

    // Remove from LRU list
    lru_list_.erase(
        std::remove_if(lru_list_.begin(), lru_list_.end(),
                      [&key](const LRUEntry& e) { return e.key == key; }),
        lru_list_.end()
    );
}

void ResponseCache::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    cache_.clear();
    lru_list_.clear();
}

size_t ResponseCache::cleanup() {
    std::lock_guard<std::mutex> lock(mutex_);

    size_t removed = 0;
    auto it = cache_.begin();
    while (it != cache_.end()) {
        if (it->second.isExpired() || shouldEvict(it->second)) {
            it = cache_.erase(it);
            removed++;
        } else {
            ++it;
        }
    }

    // Cleanup LRU list
    lru_list_.erase(
        std::remove_if(lru_list_.begin(), lru_list_.end(),
                      [this](const LRUEntry& e) {
                          return cache_.find(e.key) == cache_.end();
                      }),
        lru_list_.end()
    );

    return removed;
}

ResponseCache::Stats ResponseCache::getStats() const {
    std::lock_guard<std::mutex> lock(mutex_);

    Stats stats;
    stats.hits = hits_.load();
    stats.misses = misses_.load();
    stats.entries = cache_.size();
    stats.evictions = evictions_.load();

    size_t total_requests = stats.hits + stats.misses;
    stats.hit_rate = total_requests > 0 ?
        static_cast<double>(stats.hits) / total_requests : 0.0;

    // Calculate memory usage
    stats.memory_usage_bytes = 0;
    for (const auto& [key, entry] : cache_) {
        stats.memory_usage_bytes += estimateMemoryUsage(entry);
    }

    return stats;
}

void ResponseCache::resetStats() {
    hits_ = 0;
    misses_ = 0;
    evictions_ = 0;
}

void ResponseCache::updateLRU(const std::string& key) {
    auto now = std::chrono::steady_clock::now();

    // Remove existing entry
    lru_list_.erase(
        std::remove_if(lru_list_.begin(), lru_list_.end(),
                      [&key](const LRUEntry& e) { return e.key == key; }),
        lru_list_.end()
    );

    // Add to front (most recently used)
    lru_list_.insert(lru_list_.begin(), LRUEntry{key, now});
}

void ResponseCache::evictLRU() {
    if (lru_list_.empty()) return;

    // Remove least recently used from back
    auto lru_key = lru_list_.back().key;
    lru_list_.pop_back();
    cache_.erase(lru_key);
    evictions_++;
}

bool ResponseCache::shouldEvict(const CacheEntry& entry) const {
    // Evict if hit rate is too low for this entry's age
    auto age = std::chrono::steady_clock::now() - entry.timestamp;
    auto age_minutes = std::chrono::duration_cast<std::chrono::minutes>(age).count();

    if (age_minutes > 0) {
        double hit_rate = static_cast<double>(entry.hit_count) / age_minutes;
        return hit_rate < config_.hit_rate_threshold;
    }

    return false;
}

size_t ResponseCache::estimateMemoryUsage(const CacheEntry& entry) const {
    size_t usage = sizeof(CacheEntry) + entry.response_size + 256; // Overhead
    return usage;
}

void ResponseCache::enforceMemoryLimit() {
    size_t total_memory = 0;
    for (const auto& [key, entry] : cache_) {
        total_memory += estimateMemoryUsage(entry);
    }

    size_t max_memory_bytes = config_.max_memory_mb * 1024 * 1024;

    while (total_memory > max_memory_bytes && !lru_list_.empty()) {
        auto lru_key = lru_list_.back().key;
        auto it = cache_.find(lru_key);
        if (it != cache_.end()) {
            total_memory -= estimateMemoryUsage(it->second);
            cache_.erase(it);
        }
        lru_list_.pop_back();
        evictions_++;
    }
}

// KeyGenerator implementation
std::string KeyGenerator::hashingStrategy(const std::string& model, const nlohmann::json& request) {
    // Extract core content for caching
    std::string content = extractCoreContent(request);
    std::string combined = model + "|" + content;

    // SHA-256 hash
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(combined.c_str()), combined.length(), hash);

    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << std::setw(2) << static_cast<int>(hash[i]);
    }

    return ss.str();
}

std::string KeyGenerator::semanticStrategy(const std::string& model, const nlohmann::json& request) {
    // Semantic analysis for content similarity
    std::string content = extractCoreContent(request);

    // Normalize content (lowercase, remove extra whitespace)
    std::string normalized;
    bool in_space = false;
    for (char c : content) {
        if (std::isspace(c)) {
            if (!in_space) {
                normalized += ' ';
                in_space = true;
            }
        } else {
            normalized += std::tolower(c);
            in_space = false;
        }
    }

    return model + "|" + normalized.substr(0, 200); // Truncate for cache key
}

std::string KeyGenerator::parameterStrategy(const std::string& model, const nlohmann::json& request) {
    // Focus on key parameters for exact matching
    nlohmann::json key_params;

    if (request.contains("messages")) {
        key_params["messages"] = request["messages"];
    }
    if (request.contains("max_tokens")) {
        key_params["max_tokens"] = request["max_tokens"];
    }
    if (request.contains("temperature")) {
        key_params["temperature"] = request["temperature"];
    }

    return model + "|" + key_params.dump();
}

std::string KeyGenerator::extractCoreContent(const nlohmann::json& request) {
    std::string content;

    if (request.contains("messages") && request["messages"].is_array()) {
        for (const auto& msg : request["messages"]) {
            if (msg.contains("content") && msg["content"].is_string()) {
                content += msg["content"].get<std::string>() + " ";
            }
        }
    }

    // Add key parameters
    if (request.contains("max_tokens")) {
        content += "max:" + std::to_string(request["max_tokens"].get<int>());
    }
    if (request.contains("temperature")) {
        content += "temp:" + std::to_string(request["temperature"].get<double>());
    }

    return content;
}

size_t KeyGenerator::hashString(const std::string& str) {
    std::hash<std::string> hasher;
    return hasher(str);
}

// CacheWarmer implementation
void CacheWarmer::warmWithCommonQueries(const std::string& provider) {
    auto queries = generateTestQueries(provider);

    for (const auto& query : queries) {
        std::string key = cache_.generateKey(provider, query);
        // In practice, you would execute these queries and cache results
        // For now, we'll just prepare the cache structure
    }
}

void CacheWarmer::warmWithConfiguredQueries(const nlohmann::json& warmup_config) {
    if (!warmup_config.is_array()) return;

    for (const auto& config : warmup_config) {
        if (config.contains("provider") && config.contains("query")) {
            std::string provider = config["provider"];
            nlohmann::json query = config["query"];

            warmWithCommonQueries(provider);
        }
    }
}

std::vector<nlohmann::json> CacheWarmer::generateTestQueries(const std::string& model) {
    std::vector<nlohmann::json> queries;

    // Common prompts for warming
    std::vector<std::string> warmup_prompts = {
        "Hello, how are you?",
        "What is the weather today?",
        "Explain machine learning",
        "Write a simple function",
        "Help me debug this code"
    };

    for (const auto& prompt : warmup_prompts) {
        nlohmann::json query = {
            {"messages", nlohmann::json::array({
                {{"role", "user"}, {"content", prompt}}
            })},
            {"max_tokens", 100},
            {"temperature", 0.7}
        };
        queries.push_back(query);
    }

    return queries;
}

} // namespace cache
} // namespace aimux