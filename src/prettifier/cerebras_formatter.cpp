#include "aimux/prettifier/cerebras_formatter.hpp"
#include "aimux/core/model_registry.hpp"
#include <map>
#include <regex>
#include <sstream>
#include <chrono>
#include <algorithm>
#include <iomanip>

// TODO: Replace with proper logging when available
#define LOG_ERROR(msg, ...) do { printf("[CEREBRAS ERROR] " msg "\n", ##__VA_ARGS__); } while(0)
#define LOG_DEBUG(msg, ...) do { if (enable_detailed_metrics_) printf("[CEREBRAS DEBUG] " msg "\n", ##__VA_ARGS__); } while(0)

// Forward declaration for global model configuration
namespace aimux {
namespace config {
    extern std::map<std::string, aimux::core::ModelRegistry::ModelInfo> g_selected_models;
}
}


namespace aimux {
namespace prettifier {

// CerebrasPatterns implementation
CerebrasFormatter::CerebrasPatterns::CerebrasPatterns()
    : fast_tool_pattern(R"(\{\"type\":\"function_call\",\"function\":\{[^}]*\}\})", std::regex::optimize)
    , cerebras_json_pattern(R"(\{(?:[^{}]|\{[^{}]*\})*\})", std::regex::optimize)
    , streaming_tool_pattern(R"(data:\s*\{[^}]*\"function_call\"[^}]*\})", std::regex::optimize) {
}

// CerebrasFormatter implementation
std::string CerebrasFormatter::get_default_model() {
    
    auto it = aimux::config::g_selected_models.find("cerebras");
    if (it != aimux::config::g_selected_models.end()) {
        return it->second.model_id;
    }
    return "llama3.1-8b";
}

CerebrasFormatter::CerebrasFormatter(const std::string& model_name)
    : patterns_(std::make_unique<CerebrasPatterns>())
    , model_name_(model_name.empty() ? get_default_model() : model_name) {
    LOG_DEBUG("CerebrasFormatter initialized with speed optimization (model: %s)", model_name_.c_str());
}

std::string CerebrasFormatter::get_name() const {
    return "cerebras-speed-formatter-v1.0.0";
}

std::string CerebrasFormatter::version() const {
    return "1.0.0";
}

std::string CerebrasFormatter::description() const {
    return "Cerebras-specific formatter optimized for maximum speed and efficient tool response processing. "
           "Specializes in minimizing processing overhead while preserving Cerebras's high-performance characteristics. "
           "Features fast tool call extraction, lightweight TOON serialization, and speed-focused metrics.";
}

std::vector<std::string> CerebrasFormatter::supported_formats() const {
    return {"json", "text", "markdown", "cerebras-raw", "streaming-json"};
}

std::vector<std::string> CerebrasFormatter::output_formats() const {
    return {"toon", "json", "normalized-text", "structured-data"};
}

std::vector<std::string> CerebrasFormatter::supported_providers() const {
    return {"cerebras", "cerebras-ai"};
}

std::vector<std::string> CerebrasFormatter::capabilities() const {
    return {
        "speed-optimization",
        "fast-tool-calls",
        "streaming-support",
        "cerebras-patterns",
        "minimal-overhead",
        "toon-formatting",
        "performance-metrics",
        "fast-failover"
    };
}

ProcessingResult CerebrasFormatter::preprocess_request(const core::Request& request) {
    auto start_time = std::chrono::high_resolution_clock::now();

    try {
        ProcessingResult result;
        result.success = true;
        result.output_format = "cerebras-optimized";

        // Clone the request data for modification
        nlohmann::json optimized_data = request.data;

        // Add Cerebras-specific optimizations
        if (optimize_speed_) {
            // Set parameters for speed optimization
            if (!optimized_data.contains("temperature")) {
                optimized_data["temperature"] = 0.1; // Lower temperature for faster, more deterministic responses
            }

            if (!optimized_data.contains("max_tokens")) {
                optimized_data["max_tokens"] = 2048; // Reasonable limit for speed
            }

            // Add speed-focused metadata
            optimized_data["_cerebras_optimization"] = {
                {"speed_priority", true},
                {"minimize_latency", true},
                {"fast_tool_mode", true}
            };
        }

        // Format tool calls for Cerebras efficiency
        if (optimized_data.contains("tools") && optimize_speed_) {
            // Ensure tools are in Cerebras-preferred format
            for (auto& tool : optimized_data["tools"]) {
                if (tool.contains("function")) {
                    auto& func = tool["function"];
                    // Add Cerebras optimization hints
                    func["_optimize"] = true;
                }
            }
        }

        result.processed_content = optimized_data.dump();

        // Calculate processing time
        auto end_time = std::chrono::high_resolution_clock::now();
        auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(
            end_time - start_time).count();

        result.processing_time = std::chrono::milliseconds(elapsed_us / 1000);
        result.tokens_processed = request.data.dump().length() / 4; // Rough estimate

        update_metrics(static_cast<uint64_t>(elapsed_us), 0, false);

        LOG_DEBUG("Request preprocessing completed in %ld us", elapsed_us);
        return result;

    } catch (const std::exception& e) {
        return create_error_result("Request preprocessing failed: " + std::string(e.what()), "preprocess_error");
    }
}

ProcessingResult CerebrasFormatter::postprocess_response(
    const core::Response& response,
    const ProcessingContext& context) {

    auto start_time = std::chrono::high_resolution_clock::now();

    try {
        // Input size validation to prevent crashes on very large inputs
        const size_t MAX_INPUT_SIZE = 10 * 1024 * 1024; // 10MB limit for local development
        if (response.data.size() > MAX_INPUT_SIZE) {
            LOG_ERROR("Input size %zu exceeds maximum allowed size %zu", response.data.size(), MAX_INPUT_SIZE);
            return create_error_result("Input size too large (max 10MB)", "input_too_large");
        }

        // Check if we should trigger fast failover early
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now() - start_time).count();

        if (should_trigger_fast_failover(elapsed_ms)) {
            LOG_DEBUG("Triggering fast failover at %ld ms", elapsed_ms);
            return fast_failover_process(response, context);
        }

        ProcessingResult result;
        result.success = true;
        result.output_format = "toon";

        // Fast content normalization
        std::string normalized_content = fast_normalize_content(response.data);

        // Extract tool calls with Cerebras-optimized patterns
        std::vector<ToolCall> tool_calls = extract_cerebras_tool_calls(normalized_content);

        // Generate TOON format efficiently
        std::string toon_content = generate_fast_toon(normalized_content, tool_calls, context);

        result.processed_content = toon_content;
        result.extracted_tool_calls = tool_calls;
        result.streaming_mode = context.streaming_mode;

        // Add metadata
        result.metadata["provider"] = "cerebras";
        result.metadata["processing_time_us"] = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now() - start_time).count();
        result.metadata["speed_optimized"] = optimize_speed_;
        result.metadata["tool_calls_count"] = tool_calls.size();

        // Calculate final metrics
        auto end_time = std::chrono::high_resolution_clock::now();
        auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(
            end_time - start_time).count();

        result.processing_time = std::chrono::milliseconds(elapsed_us / 1000);
        result.tokens_processed = normalized_content.length() / 4; // Rough estimate

        update_metrics(static_cast<uint64_t>(elapsed_us), tool_calls.size(), cache_tool_patterns_);

        LOG_DEBUG("Response postprocessing completed in %ld us with %zu tool calls", elapsed_us, tool_calls.size());
        return result;

    } catch (const std::exception& e) {
        return create_error_result("Response postprocessing failed: " + std::string(e.what()), "postprocess_error");
    }
}

bool CerebrasFormatter::begin_streaming(const ProcessingContext& /*context*/) {
    try {
        streaming_buffer_.clear();
        streaming_buffer_.reserve(4096); // Pre-allocate for speed
        streaming_active_ = true;
        streaming_start_ = std::chrono::steady_clock::now();

        LOG_DEBUG("Streaming initialization completed");
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Streaming initialization failed: %s", e.what());
        return false;
    }
}

ProcessingResult CerebrasFormatter::process_streaming_chunk(
    const std::string& chunk,
    bool is_final,
    const ProcessingContext& context) {

    if (!streaming_active_) {
        return create_error_result("Streaming not initialized", "streaming_not_active");
    }

    auto start_time = std::chrono::high_resolution_clock::now();

    try {
        ProcessingResult result;
        result.success = true;
        result.streaming_mode = true;

        // Append chunk to buffer
        streaming_buffer_ += chunk;

        // Fast validation
        if (streaming_buffer_.size() > 1000000) { // 1MB limit
            if (enable_fast_failover_) {
                LOG_DEBUG("Buffer size exceeded, triggering fast failover");
                core::Response response;
                response.data = streaming_buffer_;
                response.success = true;
                return fast_failover_process(response, context);
            }
        }

        // Extract tool calls from accumulated buffer (streaming-aware)
        std::vector<ToolCall> tool_calls;
        if (is_final || streaming_buffer_.find("function_call") != std::string::npos) {
            tool_calls = extract_cerebras_tool_calls(streaming_buffer_);
        }

        // For streaming, return the raw chunk with minimal processing
        result.processed_content = chunk;
        result.extracted_tool_calls = tool_calls;

        if (is_final) {
            result.metadata["final_chunk"] = true;
            result.metadata["total_buffer_size"] = streaming_buffer_.size();
        }

        // Calculate metrics
        auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now() - start_time).count();

        update_metrics(static_cast<uint64_t>(elapsed_us), tool_calls.size(), cache_tool_patterns_);

        return result;

    } catch (const std::exception& e) {
        return create_error_result("Streaming chunk processing failed: " + std::string(e.what()), "streaming_error");
    }
}

ProcessingResult CerebrasFormatter::end_streaming(const ProcessingContext& context) {
    if (!streaming_active_) {
        return create_error_result("Streaming not active", "streaming_not_active");
    }

    try {
        auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - streaming_start_).count();

        // Process final buffer content
        std::vector<ToolCall> final_tool_calls = extract_cerebras_tool_calls(streaming_buffer_);
        std::string final_toon = generate_fast_toon(streaming_buffer_, final_tool_calls, context);

        ProcessingResult result;
        result.success = true;
        result.processed_content = final_toon;
        result.extracted_tool_calls = final_tool_calls;
        result.streaming_mode = false;
        result.processing_time = std::chrono::milliseconds(total_time);
        result.metadata["streaming_completed"] = true;
        result.metadata["total_streaming_time_ms"] = total_time;

        // Reset streaming state
        streaming_active_ = false;
        streaming_buffer_.clear();

        LOG_DEBUG("Streaming completed in %ld ms", total_time);
        return result;

    } catch (const std::exception& e) {
        streaming_active_ = false;
        return create_error_result("Streaming end failed: " + std::string(e.what()), "streaming_end_error");
    }
}

bool CerebrasFormatter::configure(const nlohmann::json& config) {
    try {
        if (config.contains("optimize_speed")) {
            optimize_speed_ = config["optimize_speed"].get<bool>();
        }

        if (config.contains("enable_detailed_metrics")) {
            enable_detailed_metrics_ = config["enable_detailed_metrics"].get<bool>();
        }

        if (config.contains("cache_tool_patterns")) {
            cache_tool_patterns_ = config["cache_tool_patterns"].get<bool>();
        }

        if (config.contains("max_processing_time_ms")) {
            max_processing_time_ms_ = config["max_processing_time_ms"].get<int>();
        }

        if (config.contains("enable_fast_failover")) {
            enable_fast_failover_ = config["enable_fast_failover"].get<bool>();
        }

        LOG_DEBUG("Configuration updated successfully");
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Configuration failed: %s", e.what());
        return false;
    }
}

bool CerebrasFormatter::validate_configuration() const {
    return max_processing_time_ms_ > 0 && max_processing_time_ms_ <= 1000;
}

nlohmann::json CerebrasFormatter::get_configuration() const {
    nlohmann::json config;
    config["optimize_speed"] = optimize_speed_;
    config["enable_detailed_metrics"] = enable_detailed_metrics_;
    config["cache_tool_patterns"] = cache_tool_patterns_;
    config["max_processing_time_ms"] = max_processing_time_ms_;
    config["enable_fast_failover"] = enable_fast_failover_;
    return config;
}

nlohmann::json CerebrasFormatter::get_metrics() const {
    nlohmann::json metrics;

    uint64_t count = total_processing_count_.load();
    uint64_t total_time = total_processing_time_us_.load();

    metrics["total_processing_count"] = count;
    metrics["total_processing_time_us"] = total_time;
    metrics["average_processing_time_us"] = count > 0 ? total_time / count : 0;
    metrics["tool_calls_extracted"] = tool_calls_extracted_.load();
    metrics["cache_hits"] = cache_hits_.load();
    metrics["cache_misses"] = cache_misses_.load();
    metrics["cache_hit_rate"] = (cache_hits_.load() + cache_misses_.load()) > 0 ?
        static_cast<double>(cache_hits_.load()) / (cache_hits_.load() + cache_misses_.load()) : 0.0;
    metrics["fast_failovers_triggered"] = fast_failovers_triggered_.load();
    metrics["speed_optimization_enabled"] = optimize_speed_;

    return metrics;
}

void CerebrasFormatter::reset_metrics() {
    total_processing_count_.store(0);
    total_processing_time_us_.store(0);
    tool_calls_extracted_.store(0);
    cache_hits_.store(0);
    cache_misses_.store(0);
    fast_failovers_triggered_.store(0);
}

nlohmann::json CerebrasFormatter::health_check() const {
    nlohmann::json health;

    try {
        // Basic health status
        health["status"] = "healthy";
        health["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        // Speed validation
        auto start = std::chrono::high_resolution_clock::now();
        std::string test_content = "{\"test\": \"speed_validation\"}";
        auto tool_calls = extract_cerebras_tool_calls(test_content);
        auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now() - start).count();

        health["speed_validation_us"] = elapsed_us;
        health["speed_within_target"] = elapsed_us < 10000; // 10ms target

        // Pattern cache validation
        health["pattern_cache_available"] = patterns_ != nullptr;

        // Configuration validation
        health["configuration_valid"] = validate_configuration();

        // Metrics summary
        auto metrics = get_metrics();
        health["processing_stats"] = {
            {"avg_processing_time_us", metrics["average_processing_time_us"]},
            {"total_processed", metrics["total_processing_count"]},
            {"cache_hit_rate", metrics["cache_hit_rate"]}
        };

    } catch (const std::exception& e) {
        health["status"] = "unhealthy";
        health["error"] = e.what();
    }

    return health;
}

nlohmann::json CerebrasFormatter::get_diagnostics() const {
    nlohmann::json diagnostics;

    diagnostics["name"] = get_name();
    diagnostics["version"] = version();
    diagnostics["status"] = "active";
    diagnostics["configuration"] = get_configuration();
    diagnostics["metrics"] = get_metrics();

    // Performance diagnostics
    uint64_t avg_time = total_processing_count_.load() > 0 ?
        total_processing_time_us_.load() / total_processing_count_.load() : 0;

    diagnostics["performance_analysis"] = {
        {"average_processing_speed_us", avg_time},
        {"meets_speed_target", avg_time < 50000}, // 50ms target
        {"cache_efficiency", get_metrics()["cache_hit_rate"]},
        {"fast_failover_utilization", fast_failovers_triggered_.load() > 0}
    };

    // Optimization recommendations
    std::vector<std::string> recommendations;
    if (avg_time > 30000) recommendations.push_back("Consider enabling more aggressive speed optimizations");
    if (get_metrics()["cache_hit_rate"] < 0.8) recommendations.push_back("Pattern cache may need tuning");
    if (!optimize_speed_) recommendations.push_back("Speed optimization is disabled");

    diagnostics["recommendations"] = recommendations;

    return diagnostics;
}

// Private helper methods implementation

std::string CerebrasFormatter::fast_normalize_content(const std::string& content) const {
    if (!optimize_speed_) {
        // Use basic normalization if speed optimization is disabled
        return content;
    }

    try {
        // Fast normalization - minimal processing for speed
        std::string normalized = content;

        // Remove common Cerebras artifacts that don't affect tool calls
        static const std::regex artifacts_pattern(R"(\[DONE\]|data:\s*\{|:\s*\"?DONE\"?)", std::regex::optimize);
        normalized = std::regex_replace(normalized, artifacts_pattern, "");

        // Basic whitespace cleanup (preserving tool call structure)
        static const std::regex whitespace_pattern(R"(\s+)", std::regex::optimize);
        normalized = std::regex_replace(normalized, whitespace_pattern, " ");

        // Trim leading/trailing whitespace
        normalized.erase(0, normalized.find_first_not_of(" \t\n\r"));
        normalized.erase(normalized.find_last_not_of(" \t\n\r") + 1);

        return normalized;

    } catch (const std::exception& e) {
        LOG_DEBUG("Fast normalization failed, returning original content: %s", e.what());
        return content; // Fallback to original content
    }
}

std::vector<ToolCall> CerebrasFormatter::extract_cerebras_tool_calls(const std::string& content) const {
    std::vector<ToolCall> tool_calls;

    try {
        auto start_time = std::chrono::high_resolution_clock::now();

        // First, try to parse the entire content as JSON (OpenAI-style format)
        auto json_opt = validate_json(content);
        if (json_opt) {
            // Check for OpenAI-style: choices[0].message.tool_calls
            if (json_opt->contains("choices") && json_opt->at("choices").is_array() && !json_opt->at("choices").empty()) {
                const auto& first_choice = json_opt->at("choices")[0];
                if (first_choice.contains("message")) {
                    const auto& message = first_choice["message"];
                    if (message.contains("tool_calls") && message["tool_calls"].is_array()) {
                        for (const auto& tc : message["tool_calls"]) {
                            ToolCall call;
                            call.status = "pending";
                            call.timestamp = std::chrono::system_clock::now();

                            if (tc.contains("function")) {
                                const auto& func = tc["function"];
                                if (func.contains("name")) {
                                    call.name = func["name"].get<std::string>();
                                }
                                if (func.contains("arguments")) {
                                    if (func["arguments"].is_string()) {
                                        // Parse arguments if they're a JSON string
                                        auto args_opt = validate_json(func["arguments"].get<std::string>());
                                        if (args_opt) {
                                            call.parameters = *args_opt;
                                        } else {
                                            call.parameters = func["arguments"];
                                        }
                                    } else {
                                        call.parameters = func["arguments"];
                                    }
                                }
                            }
                            if (tc.contains("id")) {
                                call.id = tc["id"].get<std::string>();
                            }

                            tool_calls.push_back(call);
                        }

                        LOG_DEBUG("Tool call extraction completed, found %zu calls from OpenAI-style format", tool_calls.size());
                        return tool_calls;
                    }
                }
            }
        }

        bool cache_used = false;

        // Use pre-compiled patterns for speed
        if (patterns_ && cache_tool_patterns_) {
            // Pattern 1: Fast tool call pattern (most common in Cerebras)
            std::smatch match;
            std::string search_content = content;

            while (std::regex_search(search_content, match, patterns_->fast_tool_pattern)) {
                std::string json_str = match[0].str();
                auto json_opt2 = validate_json(json_str);

                if (json_opt2 && json_opt2->contains("function")) {
                    ToolCall call;
                    call.status = "pending";
                    call.timestamp = std::chrono::system_clock::now();

                    const auto& func = (*json_opt2)["function"];
                    if (func.contains("name")) {
                        call.name = func["name"].get<std::string>();
                    }
                    if (func.contains("arguments")) {
                        call.parameters = func["arguments"];
                    }
                    if (json_opt2->contains("id")) {
                        call.id = (*json_opt2)["id"].get<std::string>();
                    }

                    tool_calls.push_back(call);
                }

                search_content = match.suffix();
                cache_used = true;
            }

            // Pattern 2: General JSON pattern (fallback)
            if (tool_calls.empty()) {
                if (std::regex_search(content, match, patterns_->cerebras_json_pattern)) {
                    std::string json_str = match[0].str();
                    auto json_opt3 = validate_json(json_str);

                    if (json_opt3 && json_opt3->contains("function_call")) {
                        ToolCall call;
                        call.status = "pending";
                        call.timestamp = std::chrono::system_clock::now();

                        const auto& fc = (*json_opt3)["function_call"];
                        if (fc.contains("name")) {
                            call.name = fc["name"].get<std::string>();
                        }
                        if (fc.contains("arguments")) {
                            call.parameters = fc["arguments"];
                        }

                        tool_calls.push_back(call);
                    }
                }
            }
        } else {
            // Fallback to base implementation
            tool_calls = extract_tool_calls(content);
        }

        auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now() - start_time).count();

        LOG_DEBUG("Tool call extraction completed in %ld us, found %zu calls, cache used: %s",
                 elapsed_us, tool_calls.size(), cache_used ? "yes" : "no");

        return tool_calls;

    } catch (const std::exception& e) {
        LOG_DEBUG("Tool call extraction failed: %s", e.what());
        return std::vector<ToolCall>(); // Return empty on error
    }
}

std::string CerebrasFormatter::generate_fast_toon(
    const std::string& content,
    const std::vector<ToolCall>& tool_calls,
    const ProcessingContext& context) const {

    try {
        // Fast TOON generation - minimal overhead
        nlohmann::json toon;

        // Basic TOON structure
        toon["format"] = "toon";
        toon["version"] = "1.0.0";
        toon["provider"] = context.provider_name;
        toon["model"] = context.model_name;

        // Content
        toon["content"] = content;

        // Tool calls (if any)
        if (!tool_calls.empty()) {
            nlohmann::json tool_array = nlohmann::json::array();
            for (const auto& tool : tool_calls) {
                tool_array.push_back(tool.to_json());
            }
            toon["tool_calls"] = tool_array;
        }

        // Metadata (lightweight for speed)
        toon["metadata"] = {
            {"processed_at", std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()},
            {"speed_optimized", optimize_speed_},
            {"source_format", context.original_format}
        };

        return toon.dump(); // Fast JSON serialization

    } catch (const std::exception& e) {
        LOG_DEBUG("Fast TOON generation failed: %s", e.what());
        // Fallback to basic format
        nlohmann::json fallback;
        fallback["format"] = "basic";
        fallback["content"] = content;
        fallback["error"] = "toon_generation_failed";
        return fallback.dump();
    }
}

void CerebrasFormatter::update_metrics(
    uint64_t processing_time_us,
    uint64_t tool_calls_count,
    bool cache_hit) const {

    total_processing_count_.fetch_add(1);
    total_processing_time_us_.fetch_add(processing_time_us);
    tool_calls_extracted_.fetch_add(tool_calls_count);

    if (cache_hit) {
        cache_hits_.fetch_add(1);
    } else {
        cache_misses_.fetch_add(1);
    }
}

bool CerebrasFormatter::should_trigger_fast_failover(int elapsed_time_ms) const {
    return enable_fast_failover_ && elapsed_time_ms > max_processing_time_ms_;
}

ProcessingResult CerebrasFormatter::fast_failover_process(
    const core::Response& response,
    const ProcessingContext& context) const {

    fast_failovers_triggered_.fetch_add(1);

    ProcessingResult result;
    result.success = true;
    result.output_format = "basic";
    result.processed_content = response.data; // Minimal processing
    result.streaming_mode = context.streaming_mode;
    result.metadata["fast_failover"] = true;
    result.metadata["reason"] = "processing_time_exceeded";

    LOG_DEBUG("Fast failover applied - minimal processing");
    return result;
}

// Factory function for plugin registration
extern "C" {
    std::shared_ptr<aimux::prettifier::PrettifierPlugin> create_cerebras_formatter() {
        return std::make_shared<aimux::prettifier::CerebrasFormatter>();
    }
}

} // namespace prettifier
} // namespace aimux