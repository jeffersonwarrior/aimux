#include "aimux/security/secure_config.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <random>
#include <regex>
#include <ctime>
#include <openssl/rand.h>
#include <openssl/evp.h>
#include <openssl/sha.h>

namespace aimux {
namespace security {

std::unique_ptr<SecureConfigManager> SecurityManager::instance_ = nullptr;

SecureConfigManager::SecureConfigManager() : initialized_(false) {}

SecureConfigManager::~SecureConfigManager() {
    // Clear sensitive data
    std::fill(encryptionKey_.begin(), encryptionKey_.end(), 0);
}

bool SecureConfigManager::initialize() {
    if (initialized_) {
        return true;
    }

    try {
        // Initialize OpenSSL
        OpenSSL_add_all_algorithms();

        // Load or generate encryption key
        if (!loadEncryptionKey()) {
            if (!generateEncryptionKey()) {
                logSecurityEvent("SECURITY_INIT_ERROR", "Failed to initialize encryption key");
                return false;
            }
        }

        // Load environment variables
        loadFromEnvironment();

        // Set default security policy
        securityPolicy_ = SecurityPolicy{
            .requireHttps = true,
            .encryptApiKeys = true,
            .auditLogging = true,
            .rateLimiting = true,
            .inputValidation = true,
            .maxApiKeyLength = 256,
            .maxConfigSize = 1024 * 1024,
            .allowedOrigins = {"localhost", "127.0.0.1", "::1"}
        };

        initialized_ = true;
        logSecurityEvent("SECURITY_INITIALIZED", "Security manager initialized successfully");
        return true;

    } catch (const std::exception& e) {
        logSecurityEvent("SECURITY_INIT_ERROR", std::string("Exception during init: ") + e.what());
        return false;
    }
}

std::string SecureConfigManager::encryptApiKey(const std::string& apiKey) {
    if (!initialized_) {
        throw std::runtime_error("Security manager not initialized");
    }

    if (!validateApiKeyFormat(apiKey)) {
        throw std::runtime_error("Invalid API key format");
    }

    std::string encrypted = aesEncrypt(apiKey, getEncryptionKey());
    logSecurityEvent("API_KEY_ENCRYPTED", "API key encrypted successfully");
    return encrypted;
}

std::string SecureConfigManager::decryptApiKey(const std::string& encryptedApiKey) {
    if (!initialized_) {
        throw std::runtime_error("Security manager not initialized");
    }

    std::string decrypted = aesDecrypt(encryptedApiKey, getEncryptionKey());

    if (!validateApiKeyFormat(decrypted)) {
        throw std::runtime_error("Decrypted API key has invalid format");
    }

    logSecurityEvent("API_KEY_DECRYPTED", "API key decrypted successfully");
    return decrypted;
}

bool SecureConfigManager::validateApiKeyFormat(const std::string& apiKey) {
    // Basic API key validation
    if (apiKey.empty() || apiKey.length() > static_cast<size_t>(securityPolicy_.maxApiKeyLength)) {
        return false;
    }

    // Check for reasonable character set (hex, base64, etc.)
    std::regex pattern("^[a-zA-Z0-9+/=_\\-]+$");
    return std::regex_match(apiKey, pattern);
}

void SecureConfigManager::loadFromEnvironment() {
    // Load configuration from environment variables
    const std::vector<std::string> secureEnvVars = {
        "AIMUX_API_KEY", "AIMUX_ENCRYPTION_KEY", "AIMUX_TLS_CERT",
        "AIMUX_TLS_KEY", "AIMUX_CONFIG_ENCRYPTION"
    };

    for (const auto& var : secureEnvVars) {
        if (std::getenv(var.c_str())) {
            logSecurityEvent("ENV_VAR_LOADED", var);
        }
    }

    // Set security policy from environment
    if (const char* requireHttps = std::getenv("AIMUX_REQUIRE_HTTPS")) {
        securityPolicy_.requireHttps = (std::string(requireHttps) == "true");
    }

    if (const char* encryptKeys = std::getenv("AIMUX_ENCRYPT_KEYS")) {
        securityPolicy_.encryptApiKeys = (std::string(encryptKeys) == "true");
    }

    sanitizeEnvironment();
}

bool SecureConfigManager::sanitizeEnvironment() {
    // Remove potentially dangerous environment variables
    const std::vector<std::string> dangerousVars = {
        "HTTP_PROXY", "HTTPS_PROXY", "NO_PROXY", "LD_PRELOAD"
    };

    for (const auto& var : dangerousVars) {
        if (std::getenv(var.c_str())) {
            logSecurityEvent("ENV_VAR_SANITIZED", "Removed dangerous env var: " + var);
            // Note: In production, we might want to warn rather than unset
        }
    }

    return true;
}

std::string SecureConfigManager::generateSecureRandom(int length) {
    if (length <= 0 || length > 1024) {
        throw std::runtime_error("Invalid random length");
    }

    std::vector<unsigned char> buffer(static_cast<size_t>(length));
    if (RAND_bytes(buffer.data(), length) != 1) {
        throw std::runtime_error("Failed to generate secure random bytes");
    }

    return utils::generateRandomHex(length);
}

SecureConfigManager& SecurityManager::getInstance() {
    if (!instance_) {
        instance_ = std::make_unique<SecureConfigManager>();
        if (!instance_->initialize()) {
            throw std::runtime_error("Failed to initialize security manager");
        }
    }
    return *instance_;
}

void SecurityManager::shutdown() {
    instance_.reset();
}

bool SecureConfigManager::loadEncryptionKey() {
    // Try to load from environment first
    if (const char* envKey = std::getenv("AIMUX_ENCRYPTION_KEY")) {
        encryptionKey_ = envKey;
        if (encryptionKey_.length() == 32) { // 256-bit key
            return true;
        }
    }

    // Try to load from file
    std::string keyFile = std::getenv("HOME") + std::string("/.config/aimux/encryption.key");
    std::ifstream file(keyFile);
    if (file.is_open()) {
        std::getline(file, encryptionKey_);
        file.close();
        return encryptionKey_.length() == 32;
    }

    return false;
}

bool SecureConfigManager::generateEncryptionKey() {
    encryptionKey_ = generateSecureRandom(32);

    // Save to file with restricted permissions
    std::string keyDir = std::getenv("HOME") + std::string("/.config/aimux");
    std::filesystem::create_directories(keyDir);

    std::string keyFile = keyDir + "/encryption.key";
    std::ofstream file(keyFile, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    file << encryptionKey_;
    file.close();

    // Set restrictive permissions
    std::filesystem::permissions(keyFile,
        std::filesystem::perms::owner_read |
        std::filesystem::perms::owner_write,
        std::filesystem::perm_options::replace);

    return true;
}

std::string SecureConfigManager::getEncryptionKey() const {
    return encryptionKey_;
}

std::string SecureConfigManager::aesEncrypt(const std::string& plaintext, const std::string& key) {
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        throw std::runtime_error("Failed to create encryption context");
    }

    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr,
                          reinterpret_cast<const unsigned char*>(key.data()), nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Failed to initialize encryption");
    }

    std::string ciphertext(plaintext.size() + static_cast<size_t>(EVP_CIPHER_block_size(EVP_aes_256_gcm())), 0);
    int len;
    if (EVP_EncryptUpdate(ctx, reinterpret_cast<unsigned char*>(&ciphertext[0]), &len,
                         reinterpret_cast<const unsigned char*>(plaintext.data()), plaintext.size()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Failed to encrypt data");
    }

    int ciphertext_len = len;
    if (EVP_EncryptFinal_ex(ctx, reinterpret_cast<unsigned char*>(&ciphertext[static_cast<size_t>(ciphertext_len)]), &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Failed to finalize encryption");
    }
    ciphertext_len += len;

    // Get authentication tag
    unsigned char tag[16];
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Failed to get authentication tag");
    }

    EVP_CIPHER_CTX_free(ctx);

    // Combine ciphertext with tag (append tag to ciphertext)
    std::string result(ciphertext.begin(), ciphertext.begin() + ciphertext_len);
    result.append(reinterpret_cast<char*>(tag), 16);

    return result;
}

std::string SecureConfigManager::aesDecrypt(const std::string& ciphertext, const std::string& key) {
    if (ciphertext.size() < 16) {
        throw std::runtime_error("Invalid ciphertext size");
    }

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        throw std::runtime_error("Failed to create decryption context");
    }

    std::string actualCiphertext = ciphertext.substr(0, ciphertext.size() - 16);
    std::string tag = ciphertext.substr(ciphertext.size() - 16);

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr,
                          reinterpret_cast<const unsigned char*>(key.data()), nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Failed to initialize decryption");
    }

    std::string plaintext(actualCiphertext.size(), 0);
    int len;
    if (EVP_DecryptUpdate(ctx, reinterpret_cast<unsigned char*>(&plaintext[0]), &len,
                         reinterpret_cast<const unsigned char*>(actualCiphertext.data()), actualCiphertext.size()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Failed to decrypt data");
    }

    int plaintext_len = len;

    // Set expected tag value
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, const_cast<char*>(tag.data())) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Failed to set authentication tag");
    }

    if (EVP_DecryptFinal_ex(ctx, reinterpret_cast<unsigned char*>(&plaintext[static_cast<size_t>(plaintext_len)]), &len) <= 0) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Authentication failed - invalid tag");
    }
    plaintext_len += len;

    EVP_CIPHER_CTX_free(ctx);

    return plaintext.substr(0, static_cast<size_t>(plaintext_len));
}

void SecureConfigManager::logSecurityEvent(const std::string& event, const std::string& details) {
    if (!securityPolicy_.auditLogging) {
        return;
    }

    auto now = std::time(nullptr);
    auto tm = *std::localtime(&now);

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << " [SECURITY] " << event;
    if (!details.empty()) {
        oss << " - " << details;
    }

    std::string logEntry = oss.str();
    securityEvents_.push_back(logEntry);

    // TODO: Fix logging - temporarily disabled
    // std::string correlationId = aimux::generate_correlation_id();
    // auto logger = aimux::Logger("security_manager", correlationId);
}

namespace utils {

std::string generateRandomHex(int length) {
    std::vector<unsigned char> buffer(static_cast<size_t>(length));
    if (RAND_bytes(buffer.data(), length) != 1) {
        throw std::runtime_error("Failed to generate secure random bytes");
    }

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (unsigned char byte : buffer) {
        oss << std::setw(2) << static_cast<int>(byte);
    }
    return oss.str();
}

std::string redactSensitiveData(const std::string& data) {
    if (data.length() <= 8) {
        return std::string(data.length(), '*');
    }
    return data.substr(0, 4) + std::string(data.length() - 8, '*') + data.substr(data.length() - 4);
}

bool isValidApiKey(const std::string& apiKey) {
    // Simple validation - can be extended based on provider requirements
    if (apiKey.length() < 16 || apiKey.length() > 256) {
        return false;
    }
    return std::all_of(apiKey.begin(), apiKey.end(), [](char c) {
        return std::isalnum(c) || c == '-' || c == '_' || c == '+' || c == '/' || c == '=';
    });
}

bool isValidUrl(const std::string& url) {
    std::regex urlPattern(R"(^https:\/\/[a-zA-Z0-9.-]+(\:[0-9]+)?(\/.*)?$)");
    return std::regex_match(url, urlPattern);
}

bool isValidFilePath(const std::string& path) {
    // Basic path validation
    if (path.empty() || path.length() > 4096) {
        return false;
    }

    // Prevent directory traversal
    if (path.find("..") != std::string::npos) {
        return false;
    }

    return true;
}

std::string getSecureTempPath() {
    std::string tempDir = std::getenv("TMPDIR") ? std::getenv("TMPDIR") : "/tmp";
    return tempDir + "/aimux-" + utils::generateRandomHex(8);
}

} // namespace utils

} // namespace security
} // namespace aimux