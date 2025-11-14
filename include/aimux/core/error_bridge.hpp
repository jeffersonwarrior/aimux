#pragma once

#include <string>
#include <nlohmann/json.hpp>
#include "aimux/core/bridge.hpp"

namespace aimux {
namespace core {

/**
 * @brief Error bridge for unsupported or misconfigured providers
 */
class ErrorBridge : public Bridge {
public:
    explicit ErrorBridge(const std::string& provider_name);
    
    Response send_request(const Request& request) override;
    bool is_healthy() const override;
    std::string get_provider_name() const override;
    nlohmann::json get_rate_limit_status() const override;

private:
    std::string provider_name_;
    size_t request_count_;
};

} // namespace core
} // namespace aimux
