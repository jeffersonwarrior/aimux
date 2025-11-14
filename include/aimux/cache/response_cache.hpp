#pragma once

#include <string>
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <memory>
#include <functional>
#include <atomic>
#include <nlohmann/json.hpp>
#include <optional>

namespace aimux {
namespace cache {

/**
 * @brief Cached response entry with TTL
 */
struct CacheEntry {
    nlohmann::json response;
    std::chrono::steady_clock::time_point timestamp;
    std::chrono::milliseconds ttl;
    int hit_count = 0;
    size_t response_size;

    bool isExpired() const {
        return std::chrono::steady_clock::now() > (timestamp + ttl);
    }
};

/**
 * @brief Intelligent response caching system with LRU eviction
 */
class ResponseCache {
public:
    /**
     * @brief Cache configuration
     */
    struct Config {
        size_t max_entries = 1000;
        size_t max_memory_mb = 100;
        std::chrono::milliseconds default_ttl{300000}; // 5 minutes
        std::chrono::milliseconds max_ttl{3600000}; // 1 hour
        double hit_rate_threshold = 0.7;
        bool enable_smart_ttl = true;
    };

    explicit ResponseCache(const Config& config = Config());
    ~ResponseCache() = default;

    // Non-copyable but movable
    ResponseCache(const ResponseCache&) = delete;
    ResponseCache& operator=(const ResponseCache&) = delete;
    ResponseCache(ResponseCache&&) = default;
    ResponseCache& operator=(ResponseCache&&) = default;

    /**
     * @brief Generate cache key from request
     */
    std::string generateKey(const std::string& model, const nlohmann::json& request) const;

    /**
     * @brief Get cached response
     */
    std::optional<nlohmann::json> get(const std::string& key);

    /**
     * @brief Store response in cache
     */
    void put(const std::string& key, const nlohmann::json& response,
             std::optional<std::chrono::milliseconds> ttl = std::nullopt);

    /**
     * @brief Remove entry from cache
     */
    void remove(const std::string& key);

    /**
     * @brief Clear all cache entries
     */
    void clear();

    /**
     * @brief Cleanup expired entries
     */
    size_t cleanup();

    // Statistics
    struct Stats {
        size_t hits = 0;
        size_t misses = 0;
        size_t entries = 0;
        size_t memory_usage_bytes = 0;
        double hit_rate = 0.0;
        size_t evictions = 0;
    };

    Stats getStats() const;
    void resetStats();

    // Advanced features
    void enableAdaptiveTTL(bool enable) { config_.enable_smart_ttl = enable; }
    void setTTLMultiplier(double multiplier) { ttl_multiplier_ = multiplier; }

private:
    struct LRUEntry {
        std::string key;
        std::chrono::steady_clock::time_point last_access;
    };

    Config config_;
    mutable std::mutex mutex_;

    std::unordered_map<std::string, CacheEntry> cache_;
    std::vector<LRUEntry> lru_list_;

    // Statistics
    mutable std::atomic<size_t> hits_{0};
    mutable std::atomic<size_t> misses_{0};
    mutable std::atomic<size_t> evictions_{0};

    double ttl_multiplier_ = 1.0;

    // Internal methods
    void updateLRU(const std::string& key);
    void evictLRU();
    bool shouldEvict(const CacheEntry& entry) const;
    std::chrono::milliseconds calculateTTL(const nlohmann::json& request,
                                          const nlohmann::json& response) const;
    size_t estimateMemoryUsage(const CacheEntry& entry) const;
    void enforceMemoryLimit();
};

/**
 * @brief Cache key generation strategies
 */
class KeyGenerator {
public:
    static std::string hashingStrategy(const std::string& model, const nlohmann::json& request);
    static std::string semanticStrategy(const std::string& model, const nlohmann::json& request);
    static std::string parameterStrategy(const std::string& model, const nlohmann::json& request);

private:
    static std::string extractCoreContent(const nlohmann::json& request);
    static size_t hashString(const std::string& str);
};

/**
 * @brief Cache warming for frequently used requests
 */
class CacheWarmer {
public:
    explicit CacheWarmer(ResponseCache& cache) : cache_(cache) {}

    void warmWithCommonQueries(const std::string& provider);
    void warmWithConfiguredQueries(const nlohmann::json& warmup_config);

private:
    ResponseCache& cache_;
    std::vector<nlohmann::json> generateTestQueries(const std::string& model);
};

} // namespace cache
} // namespace aimux