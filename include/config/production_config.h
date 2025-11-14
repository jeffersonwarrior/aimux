#ifndef AIMUX_PRODUCTION_CONFIG_H
#define AIMUX_PRODUCTION_CONFIG_H

#include <string>
#include <map>
#include <vector>
#include <memory>
#include <optional>
#include <functional>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <mutex>
#include <atomic>
#include <thread>
#include "aimux/security/secure_config.h"

namespace aimux {
namespace config {

/**
 * Production configuration management system
 * Features: validation, migration, environment override, hot reload, encryption
 */

// Configuration structure definitions
struct ProviderConfig {
    std::string name;
    std::string api_key;
    std::string endpoint;
    std::optional<std::string> group_id;
    std::vector<std::string> models;
    bool enabled = true;
    int max_requests_per_minute = 60;
    int priority = 1;
    int retry_attempts = 3;
    int timeout_ms = 30000;
    std::map<std::string, nlohmann::json> custom_settings;

    nlohmann::json toJson() const {
        nlohmann::json j;
        j["name"] = name;
        j["api_key"] = api_key; // Will be encrypted/decrypted at rest
        j["endpoint"] = endpoint;
        if (group_id) j["group_id"] = *group_id;
        j["models"] = models;
        j["enabled"] = enabled;
        j["max_requests_per_minute"] = max_requests_per_minute;
        j["priority"] = priority;
        j["retry_attempts"] = retry_attempts;
        j["timeout_ms"] = timeout_ms;
        if (!custom_settings.empty()) {
            j["custom_settings"] = custom_settings;
        }
        return j;
    }

    static ProviderConfig fromJson(const nlohmann::json& j) {
        ProviderConfig config;
        config.name = j.value("name", "");
        config.api_key = j.value("api_key", "");
        config.endpoint = j.value("endpoint", "");
        if (j.contains("group_id") && !j["group_id"].is_null()) {
            config.group_id = j["group_id"];
        }
        config.models = j.value("models", std::vector<std::string>{});
        config.enabled = j.value("enabled", true);
        config.max_requests_per_minute = j.value("max_requests_per_minute", 60);
        config.priority = j.value("priority", 1);
        config.retry_attempts = j.value("retry_attempts", 3);
        config.timeout_ms = j.value("timeout_ms", 30000);
        config.custom_settings = j.value("custom_settings", nlohmann::json::object());

        return config;
    }
};

struct WebUIConfig {
    bool enabled = true;
    int port = 8080;
    int ssl_port = 8443;
    bool ssl_enabled = false;
    std::string cert_file;
    std::string key_file;
    bool cors_enabled = true;
    std::vector<std::string> cors_origins = {"localhost", "127.0.0.1"};
    bool api_docs = true;
    bool real_time_metrics = true;

    // Enhanced IP configuration
    std::string bind_address = "auto";  // "auto", "0.0.0.0", specific IP, or "zerotier"
    bool auto_ip_discovery = true;
    std::string preferred_interface = "zerotier";
    std::string detected_ip = "";
    std::string zerotier_interface_prefix = "zt";

    // MetricsStreamer configuration
    uint32_t metrics_update_interval_ms = 1000;
    uint32_t websocket_broadcast_interval_ms = 2000;
    uint32_t max_websocket_connections = 100;
    bool enable_delta_compression = true;
    bool enable_websocket_auth = false;
    std::string websocket_auth_token = "";
    uint32_t history_retention_minutes = 60;

    nlohmann::json toJson() const {
        nlohmann::json j;
        j["enabled"] = enabled;
        j["port"] = port;
        j["ssl_port"] = ssl_port;
        j["ssl_enabled"] = ssl_enabled;
        if (!cert_file.empty()) j["cert_file"] = cert_file;
        if (!key_file.empty()) j["key_file"] = key_file;
        j["cors_enabled"] = cors_enabled;
        j["cors_origins"] = cors_origins;
        j["api_docs"] = api_docs;
        j["real_time_metrics"] = real_time_metrics;

        // Enhanced IP configuration
        j["bind_address"] = bind_address;
        j["auto_ip_discovery"] = auto_ip_discovery;
        j["preferred_interface"] = preferred_interface;
        if (!detected_ip.empty()) j["detected_ip"] = detected_ip;
        j["zerotier_interface_prefix"] = zerotier_interface_prefix;

        return j;
    }

    static WebUIConfig fromJson(const nlohmann::json& j) {
        WebUIConfig config;
        config.enabled = j.value("enabled", true);
        config.port = j.value("port", 8080);
        config.ssl_port = j.value("ssl_port", 8443);
        config.ssl_enabled = j.value("ssl_enabled", false);
        config.cert_file = j.value("cert_file", "");
        config.key_file = j.value("key_file", "");
        config.cors_enabled = j.value("cors_enabled", true);
        config.cors_origins = j.value("cors_origins", std::vector<std::string>{"localhost", "127.0.0.1"});
        config.api_docs = j.value("api_docs", true);
        config.real_time_metrics = j.value("real_time_metrics", true);

        // Enhanced IP configuration
        config.bind_address = j.value("bind_address", "auto");
        config.auto_ip_discovery = j.value("auto_ip_discovery", true);
        config.preferred_interface = j.value("preferred_interface", "zerotier");
        config.detected_ip = j.value("detected_ip", "");
        config.zerotier_interface_prefix = j.value("zerotier_interface_prefix", "zt");

        return config;
    }
};

struct DaemonConfig {
    bool enabled = true;
    std::string user = "aimux";
    std::string group = "aimux";
    std::string working_directory = "/var/lib/aimux";
    std::string log_file = "/var/log/aimux/aimux.log";
    std::string pid_file = "/var/run/aimux.pid";
    bool auto_restart = true;

    nlohmann::json toJson() const {
        nlohmann::json j;
        j["enabled"] = enabled;
        j["user"] = user;
        j["group"] = group;
        j["working_directory"] = working_directory;
        j["log_file"] = log_file;
        j["pid_file"] = pid_file;
        j["auto_restart"] = auto_restart;
        return j;
    }

    static DaemonConfig fromJson(const nlohmann::json& j) {
        DaemonConfig config;
        config.enabled = j.value("enabled", true);
        config.user = j.value("user", "aimux");
        config.group = j.value("group", "aimux");
        config.working_directory = j.value("working_directory", "/var/lib/aimux");
        config.log_file = j.value("log_file", "/var/log/aimux/aimux.log");
        config.pid_file = j.value("pid_file", "/var/run/aimux.pid");
        config.auto_restart = j.value("auto_restart", true);
        return config;
    }
};

struct RateLimitingConfig {
    bool enabled = true;
    int requests_per_minute = 1000;
    int ban_duration_minutes = 60;

    nlohmann::json toJson() const {
        nlohmann::json j;
        j["enabled"] = enabled;
        j["requests_per_minute"] = requests_per_minute;
        j["ban_duration_minutes"] = ban_duration_minutes;
        return j;
    }

    static RateLimitingConfig fromJson(const nlohmann::json& j) {
        RateLimitingConfig config;
        config.enabled = j.value("enabled", true);
        config.requests_per_minute = j.value("requests_per_minute", 1000);
        config.ban_duration_minutes = j.value("ban_duration_minutes", 60);
        return config;
    }
};

struct SecurityConfig {
    bool api_key_encryption = true;
    bool audit_logging = true;
    bool input_validation = true;
    RateLimitingConfig rate_limiting;
    bool ssl_verification = true;
    bool require_https = true;
    std::vector<std::string> allowed_origins = {"localhost", "127.0.0.1", "::1"};

    nlohmann::json toJson() const {
        nlohmann::json j;
        j["api_key_encryption"] = api_key_encryption;
        j["audit_logging"] = audit_logging;
        j["input_validation"] = input_validation;
        j["rate_limiting"] = rate_limiting.toJson();
        j["ssl_verification"] = ssl_verification;
        j["require_https"] = require_https;
        j["allowed_origins"] = allowed_origins;
        return j;
    }

    static SecurityConfig fromJson(const nlohmann::json& j) {
        SecurityConfig config;
        config.api_key_encryption = j.value("api_key_encryption", true);
        config.audit_logging = j.value("audit_logging", true);
        config.input_validation = j.value("input_validation", true);
        config.rate_limiting = RateLimitingConfig::fromJson(j.value("rate_limiting", nlohmann::json::object()));
        config.ssl_verification = j.value("ssl_verification", true);
        config.require_https = j.value("require_https", true);
        config.allowed_origins = j.value("allowed_origins", std::vector<std::string>{"localhost", "127.0.0.1", "::1"});
        return config;
    }
};

struct SystemConfig {
    std::string environment = "production";
    std::string log_level = "info";
    bool structured_logging = true;
    int max_concurrent_requests = 1000;
    std::string log_dir = "/var/log/aimux";
    std::string backup_dir = "/var/backups/aimux";
    int backup_retention_days = 30;

    nlohmann::json toJson() const {
        nlohmann::json j;
        j["environment"] = environment;
        j["log_level"] = log_level;
        j["structured_logging"] = structured_logging;
        j["max_concurrent_requests"] = max_concurrent_requests;
        j["log_dir"] = log_dir;
        j["backup_dir"] = backup_dir;
        j["backup_retention_days"] = backup_retention_days;
        return j;
    }

    static SystemConfig fromJson(const nlohmann::json& j) {
        SystemConfig config;
        config.environment = j.value("environment", "production");
        config.log_level = j.value("log_level", "info");
        config.structured_logging = j.value("structured_logging", true);
        config.max_concurrent_requests = j.value("max_concurrent_requests", 1000);
        config.log_dir = j.value("log_dir", "/var/log/aimux");
        config.backup_dir = j.value("backup_dir", "/var/backups/aimux");
        config.backup_retention_days = j.value("backup_retention_days", 30);
        return config;
    }
};

struct ProductionConfig {
    std::vector<ProviderConfig> providers;
    WebUIConfig webui;
    DaemonConfig daemon;
    SecurityConfig security;
    SystemConfig system;

    nlohmann::json toJson(bool encryptApiKeys = true) const {
        nlohmann::json j;
        j["providers"] = nlohmann::json::array();

        for (const auto& provider : providers) {
            auto providerJson = provider.toJson();
            if (encryptApiKeys && !provider.api_key.empty()) {
                try {
                    auto& securityManager = aimux::security::SecurityManager::getInstance();
                    providerJson["api_key"] = securityManager.encryptApiKey(provider.api_key);
                } catch (const std::exception&) {
                    // Fallback to redaction if encryption fails
                    providerJson["api_key"] = "***ENCRYPTED***";
                }
            }
            j["providers"].push_back(providerJson);
        }

        j["webui"] = webui.toJson();
        j["daemon"] = daemon.toJson();
        j["security"] = security.toJson();
        j["system"] = system.toJson();
        return j;
    }

    static ProductionConfig fromJson(const nlohmann::json& j) {
        ProductionConfig config;

        if (j.contains("providers")) {
            for (const auto& providerJson : j["providers"]) {
                auto providerConfig = ProviderConfig::fromJson(providerJson);

                // Decrypt API keys if encrypted
                if (!providerConfig.api_key.empty() && providerConfig.api_key != "***ENCRYPTED***") {
                    try {
                        auto& securityManager = aimux::security::SecurityManager::getInstance();
                        // Try decryption first, if it fails assume it's plaintext
                        try {
                            providerConfig.api_key = securityManager.decryptApiKey(providerConfig.api_key);
                        } catch (const std::exception&) {
                            // Already plaintext
                        }
                    } catch (const std::exception&) {
                        // Security manager not available
                    }
                }

                config.providers.push_back(providerConfig);
            }
        }

        config.webui = WebUIConfig::fromJson(j.value("webui", nlohmann::json::object()));
        config.daemon = DaemonConfig::fromJson(j.value("daemon", nlohmann::json::object()));
        config.security = SecurityConfig::fromJson(j.value("security", nlohmann::json::object()));
        config.system = SystemConfig::fromJson(j.value("system", nlohmann::json::object()));

        return config;
    }
};

// Configuration manager
class ProductionConfigManager {
public:
    static ProductionConfigManager& getInstance();

    // Configuration loading
    const ProductionConfig& getConfig() const;
    bool loadConfig(const std::string& configPath, bool createIfMissing = false);
    bool saveConfig(const std::string& configPath = "");
    bool reloadConfig();

    // Environment override
    void loadEnvironmentOverrides();
    void validateEnvironment() const;

    // Configuration validation
    std::vector<std::string> validateConfig() const;
    bool isConfigValid() const;

    // Hot reload
    void startWatching(const std::string& configPath);
    void stopWatching();
    void setConfigChangeCallback(std::function<void(const ProductionConfig&)> callback);

    // Configuration templates
    ProductionConfig createProductionTemplate() const;
    ProductionConfig createDevelopmentTemplate() const;
    bool migrateConfig(const std::string& fromVersion, const std::string& toVersion);

    // Backup and restore
    bool backupConfig(const std::string& backupPath = "") const;
    bool restoreConfig(const std::string& backupPath);
    std::vector<std::string> listBackups() const;

    // Configuration security
    nlohmann::json getRedactedConfig() const;
    bool encryptSensitiveData(ProductionConfig& config) const;

    // IP discovery utilities
    std::string detectZeroTierIP() const;
    std::string getAutoIPAddress(const std::string& preferred_interface = "zerotier") const;
    std::vector<std::string> getAvailableIPAddresses() const;
    std::string resolveBindAddress(const WebUIConfig& webui_config) const;

    // Service management utilities
    bool installService() const;
    bool uninstallService() const;
    bool reinstallService() const;
    bool isServiceInstalled() const;
    bool isServiceRunning() const;
    std::string getServiceStatus() const;

private:
    ProductionConfigManager() = default;
    ~ProductionConfigManager() = default;
    ProductionConfigManager(const ProductionConfigManager&) = delete;
    ProductionConfigManager& operator=(const ProductionConfigManager&) = delete;

    void fileWatcherLoop();
    void applyEnvironmentOverride(const std::string& key, const std::string& value);

    mutable std::mutex configMutex_;
    ProductionConfig config_;
    std::string currentConfigPath_;

    // File watching
    std::atomic<bool> watching_{false};
    std::thread watcherThread_;
    std::string watchPath_;
    std::chrono::steady_clock::time_point lastModified_;
    std::function<void(const ProductionConfig&)> changeCallback_;

    // Validation helpers
    bool validateProvider(const ProviderConfig& provider) const;
    bool validateWebUI(const WebUIConfig& webui) const;
    bool validateSecurity(const SecurityConfig& security) const;
    bool validateSystem(const SystemConfig& system) const;

    // Migration helpers
    bool migrateFromV1(const nlohmann::json& v1Config, ProductionConfig& newConfig) const;
    bool migrateFromV2(const nlohmann::json& v2Config, ProductionConfig& newConfig) const;
};

// Configuration validation utilities
namespace validation {
    bool isValidPort(int port);
    bool isValidApiKey(const std::string& apiKey);
    bool isValidEndpoint(const std::string& endpoint);
    bool isValidPath(const std::string& path);
    bool isValidLogLevel(const std::string& level);
    std::vector<std::string> validateProviderConfig(const ProviderConfig& config);
    std::vector<std::string> validateSystemConfig(const SystemConfig& config);
    std::vector<std::string> validateWebUIConfig(const WebUIConfig& config);
    std::vector<std::string> validateSecurityConfig(const SecurityConfig& config);
    std::vector<std::string> validateDaemonConfig(const DaemonConfig& config);
    nlohmann::json validateConfigWithSchema(const nlohmann::json& config);
}

// Environment variable utilities
namespace env {
    std::optional<std::string> getString(const std::string& key, const std::string& defaultValue = "");
    std::optional<int> getInt(const std::string& key, int defaultValue = 0);
    std::optional<bool> getBool(const std::string& key, bool defaultValue = false);
    std::optional<std::vector<std::string>> getStringList(const std::string& key);

    // Specific environment variables
    std::string getConfigFile();
    std::string getLogLevel();
    int getWebUIPort();
    std::vector<ProviderConfig> getProvidersFromEnv();
}

} // namespace config
} // namespace aimux

#endif // AIMUX_PRODUCTION_CONFIG_H