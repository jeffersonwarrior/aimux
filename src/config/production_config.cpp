#include "config/production_config.h"
#include "aimux/config/startup_validator.hpp"
#include "aimux/logging/logger.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <regex>
#include <cstdlib>
#include <filesystem>
#include <cstdio>
#include <iostream>
#include <cstring>

namespace aimux {
namespace config {

static ProductionConfigManager* instance __attribute__((unused)) = nullptr;

ProductionConfigManager& ProductionConfigManager::getInstance() {
    static ProductionConfigManager manager;
    return manager;
}

const ProductionConfig& ProductionConfigManager::getConfig() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return config_;
}

bool ProductionConfigManager::loadConfig(const std::string& configPath, bool createIfMissing) {
    std::lock_guard<std::mutex> lock(configMutex_);

    try {
        if (!std::filesystem::exists(configPath)) {
            if (createIfMissing) {
                AIMUX_INFO("Creating default config at: " + configPath);
                config_ = createProductionTemplate();

                // Ensure directory exists
                auto dir = std::filesystem::path(configPath).parent_path();
                if (!dir.empty()) {
                    std::filesystem::create_directories(dir);
                }

                if (!saveConfig(configPath)) {
                    return false;
                }
            } else {
                throw std::runtime_error("Config file not found: " + configPath);
            }
        } else {
            std::ifstream file(configPath);
            if (!file.is_open()) {
                throw std::runtime_error("Unable to open config file: " + configPath);
            }

            std::stringstream buffer;
            buffer << file.rdbuf();
            file.close();

            auto configJson = nlohmann::json::parse(buffer.str());
            config_ = ProductionConfig::fromJson(configJson);

            AIMUX_INFO("Loaded configuration from: " + configPath);
        }

        currentConfigPath_ = configPath;

        // Apply environment overrides
        loadEnvironmentOverrides();

        // CRITICAL: Validate configuration with abort-on-fail behavior
        try {
            AIMUX_INFO("Performing critical configuration validation");

            // Use the new startup validator with abort-on-fail policy
            aimux::config::StartupValidator::validate_or_abort(config_, configPath, config_.system.environment);

            std::cout << "✅ Critical configuration validation passed\n";

        } catch (const aimux::config::ConfigurationValidationError& e) {
            std::cerr << "CRITICAL: Configuration validation failed:\n";
            for (const auto& error : e.get_errors()) {
                std::cerr << "  - " << error << "\n";
            }
            return false;
        } catch (const std::exception& e) {
            std::cerr << "CRITICAL: Configuration validation error: " << e.what() << "\n";
            return false;
        }

        return true;

    } catch (const std::exception& e) {
        AIMUX_ERROR("Failed to load config: " + std::string(e.what()));
        return false;
    }
}

bool ProductionConfigManager::saveConfig(const std::string& configPath) {
    std::string path = configPath.empty() ? currentConfigPath_ : configPath;
    if (path.empty()) {
        throw std::runtime_error("No config path specified");
    }

    std::lock_guard<std::mutex> lock(configMutex_);

    try {
        // Ensure directory exists
        auto dir = std::filesystem::path(path).parent_path();
        if (!dir.empty()) {
            std::filesystem::create_directories(dir);
        }

        auto configJson = config_.toJson(true); // Encrypt API keys

        std::ofstream file(path);
        if (!file.is_open()) {
            throw std::runtime_error("Unable to write config file: " + path);
        }

        file << configJson.dump(4);
        file.close();

        // Set secure permissions
        std::filesystem::permissions(path,
            std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
            std::filesystem::perm_options::replace);

        AIMUX_INFO("Saved configuration to: " + path);
        return true;

    } catch (const std::exception& e) {
        AIMUX_ERROR("Failed to save config: " + std::string(e.what()));
        return false;
    }
}

bool ProductionConfigManager::reloadConfig() {
    if (currentConfigPath_.empty()) {
        return false;
    }
    return loadConfig(currentConfigPath_);
}

void ProductionConfigManager::loadEnvironmentOverrides() {
    // System overrides
    if (auto envLog = env::getString("AIMUX_LOG_LEVEL")) {
        config_.system.log_level = *envLog;
    }

    if (auto envEnv = env::getString("AIMUX_ENVIRONMENT")) {
        config_.system.environment = *envEnv;
    }

    if (auto envMaxConcurrent = env::getInt("AIMUX_MAX_CONCURRENT_REQUESTS")) {
        config_.system.max_concurrent_requests = *envMaxConcurrent;
    }

    // WebUI overrides
    if (auto envPort = env::getInt("AIMUX_WEBUI_PORT")) {
        config_.webui.port = *envPort;
    }

    if (auto envSslPort = env::getInt("AIMUX_WEBUI_SSL_PORT")) {
        config_.webui.ssl_port = *envSslPort;
    }

    if (auto envSsl = env::getBool("AIMUX_WEBUI_SSL_ENABLED")) {
        config_.webui.ssl_enabled = *envSsl;
    }

    // Security overrides
    if (auto envRequireHttps = env::getBool("AIMUX_REQUIRE_HTTPS")) {
        config_.security.require_https = *envRequireHttps;
    }

    if (auto envEncryptKeys = env::getBool("AIMUX_ENCRYPT_KEYS")) {
        config_.security.api_key_encryption = *envEncryptKeys;
    }

    // Provider overrides
    auto envProviders = env::getProvidersFromEnv();
    for (const auto& envProvider : envProviders) {
        // Find existing provider by name or add new one
        auto it = std::find_if(config_.providers.begin(), config_.providers.end(),
            [&envProvider](const ProviderConfig& p) { return p.name == envProvider.name; });

        if (it != config_.providers.end()) {
            *it = envProvider;
        } else {
            config_.providers.push_back(envProvider);
        }
    }

    AIMUX_DEBUG("Applied environment overrides");
}

std::vector<std::string> ProductionConfigManager::validateConfig() const {
    std::vector<std::string> errors;

    // Validate providers
    for (const auto& provider : config_.providers) {
        auto providerErrors = validation::validateProviderConfig(provider);
        errors.insert(errors.end(), providerErrors.begin(), providerErrors.end());
    }

    // Validate system config
    auto systemErrors = validation::validateSystemConfig(config_.system);
    errors.insert(errors.end(), systemErrors.begin(), systemErrors.end());

    // Validate WebUI config
    if (!validateWebUI(config_.webui)) {
        errors.push_back("WebUI configuration validation failed");
    }

    // Validate prettifier config
    auto prettifierErrors = validation::validatePrettifierConfig(config_.prettifier);
    errors.insert(errors.end(), prettifierErrors.begin(), prettifierErrors.end());

    // Validate security config
    if (!validateSecurity(config_.security)) {
        errors.push_back("Security configuration validation failed");
    }

    return errors;
}

bool ProductionConfigManager::isConfigValid() const {
    return validateConfig().empty();
}

ProductionConfig ProductionConfigManager::createProductionTemplate() const {
    ProductionConfig config;

    // Default providers (disabled, user must configure)
    config.providers = {
        {
            .name = "minimax",
            .api_key = "",
            .endpoint = "https://api.minimax.io/anthropic",
            .group_id = "",
            .models = {"minimax-m2-100k"},
            .enabled = false,
            .max_requests_per_minute = 60,
            .priority = 1,
            .retry_attempts = 3,
            .timeout_ms = 30000,
            .custom_settings = {}
        },
        {
            .name = "zai",
            .api_key = "",
            .endpoint = "https://api.z.ai",
            .group_id = "",
            .models = {"zai-claude-3"},
            .enabled = false,
            .max_requests_per_minute = 200,
            .priority = 2,
            .retry_attempts = 3,
            .timeout_ms = 25000,
            .custom_settings = {}
        },
        {
            .name = "cerebras",
            .api_key = "",
            .endpoint = "https://api.cerebras.ai",
            .group_id = "",
            .models = {"cerebras-claude-august-3-7b"},
            .enabled = false,
            .max_requests_per_minute = 300,
            .priority = 3,
            .retry_attempts = 3,
            .timeout_ms = 30000,
            .custom_settings = {}
        },
        {
            .name = "synthetic",
            .api_key = "",
            .endpoint = "https://synthetic.test",
            .group_id = "",
            .models = {"synthetic-1"},
            .enabled = false,
            .max_requests_per_minute = 100,
            .priority = 4,
            .retry_attempts = 1,
            .timeout_ms = 5000,
            .custom_settings = {}
        }
    };

    // WebUI config
    config.webui = {
        .enabled = true,
        .port = 8080,
        .ssl_port = 8443,
        .ssl_enabled = false,
        .cert_file = "",
        .key_file = "",
        .cors_enabled = true,
        .cors_origins = {"localhost", "127.0.0.1", "::1"},
        .api_docs = true,
        .real_time_metrics = true
    };

    // Daemon config
    config.daemon = {
        .enabled = true,
        .user = "aimux",
        .group = "aimux",
        .working_directory = "/var/lib/aimux",
        .log_file = "/var/log/aimux/aimux.log",
        .pid_file = "/var/run/aimux.pid",
        .auto_restart = true
    };

    // Security config
    config.security = {
        .api_key_encryption = true,
        .audit_logging = true,
        .input_validation = true,
        .rate_limiting = true,
        .ssl_verification = true,
        .require_https = true,
        .allowed_origins = {"localhost", "127.0.0.1", "::1"}
    };

    // System config
    config.system = {
        .environment = "production",
        .log_level = "warn",
        .structured_logging = true,
        .max_concurrent_requests = 1000,
        .log_dir = "/var/log/aimux",
        .backup_dir = "/var/backups/aimux",
        .backup_retention_days = 30
    };

    // Prettifier configuration
    config.prettifier = {
        .enabled = true,
        .default_prettifier = "toon",
        .plugin_directory = "./plugins",
        .auto_discovery = true,
        .cache_ttl_minutes = 60,
        .max_cache_size = 1000,
        .performance_monitoring = true,
        .provider_mappings = {
            {"cerebras", "toon"},
            {"openai", "toon"},
            {"anthropic", "toon"},
            {"synthetic", "toon"}
        },
        .toon_config = {
            .include_metadata = true,
            .include_tools = true,
            .include_thinking = true,
            .preserve_timestamps = true,
            .enable_compression = false,
            .max_content_length = 1000000,
            .indent = "    "
        }
    };

    return config;
}

bool ProductionConfigManager::backupConfig(const std::string& backupPath) const {
    std::string path = backupPath;
    if (path.empty()) {
        auto timestamp = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(timestamp);
        std::ostringstream oss;
        oss << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");
        path = config_.system.backup_dir + "/config_" + oss.str() + ".json";
    }

    return const_cast<ProductionConfigManager*>(this)->saveConfig(path);
}

nlohmann::json ProductionConfigManager::getRedactedConfig() const {
    std::lock_guard<std::mutex> lock(configMutex_);

    auto json = config_.toJson(false); // Don't encrypt, just redact

    // Redact sensitive fields
    if (json.contains("providers")) {
        for (auto& provider : json["providers"]) {
            if (provider.contains("api_key") && !provider["api_key"].is_null()) {
                std::string key = provider["api_key"];
                if (key.length() > 8) {
                    provider["api_key"] = key.substr(0, 4) + "***REDACTED***" + key.substr(key.length() - 4);
                } else {
                    provider["api_key"] = "***REDACTED***";
                }
            }
        }
    }

    return json;
}

bool ProductionConfigManager::validateWebUI(const WebUIConfig& webui) const {
    if (!validation::isValidPort(webui.port)) {
        return false;
    }
    if (!validation::isValidPort(webui.ssl_port) || webui.ssl_port == webui.port) {
        return false;
    }
    if (webui.ssl_enabled && webui.cert_file.empty()) {
        return false;
    }
    return true;
}

bool ProductionConfigManager::validateSecurity(const SecurityConfig& security) const {
    if (security.require_https && config_.webui.enabled && !config_.webui.ssl_enabled) {
        return false;
    }
    return true;
}

bool ProductionConfigManager::validateSystem(const SystemConfig& system) const {
    if (!validation::isValidLogLevel(system.log_level)) {
        return false;
    }
    if (system.max_concurrent_requests <= 0 || system.max_concurrent_requests > 10000) {
        return false;
    }
    return true;
}

// IP Discovery utilities
std::string ProductionConfigManager::detectZeroTierIP() const {
    std::string command = "ip addr list | grep " + config_.webui.zerotier_interface_prefix + " | grep 'inet ' | awk '{print $2}' | cut -d/ -f1 | head -1";

    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        return "";
    }

    char buffer[128];
    std::string result = "";

    if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result = buffer;
        result.erase(std::remove_if(result.begin(), result.end(), ::isspace), result.end());
    }

    pclose(pipe);

    return result;
}

std::string ProductionConfigManager::getAutoIPAddress(const std::string& preferred_interface) const {
    // Try ZeroTier first if preferred
    if (preferred_interface == "zerotier") {
        std::string zerotier_ip = detectZeroTierIP();
        if (!zerotier_ip.empty()) {
            return zerotier_ip;
        }
    }

    // Try to get the first non-localhost IP
    std::string command = "ip route get 1.1.1.1 | awk '{print $7}' | head -1";

    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        return "127.0.0.1";
    }

    char buffer[128];
    std::string result = "127.0.0.1";

    if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result = buffer;
        result.erase(std::remove_if(result.begin(), result.end(), ::isspace), result.end());

        // Validate that it's a valid IP and not localhost
        if (result.empty() || result == "127.0.0.1" or result == "::1") {
            result = "127.0.0.1";
        }
    }

    pclose(pipe);

    return result;
}

std::vector<std::string> ProductionConfigManager::getAvailableIPAddresses() const {
    std::vector<std::string> ips;

    // Get all IP addresses
    FILE* pipe = popen("ip addr show | grep 'inet ' | awk '{print $2}' | cut -d/ -f1", "r");
    if (!pipe) {
        return ips;
    }

    char buffer[128];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        std::string ip = buffer;
        ip.erase(std::remove_if(ip.begin(), ip.end(), ::isspace), ip.end());

        // Filter out localhost and invalid IPs
        if (!ip.empty() && ip != "127.0.0.1" && ip != "::1") {
            ips.push_back(ip);
        }
    }

    pclose(pipe);

    return ips;
}

std::string ProductionConfigManager::resolveBindAddress(const WebUIConfig& webui_config) const {
    if (webui_config.bind_address == "auto") {
        if (webui_config.auto_ip_discovery) {
            std::string detected_ip = getAutoIPAddress(webui_config.preferred_interface);
            return detected_ip;
        }
        return "127.0.0.1";
    } else if (webui_config.bind_address == "0.0.0.0") {
        return "0.0.0.0";
    } else if (webui_config.bind_address == "zerotier") {
        std::string zerotier_ip = detectZeroTierIP();
        return zerotier_ip.empty() ? "127.0.0.1" : zerotier_ip;
    } else {
        // Assume it's a specific IP address, validate it
        return webui_config.bind_address;
    }
}

// Service Management utilities
bool ProductionConfigManager::installService() const {
    // Check if service is already installed
    if (isServiceInstalled()) {
        return true;
    }

    // Determine platform and install accordingly
    std::string command;

    // Check for systemd (Linux)
    if (std::system("which systemctl > /dev/null 2>&1") == 0) {
        command = "sudo bash -c 'cat > /etc/systemd/system/aimux.service << EOF\n"
                  "[Unit]\n"
                  "Description=Aimux AI Service Router\n"
                  "After=network.target\n\n"
                  "[Service]\n"
                  "Type=simple\n"
                  "User=" + config_.daemon.user + "\n"
                  "Group=" + config_.daemon.group + "\n"
                  "WorkingDirectory=" + config_.daemon.working_directory + "\n"
                  "ExecStart=/usr/local/bin/aimux --daemon\n"
                  "Restart=always\n"
                  "RestartSec=10\n\n"
                  "[Install]\n"
                  "WantedBy=multi-user.target\n"
                  "EOF'";

        if (std::system(command.c_str()) != 0) {
            return false;
        }

        // Enable and start the service
        if (std::system("sudo systemctl daemon-reload") != 0) {
            return false;
        }

        if (std::system("sudo systemctl enable aimux") != 0) {
            return false;
        }

        return true;
    }

    // Check for launchd (macOS)
    if (std::system("which launchctl > /dev/null 2>&1") == 0) {
        std::string home_dir = std::getenv("HOME") ? std::getenv("HOME") : "/tmp";
        std::string launchd_dir = home_dir + "/Library/LaunchAgents";

        command = "mkdir -p " + launchd_dir + " && bash -c 'cat > " + launchd_dir + "/com.aimux.daemon.plist << EOF\n"
                  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                  "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
                  "<plist version=\"1.0\">\n"
                  "<dict>\n"
                  "    <key>Label</key>\n"
                  "    <string>com.aimux.daemon</string>\n"
                  "    <key>ProgramArguments</key>\n"
                  "    <array>\n"
                  "        <string>/usr/local/bin/aimux</string>\n"
                  "        <string>--daemon</string>\n"
                  "    </array>\n"
                  "    <key>RunAtLoad</key>\n"
                  "    <true/>\n"
                  "    <key>KeepAlive</key>\n"
                  "    <true/>\n"
                  "    <key>WorkingDirectory</key>\n"
                  "    <string>" + config_.daemon.working_directory + "</string>\n"
                  "</dict>\n"
                  "</plist>\n"
                  "EOF'";

        if (std::system(command.c_str()) != 0) {
            return false;
        }

        // Load the service
        command = "launchctl load " + launchd_dir + "/com.aimux.daemon.plist";
        return std::system(command.c_str()) == 0;
    }

    return false; // Unsupported platform
}

bool ProductionConfigManager::uninstallService() const {

    // Check for systemd (Linux)
    if (std::system("which systemctl > /dev/null 2>&1") == 0) {
        // Stop and disable the service
        std::system("sudo systemctl stop aimux 2>/dev/null");
        std::system("sudo systemctl disable aimux 2>/dev/null");

        // Remove the service file
        std::system("sudo rm -f /etc/systemd/system/aimux.service");
        std::system("sudo systemctl daemon-reload");

        return true;
    }

    // Check for launchd (macOS)
    if (std::system("which launchctl > /dev/null 2>&1") == 0) {
        std::string home_dir = std::getenv("HOME") ? std::getenv("HOME") : "/tmp";
        std::string launchd_dir = home_dir + "/Library/LaunchAgents";
        std::string plist_file = launchd_dir + "/com.aimux.daemon.plist";

        // Unload and remove the service
        std::system(("launchctl unload " + plist_file + " 2>/dev/null").c_str());
        std::system(("rm -f " + plist_file).c_str());

        return true;
    }

    return false; // Unsupported platform
}

bool ProductionConfigManager::reinstallService() const {
    return uninstallService() && installService();
}

bool ProductionConfigManager::isServiceInstalled() const {
    // Check for systemd (Linux)
    if (std::system("which systemctl > /dev/null 2>&1") == 0) {
        return std::system("systemctl is-enabled aimux >/dev/null 2>&1") == 0;
    }

    // Check for launchd (macOS)
    if (std::system("which launchctl > /dev/null 2>&1") == 0) {
        std::string home_dir = std::getenv("HOME") ? std::getenv("HOME") : "/tmp";
        std::string plist_file = home_dir + "/Library/LaunchAgents/com.aimux.daemon.plist";
        return std::system(("test -f " + plist_file).c_str()) == 0;
    }

    return false;
}

bool ProductionConfigManager::isServiceRunning() const {
    // Check for systemd (Linux)
    if (std::system("which systemctl > /dev/null 2>&1") == 0) {
        return std::system("systemctl is-active aimux >/dev/null 2>&1") == 0;
    }

    // Check for launchd (macOS)
    if (std::system("which launchctl > /dev/null 2>&1") == 0) {
        return std::system("launchctl list | grep com.aimux.daemon >/dev/null 2>&1") == 0;
    }

    // Check process table as fallback
    return std::system("pgrep -f 'aimux --daemon' >/dev/null 2>&1") == 0;
}

std::string ProductionConfigManager::getServiceStatus() const {
    if (!isServiceInstalled()) {
        return "Not installed";
    }

    if (isServiceRunning()) {
        std::string command;

        // Get more detailed status if available
        if (std::system("which systemctl > /dev/null 2>&1") == 0) {
            FILE* pipe = popen("systemctl status aimux --no-pager -l 2>/dev/null | grep Active | awk '{print $2}'", "r");
            if (pipe) {
                char buffer[64];
                if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                    std::string result = buffer;
                    result.erase(std::remove_if(result.begin(), result.end(), ::isspace), result.end());
                    pclose(pipe);
                    return result.empty() ? "Running" : result;
                }
                pclose(pipe);
            }
        }

        return "Running";
    }

    return "Installed but not running";
}

// Validation namespace implementations
namespace validation {

bool isValidPort(int port) {
    return port > 0 && port < 65536;
}

bool isValidApiKey(const std::string& apiKey) {
    if (apiKey.empty()) return false; // Empty is invalid, must be configured
    return apiKey.length() >= 16 && apiKey.length() <= 256;
}

bool isValidEndpoint(const std::string& endpoint) {
    // Enhanced URL validation supporting both HTTP and HTTPS
    static const std::vector<std::regex> urlPatterns = {
        // HTTPS URLs with optional port and path
        std::regex(R"(^https:\/\/[a-zA-Z0-9]([a-zA-Z0-9\-]{0,61}[a-zA-Z0-9])?(\.[a-zA-Z0-9]([a-zA-Z0-9\-]{0,61}[a-zA-Z0-9])?)*(\:[0-9]{1,5})?(\/.*)?$)"),
        // HTTP URLs with optional port and path (insecure but allowed for development)
        std::regex(R"(^http:\/\/[a-zA-Z0-9]([a-zA-Z0-9\-]{0,61}[a-zA-Z0-9])?(\.[a-zA-Z0-9]([a-zA-Z0-9\-]{0,61}[a-zA-Z0-9])?)*(\:[0-9]{1,5})?(\/.*)?$)"),
        // Localhost variations
        std::regex(R"(^https?:\/\/localhost(\:[0-9]{1,5})?(\/.*)?$)"),
        std::regex(R"(^https?:\/\/127\.0\.0\.1(\:[0-9]{1,5})?(\/.*)?$)"),
        // IPv4 addresses
        std::regex(R"(^https?:\/\/(?:[0-9]{1,3}\.){3}[0-9]{1,3}(\:[0-9]{1,5})?(\/.*)?$)")
    };

    if (endpoint.empty() || endpoint.length() > 2048) {
        return false;
    }

    return std::any_of(urlPatterns.begin(), urlPatterns.end(),
        [&endpoint](const std::regex& pattern) {
            return std::regex_match(endpoint, pattern);
        });
}

bool isValidPath(const std::string& path) {
    if (path.empty() || path.length() > 4096) {
        return false;
    }
    if (path.find("..") != std::string::npos) {
        return false;
    }
    return true;
}

bool isValidLogLevel(const std::string& level) {
    return level == "trace" || level == "debug" || level == "info" ||
           level == "warn" || level == "error" || level == "fatal";
}

std::vector<std::string> validateProviderConfig(const ProviderConfig& config) {
    std::vector<std::string> errors;

    // Name validation
    if (config.name.empty()) {
        errors.push_back("Provider name is required");
    } else if (config.name.length() > 100) {
        errors.push_back("Provider name too long (max 100 characters): " + config.name);
    } else if (!std::all_of(config.name.begin(), config.name.end(),
               [](char c) { return std::isalnum(c) || c == '-' || c == '_'; })) {
        errors.push_back("Provider name contains invalid characters: " + config.name);
    }

    // API key validation
    if (!config.api_key.empty() && !isValidApiKey(config.api_key)) {
        errors.push_back("Invalid API key format for provider: " + config.name +
                        " (must be 16-256 characters, alphanumeric with allowed symbols)");
    }

    // Endpoint validation with enhanced checking
    if (!isValidEndpoint(config.endpoint)) {
        errors.push_back("Invalid endpoint URL for provider: " + config.name +
                        " (must be valid HTTP/HTTPS URL)");
    }

    // Group ID validation
    if (config.group_id.has_value()) {
        const std::string& group_id = config.group_id.value();
        if (group_id.length() > 50) {
            errors.push_back("Group ID too long (max 50 characters) for provider: " + config.name);
        }
    }

    // Models validation
    if (config.models.empty()) {
        errors.push_back("At least one model is required for provider: " + config.name);
    } else {
        for (const auto& model : config.models) {
            if (model.empty()) {
                errors.push_back("Empty model name found in provider: " + config.name);
            } else if (model.length() > 100) {
                errors.push_back("Model name too long (max 100 characters): " + model +
                                " in provider: " + config.name);
            }
        }
    }

    // Comprehensive range validation for numeric parameters
    if (config.max_requests_per_minute < 1 || config.max_requests_per_minute > 10000) {
        errors.push_back("Invalid rate limit (must be 1-10000/minute) for provider: " + config.name +
                        " (current: " + std::to_string(config.max_requests_per_minute) + ")");
    }

    if (config.priority < 1 || config.priority > 100) {
        errors.push_back("Invalid priority (must be 1-100) for provider: " + config.name +
                        " (current: " + std::to_string(config.priority) + ")");
    }

    if (config.retry_attempts < 0 || config.retry_attempts > 10) {
        errors.push_back("Invalid retry attempts (must be 0-10) for provider: " + config.name +
                        " (current: " + std::to_string(config.retry_attempts) + ")");
    }

    if (config.timeout_ms < 100 || config.timeout_ms > 300000) {
        errors.push_back("Invalid timeout (must be 100-300000ms) for provider: " + config.name +
                        " (current: " + std::to_string(config.timeout_ms) + ")");
    }

    // Custom settings validation
    if (!config.custom_settings.empty()) {
        for (const auto& [key, value] : config.custom_settings) {
            if (key.length() > 100) {
                errors.push_back("Custom setting key too long (max 100 chars): " + key +
                                " for provider: " + config.name);
            }
            if (value.dump().length() > 1000) {
                errors.push_back("Custom setting value too long (max 1000 chars): " + key +
                                " for provider: " + config.name);
            }
        }
    }

    return errors;
}

std::vector<std::string> validateSystemConfig(const SystemConfig& config) {
    std::vector<std::string> errors;

    // Enhanced log level validation
    if (!isValidLogLevel(config.log_level)) {
        errors.push_back("Invalid log level: " + config.log_level +
                        " (must be: trace, debug, info, warn, error, fatal)");
    }

    // Range validation with detailed messages
    if (config.max_concurrent_requests < 1 || config.max_concurrent_requests > 10000) {
        errors.push_back("Invalid max_concurrent_requests (must be 1-10000): " +
                        std::to_string(config.max_concurrent_requests));
    }

    // These fields don't exist in SystemConfig - commenting out until added
    /*
    if (config.request_timeout_ms < 100 || config.request_timeout_ms > 300000) {
        errors.push_back("Invalid request_timeout_ms (must be 100-300000ms): " +
                        std::to_string(config.request_timeout_ms));
    }

    if (config.health_check_interval_ms < 1000 || config.health_check_interval_ms > 300000) {
        errors.push_back("Invalid health_check_interval_ms (must be 1000-300000ms): " +
                        std::to_string(config.health_check_interval_ms));
    }
    */

    // Path validation with enhanced checking
    if (!isValidPath(config.log_dir)) {
        errors.push_back("Invalid log directory path: " + config.log_dir +
                        " (must not contain '..' and be < 4096 chars)");
    }

    if (!isValidPath(config.backup_dir)) {
        errors.push_back("Invalid backup directory path: " + config.backup_dir +
                        " (must not contain '..' and be < 4096 chars)");
    }

    // Config file format validation - field doesn't exist in SystemConfig
    /*
    if (config.config_format != "json" && config.config_format != "yaml" && config.config_format != "toml") {
        errors.push_back("Invalid config_format (must be json, yaml, or toml): " + config.config_format);
    }
    */

    return errors;
}

// Enhanced validation functions for all configuration types
std::vector<std::string> validateWebUIConfig(const WebUIConfig& config) {
    std::vector<std::string> errors;

    // Port validation with security considerations
    if (!isValidPort(config.port)) {
        errors.push_back("Invalid WebUI port (must be 1-65535): " + std::to_string(config.port));
    } else if (config.port < 1024 && config.port != 80) {
        errors.push_back("WebUI port " + std::to_string(config.port) +
                        " requires root privileges - consider using ports >1024");
    }

    if (!isValidPort(config.ssl_port)) {
        errors.push_back("Invalid WebUI SSL port (must be 1-65535): " + std::to_string(config.ssl_port));
    }

    if (config.port == config.ssl_port) {
        errors.push_back("WebUI HTTP and SSL ports cannot be the same: " + std::to_string(config.port));
    }

    // SSL certificate validation
    if (config.ssl_enabled) {
        if (config.cert_file.empty()) {
            errors.push_back("SSL certificate file required when SSL is enabled");
        } else if (!isValidPath(config.cert_file)) {
            errors.push_back("Invalid SSL certificate file path: " + config.cert_file);
        }

        if (config.key_file.empty()) {
            errors.push_back("SSL private key file required when SSL is enabled");
        } else if (!isValidPath(config.key_file)) {
            errors.push_back("Invalid SSL key file path: " + config.key_file);
        }
    }

    // Bind address validation
    if (config.bind_address != "auto" && config.bind_address != "0.0.0.0") {
        // Check if it's a valid IP address or hostname
        static const std::regex ipRegex(R"(^(\d{1,3}\.){3}\d{1,3}$)");
        static const std::regex hostnameRegex(R"(^[a-zA-Z0-9]([a-zA-Z0-9\-]{0,61}[a-zA-Z0-9])?(\.[a-zA-Z0-9]([a-zA-Z0-9\-]{0,61}[a-zA-Z0-9])?)*$)");

        if (!std::regex_match(config.bind_address, ipRegex) &&
            !std::regex_match(config.bind_address, hostnameRegex)) {
            errors.push_back("Invalid bind address: " + config.bind_address +
                            " (must be 'auto', '0.0.0.0', valid IP, or hostname)");
        }
    }

    // Interface validation
    if (config.preferred_interface.length() > 50) {
        errors.push_back("Preferred interface name too long (max 50 chars): " + config.preferred_interface);
    }

    if (config.zerotier_interface_prefix.length() > 10) {
        errors.push_back("ZeroTier interface prefix too long (max 10 chars): " + config.zerotier_interface_prefix);
    }

    // CORS origins validation
    for (const auto& origin : config.cors_origins) {
        if (origin.empty() || origin.length() > 200) {
            errors.push_back("Invalid CORS origin (must be 1-200 characters): " + origin);
        }
    }

    // Metrics configuration validation
    if (config.metrics_update_interval_ms < 100 || config.metrics_update_interval_ms > 60000) {
        errors.push_back("Invalid metrics_update_interval_ms (must be 100-60000): " +
                        std::to_string(config.metrics_update_interval_ms));
    }

    if (config.websocket_broadcast_interval_ms < 500 || config.websocket_broadcast_interval_ms > 30000) {
        errors.push_back("Invalid websocket_broadcast_interval_ms (must be 500-30000): " +
                        std::to_string(config.websocket_broadcast_interval_ms));
    }

    if (config.max_websocket_connections < 1 || config.max_websocket_connections > 1000) {
        errors.push_back("Invalid max_websocket_connections (must be 1-1000): " +
                        std::to_string(config.max_websocket_connections));
    }

    if (config.history_retention_minutes < 1 || config.history_retention_minutes > 1440) {
        errors.push_back("Invalid history_retention_minutes (must be 1-1440): " +
                        std::to_string(config.history_retention_minutes));
    }

    return errors;
}

std::vector<std::string> validateSecurityConfig(const SecurityConfig& config) {
    std::vector<std::string> errors;

    // JWT and TLS settings validation - fields don't exist in SecurityConfig
    /*
    if (config.jwt_enabled) {
        if (config.jwt_secret.empty()) {
            errors.push_back("JWT secret required when JWT is enabled");
        } else if (config.jwt_secret.length() < 32) {
            errors.push_back("JWT secret too short (min 32 characters)");
        } else if (config.jwt_secret.length() > 512) {
            errors.push_back("JWT secret too long (max 512 characters)");
        }

        if (config.jwt_expiry_minutes < 5 || config.jwt_expiry_minutes > 1440) {
            errors.push_back("Invalid jwt_expiry_minutes (must be 5-1440): " +
                            std::to_string(config.jwt_expiry_minutes));
        }
    }

    if (config.tls_enabled) {
        if (config.cert_file.empty()) {
            errors.push_back("SSL certificate file required when TLS is enabled");
        } else if (!isValidPath(config.cert_file)) {
            errors.push_back("Invalid SSL certificate file path: " + config.cert_file);
        }

        if (config.key_file.empty()) {
            errors.push_back("SSL private key file required when TLS is enabled");
        } else if (!isValidPath(config.key_file)) {
            errors.push_back("Invalid SSL key file path: " + config.key_file);
        }
    }
    */

    // Rate limiting validation
    if (config.rate_limiting.enabled) {
        if (config.rate_limiting.requests_per_minute < 1 ||
            config.rate_limiting.requests_per_minute > 100000) {
            errors.push_back("Invalid rate_limiting.requests_per_minute (must be 1-100000): " +
                            std::to_string(config.rate_limiting.requests_per_minute));
        }

        if (config.rate_limiting.ban_duration_minutes < 1 ||
            config.rate_limiting.ban_duration_minutes > 1440) {
            errors.push_back("Invalid rate_limiting.ban_duration_minutes (must be 1-1440): " +
                            std::to_string(config.rate_limiting.ban_duration_minutes));
        }
    }

    return errors;
}

std::vector<std::string> validateDaemonConfig(const DaemonConfig& config) {
    std::vector<std::string> errors;

    // User/group validation
    if (config.user.empty() || config.user.length() > 32) {
        errors.push_back("Invalid daemon user (must be 1-32 characters): " + config.user);
    }

    if (config.group.empty() || config.group.length() > 32) {
        errors.push_back("Invalid daemon group (must be 1-32 characters): " + config.group);
    }

    // Path validation with proper directory checking
    if (!isValidPath(config.working_directory)) {
        errors.push_back("Invalid working directory path: " + config.working_directory);
    }

    if (!isValidPath(config.log_file)) {
        errors.push_back("Invalid log file path: " + config.log_file);
    }

    if (!isValidPath(config.pid_file)) {
        errors.push_back("Invalid PID file path: " + config.pid_file);
    }

    return errors;
}

std::vector<std::string> validatePrettifierConfig(const PrettifierConfig& config) {
    std::vector<std::string> errors;

    // Default prettifier validation
    if (config.default_prettifier.empty() || config.default_prettifier.length() > 50) {
        errors.push_back("Invalid default_prettifier (must be 1-50 characters): " + config.default_prettifier);
    }

    // Plugin directory validation
    if (!isValidPath(config.plugin_directory)) {
        errors.push_back("Invalid plugin directory path: " + config.plugin_directory);
    }

    // Cache TTL validation (1-1440 minutes)
    if (config.cache_ttl_minutes < 1 || config.cache_ttl_minutes > 1440) {
        errors.push_back("Invalid cache_ttl_minutes (must be 1-1440): " +
                        std::to_string(config.cache_ttl_minutes));
    }

    // Max cache size validation (10-10000 entries)
    if (config.max_cache_size < 10 || config.max_cache_size > 10000) {
        errors.push_back("Invalid max_cache_size (must be 10-10000): " +
                        std::to_string(config.max_cache_size));
    }

    // TOON config validation
    if (config.toon_config.max_content_length > 10000000) {  // 10MB max
        errors.push_back("TOON max_content_length too large (max 10MB): " +
                        std::to_string(config.toon_config.max_content_length));
    }

    // Provider mappings validation
    for (const auto& [provider, prettifier] : config.provider_mappings) {
        if (provider.empty() || provider.length() > 50) {
            errors.push_back("Invalid provider name in provider_mappings: " + provider);
        }
        if (prettifier.empty() || prettifier.length() > 50) {
            errors.push_back("Invalid prettifier name in provider_mappings: " + prettifier);
        }
    }

    return errors;
}

// JSON Schema validation for the entire configuration
nlohmann::json validateConfigWithSchema(const nlohmann::json& config) {
    std::vector<std::string> errors;
    nlohmann::json result;
    result["valid"] = true;
    result["errors"] = nlohmann::json::array();
    result["warnings"] = nlohmann::json::array();

    // Basic structure validation
    if (!config.is_object()) {
        result["valid"] = false;
        result["errors"].push_back("Configuration must be a JSON object");
        return result;
    }

    // Required sections validation
    const std::vector<std::string> required_sections = {
        "providers", "system", "webui", "security", "daemon", "prettifier"
    };

    for (const auto& section : required_sections) {
        if (!config.contains(section)) {
            result["valid"] = false;
            result["errors"].push_back("Missing required configuration section: " + section);
        } else if (!config[section].is_object()) {
            result["valid"] = false;
            result["errors"].push_back("Section '" + section + "' must be a JSON object");
        }
    }

    // Provider section validation
    if (config.contains("providers") && config["providers"].is_array()) {
        if (config["providers"].empty()) {
            result["valid"] = false;
            result["errors"].push_back("At least one provider must be configured");
        }

        for (const auto& provider : config["providers"]) {
            if (!provider.is_object()) {
                result["valid"] = false;
                result["errors"].push_back("Each provider must be a JSON object");
                continue;
            }

            // Validate required provider fields
            if (!provider.contains("name") || !provider["name"].is_string()) {
                result["valid"] = false;
                result["errors"].push_back("Provider missing required 'name' field (string)");
            }

            if (!provider.contains("endpoint") || !provider["endpoint"].is_string()) {
                result["valid"] = false;
                result["errors"].push_back("Provider missing required 'endpoint' field (string)");
            }

            if (!provider.contains("models") || !provider["models"].is_array()) {
                result["valid"] = false;
                result["errors"].push_back("Provider missing required 'models' field (array)");
            }

            // Type validation for provider fields
            if (provider.contains("enabled") && !provider["enabled"].is_boolean()) {
                result["valid"] = false;
                result["errors"].push_back("Provider 'enabled' field must be boolean");
            }

            if (provider.contains("max_requests_per_minute") && !provider["max_requests_per_minute"].is_number_integer()) {
                result["valid"] = false;
                result["errors"].push_back("Provider 'max_requests_per_minute' field must be integer");
            }

            if (provider.contains("priority") && !provider["priority"].is_number_integer()) {
                result["valid"] = false;
                result["errors"].push_back("Provider 'priority' field must be integer");
            }
        }
    }

    // System section validation
    if (config.contains("system") && config["system"].is_object()) {
        const auto& system = config["system"];

        if (system.contains("log_level") && !system["log_level"].is_string()) {
            result["valid"] = false;
            result["errors"].push_back("System 'log_level' field must be string");
        }

        if (system.contains("max_concurrent_requests") && !system["max_concurrent_requests"].is_number_integer()) {
            result["valid"] = false;
            result["errors"].push_back("System 'max_concurrent_requests' field must be integer");
        }

        if (system.contains("request_timeout_ms") && !system["request_timeout_ms"].is_number_integer()) {
            result["valid"] = false;
            result["errors"].push_back("System 'request_timeout_ms' field must be integer");
        }
    }

    // WebUI section validation
    if (config.contains("webui") && config["webui"].is_object()) {
        const auto& webui = config["webui"];

        if (webui.contains("port") && !webui["port"].is_number_integer()) {
            result["valid"] = false;
            result["errors"].push_back("WebUI 'port' field must be integer");
        }

        if (webui.contains("ssl_port") && !webui["ssl_port"].is_number_integer()) {
            result["valid"] = false;
            result["errors"].push_back("WebUI 'ssl_port' field must be integer");
        }

        if (webui.contains("enabled") && !webui["enabled"].is_boolean()) {
            result["valid"] = false;
            result["errors"].push_back("WebUI 'enabled' field must be boolean");
        }

        if (webui.contains("ssl_enabled") && !webui["ssl_enabled"].is_boolean()) {
            result["valid"] = false;
            result["errors"].push_back("WebUI 'ssl_enabled' field must be boolean");
        }

        if (webui.contains("cors_origins") && !webui["cors_origins"].is_array()) {
            result["valid"] = false;
            result["errors"].push_back("WebUI 'cors_origins' field must be array");
        }
    }

    // Prettifier section validation
    if (config.contains("prettifier") && config["prettifier"].is_object()) {
        const auto& prettifier = config["prettifier"];

        if (prettifier.contains("enabled") && !prettifier["enabled"].is_boolean()) {
            result["valid"] = false;
            result["errors"].push_back("Prettifier 'enabled' field must be boolean");
        }

        if (prettifier.contains("default_prettifier") && !prettifier["default_prettifier"].is_string()) {
            result["valid"] = false;
            result["errors"].push_back("Prettifier 'default_prettifier' field must be string");
        }

        if (prettifier.contains("plugin_directory") && !prettifier["plugin_directory"].is_string()) {
            result["valid"] = false;
            result["errors"].push_back("Prettifier 'plugin_directory' field must be string");
        }

        if (prettifier.contains("auto_discovery") && !prettifier["auto_discovery"].is_boolean()) {
            result["valid"] = false;
            result["errors"].push_back("Prettifier 'auto_discovery' field must be boolean");
        }

        if (prettifier.contains("cache_ttl_minutes") && !prettifier["cache_ttl_minutes"].is_number_integer()) {
            result["valid"] = false;
            result["errors"].push_back("Prettifier 'cache_ttl_minutes' field must be integer");
        }

        if (prettifier.contains("max_cache_size") && !prettifier["max_cache_size"].is_number_integer()) {
            result["valid"] = false;
            result["errors"].push_back("Prettifier 'max_cache_size' field must be integer");
        }

        if (prettifier.contains("performance_monitoring") && !prettifier["performance_monitoring"].is_boolean()) {
            result["valid"] = false;
            result["errors"].push_back("Prettifier 'performance_monitoring' field must be boolean");
        }

        if (prettifier.contains("provider_mappings") && !prettifier["provider_mappings"].is_object()) {
            result["valid"] = false;
            result["errors"].push_back("Prettifier 'provider_mappings' field must be object");
        }

        // TOON config validation
        if (prettifier.contains("toon_config") && !prettifier["toon_config"].is_object()) {
            result["valid"] = false;
            result["errors"].push_back("Prettifier 'toon_config' field must be object");
        }
    }

    return result;
}

} // namespace validation

// Environment namespace implementations
namespace env {

std::optional<std::string> getString(const std::string& key, const std::string& defaultValue) {
    if (const char* value = std::getenv(key.c_str())) {
        return std::string(value);
    }
    return defaultValue.empty() ? std::nullopt : std::optional<std::string>(defaultValue);
}

std::optional<int> getInt(const std::string& key, int defaultValue) {
    if (const char* value = std::getenv(key.c_str())) {
        try {
            return std::stoi(value);
        } catch (const std::exception&) {
            // Invalid integer, return default
        }
    }
    return defaultValue != 0 ? std::optional<int>(defaultValue) : std::nullopt;
}

std::optional<bool> getBool(const std::string& key, bool defaultValue) {
    if (const char* value = std::getenv(key.c_str())) {
        std::string str(value);
        std::transform(str.begin(), str.end(), str.begin(), ::tolower);
        return str == "true" || str == "1" || str == "yes" || str == "on";
    }
    return defaultValue ? std::optional<bool>(defaultValue) : std::nullopt;
}

std::optional<std::vector<std::string>> getStringList(const std::string& key) {
    std::vector<std::string> result;
    if (const char* value = std::getenv(key.c_str())) {
        std::istringstream iss(value);
        std::string item;
        while (std::getline(iss, item, ',')) {
            // Trim whitespace
            item.erase(0, item.find_first_not_of(" \t"));
            item.erase(item.find_last_not_of(" \t") + 1);
            if (!item.empty()) {
                result.push_back(item);
            }
        }
    }
    return result.empty() ? std::nullopt : std::optional<std::vector<std::string>>(result);
}

std::vector<ProviderConfig> getProvidersFromEnv() {
    std::vector<ProviderConfig> providers;

    // Get provider names from environment
    if (const char* providerNames = std::getenv("AIMUX_PROVIDERS")) {
        std::istringstream iss(providerNames);
        std::string providerName;
        while (std::getline(iss, providerName, ',')) {
            // Trim whitespace
            providerName.erase(0, providerName.find_first_not_of(" \t"));
            providerName.erase(providerName.find_last_not_of(" \t") + 1);

            if (!providerName.empty()) {
                ProviderConfig config;
                config.name = providerName;

                // Get provider-specific environment variables
                std::string prefix = "AIMUX_" + providerName + "_";

                if (auto apiKey = getString(prefix + "API_KEY")) {
                    config.api_key = *apiKey;
                }
                if (auto endpoint = getString(prefix + "ENDPOINT")) {
                    config.endpoint = *endpoint;
                }
                if (auto groupId = getString(prefix + "GROUP_ID")) {
                    config.group_id = *groupId;
                }
                if (auto enabled = getBool(prefix + "ENABLED")) {
                    config.enabled = *enabled;
                }
                if (auto maxRpm = getInt(prefix + "MAX_REQUESTS_PER_MINUTE")) {
                    config.max_requests_per_minute = *maxRpm;
                }
                if (auto priority = getInt(prefix + "PRIORITY")) {
                    config.priority = *priority;
                }
                if (auto timeout = getInt(prefix + "TIMEOUT_MS")) {
                    config.timeout_ms = *timeout;
                }

                providers.push_back(config);
            }
        }
    }

    return providers;
}

} // namespace env

} // namespace config
} // namespace aimux