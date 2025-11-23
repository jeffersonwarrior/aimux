#ifndef AIMUX_SECURE_CONFIG_H
#define AIMUX_SECURE_CONFIG_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

namespace aimux {
namespace security {

/**
 * Enhanced security manager for production deployments
 * Handles API key encryption, secure config management, and security policies
 */
class SecureConfigManager {
public:
    SecureConfigManager();
    ~SecureConfigManager();

    // Initialize security subsystem
    bool initialize();

    // API key encryption/decryption
    std::string encryptApiKey(const std::string& apiKey);
    std::string decryptApiKey(const std::string& encryptedApiKey);
    bool validateApiKeyFormat(const std::string& apiKey);

    // Configuration security
    bool loadSecureConfig(const std::string& configPath);
    bool saveSecureConfig(const std::string& configPath);
    std::string getRedactedConfig() const;

    // Environment variable security
    void loadFromEnvironment();
    bool sanitizeEnvironment();

    // HTTPS/TLS configuration
    struct TLSConfig {
        bool enabled = true;
        std::string certFile;
        std::string keyFile;
        std::string caFile;
        bool verifyPeer = true;
        bool verifyHost = true;
        std::vector<std::string> ciphers;
    };

    bool loadTLSConfig(TLSConfig& config);
    bool validateTLSConfig(const TLSConfig& config);

    // Security policies
    struct SecurityPolicy {
        bool requireHttps = true;
        bool encryptApiKeys = true;
        bool auditLogging = true;
        bool rateLimiting = true;
        bool inputValidation = true;
        int maxApiKeyLength = 256;
        int maxConfigSize = 1024 * 1024; // 1MB
        std::vector<std::string> allowedOrigins;
    };

    void setSecurityPolicy(const SecurityPolicy& policy);
    SecurityPolicy getSecurityPolicy() const;
    bool validateSecurityPolicy() const;

    // Password/secret generation
    std::string generateSecureRandom(int length = 32);
    std::string hashPassword(const std::string& password, const std::string& salt = "");
    bool verifyPassword(const std::string& password, const std::string& hash, const std::string& salt = "");

    // Audit logging
    void logSecurityEvent(const std::string& event, const std::string& details = "");
    std::vector<std::string> getSecurityEvents() const;
    void clearSecurityEvents();

    // Configuration validation
    bool validateConfigSecurity(const std::string& config) const;
    std::vector<std::string> getSecurityIssues() const;

private:
    // Encryption key management
    bool loadEncryptionKey();
    bool generateEncryptionKey();
    std::string getEncryptionKey() const;

    // OpenSSL helpers
    std::string aesEncrypt(const std::string& plaintext, const std::string& key);
    std::string aesDecrypt(const std::string& ciphertext, const std::string& key);
    std::string hashSHA256(const std::string& data) const;

    // Environment validation
    bool isSecureEnvironment() const;
    void sanitizeEnvVar(const std::string& name, const std::string& pattern);

    // Internal state
    SecurityPolicy securityPolicy_;
    TLSConfig tlsConfig_;
    std::string encryptionKey_;
    std::vector<std::string> securityEvents_;
    bool initialized_;
    std::string configPath_;
};

// Singleton access
class SecurityManager {
public:
    static SecureConfigManager& getInstance();
    static void shutdown();

private:
    SecurityManager() = default;
    ~SecurityManager() = default;
    SecurityManager(const SecurityManager&) = delete;
    SecurityManager& operator=(const SecurityManager&) = delete;

    static std::unique_ptr<SecureConfigManager> instance_;
};

// Utility functions
namespace utils {
    std::string generateRandomHex(int length);
    std::string redactSensitiveData(const std::string& data);
    bool isValidApiKey(const std::string& apiKey);
    bool isValidUrl(const std::string& url);
    bool isValidFilePath(const std::string& path);
    std::string getSecureTempPath();
}

} // namespace security
} // namespace aimux

#endif // AIMUX_SECURE_CONFIG_H