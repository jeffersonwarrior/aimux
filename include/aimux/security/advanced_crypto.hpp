#pragma once

#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/pbkdf2.h>

namespace aimux {
namespace security {

/**
 * @brief Advanced cryptographic utilities for secure API key management
 *
 * This class provides AES-256-GCM encryption for API keys with secure key derivation
 * using PBKDF2. It includes key rotation capabilities and hardware security module
 * (HSM) integration readiness.
 */
class AdvancedCrypto {
public:
    /**
     * @brief Encryption result structure
     */
    struct EncryptedData {
        std::vector<uint8_t> ciphertext;
        std::vector<uint8_t> iv;        // Initialization vector
        std::vector<uint8_t> tag;       // Authentication tag
        std::vector<uint8_t> salt;      // Salt for key derivation
    };

    /**
     * @brief Key rotation metadata
     */
    struct KeyMetadata {
        std::string key_id;
        std::chrono::system_clock::time_point created_at;
        std::chrono::system_clock::time_point expires_at;
        bool is_active;
        int version;
    };

    /**
     * @brief Constructor - initializes crypto context and master key
     *
     * @param master_key Optional master key. If empty, generates a new one
     * @throws std::runtime_error if crypto initialization fails
     */
    explicit AdvancedCrypto(const std::string& master_key = "");

    /**
     * @brief Destructor - securely cleans up crypto context
     */
    ~AdvancedCrypto();

    // Disable copy constructor and assignment operator for security
    AdvancedCrypto(const AdvancedCrypto&) = delete;
    AdvancedCrypto& operator=(const AdvancedCrypto&) = delete;

    /**
     * @brief Encrypt API key using AES-256-GCM
     *
     * @param api_key Plain text API key to encrypt
     * @return Encrypted data structure with ciphertext, IV, tag, and salt
     * @throws std::runtime_error if encryption fails
     */
    EncryptedData encrypt_api_key(const std::string& api_key);

    /**
     * @brief Decrypt API key using AES-256-GCM
     *
     * @param encrypted_data Encrypted data structure
     * @return Decrypted plain text API key
     * @throws std::runtime_error if decryption fails or authentication fails
     */
    std::string decrypt_api_key(const EncryptedData& encrypted_data);

    /**
     * @brief Encrypt API key and serialize to base64 string
     *
     * @param api_key Plain text API key to encrypt
     * @return Base64 encoded encrypted data
     */
    std::string encrypt_api_key_to_base64(const std::string& api_key);

    /**
     * @brief Decrypt API key from base64 string
     *
     * @param base64_encrypted Base64 encoded encrypted data
     * @return Decrypted plain text API key
     */
    std::string decrypt_api_key_from_base64(const std::string& base64_encrypted);

    /**
     * @brief Rotate the master encryption key
     *
     * Generates a new master key and marks old one for gradual rotation.
     * Old keys remain valid for a grace period to allow decryption of existing data.
     *
     * @param grace_period_hours Grace period in hours before old keys are invalidated
     */
    void rotate_master_key(int grace_period_hours = 24);

    /**
     * @brief Generate a cryptographically secure random key
     *
     * @param key_length Length of key in bytes (default: 32 for 256-bit)
     * @return Random key as hex string
     */
    static std::string generate_secure_key(size_t key_length = 32);

    /**
     * @brief Derive encryption key from password using PBKDF2
     *
     * @param password Password to derive from
     * @param salt Salt for key derivation
     * @param iterations Number of PBKDF2 iterations (default: 100000)
     * @param key_length Length of derived key in bytes
     * @return Derived key
     */
    static std::vector<uint8_t> derive_key_pbkdf2(
        const std::string& password,
        const std::vector<uint8_t>& salt,
        int iterations = 100000,
        size_t key_length = 32
    );

    /**
     * @brief Generate secure random bytes
     *
     * @param length Number of bytes to generate
     * @return Vector of random bytes
     */
    static std::vector<uint8_t> generate_random_bytes(size_t length);

    /**
     * @brief Convert bytes to hex string
     *
     * @param bytes Byte vector to convert
     * @return Hex encoded string
     */
    static std::string bytes_to_hex(const std::vector<uint8_t>& bytes);

    /**
     * @brief Convert hex string to bytes
     *
     * @param hex Hex string to convert
     * @return Byte vector
     */
    static std::vector<uint8_t> hex_to_bytes(const std::string& hex);

    /**
     * @brief Base64 encode bytes
     *
     * @param bytes Byte vector to encode
     * @return Base64 encoded string
     */
    static std::string base64_encode(const std::vector<uint8_t>& bytes);

    /**
     * @brief Base64 decode to bytes
     *
     * @param base64 Base64 encoded string
     * @return Decoded byte vector
     */
    static std::vector<uint8_t> base64_decode(const std::string& base64);

    /**
     * @brief Get master key metadata
     *
     * @return Current master key information
     */
    KeyMetadata get_master_key_metadata() const;

    /**
     * @brief Check if a key (old or current) is still valid
     *
     * @param key_id Key identifier to check
     * @return True if key is valid for decryption
     */
    bool is_key_valid(const std::string& key_id) const;

private:
    struct KeyInfo {
        std::vector<uint8_t> key;
        KeyMetadata metadata;
    };

    std::vector<uint8_t> master_key_;
    std::vector<KeyInfo> key_history_;  // For key rotation support
    std::unique_ptr<EVP_CIPHER_CTX, void(*)(EVP_CIPHER_CTX*)> cipher_ctx_;

    std::string current_key_id_;
    static const int AES_KEY_SIZE = 32;  // 256 bits
    static const int IV_SIZE = 12;       // 96 bits for GCM
    static const int TAG_SIZE = 16;      // 128 bits for GCM
    static const int SALT_SIZE = 16;     // 128 bits

    /**
     * @brief Initialize OpenSSL crypto context
     */
    void initialize_crypto_context();

    /**
     * @brief Generate new master key
     *
     * @return Generated master key
     */
    std::vector<uint8_t> generate_master_key();

    /**
     * @brief Derive encryption key from master key and salt
     *
     * @param master_key Master encryption key
     * @param salt Salt for derivation
     * @return Derived encryption key
     */
    std::vector<uint8_t> derive_encryption_key(
        const std::vector<uint8_t>& master_key,
        const std::vector<uint8_t>& salt
    );

    /**
     * @brief Securely clear memory
     *
     * @param data Data to clear
     * @param length Length of data in bytes
     */
    static void secure_clear(void* data, size_t length);

    /**
     * @brief Generate unique key ID
     *
     * @return Unique identifier for key
     */
    std::string generate_key_id() const;
};

/**
 * @brief Utility class for secure string handling
 */
class SecureString {
public:
    /**
     * @brief Create secure string from regular string
     *
     * @param str Source string
     */
    explicit SecureString(const std::string& str);

    /**
     * @brief Destructor - securely clears memory
     */
    ~SecureString();

    /**
     * @brief Get string value (use with caution)
     *
     * @return String value
     */
    std::string get() const;

    /**
     * @brief Check if string is empty
     *
     * @return True if empty
     */
    bool empty() const;

    /**
     * @brief Get string length
     *
     * @return String length
     */
    size_t size() const;

private:
    std::unique_ptr<char[]> data_;
    size_t size_;
};

} // namespace security
} // namespace aimux