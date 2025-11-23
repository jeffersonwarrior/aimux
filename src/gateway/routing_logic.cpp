#include "aimux/gateway/routing_logic.hpp"
#include "aimux/logging/logger.hpp"
#include <algorithm>
#include <random>
#include <regex>
#include <sstream>
#include <cmath>
#include <unordered_set>

namespace aimux {
namespace gateway {

// ============================================================================
// Utility Functions Implementation
// ============================================================================

std::string request_type_to_string(RequestType type) {
    switch (type) {
        case RequestType::STANDARD: return "standard";
        case RequestType::THINKING: return "thinking";
        case RequestType::VISION: return "vision";
        case RequestType::MULTIMODAL: return "multimodal";
        case RequestType::TOOLS: return "tools";
        case RequestType::STREAMING: return "streaming";
        case RequestType::LONG_CONTEXT: return "long_context";
        default: return "unknown";
    }
}

RequestType string_to_request_type(const std::string& type_str) {
    if (type_str == "thinking") return RequestType::THINKING;
    if (type_str == "vision") return RequestType::VISION;
    if (type_str == "multimodal") return RequestType::MULTIMODAL;
    if (type_str == "tools") return RequestType::TOOLS;
    if (type_str == "streaming") return RequestType::STREAMING;
    if (type_str == "long_context") return RequestType::LONG_CONTEXT;
    return RequestType::STANDARD;
}

std::string routing_priority_to_string(RoutingPriority priority) {
    switch (priority) {
        case RoutingPriority::COST: return "cost";
        case RoutingPriority::PERFORMANCE: return "performance";
        case RoutingPriority::RELIABILITY: return "reliability";
        case RoutingPriority::BALANCED: return "balanced";
        case RoutingPriority::CUSTOM: return "custom";
        default: return "unknown";
    }
}

RoutingPriority string_to_routing_priority(const std::string& priority_str) {
    if (priority_str == "cost") return RoutingPriority::COST;
    if (priority_str == "performance") return RoutingPriority::PERFORMANCE;
    if (priority_str == "reliability") return RoutingPriority::RELIABILITY;
    if (priority_str == "custom") return RoutingPriority::CUSTOM;
    return RoutingPriority::BALANCED;
}

std::string capabilities_to_string(ProviderCapability capability) {
    std::vector<std::string> capabilities;

    if ((capability & ProviderCapability::THINKING) != ProviderCapability{}) {
        capabilities.push_back("thinking");
    }
    if ((capability & ProviderCapability::VISION) != ProviderCapability{}) {
        capabilities.push_back("vision");
    }
    if ((capability & ProviderCapability::TOOLS) != ProviderCapability{}) {
        capabilities.push_back("tools");
    }
    if ((capability & ProviderCapability::STREAMING) != ProviderCapability{}) {
        capabilities.push_back("streaming");
    }
    if ((capability & ProviderCapability::JSON_MODE) != ProviderCapability{}) {
        capabilities.push_back("json_mode");
    }
    if ((capability & ProviderCapability::FUNCTION_CALLING) != ProviderCapability{}) {
        capabilities.push_back("function_calling");
    }

    if (capabilities.empty()) {
        return "none";
    }

    std::ostringstream oss;
    for (size_t i = 0; i < capabilities.size(); ++i) {
        if (i > 0) oss << "|";
        oss << capabilities[i];
    }
    return oss.str();
}

// ============================================================================
// RequestAnalysis Implementation
// ============================================================================

nlohmann::json RequestAnalysis::to_json() const {
    nlohmann::json j;
    j["type"] = request_type_to_string(type_);
    j["required_capabilities"] = capabilities_to_string(required_capabilities_);
    j["estimated_tokens"] = estimated_tokens_;
    j["expected_response_time_ms"] = expected_response_time_ms_;
    j["requires_streaming"] = requires_streaming_;
    j["requires_tools"] = requires_tools_;
    j["requires_json_mode"] = requires_json_mode_;
    j["requires_function_calling"] = requires_function_calling_;
    j["cost_sensitivity"] = cost_sensitivity_;
    j["latency_sensitivity"] = latency_sensitivity_;
    return j;
}

// ============================================================================
// RoutingDecision Implementation
// ============================================================================

nlohmann::json RoutingDecision::to_json() const {
    nlohmann::json j;
    j["selected_provider"] = selected_provider_;
    j["reasoning"] = reasoning_;
    j["priority_used"] = routing_priority_to_string(priority_used_);
    j["selection_score"] = selection_score_;
    j["alternative_providers"] = alternative_providers_;
    return j;
}

// ============================================================================
// Load Balancer Implementations
// ============================================================================

std::string RoundRobinBalancer::select_provider(
    const std::vector<std::string>& providers,
    RequestType /* request_type */) const {

    if (providers.empty()) {
        return "";
    }

    size_t index = counter_.fetch_add(1) % providers.size();
    return providers[index];
}

WeightedBalancer::WeightedBalancer(ProviderHealthMonitor* health_monitor)
    : health_monitor_(health_monitor) {}

std::string WeightedBalancer::select_provider(
    const std::vector<std::string>& providers,
    RequestType request_type) const {

    if (providers.empty()) {
        return "";
    }

    if (providers.size() == 1) {
        return providers[0];
    }

    // Calculate weights for each provider
    std::vector<std::pair<std::string, double>> weighted_providers;
    double total_weight = 0.0;

    for (const auto& provider : providers) {
        if (!provider_is_suitable(provider, request_type)) {
            continue;
        }

        double weight = calculate_weight(provider, request_type);
        if (weight > 0.0) {
            weighted_providers.emplace_back(provider, weight);
            total_weight += weight;
        }
    }

    if (weighted_providers.empty() || total_weight <= 0.0) {
        // Fallback to first provider
        return providers[0];
    }

    // Random selection based on weights
    static thread_local std::random_device rd;
    static thread_local std::mt19937 gen(rd());
    std::uniform_real_distribution<double> dist(0.0, total_weight);
    double random_value = dist(gen);

    double cumulative_weight = 0.0;
    for (const auto& [provider, weight] : weighted_providers) {
        cumulative_weight += weight;
        if (random_value <= cumulative_weight) {
            return provider;
        }
    }

    return weighted_providers.back().first;
}

double WeightedBalancer::calculate_weight(const std::string& provider,
                                          RequestType /* request_type */) const {
    ProviderHealth* health = health_monitor_->get_provider_health(provider);
    if (!health) {
        return 0.0;
    }

    if (!health->is_healthy()) {
        return 0.0;
    }

    double weight = 1.0;

    // Performance score (higher is better)
    double performance_score = health->metrics_.performance_score_;
    weight *= performance_score;

    // Success rate (higher is better)
    double success_rate = health->metrics_.success_rate_;
    weight *= success_rate;

    // Response time (lower is better, so invert)
    double response_time = health->metrics_.avg_response_time_ms_;
    if (response_time > 0.0) {
        weight *= (1000.0 / response_time); // Invert and normalize
    }

    // Cost factor (lower cost is better, so invert)
    double cost_score = health->metrics_.cost_score_;
    if (cost_score > 0.0) {
        weight *= (1.0 / cost_score); // Invert
    }

    return std::max(0.0, weight);
}

bool WeightedBalancer::provider_is_suitable(const std::string& provider,
                                           RequestType request_type) const {
    ProviderHealth* health = health_monitor_->get_provider_health(provider);
    if (!health || !health->is_healthy()) {
        return false;
    }

    // Check capability requirements
    ProviderCapability required_caps = static_cast<ProviderCapability>(0);

    switch (request_type) {
        case RequestType::THINKING:
            required_caps = ProviderCapability::THINKING;
            break;
        case RequestType::VISION:
        case RequestType::MULTIMODAL:
            required_caps = ProviderCapability::VISION;
            break;
        case RequestType::TOOLS:
            required_caps = ProviderCapability::TOOLS;
            break;
        case RequestType::STREAMING:
            required_caps = ProviderCapability::STREAMING;
            break;
        case RequestType::STANDARD:
        default:
            // No special requirements
            break;
    }

    return health->has_capability(required_caps);
}

LeastConnectionsBalancer::LeastConnectionsBalancer(ProviderHealthMonitor* health_monitor)
    : health_monitor_(health_monitor) {}

std::string LeastConnectionsBalancer::select_provider(
    const std::vector<std::string>& providers,
    RequestType /* request_type */) const {

    if (providers.empty()) {
        return "";
    }

    std::string best_provider;
    int min_connections = std::numeric_limits<int>::max();

    for (const auto& provider : providers) {
        ProviderHealth* health = health_monitor_->get_provider_health(provider);
        if (!health || !health->is_healthy()) {
            continue;
        }

        int current_requests = health->metrics_.requests_per_minute_;
        if (current_requests < min_connections) {
            min_connections = current_requests;
            best_provider = provider;
        }
    }

    return best_provider.empty() ? providers[0] : best_provider;
}

// ============================================================================
// RoutingLogic Implementation
// ============================================================================

RoutingLogic::RoutingLogic(ProviderHealthMonitor* health_monitor)
    : health_monitor_(health_monitor) {}

RoutingDecision RoutingLogic::route_request(
    const core::Request& request,
    RoutingPriority priority) {

    // Analyze the request
    RequestAnalysis analysis = analyze_request(request);

    // Get all healthy providers
    std::vector<std::string> healthy_providers = health_monitor_->get_healthy_providers();

    if (healthy_providers.empty()) {
        RoutingDecision decision;
        decision.selected_provider_ = "";
        decision.reasoning_ = "No healthy providers available";
        decision.priority_used_ = priority;
        decision.selection_score_ = 0.0;
        return decision;
    }

    // Filter providers based on request requirements
    std::vector<std::string> capable_providers = filter_by_request_type(healthy_providers, analysis.type_);

    if (capable_providers.empty()) {
        // If no capable providers, try with all healthy providers
        capable_providers = healthy_providers;
    }

    // Select provider based on priority
    std::string selected_provider;

    switch (priority) {
        case RoutingPriority::COST:
            selected_provider = select_by_cost(capable_providers);
            break;
        case RoutingPriority::PERFORMANCE:
            selected_provider = select_by_performance(capable_providers);
            break;
        case RoutingPriority::RELIABILITY:
            selected_provider = select_by_reliability(capable_providers);
            break;
        case RoutingPriority::BALANCED:
            selected_provider = select_balanced(capable_providers, analysis);
            break;
        case RoutingPriority::CUSTOM:
            if (custom_priority_function_) {
                selected_provider = select_custom(capable_providers, analysis, custom_priority_function_);
            } else {
                selected_provider = select_balanced(capable_providers, analysis);
            }
            break;
        default:
            selected_provider = select_balanced(capable_providers, analysis);
            break;
    }

    // Prepare alternative providers
    std::vector<std::string> alternatives;
    for (const auto& provider : capable_providers) {
        if (provider != selected_provider) {
            alternatives.push_back(provider);
        }
    }

    // Create routing decision
    RoutingDecision decision;
    decision.selected_provider_ = selected_provider;
    decision.priority_used_ = priority;
    decision.alternative_providers_ = alternatives;
    decision.selection_score_ = calculate_provider_score(selected_provider, analysis.required_capabilities_, priority);
    decision.reasoning_ = generate_reasoning(decision, analysis, capable_providers);

    // Record the routing decision
    record_routing_decision(decision);

    return decision;
}

std::string RoutingLogic::select_by_cost(const std::vector<std::string>& providers) {
    if (providers.empty()) {
        return "";
    }

    std::string cheapest_provider;
    double min_cost = std::numeric_limits<double>::max();

    for (const auto& provider : providers) {
        ProviderHealth* health = health_monitor_->get_provider_health(provider);
        if (!health) {
            continue;
        }

        double cost = health->metrics_.cost_per_output_token_;
        if (cost < min_cost) {
            min_cost = cost;
            cheapest_provider = provider;
        }
    }

    return cheapest_provider.empty() ? providers[0] : cheapest_provider;
}

std::string RoutingLogic::select_by_performance(const std::vector<std::string>& providers) {
    if (providers.empty()) {
        return "";
    }

    std::string fastest_provider;
    double min_response_time = std::numeric_limits<double>::max();

    for (const auto& provider : providers) {
        ProviderHealth* health = health_monitor_->get_provider_health(provider);
        if (!health) {
            continue;
        }

        double response_time = health->metrics_.avg_response_time_ms_;
        if (response_time < min_response_time) {
            min_response_time = response_time;
            fastest_provider = provider;
        }
    }

    return fastest_provider.empty() ? providers[0] : fastest_provider;
}

std::string RoutingLogic::select_by_reliability(const std::vector<std::string>& providers) {
    if (providers.empty()) {
        return "";
    }

    std::string most_reliable_provider;
    double max_success_rate = -1.0;

    for (const auto& provider : providers) {
        ProviderHealth* health = health_monitor_->get_provider_health(provider);
        if (!health) {
            continue;
        }

        double success_rate = health->metrics_.success_rate_;
        if (success_rate > max_success_rate) {
            max_success_rate = success_rate;
            most_reliable_provider = provider;
        }
    }

    return most_reliable_provider.empty() ? providers[0] : most_reliable_provider;
}

std::string RoutingLogic::select_balanced(const std::vector<std::string>& providers,
                                         const RequestAnalysis& analysis) {
    if (providers.empty()) {
        return "";
    }

    if (providers.size() == 1) {
        return providers[0];
    }

    // Apply load balancing if configured
    if (load_balancer_) {
        return load_balancer_->select_provider(providers, analysis.type_);
    }

    // Fallback to weighted selection based on performance metrics
    std::string best_provider;
    double best_score = -1.0;

    for (const auto& provider : providers) {
        double score = calculate_provider_score(provider, analysis.required_capabilities_, RoutingPriority::BALANCED);
        if (score > best_score) {
            best_score = score;
            best_provider = provider;
        }
    }

    return best_provider.empty() ? providers[0] : best_provider;
}

std::string RoutingLogic::select_custom(const std::vector<std::string>& providers,
                                       const RequestAnalysis& analysis,
                                       CustomPriorityFunction custom_func) {
    if (!custom_func || providers.empty()) {
        return "";
    }

    // Prepare health states for custom function
    std::unordered_map<std::string, ProviderHealth*> health_states;
    for (const auto& provider : providers) {
        health_states[provider] = health_monitor_->get_provider_health(provider);
    }

    return custom_func(providers, analysis, health_states);
}

void RoutingLogic::set_load_balancer(std::unique_ptr<LoadBalancer> balancer) {
    load_balancer_ = std::move(balancer);
}

std::string RoutingLogic::apply_load_balancing(
    const std::vector<std::string>& providers,
    RequestType request_type) {

    if (load_balancer_) {
        return load_balancer_->select_provider(providers, request_type);
    }

    // Default to round-robin if no load balancer is configured
    static thread_local std::atomic<size_t> counter{0};
    size_t index = counter.fetch_add(1) % providers.size();
    return providers[index];
}

// ============================================================================
// Request Analysis Implementation
// ============================================================================

RequestAnalysis RoutingLogic::analyze_request(const core::Request& request) {
    RequestAnalysis analysis;

    try {
        // Extract messages from request data
        nlohmann::json messages;
        if (request.data.contains("messages")) {
            messages = request.data["messages"];
        } else if (request.data.contains("prompt")) {
            // Handle single prompt format
            messages = nlohmann::json::array({{
                {"role", "user"},
                {"content", request.data["prompt"]}
            }});
        } else if (request.data.contains("content")) {
            // Handle single content format
            messages = nlohmann::json::array({{
                {"role", "user"},
                {"content", request.data["content"]}
            }});
        }

        // Analyze content
        std::string content_text;
        bool has_images = false;
        bool requires_tools = false;
        bool requires_streaming = false;
        bool requires_json_mode = false;
        bool requires_function_calling = false;

        // Extract and analyze content
        for (const auto& message : messages) {
            if (message.contains("content")) {
                const auto& content = message["content"];

                if (content.is_string()) {
                    content_text += content.get<std::string>() + " ";
                } else if (content.is_array()) {
                    // Handle multimodal content
                    for (const auto& content_item : content) {
                        if (content_item.contains("type")) {
                            std::string type = content_item["type"];
                            if (type == "text" && content_item.contains("text")) {
                                content_text += content_item["text"].get<std::string>() + " ";
                            } else if (type == "image") {
                                has_images = true;
                            }
                        }
                    }
                }
            }

            // Check for function/tool calls
            if (message.contains("tool_calls") || message.contains("function_call")) {
                requires_function_calling = true;
            }

            // Check for tool use in the content
            std::string message_str = message.dump();
            if (message_str.find("tool_call") != std::string::npos ||
                message_str.find("function_call") != std::string::npos) {
                requires_tools = true;
            }
        }

        // Analyze request parameters
        if (request.data.contains("stream")) {
            requires_streaming = request.data["stream"].get<bool>();
        }

        if (request.data.contains("response_format")) {
            const auto& format = request.data["response_format"];
            if (format.contains("type") && format["type"] == "json") {
                requires_json_mode = true;
            }
        }

        // Determine request type
        if (has_images) {
            analysis.type_ = RequestType::MULTIMODAL;
        } else if (is_thinking_request(content_text)) {
            analysis.type_ = RequestType::THINKING;
        } else if (requires_tools || requires_function_calling) {
            analysis.type_ = RequestType::TOOLS;
        } else if (requires_streaming) {
            analysis.type_ = RequestType::STREAMING;
        } else if (content_text.length() > 10000) { // Long content threshold
            analysis.type_ = RequestType::LONG_CONTEXT;
        } else {
            analysis.type_ = RequestType::STANDARD;
        }

        // Set required capabilities
        ProviderCapability caps = static_cast<ProviderCapability>(0);

        if (analysis.type_ == RequestType::THINKING) {
            caps = caps | ProviderCapability::THINKING;
        }

        if (has_images || analysis.type_ == RequestType::MULTIMODAL) {
            caps = caps | ProviderCapability::VISION;
        }

        if (requires_tools) {
            caps = caps | ProviderCapability::TOOLS;
        }

        if (requires_streaming) {
            caps = caps | ProviderCapability::STREAMING;
        }

        if (requires_json_mode) {
            caps = caps | ProviderCapability::JSON_MODE;
        }

        if (requires_function_calling) {
            caps = caps | ProviderCapability::FUNCTION_CALLING;
        }

        analysis.required_capabilities_ = caps;
        analysis.requires_streaming_ = requires_streaming;
        analysis.requires_tools_ = requires_tools;
        analysis.requires_json_mode_ = requires_json_mode;
        analysis.requires_function_calling_ = requires_function_calling;

        // Estimate tokens (rough estimate: 1 token ≈ 4 characters)
        analysis.estimated_tokens_ = std::max(100, static_cast<int>(content_text.length() / 4));

        // Set sensitivity values based on request type
        if (analysis.type_ == RequestType::THINKING || analysis.type_ == RequestType::LONG_CONTEXT) {
            analysis.cost_sensitivity_ = 0.3;  // Less cost sensitive for complex requests
            analysis.latency_sensitivity_ = 0.4;  // More patient for complex requests
        } else if (analysis.type_ == RequestType::STREAMING) {
            analysis.cost_sensitivity_ = 0.7;
            analysis.latency_sensitivity_ = 0.8;  // High latency sensitivity for streaming
        } else {
            analysis.cost_sensitivity_ = 0.5;
            analysis.latency_sensitivity_ = 0.5;
        }

        // Estimate response time based on request complexity
        double base_time = 1000.0; // 1 second base
        if (analysis.type_ == RequestType::THINKING) {
            analysis.expected_response_time_ms_ = base_time * 3.0;
        } else if (analysis.type_ == RequestType::MULTIMODAL) {
            analysis.expected_response_time_ms_ = base_time * 2.0;
        } else if (analysis.type_ == RequestType::LONG_CONTEXT) {
            analysis.expected_response_time_ms_ = base_time * 2.5;
        } else {
            analysis.expected_response_time_ms_ = base_time;
        }

    } catch (const std::exception& e) {
        aimux::error("Error analyzing request: " + std::string(e.what()));
        // Set default analysis
        analysis.type_ = RequestType::STANDARD;
        analysis.estimated_tokens_ = 1000;
        analysis.expected_response_time_ms_ = 1000.0;
    }

    return analysis;
}

bool RoutingLogic::is_thinking_request(const std::string& content) {
    std::string lower_content = content;
    std::transform(lower_content.begin(), lower_content.end(), lower_content.begin(), ::tolower);

    // Check for thinking keywords
    for (const auto& keyword : thinking_keywords_) {
        if (lower_content.find(keyword) != std::string::npos) {
            return true;
        }
    }

    // Additional patterns for thinking requests
    std::vector<std::regex> thinking_patterns = {
        std::regex(R"(\bstep\s+by\s+step\b)"),
        std::regex(R"(\bbreak\s+down\b)"),
        std::regex(R"(\bexplain\s+your\s+reasoning\b)"),
        std::regex(R"(\bthink\s+aloud\b)"),
        std::regex(R"(\bshow\s+your\s+work\b)"),
        std::regex(R"(\bwalk\s+me\s+through\b)"),
        std::regex(R"(\bhow\s+would\s+you\s+approach\b)"),
        std::regex(R"(\bwhat\s+are\s+the\s+steps\b)")
    };

    for (const auto& pattern : thinking_patterns) {
        if (std::regex_search(lower_content, pattern)) {
            return true;
        }
    }

    return false;
}

bool RoutingLogic::has_vision_content(const nlohmann::json& content) {
    try {
        if (content.is_string()) {
            std::string str = content.get<std::string>();
            std::transform(str.begin(), str.end(), str.begin(), ::tolower);

            // Check for vision keywords
            for (const auto& keyword : vision_keywords_) {
                if (str.find(keyword) != std::string::npos) {
                    return true;
                }
            }
        } else if (content.is_array()) {
            // Check for image content in multimodal messages
            for (const auto& item : content) {
                if (item.contains("type") && item["type"] == "image") {
                    return true;
                }
                if (item.contains("image_url")) {
                    return true;
                }
            }
        } else if (content.is_object()) {
            // Check for image-related fields
            if (content.contains("image_url") ||
                content.contains("image") ||
                content.contains("base64")) {
                return true;
            }
        }
    } catch (const std::exception&) {
        // Return false on parsing errors
    }

    return false;
}

bool RoutingLogic::requires_tools(const nlohmann::json& message) {
    try {
        std::string message_str = message.dump();

        // Check for tool-related keywords
        if (message_str.find("tool_call") != std::string::npos ||
            message_str.find("function_call") != std::string::npos ||
            message_str.find("tools") != std::string::npos) {
            return true;
        }

        // Check for tool call structure
        if (message.contains("tool_calls") && !message["tool_calls"].empty()) {
            return true;
        }

        if (message.contains("function_call")) {
            return true;
        }

        // Check in content field
        if (message.contains("content")) {
            const auto& content = message["content"];
            if (content.is_string()) {
                std::string content_str = content.get<std::string>();
                std::transform(content_str.begin(), content_str.end(), content_str.begin(), ::tolower);

                if (content_str.find("use the ") != std::string::npos ||
                    content_str.find("call the ") != std::string::npos ||
                    content_str.find("access the ") != std::string::npos) {
                    return true;
                }
            }
        }

    } catch (const std::exception&) {
        // Return false on parsing errors
    }

    return false;
}

bool RoutingLogic::requires_streaming(const nlohmann::json& data) {
    if (data.contains("stream")) {
        return data["stream"].get<bool>();
    }

    // Check for streaming keywords in the request
    std::string data_str = data.dump();
    std::transform(data_str.begin(), data_str.end(), data_str.begin(), ::tolower);

    return data_str.find("stream") != std::string::npos &&
           (data_str.find("true") != std::string::npos ||
            data_str.find("yes") != std::string::npos);
}

// ============================================================================
// Configuration Management
// ============================================================================

void RoutingLogic::set_thinking_keywords(const std::vector<std::string>& keywords) {
    thinking_keywords_ = keywords;
}

void RoutingLogic::set_vision_keywords(const std::vector<std::string>& keywords) {
    vision_keywords_ = keywords;
}

// ============================================================================
// Provider Filtering
// ============================================================================

std::vector<std::string> RoutingLogic::filter_by_capability(
    const std::vector<std::string>& providers,
    ProviderCapability capability) {

    std::vector<std::string> filtered;

    for (const auto& provider : providers) {
        ProviderHealth* health = health_monitor_->get_provider_health(provider);
        if (health && health->has_capability(capability)) {
            filtered.push_back(provider);
        }
    }

    return filtered;
}

std::vector<std::string> RoutingLogic::filter_by_request_type(
    const std::vector<std::string>& providers,
    RequestType request_type) {

    ProviderCapability required_caps = static_cast<ProviderCapability>(0);

    switch (request_type) {
        case RequestType::THINKING:
            required_caps = ProviderCapability::THINKING;
            break;
        case RequestType::VISION:
        case RequestType::MULTIMODAL:
            required_caps = ProviderCapability::VISION;
            break;
        case RequestType::TOOLS:
            required_caps = ProviderCapability::TOOLS;
            break;
        case RequestType::STREAMING:
            required_caps = ProviderCapability::STREAMING;
            break;
        default:
            // No specific capability required
            return providers;
    }

    return filter_by_capability(providers, required_caps);
}

std::vector<std::string> RoutingLogic::filter_by_capacity(
    const std::vector<std::string>& providers,
    int additional_requests) {

    std::vector<std::string> filtered;

    for (const auto& provider : providers) {
        ProviderHealth* health = health_monitor_->get_provider_health(provider);
        if (health && health->can_accept_requests()) {
            // Check if provider has capacity
            int current_requests = health->metrics_.requests_per_minute_;
            int max_requests = health->metrics_.max_requests_per_minute_;

            if (current_requests + additional_requests <= max_requests) {
                filtered.push_back(provider);
            }
        }
    }

    return filtered;
}

// ============================================================================
// Metrics and Monitoring
// ============================================================================

void RoutingLogic::record_routing_decision(const RoutingDecision& decision) {
    std::unique_lock<std::shared_mutex> lock(metrics_mutex_);

    // Update provider selection counts
    provider_selection_counts_[decision.selected_provider_]++;
    total_routings_++;

    // Update request type counts
    priority_usage_counts_[decision.priority_used_]++;
}

nlohmann::json RoutingLogic::get_routing_metrics() const {
    std::shared_lock<std::shared_mutex> lock(metrics_mutex_);

    nlohmann::json metrics;
    metrics["total_routings"] = total_routings_.load();
    metrics["provider_selection_counts"] = provider_selection_counts_;
    metrics["request_type_counts"] = request_type_counts_;

    // Calculate percentages
    if (total_routings_.load() > 0) {
        nlohmann::json percentages;
        for (const auto& [provider, count] : provider_selection_counts_) {
            double percentage = (static_cast<double>(count) / total_routings_.load()) * 100.0;
            percentages[provider] = percentage;
        }
        metrics["provider_selection_percentages"] = percentages;
    }

    return metrics;
}

// ============================================================================
// Internal Helper Functions
// ============================================================================

ProviderCapability RoutingLogic::get_required_capabilities(const RequestAnalysis& analysis) {
    ProviderCapability caps = analysis.required_capabilities_;

    // Add implicit capabilities based on request type
    if (analysis.type_ == RequestType::STREAMING) {
        caps = caps | ProviderCapability::STREAMING;
    }

    return caps;
}

std::vector<std::string> RoutingLogic::get_capable_providers(ProviderCapability capabilities) {
    std::vector<std::string> all_providers = health_monitor_->get_healthy_providers();
    return filter_by_capability(all_providers, capabilities);
}

double RoutingLogic::calculate_provider_score(const std::string& provider,
                                             ProviderCapability /* capabilities */,
                                             RoutingPriority priority) {
    ProviderHealth* health = health_monitor_->get_provider_health(provider);
    if (!health || !health->is_healthy()) {
        return 0.0;
    }

    double score = 0.0;

    // Base score from provider health
    score += health->metrics_.success_rate_ * 0.4;  // 40% weight for success rate
    score += health->metrics_.performance_score_ * 0.3;  // 30% weight for performance

    // Priority-specific scoring
    switch (priority) {
        case RoutingPriority::COST:
            // Lower cost = higher score
            if (health->metrics_.cost_per_output_token_ > 0) {
                score += (1.0 / health->metrics_.cost_per_output_token_) * 0.3;
            }
            break;

        case RoutingPriority::PERFORMANCE:
            // Lower response time = higher score
            if (health->metrics_.avg_response_time_ms_ > 0) {
                score += (1000.0 / health->metrics_.avg_response_time_ms_) * 0.3;
            }
            break;

        case RoutingPriority::RELIABILITY:
            // Higher success rate = higher score
            score += health->metrics_.success_rate_ * 0.3;
            break;

        case RoutingPriority::BALANCED:
        default:
            // Balanced scoring
            if (health->metrics_.cost_per_output_token_ > 0) {
                score += (1.0 / health->metrics_.cost_per_output_token_) * 0.15;
            }
            if (health->metrics_.avg_response_time_ms_ > 0) {
                score += (1000.0 / health->metrics_.avg_response_time_ms_) * 0.15;
            }
            break;
    }

    return std::min(1.0, score);
}

std::string RoutingLogic::generate_reasoning(const RoutingDecision& decision,
                                            const RequestAnalysis& analysis,
                                            const std::vector<std::string>& candidates) {
    std::ostringstream reasoning;

    reasoning << "Selected provider '" << decision.selected_provider_
              << "' based on " << routing_priority_to_string(decision.priority_used_)
              << " priority. ";

    ProviderHealth* health = health_monitor_->get_provider_health(decision.selected_provider_);
    if (health) {
        reasoning << "Provider health: " << health_status_to_string(health->status_.load())
                  << ", Success rate: " << std::fixed << std::setprecision(2)
                  << (health->metrics_.success_rate_ * 100) << "%, "
                  << "Avg response time: " << std::setprecision(0)
                  << health->metrics_.avg_response_time_ms_ << "ms. ";
    }

    reasoning << "Request type: " << request_type_to_string(analysis.type_) << ". ";
    reasoning << "Evaluated " << candidates.size() << " candidate providers. ";
    reasoning << "Selection score: " << std::fixed << std::setprecision(3)
              << decision.selection_score_;

    return reasoning.str();
}

} // namespace gateway
} // namespace aimux