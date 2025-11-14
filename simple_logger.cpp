#include <iostream>
#include <nlohmann/json.hpp>

namespace aimux {

void info(const std::string& message, const nlohmann::json& context = {}) {
    std::cout << "[INFO] " << message;
    if (!context.empty()) {
        std::cout << " " << context.dump();
    }
    std::cout << std::endl;
}

void warn(const std::string& message, const nlohmann::json& context = {}) {
    std::cout << "[WARN] " << message;
    if (!context.empty()) {
        std::cout << " " << context.dump();
    }
    std::cout << std::endl;
}

void error(const std::string& message, const nlohmann::json& context = {}) {
    std::cout << "[ERROR] " << message;
    if (!context.empty()) {
        std::cout << " " << context.dump();
    }
    std::cout << std::endl;
}

void debug(const std::string& message, const nlohmann::json& context = {}) {
    std::cout << "[DEBUG] " << message;
    if (!context.empty()) {
        std::cout << " " << context.dump();
    }
    std::cout << std::endl;
}

} // namespace aimux