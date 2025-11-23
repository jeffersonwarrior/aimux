/**
 * Fault Injection Testing Framework for Aimux
 *
 * Provides comprehensive fault injection capabilities for testing system resilience:
 * - Network fault injection (timeouts, connection failures, rate limiting)
 * - Resource exhaustion testing (memory, CPU, file handles)
 * - Process failure simulation (crashes, signals, exit codes)
 * - Timing injection (delays, race conditions, deadlocks)
 * - Data corruption testing (JSON corruption, byte flipping, etc.)
 * - Dependency failure simulation (database, external services)
 *
 * @author Aimux Testing Framework
 * @version 2.0.0
 */

#pragma once

#include <memory>
#include <functional>
#include <random>
#include <chrono>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <vector>
#include <string>
#include <exception>
#include <future>
#include <csignal>
#include <system_error>
#include "nlohmann/json.hpp"

namespace aimux::testing {

/**
 * Base fault injector interface
 */
class FaultInjector {
public:
    virtual ~FaultInjector() = default;
    virtual void inject() = 0;
    virtual void reset() = 0;
    virtual std::string description() const = 0;
    virtual nlohmann::json to_json() const = 0;
};

/**
 * Network fault injectors
 */
class NetworkFaultInjector : public FaultInjector {
public:
    enum class FaultType {
        TIMEOUT,
        CONNECTION_REFUSED,
        RATE_LIMIT,
        PARTIAL_FAILURE,
        CORRUPTION,
        SLOW_RESPONSE
    };

    NetworkFaultInjector(FaultType type, double probability,
                        std::chrono::milliseconds delay = std::chrono::milliseconds(0))
        : type_(type), probability_(probability), delay_(delay), injection_count_(0) {}

    void inject() override {
        if (should_inject()) {
            injection_count_++;
            apply_fault();
            if (delay_ > std::chrono::milliseconds(0)) {
                std::this_thread::sleep_for(delay_);
            }
        }
    }

    void reset() override {
        injection_count_.store(0);
    }

    std::string description() const override {
        std::ostringstream oss;
        oss << "NetworkFault[";
        switch (type_) {
            case FaultType::TIMEOUT: oss << "TIMEOUT"; break;
            case FaultType::CONNECTION_REFUSED: oss << "CONNECTION_REFUSED"; break;
            case FaultType::RATE_LIMIT: oss << "RATE_LIMIT"; break;
            case FaultType::PARTIAL_FAILURE: oss << "PARTIAL_FAILURE"; break;
            case FaultType::CORRUPTION: oss << "CORRUPTION"; break;
            case FaultType::SLOW_RESPONSE: oss << "SLOW_RESPONSE"; break;
        }
        oss << ", prob=" << probability_ << ", delay=" << delay_.count() << "ms]";
        return oss.str();
    }

    nlohmann::json to_json() const override {
        return nlohmann::json{
            {"type", "network"},
            {"fault_type", static_cast<int>(type_)},
            {"probability", probability_},
            {"delay_ms", delay_.count()},
            {"injection_count", injection_count_.load()}
        };
    }

    FaultType get_type() const { return type_; }
    double get_probability() const { return probability_; }
    size_t get_injection_count() const { return injection_count_.load(); }

private:
    bool should_inject() {
        static thread_local std::mt19937_64 rng(std::random_device{}());
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        return dist(rng) < probability_;
    }

    void apply_fault() {
        switch (type_) {
            case FaultType::TIMEOUT:
                throw std::system_error(std::make_error_code(std::errc::timed_out),
                                       "Injected network timeout");

            case FaultType::CONNECTION_REFUSED:
                throw std::system_error(std::make_error_code(std::errc::connection_refused),
                                       "Injected connection refused");

            case FaultType::RATE_LIMIT:
                throw std::runtime_error("HTTP 429: Rate limit exceeded (injected)");

            case FaultType::PARTIAL_FAILURE:
                // Random choice between error codes
                if (should_inject()) {
                    throw std::runtime_error("HTTP 500: Internal server error (injected)");
                } else {
                    throw std::runtime_error("HTTP 503: Service unavailable (injected)");
                }

            case FaultType::CORRUPTION:
                throw std::runtime_error("Data corruption detected (injected)");

            case FaultType::SLOW_RESPONSE:
                // The delay is already applied in inject()
                break;
        }
    }

    FaultType type_;
    double probability_;
    std::chrono::milliseconds delay_;
    std::atomic<size_t> injection_count_;
};

/**
 * Resource exhaustion injectors
 */
class ResourceExhaustionInjector : public FaultInjector {
public:
    enum class ResourceType {
        MEMORY,
        CPU,
        FILE_HANDLES,
        THREADS
    };

    ResourceExhaustionInjector(ResourceType type, size_t amount, bool temporary = false)
        : type_(type), amount_(amount), temporary_(temporary), injection_count_(0) {}

    void inject() override {
        injection_count_++;
        switch (type_) {
            case ResourceType::MEMORY:
                exhaust_memory();
                break;
            case ResourceType::CPU:
                exhaust_cpu();
                break;
            case ResourceType::FILE_HANDLES:
                exhaust_file_handles();
                break;
            case ResourceType::THREADS:
                exhaust_threads();
                break;
        }

        if (temporary_) {
            // Reset after some time
            std::thread([this]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                reset();
            }).detach();
        }
    }

    void reset() override {
        allocated_memory_.clear();
        for (auto& handle : open_file_handles_) {
            if (handle) {
                std::fclose(handle);
            }
        }
        open_file_handles_.clear();
        injection_count_.store(0);
    }

    std::string description() const override {
        std::ostringstream oss;
        oss << "ResourceExhaustion[";
        switch (type_) {
            case ResourceType::MEMORY: oss << "MEMORY"; break;
            case ResourceType::CPU: oss << "CPU"; break;
            case ResourceType::FILE_HANDLES: oss << "FILE_HANDLES"; break;
            case ResourceType::THREADS: oss << "THREADS"; break;
        }
        oss << ", amount=" << amount_ << ", temporary=" << temporary_ << "]";
        return oss.str();
    }

    nlohmann::json to_json() const override {
        return nlohmann::json{
            {"type", "resource_exhaustion"},
            {"resource_type", static_cast<int>(type_)},
            {"amount", amount_},
            {"temporary", temporary_},
            {"injection_count", injection_count_.load()}
        };
    }

private:
    void exhaust_memory() {
        allocated_memory_.push_back(std::make_unique<char[]>(amount_ * 1024));
        // Touch memory to ensure it's actually allocated
        char* ptr = allocated_memory_.back().get();
        for (size_t i = 0; i < amount_ * 1024; i += 4096) {
            ptr[i] = static_cast<char>(i % 256);
        }
    }

    void exhaust_cpu() {
        auto start = std::chrono::steady_clock::now();
        while (std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count() < amount_) {
            // CPU-intensive work
            volatile long long counter = 0;
            for (int i = 0; i < 10000; ++i) {
                counter += i * i;
            }
        }
    }

    void exhaust_file_handles() {
        for (size_t i = 0; i < amount_; ++i) {
            FILE* handle = std::tmpfile();
            if (handle) {
                open_file_handles_.push_back(handle);
            } else {
                break; // Can't open more files
            }
        }
    }

    void exhaust_threads() {
        for (size_t i = 0; i < amount_; ++i) {
            background_threads_.push_back(std::thread([]() {
                // Keep thread alive
                std::this_thread::sleep_for(std::chrono::seconds(10));
            }));
        }
    }

    ResourceType type_;
    size_t amount_;
    bool temporary_;
    std::atomic<size_t> injection_count_;
    std::vector<std::unique_ptr<char[]>> allocated_memory_;
    std::vector<FILE*> open_file_handles_;
    std::vector<std::thread> background_threads_;
};

/**
 * Timing fault injectors for race conditions and timing issues
 */
class TimingFaultInjector : public FaultInjector {
public:
    enum class TimingType {
        DELAY,
        JITTER,
        FREEZE,
        SLOW_CLOCK,
        FAST_CLOCK
    };

    TimingFaultInjector(TimingType type, std::chrono::milliseconds duration,
                       double probability = 1.0)
        : type_(type), duration_(duration), probability_(probability), injection_count_(0) {}

    void inject() override {
        if (should_inject()) {
            injection_count_++;
            apply_timing_fault();
        }
    }

    void reset() override {
        injection_count_.store(0);
    }

    std::string description() const override {
        std::ostringstream oss;
        oss << "TimingFault[";
        switch (type_) {
            case TimingType::DELAY: oss << "DELAY"; break;
            case TimingType::JITTER: oss << "JITTER"; break;
            case TimingType::FREEZE: oss << "FREEZE"; break;
            case TimingType::SLOW_CLOCK: oss << "SLOW_CLOCK"; break;
            case TimingType::FAST_CLOCK: oss << "FAST_CLOCK"; break;
        }
        oss << ", duration=" << duration_.count() << "ms, prob=" << probability_ << "]";
        return oss.str();
    }

    nlohmann::json to_json() const override {
        return nlohmann::json{
            {"type", "timing"},
            {"timing_type", static_cast<int>(type_)},
            {"duration_ms", duration_.count()},
            {"probability", probability_},
            {"injection_count", injection_count_.load()}
        };
    }

private:
    bool should_inject() {
        static thread_local std::mt19937_64 rng(std::random_device{}());
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        return dist(rng) < probability_;
    }

    void apply_timing_fault() {
        switch (type_) {
            case TimingType::DELAY:
                std::this_thread::sleep_for(duration_);
                break;

            case TimingType::JITTER:
                {
                    static thread_local std::mt19937_64 rng(std::random_device{}());
                    std::uniform_int_distribution<int> jitter_dist(0, duration_.count());
                    std::this_thread::sleep_for(std::chrono::milliseconds(jitter_dist(rng)));
                }
                break;

            case TimingType::FREEZE:
                std::this_thread::sleep_for(duration_);
                break;

            case TimingType::SLOW_CLOCK:
                // Simulate slower clock by sleeping a fraction of the duration
                std::this_thread::sleep_for(duration_ / 2);
                break;

            case TimingType::FAST_CLOCK:
                // No delay to simulate faster clock
                break;
        }
    }

    TimingType type_;
    std::chrono::milliseconds duration_;
    double probability_;
    std::atomic<size_t> injection_count_;
};

/**
 * Data corruption injector
 */
class DataCorruptionInjector : public FaultInjector {
public:
    enum class CorruptionType {
        BIT_FLIP,
        BYTE_SWAP,
        TRUNCATION,
        DUPLICATION,
        JSON_CORRUPTION
    };

    DataCorruptionInjector(CorruptionType type, double corruption_rate)
        : type_(type), corruption_rate_(corruption_rate), injection_count_(0) {}

    void inject() override {
        injection_count_++;
        // This will be used with specific data corruption functions
    }

    template<typename T>
    T corrupt_data(const T& original_data) {
        if (should_corrupt()) {
            return apply_corruption(original_data);
        }
        return original_data;
    }

    std::string corrupt_string(const std::string& original) {
        if (should_corrupt()) {
            std::string corrupted = original;
            switch (type_) {
                case CorruptionType::BIT_FLIP:
                    return apply_bit_flip(corrupted);
                case CorruptionType::BYTE_SWAP:
                    return apply_byte_swap(corrupted);
                case CorruptionType::TRUNCATION:
                    return corrupted.substr(0, corrupted.size() / 2);
                case CorruptionType::DUPLICATION:
                    return corrupted + corrupted;
                case CorruptionType::JSON_CORRUPTION:
                    return apply_json_corruption(corrupted);
            }
        }
        return original;
    }

    nlohmann::json corrupt_json(const nlohmann::json& original) {
        if (should_corrupt() && type_ == CorruptionType::JSON_CORRUPTION) {
            std::string str = original.dump();
            str = corrupt_string(str);
            try {
                return nlohmann::json::parse(str);
            } catch (...) {
                return nlohmann::json{{"corrupted", true}};
            }
        }
        return original;
    }

    void reset() override {
        injection_count_.store(0);
    }

    std::string description() const override {
        std::ostringstream oss;
        oss << "DataCorruption[";
        switch (type_) {
            case CorruptionType::BIT_FLIP: oss << "BIT_FLIP"; break;
            case CorruptionType::BYTE_SWAP: oss << "BYTE_SWAP"; break;
            case CorruptionType::TRUNCATION: oss << "TRUNCATION"; break;
            case CorruptionType::DUPLICATION: oss << "DUPLICATION"; break;
            case CorruptionType::JSON_CORRUPTION: oss << "JSON_CORRUPTION"; break;
        }
        oss << ", rate=" << corruption_rate_ << "]";
        return oss.str();
    }

    nlohmann::json to_json() const override {
        return nlohmann::json{
            {"type", "data_corruption"},
            {"corruption_type", static_cast<int>(type_)},
            {"corruption_rate", corruption_rate_},
            {"injection_count", injection_count_.load()}
        };
    }

private:
    bool should_corrupt() {
        static thread_local std::mt19937_64 rng(std::random_device{}());
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        return dist(rng) < corruption_rate_;
    }

    std::string apply_bit_flip(std::string data) {
        if (data.empty()) return data;

        static thread_local std::mt19937_64 rng(std::random_device{}());
        std::uniform_int_distribution<size_t> pos_dist(0, data.size() * 8 - 1);

        size_t bit_pos = pos_dist(rng);
        size_t byte_pos = bit_pos / 8;
        size_t bit_offset = bit_pos % 8;

        if (byte_pos < data.size()) {
            data[byte_pos] ^= (1 << bit_offset);
        }

        return data;
    }

    std::string apply_byte_swap(std::string data) {
        if (data.size() < 2) return data;

        static thread_local std::mt19937_64 rng(std::random_device{}());
        std::uniform_int_distribution<size_t> pos_dist(0, data.size() - 2);

        size_t pos = pos_dist(rng);
        std::swap(data[pos], data[pos + 1]);

        return data;
    }

    std::string apply_json_corruption(std::string json_str) {
        // Common JSON corruption patterns
        static thread_local std::mt19937_64 rng(std::random_device{}());
        std::uniform_int_distribution<int> pattern_dist(0, 3);

        switch (pattern_dist(rng)) {
            case 0: // Remove closing brace
                if (!json_str.empty() && json_str.back() == '}') json_str.pop_back();
                break;
            case 1: // Add extra comma
                json_str += ",,";
                break;
            case 2: // Corrupt quotes
                size_t quote_pos = json_str.find('"');
                if (quote_pos != std::string::npos) json_str[quote_pos] = '\'';
                break;
            case 3: // Add trailing data
                json_str += "invalid_json_trailer";
                break;
        }

        return json_str;
    }

    template<typename T>
    T apply_corruption(const T& original_data) {
        // Generic corruption through serialization
        if constexpr (std::is_same_v<T, std::string>) {
            return corrupt_string(original_data);
        } else if constexpr (std::is_same_v<T, nlohmann::json>) {
            return corrupt_json(original_data);
        } else {
            return original_data; // No corruption for unsupported types
        }
    }

    CorruptionType type_;
    double corruption_rate_;
    std::atomic<size_t> injection_count_;
};

/**
 * Fault injection manager for orchestrating multiple faults
 */
class FaultInjectionManager {
public:
    using FaultId = std::string;

    FaultId add_injector(const std::string& name, std::unique_ptr<FaultInjector> injector) {
        FaultId id = name + "_" + std::to_string(next_id_++);
        injectors_[id] = std::move(injector);
        return id;
    }

    void remove_injector(const FaultId& id) {
        injectors_.erase(id);
    }

    void inject_all() {
        for (auto& [id, injector] : injectors_) {
            injector->inject();
        }
    }

    void inject_named(const std::string& name) {
        for (auto& [id, injector] : injectors_) {
            if (id.find(name) != std::string::npos) {
                injector->inject();
            }
        }
    }

    void reset_all() {
        for (auto& [id, injector] : injectors_) {
            injector->reset();
        }
    }

    void enable_random(double probability) {
        enabled_random_ = probability;
    }

    void inject_random() {
        if (enabled_random_ > 0.0 && should_inject_random()) {
            if (!injectors_.empty()) {
                static thread_local std::mt19937_64 rng(std::random_device{}());
                std::uniform_int_distribution<size_t> dist(0, injectors_.size() - 1);
                auto it = injectors_.begin();
                std::advance(it, dist(rng));
                it->second->inject();
            }
        }
    }

    nlohmann::json get_status() const {
        nlohmann::json status = {
            {"total_injectors", injectors_.size()},
            {"random_enabled", enabled_random_},
            {"injectors", nlohmann::json::object()}
        };

        for (const auto& [id, injector] : injectors_) {
            status["injectors"][id] = injector->to_json();
        }

        return status;
    }

    void print_status() const {
        auto status = get_status();
        std::cout << "Fault Injection Status:\n" << status.dump(2) << "\n";
    }

private:
    bool should_inject_random() {
        static thread_local std::mt19937_64 rng(std::random_device{}());
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        return dist(rng) < enabled_random_;
    }

    std::unordered_map<FaultId, std::unique_ptr<FaultInjector>> injectors_;
    double enabled_random_ = 0.0;
    std::atomic<uint64_t> next_id_{1};
};

// Global fault injection manager instance
inline FaultInjectionManager& get_fault_manager() {
    static FaultInjectionManager instance;
    return instance;
}

// Convenience macros
#define AIMUX_INJECT_NETWORK_TIMEOUT(prob) \
    aimux::testing::get_fault_manager().add_injector( \
        "network_timeout", \
        std::make_unique<aimux::testing::NetworkFaultInjector>( \
            aimux::testing::NetworkFaultInjector::FaultType::TIMEOUT, prob))

#define AIMUX_INJECT_MEMORY_EXHAUSTION(kb, temp) \
    aimux::testing::get_fault_manager().add_injector( \
        "memory_exhaustion", \
        std::make_unique<aimux::testing::ResourceExhaustionInjector>( \
            aimux::testing::ResourceExhaustionInjector::ResourceType::MEMORY, kb, temp))

#define AIMUX_INJECT_TIMING_DELAY(ms, prob) \
    aimux::testing::get_fault_manager().add_injector( \
        "timing_delay", \
        std::make_unique<aimux::testing::TimingFaultInjector>( \
            aimux::testing::TimingFaultInjector::TimingType::DELAY, std::chrono::milliseconds(ms), prob))

#define AIMUX_INJECT_DATA_CORRUPTION(rate) \
    aimux::testing::get_fault_manager().add_injector( \
        "data_corruption", \
        std::make_unique<aimux::testing::DataCorruptionInjector>( \
            aimux::testing::DataCorruptionInjector::CorruptionType::BIT_FLIP, rate))

} // namespace aimux::testing