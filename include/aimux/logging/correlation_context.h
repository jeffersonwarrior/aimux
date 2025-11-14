#pragma once

#include <string>
#include <thread>
#include <unordered_map>
#include <mutex>
#include <memory>
#include <stack>

namespace aimux {
namespace logging {

/**
 * @brief Thread-local correlation context manager
 *
 * Provides hierarchical correlation ID management for distributed tracing.
 * Each thread can have a stack of correlation IDs for nested operations.
 *
 * Features:
 * - Thread-local storage for isolation
 * - Stack-based hierarchy for nested operations
 * - Parent-child relationship tracking
 * - Automatic cleanup on thread exit
 * - Performance optimized with minimal overhead
 */
class CorrelationContext {
public:
    /**
     * @brief Singleton accessor
     * @return Reference to global instance
     */
    static CorrelationContext& getInstance();

    /**
     * @brief Generate a new correlation ID
     * @return New unique correlation ID
     */
    static std::string generateCorrelationId();

    /**
     * @brief Push a correlation ID onto the thread-local stack
     * @param correlationId Correlation ID to push (empty = generate new)
     * @return The correlation ID that was pushed
     */
    std::string pushCorrelationId(const std::string& correlationId = "");

    /**
     * @brief Pop the current correlation ID from the stack
     * @return The correlation ID that was popped, or empty if stack was empty
     */
    std::string popCorrelationId();

    /**
     * @brief Get the current correlation ID
     * @return Current correlation ID, or empty if none set
     */
    std::string getCurrentCorrelationId() const;

    /**
     * @brief Get the root correlation ID for the current thread
     * @return Root correlation ID, or empty if none set
     */
    std::string getRootCorrelationId() const;

    /**
     * @brief Get the correlation ID stack depth
     * @return Current stack depth
     */
    size_t getDepth() const;

    /**
     * @brief Clear all correlation IDs for current thread
     */
    void clear();

    /**
     * @brief Set correlation ID for the entire thread scope
     * @param correlationId Correlation ID to set
     */
    void setThreadCorrelationId(const std::string& correlationId);

    /**
     * @brief Get correlation context as JSON for logging
     * @return JSON object with correlation context
     */
    nlohmann::json toJson() const;

private:
    CorrelationContext() = default;
    ~CorrelationContext() = default;
    CorrelationContext(const CorrelationContext&) = delete;
    CorrelationContext& operator=(const CorrelationContext&) = delete;

    // Thread-local correlation ID stack
    thread_local static std::stack<std::string> correlationStack_;
    thread_local static std::string threadCorrelationId_;
    thread_local static bool initialized_;
};

/**
 * @brief RAII helper for automatic correlation ID management
 *
 * Automatically pushes correlation ID on construction and pops on destruction.
 * Ensures proper cleanup even with exceptions.
 */
class CorrelationScope {
public:
    /**
     * @brief Constructor - pushes correlation ID
     * @param correlationId Correlation ID to use (empty = generate new)
     */
    explicit CorrelationScope(const std::string& correlationId = "")
        : correlationId_(CorrelationContext::getInstance().pushCorrelationId(correlationId)) {}

    /**
     * @brief Destructor - automatically pops correlation ID
     */
    ~CorrelationScope() {
        CorrelationContext::getInstance().popCorrelationId();
    }

    /**
     * @brief Get the correlation ID for this scope
     * @return The correlation ID being used
     */
    const std::string& getCorrelationId() const {
        return correlationId_;
    }

    /**
     * @brief Prevent copy construction
     */
    CorrelationScope(const CorrelationScope&) = delete;

    /**
     * @brief Prevent copy assignment
     */
    CorrelationScope& operator=(const CorrelationScope&) = delete;

    /**
     * @brief Allow move construction
     */
    CorrelationScope(CorrelationScope&& other) noexcept
        : correlationId_(other.correlationId_) {
        other.correlationId_.clear();
    }

    /**
     * @brief Allow move assignment
     */
    CorrelationScope& operator=(CorrelationScope&& other) noexcept {
        if (this != &other) {
            CorrelationContext::getInstance().popCorrelationId();
            correlationId_ = other.correlationId_;
            other.correlationId_.clear();
        }
        return *this;
    }

private:
    std::string correlationId_;
};

/**
 * @brief Convenience macros for correlation ID management
 */
#define AIMUX_CORRELATION_SCOPE() \
    aimux::logging::CorrelationScope _correlation_scope

#define AIMUX_CORRELATION_SCOPE_NAMED(name) \
    aimux::logging::CorrelationScope _correlation_scope(name)

#define AIMUX_CURRENT_CORRELATION_ID() \
    aimux::logging::CorrelationContext::getInstance().getCurrentCorrelationId()

#define AIMUX_SET_CORRELATION_ID(id) \
    aimux::logging::CorrelationContext::getInstance().setThreadCorrelationId(id)

} // namespace logging
} // namespace aimux