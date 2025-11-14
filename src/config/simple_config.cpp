#include <string>
#include "config/production_config.h"

namespace aimux {
namespace config {

// Simple fallback implementation for ProductionConfigManager
class SimpleConfigManager {
public:
    static SimpleConfigManager& getInstance() {
        static SimpleConfigManager instance;
        return instance;
    }

    std::string resolveBindAddress(const WebUIConfig& config) const {
        if (config.bind_address == "auto" || config.bind_address.empty()) {
            return "127.0.0.1";
        }
        return config.bind_address;
    }

    std::string detectZeroTierIP() const {
        return ""; // Not implemented in simple version
    }

    std::vector<std::string> getAvailableIPAddresses() const {
        std::vector<std::string> ips;
        ips.push_back("127.0.0.1");
        ips.push_back("localhost");
        return ips;
    }

    std::string getServiceStatus() const {
        return "unknown";
    }

    bool installService() const {
        return false;
    }

    bool uninstallService() const {
        return false;
    }

    bool reinstallService() const {
        return false;
    }
};

// Global wrapper functions
ProductionConfigManager& ProductionConfigManager::getInstance() {
    return reinterpret_cast<ProductionConfigManager&>(SimpleConfigManager::getInstance());
}

std::string ProductionConfigManager::resolveBindAddress(const WebUIConfig& config) const {
    return SimpleConfigManager::getInstance().resolveBindAddress(config);
}

std::string ProductionConfigManager::detectZeroTierIP() const {
    return SimpleConfigManager::getInstance().detectZeroTierIP();
}

std::vector<std::string> ProductionConfigManager::getAvailableIPAddresses() const {
    return SimpleConfigManager::getInstance().getAvailableIPAddresses();
}

std::string ProductionConfigManager::getServiceStatus() const {
    return SimpleConfigManager::getInstance().getServiceStatus();
}

bool ProductionConfigManager::installService() const {
    return SimpleConfigManager::getInstance().installService();
}

bool ProductionConfigManager::uninstallService() const {
    return SimpleConfigManager::getInstance().uninstallService();
}

bool ProductionConfigManager::reinstallService() const {
    return SimpleConfigManager::getInstance().reinstallService();
}

} // namespace config
} // namespace aimux