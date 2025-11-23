#include "aimux/logging/correlation_context.h"
#include <atomic>
#include <chrono>
#include <sstream>
#include <iomanip>

namespace aimux {
namespace logging {

// Thread-local storage declarations
thread_local std::stack<std::string> CorrelationContext::correlationStack_;
thread_local std::string CorrelationContext::threadCorrelationId_;
thread_local bool CorrelationContext::initialized_ = false;

CorrelationContext& CorrelationContext::getInstance() {
    static CorrelationContext instance;
    return instance;
}

std::string CorrelationContext::generateCorrelationId() {
    static std::atomic<uint64_t> counter{0};

    auto now = std::chrono::high_resolution_clock::now();
    auto timestamp = now.time_since_epoch().count();
    auto count = counter++;

    // Format: hex(timestamp)-hex(counter)-hex(thread_id)
    std::ostringstream oss;
    oss << std::hex << timestamp << "-" << std::hex << count;

    // Add thread ID for multi-threaded uniqueness
    std::ostringstream threadOss;
    threadOss << std::this_thread::get_id();
    std::string threadIdStr = threadOss.str();

    // Convert thread ID to hex for consistency
    std::hash<std::string> hasher;
    auto threadHash = hasher(threadIdStr);
    oss << "-" << std::hex << threadHash;

    return oss.str();
}

std::string CorrelationContext::pushCorrelationId(const std::string& correlationId) {
    std::string idToPush = correlationId.empty() ? generateCorrelationId() : correlationId;

    correlationStack_.push(idToPush);

    // Set thread correlation ID if this is the first one
    if (correlationStack_.size() == 1) {
        threadCorrelationId_ = idToPush;
    }

    initialized_ = true;

    return idToPush;
}

std::string CorrelationContext::popCorrelationId() {
    if (!initialized_ || correlationStack_.empty()) {
        return "";
    }

    std::string poppedId = correlationStack_.top();
    correlationStack_.pop();

    // Update thread correlation ID to the new top, or clear if empty
    if (!correlationStack_.empty()) {
        threadCorrelationId_ = correlationStack_.top();
    } else {
        threadCorrelationId_.clear();
    }

    return poppedId;
}

std::string CorrelationContext::getCurrentCorrelationId() const {
    if (!initialized_ || correlationStack_.empty()) {
        return threadCorrelationId_;
    }

    return correlationStack_.top();
}

std::string CorrelationContext::getRootCorrelationId() const {
    if (!initialized_) {
        return "";
    }

    return threadCorrelationId_;
}

size_t CorrelationContext::getDepth() const {
    if (!initialized_) {
        return 0;
    }

    return correlationStack_.size();
}

void CorrelationContext::clear() {
    while (!correlationStack_.empty()) {
        correlationStack_.pop();
    }
    threadCorrelationId_.clear();
    initialized_ = false;
}

void CorrelationContext::setThreadCorrelationId(const std::string& correlationId) {
    threadCorrelationId_ = correlationId;

    // If the stack is empty, push this as the root
    if (correlationStack_.empty()) {
        pushCorrelationId(correlationId);
    } else {
        // Replace the root correlation ID
        std::stack<std::string> tempStack;

        // Pop all to get to the root
        while (!correlationStack_.empty()) {
            tempStack.push(correlationStack_.top());
            correlationStack_.pop();
        }

        // The last popped was the root, discard it
        if (!tempStack.empty()) {
            tempStack.pop();
        }

        // Push the new root
        threadCorrelationId_ = correlationId;

        // Restore the stack with new root at bottom
        std::vector<std::string> restoreOrder;
        while (!tempStack.empty()) {
            restoreOrder.push_back(tempStack.top());
            tempStack.pop();
        }

        // Push back in reverse order
        for (auto it = restoreOrder.rbegin(); it != restoreOrder.rend(); ++it) {
            correlationStack_.push(*it);
        }
    }

    initialized_ = true;
}

nlohmann::json CorrelationContext::toJson() const {
    nlohmann::json context;

    if (initialized_) {
        context["current_correlation_id"] = getCurrentCorrelationId();
        context["root_correlation_id"] = getRootCorrelationId();
        context["stack_depth"] = getDepth();

        // Add stack trace for debugging (only in debug builds)
#ifndef NDEBUG
        std::vector<std::string> stackTrace;
        std::stack<std::string> tempStack = correlationStack_;

        while (!tempStack.empty()) {
            stackTrace.push_back(tempStack.top());
            tempStack.pop();
        }

        // Reverse to get chronological order
        std::reverse(stackTrace.begin(), stackTrace.end());
        context["correlation_stack"] = stackTrace;
#endif
    } else {
        context["correlation_id"] = "";
        context["initialized"] = false;
    }

    return context;
}

} // namespace logging
} // namespace aimux