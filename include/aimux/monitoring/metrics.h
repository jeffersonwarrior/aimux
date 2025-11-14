#pragma once

#include <chrono>
#include <atomic>
#include <unordered_map>
#include <string>
#include <deque>
#include <nlohmann/json.hpp>

namespace aimux {
namespace monitoring {

// Basic metrics structures for the performance monitoring system
struct MetricsValue {
    double value;
    std::chrono::system_clock::time_point timestamp;
    std::unordered_map<std::string, std::string> tags;

    MetricsValue(double v) : value(v), timestamp(std::chrono::system_clock::now()) {}
    MetricsValue(double v, const std::unordered_map<std::string, std::string>& t)
        : value(v), timestamp(std::chrono::system_clock::now()), tags(t) {}
};

struct Counter {
    std::atomic<uint64_t> value{0};

    void increment() { value++; }
    void increment(uint64_t amount) { value += amount; }
    uint64_t get() const { return value.load(); }
    void reset() { value = 0; }
};

struct Gauge {
    std::atomic<double> value{0.0};

    void set(double v) { value = v; }
    void increment() { value.store(value.load() + 1.0); }
    void decrement() { value.store(value.load() - 1.0); }
    double get() const { return value.load(); }
};

struct Histogram {
    std::deque<double> observations;
    std::atomic<uint64_t> count{0};
    std::atomic<double> sum{0.0};

    void observe(double value) {
        observations.push_back(value);
        count++;
        sum += value;

        // Keep only last 1000 observations to limit memory
        if (observations.size() > 1000) {
            observations.pop_front();
        }
    }

    double get_percentile(double percentile) const {
        if (observations.empty()) return 0.0;

        auto sorted_obs = observations;
        std::sort(sorted_obs.begin(), sorted_obs.end());

        size_t index = static_cast<size_t>(percentile * sorted_obs.size() / 100.0);
        index = std::min(index, sorted_obs.size() - 1);

        return sorted_obs[index];
    }

    double get_average() const {
        uint64_t c = count.load();
        return c > 0 ? sum.load() / c : 0.0;
    }

    uint64_t get_count() const { return count.load(); }
    void reset() {
        observations.clear();
        count = 0;
        sum = 0.0;
    }
};

} // namespace monitoring
} // namespace aimux