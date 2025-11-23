#include "aimux/security/advanced_crypto.hpp"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/pbkdf2.h>
#include <openssl/aes.h>
#include <openssl/base64.h>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <cstring>
#include <iomanip>

namespace aimux {
namespace security {

AdvancedCrypto::AdvancedCrypto(const std::string& master_key)
    : cipher_ctx_(nullptr, [](EVP_CIPHER_CTX* ctx) { if (ctx) EVP_CIPHER_CTX_free(ctx); }) {

    initialize_crypto_context();

    if (master_key.empty()) {
        master_key_ = generate_master_key();
    } else {
        master_key_ = hex_to_bytes(master_key);
    }

    // Initialize current key metadata
    KeyInfo current_key;
    current_key.key = master_key_;
    current_key.metadata.key_id = generate_key_id();
    current_key.metadata.created_at = std::chrono::system_clock::now();
    current_key.metadata.expires_at = std::chrono::system_clock::now() + std::chrono::hours(24 * 30); // 30 days
    current_key.metadata.is_active = true;
    current_key.metadata.version = 1;

    key_history_.push_back(current_key);
    current_key_id_ = current_key.metadata.key_id;

    std::cout << "✅ AdvancedCrypto initialized with key ID: " << current_key_id_ << std::endl;
}

AdvancedCrypto::~AdvancedCrypto() {
    // Securely clear sensitive data
    secure_clear(master_key_.data(), master_key_.size());
    for (auto& key_info : key_history_) {
        secure_clear(key_info.key.data(), key_info.key.size());
    }
}

void AdvancedCrypto::initialize_crypto_context() {
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        throw std::runtime_error("Failed to create OpenSSL cipher context");
    }
    cipher_ctx_.reset(ctx);
}

std::vector<uint8_t> AdvancedCrypto::generate_master_key() {
    return generate_random_bytes(AES_KEY_SIZE);
}

AdvancedCrypto::EncryptedData AdvancedCrypto::encrypt_api_key(const std::string& api_key) {
    if (api_key.empty()) {
        throw std::runtime_error("API key cannot be empty");
    }

    // Generate salt and IV
    auto salt = generate_random_bytes(SALT_SIZE);
    auto iv = generate_random_bytes(IV_SIZE);

    // Derive encryption key from master key and salt
    auto encryption_key = derive_encryption_key(master_key_, salt);

    // Encrypt using AES-256-GCM
    try {
        EncryptedData result;
        result.salt = salt;
        result.iv = iv;

        // Convert string to bytes
        std::vector<uint8_t> plaintext(api_key.begin(), api_key.end());
        result.ciphertext.resize(plaintext.size());
        result.tag.resize(TAG_SIZE);

        if (EVP_EncryptInit_ex(cipher_ctx_.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
            throw std::runtime_error("Failed to initialize AES-256-GCM encryption");
        }

        if (EVP_EncryptInit_ex(cipher_ctx_.get(), nullptr, nullptr, encryption_key.data(), iv.data()) != 1) {
            throw std::runtime_error("Failed to set encryption key and IV");
        }

        int len;
        if (EVP_EncryptUpdate(cipher_ctx_.get(), result.ciphertext.data(), &len,
                             plaintext.data(), plaintext.size()) != 1) {
            throw std::runtime_error("Failed to encrypt data");
        }

        int ciphertext_len = len;

        if (EVP_EncryptFinal_ex(cipher_ctx_.get(), result.ciphertext.data() + len, &len) != 1) {
            throw std::runtime_error("Failed to finalize encryption");
        }
        ciphertext_len += len;

        result.ciphertext.resize(ciphertext_len);

        // Get authentication tag
        if (EVP_CIPHER_CTX_ctrl(cipher_ctx_.get(), EVP_CTRL_GCM_GET_TAG, TAG_SIZE,
                               result.tag.data()) != 1) {
            throw std::runtime_error("Failed to get authentication tag");
        }

        // Securely clear the derived key
        secure_clear(encryption_key.data(), encryption_key.size());

        return result;

    } catch (const std::exception& e) {
        secure_clear(encryption_key.data(), encryption_key.size());
        throw std::runtime_error("Encryption failed: " + std::string(e.what()));
    }
}

std::string AdvancedCrypto::decrypt_api_key(const EncryptedData& encrypted_data) {
    if (encrypted_data.ciphertext.empty() || encrypted_data.salt.empty() ||
        encrypted_data.iv.empty() || encrypted_data.tag.empty()) {
        throw std::runtime_error("Invalid encrypted data structure");
    }

    // Try decrypting with current and all valid keys
    for (const auto& key_info : key_history_) {
        if (!key_info.metadata.is_active && key_info.metadata.expires_at < std::chrono::system_clock::now()) {
            continue; // Skip expired keys
        }

        try {
            auto encryption_key = derive_encryption_key(key_info.key, encrypted_data.salt);

            std::vector<uint8_t> plaintext(encrypted_data.ciphertext.size() + AES_BLOCK_SIZE);

            if (EVP_DecryptInit_ex(cipher_ctx_.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
                secure_clear(encryption_key.data(), encryption_key.size());
                continue;
            }

            if (EVP_DecryptInit_ex(cipher_ctx_.get(), nullptr, nullptr,
                                  encryption_key.data(), encrypted_data.iv.data()) != 1) {
                secure_clear(encryption_key.data(), encryption_key.size());
                continue;
            }

            int len;
            if (EVP_DecryptUpdate(cipher_ctx_.get(), plaintext.data(), &len,
                                 encrypted_data.ciphertext.data(), encrypted_data.ciphertext.size()) != 1) {
                secure_clear(encryption_key.data(), encryption_key.size());
                continue;
            }

            int plaintext_len = len;

            // Set the expected tag value
            if (EVP_CIPHER_CTX_ctrl(cipher_ctx_.get(), EVP_CTRL_GCM_SET_TAG, TAG_SIZE,
                                   const_cast<uint8_t*>(encrypted_data.tag.data())) != 1) {
                secure_clear(encryption_key.data(), encryption_key.size());
                continue;
            }

            if (EVP_DecryptFinal_ex(cipher_ctx_.get(), plaintext.data() + len, &len) <= 0) {
                // Authentication failed
                secure_clear(encryption_key.data(), encryption_key.size());
                continue;
            }

            plaintext_len += len;
            plaintext.resize(plaintext_len);

            secure_clear(encryption_key.data(), encryption_key.size());

            return std::string(plaintext.begin(), plaintext.end());

        } catch (const std::exception&) {
            continue; // Try next key
        }
    }

    throw std::runtime_error("Decryption failed: No valid key could decrypt the data");
}

std::string AdvancedCrypto::encrypt_api_key_to_base64(const std::string& api_key) {
    auto encrypted = encrypt_api_key(api_key);

    // Serialize encrypted data
    std::vector<uint8_t> serialized;
    serialized.insert(serialized.end(), encrypted.salt.begin(), encrypted.salt.end());
    serialized.insert(serialized.end(), encrypted.iv.begin(), encrypted.iv.end());
    serialized.insert(serialized.end(), encrypted.tag.begin(), encrypted.tag.end());
    serialized.insert(serialized.end(), encrypted.ciphertext.begin(), encrypted.ciphertext.end());

    return base64_encode(serialized);
}

std::string AdvancedCrypto::decrypt_api_key_from_base64(const std::string& base64_encrypted) {
    auto serialized = base64_decode(base64_encrypted);

    if (serialized.size() < SALT_SIZE + IV_SIZE + TAG_SIZE) {
        throw std::runtime_error("Invalid base64 encrypted data: too short");
    }

    EncryptedData encrypted;
    size_t pos = 0;

    encrypted.salt.assign(serialized.begin(), serialized.begin() + SALT_SIZE);
    pos += SALT_SIZE;

    encrypted.iv.assign(serialized.begin() + pos, serialized.begin() + pos + IV_SIZE);
    pos += IV_SIZE;

    encrypted.tag.assign(serialized.begin() + pos, serialized.begin() + pos + TAG_SIZE);
    pos += TAG_SIZE;

    encrypted.ciphertext.assign(serialized.begin() + pos, serialized.end());

    return decrypt_api_key(encrypted);
}

void AdvancedCrypto::rotate_master_key(int grace_period_hours) {
    // Mark current key as inactive but still valid for grace period
    for (auto& key_info : key_history_) {
        if (key_info.metadata.key_id == current_key_id_) {
            key_info.metadata.is_active = false;
            key_info.metadata.expires_at = std::chrono::system_clock::now() +
                                         std::chrono::hours(grace_period_hours);
            break;
        }
    }

    // Generate new master key
    std::vector<uint8_t> old_master_key = master_key_;
    master_key_ = generate_master_key();

    // Create new metadata
    KeyInfo new_key;
    new_key.key = master_key_;
    new_key.metadata.key_id = generate_key_id();
    new_key.metadata.created_at = std::chrono::system_clock::now();
    new_key.metadata.expires_at = std::chrono::system_clock::now() + std::chrono::hours(24 * 30); // 30 days
    new_key.metadata.is_active = true;
    new_key.metadata.version = key_history_.back().metadata.version + 1;

    key_history_.push_back(new_key);
    current_key_id_ = new_key.metadata.key_id;

    // Securely clear old key
    secure_clear(old_master_key.data(), old_master_key.size());

    std::cout << "🔄 Master key rotated. New key ID: " << current_key_id_
              << " (Grace period: " << grace_period_hours << " hours)" << std::endl;
}

std::string AdvancedCrypto::generate_secure_key(size_t key_length) {
    auto bytes = generate_random_bytes(key_length);
    std::string key = bytes_to_hex(bytes);
    secure_clear(bytes.data(), bytes.size());
    return key;
}

std::vector<uint8_t> AdvancedCrypto::derive_key_pbkdf2(
    const std::string& password,
    const std::vector<uint8_t>& salt,
    int iterations,
    size_t key_length
) {
    std::vector<uint8_t> derived_key(key_length);

    if (PKCS5_PBKDF2_HMAC(password.c_str(), password.length(),
                          salt.data(), salt.size(),
                          iterations, EVP_sha256(),
                          key_length, derived_key.data()) != 1) {
        throw std::runtime_error("PBKDF2 key derivation failed");
    }

    return derived_key;
}

std::vector<uint8_t> AdvancedCrypto::generate_random_bytes(size_t length) {
    std::vector<uint8_t> bytes(length);
    if (RAND_bytes(bytes.data(), length) != 1) {
        throw std::runtime_error("Failed to generate secure random bytes");
    }
    return bytes;
}

std::string AdvancedCrypto::bytes_to_hex(const std::vector<uint8_t>& bytes) {
    std::ostringstream oss;
    for (const auto& byte : bytes) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    }
    return oss.str();
}

std::vector<uint8_t> AdvancedCrypto::hex_to_bytes(const std::string& hex) {
    if (hex.length() % 2 != 0) {
        throw std::runtime_error("Invalid hex string: odd length");
    }

    std::vector<uint8_t> bytes;
    for (size_t i = 0; i < hex.length(); i += 2) {
        try {
            std::string byte_str = hex.substr(i, 2);
            unsigned char byte = static_cast<unsigned char>(std::stoi(byte_str, nullptr, 16));
            bytes.push_back(byte);
        } catch (const std::exception&) {
            throw std::runtime_error("Invalid hex string: contains non-hex characters");
        }
    }
    return bytes;
}

std::string AdvancedCrypto::base64_encode(const std::vector<uint8_t>& bytes) {
    BIO* bio = BIO_new(BIO_s_mem());
    BIO* b64 = BIO_new(BIO_f_base64());
    bio = BIO_push(b64, bio);

    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(bio, bytes.data(), bytes.size());
    BIO_flush(bio);

    BUF_MEM* buffer_mem;
    BIO_get_mem_ptr(bio, &buffer_mem);

    std::string result(buffer_mem->data, buffer_mem->length);
    BIO_free_all(bio);

    return result;
}

std::vector<uint8_t> AdvancedCrypto::base64_decode(const std::string& base64) {
    BIO* bio = BIO_new_mem_buf(base64.c_str(), base64.length());
    BIO* b64 = BIO_new(BIO_f_base64());
    bio = BIO_push(b64, bio);

    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);

    std::vector<uint8_t> result;
    char buffer[1024];
    int length;

    while ((length = BIO_read(bio, buffer, sizeof(buffer))) > 0) {
        result.insert(result.end(), buffer, buffer + length);
    }

    BIO_free_all(bio);
    return result;
}

AdvancedCrypto::KeyMetadata AdvancedCrypto::get_master_key_metadata() const {
    for (const auto& key_info : key_history_) {
        if (key_info.metadata.key_id == current_key_id_) {
            return key_info.metadata;
        }
    }
    throw std::runtime_error("Current master key metadata not found");
}

bool AdvancedCrypto::is_key_valid(const std::string& key_id) const {
    for (const auto& key_info : key_history_) {
        if (key_info.metadata.key_id == key_id) {
            return key_info.metadata.is_active || key_info.metadata.expires_at > std::chrono::system_clock::now();
        }
    }
    return false;
}

std::vector<uint8_t> AdvancedCrypto::derive_encryption_key(
    const std::vector<uint8_t>& master_key,
    const std::vector<uint8_t>& salt
) {
    return derive_key_pbkdf2(
        std::string(master_key.begin(), master_key.end()),
        salt,
        100000, // PBKDF2 iterations
        AES_KEY_SIZE
    );
}

void AdvancedCrypto::secure_clear(void* data, size_t length) {
    if (data && length > 0) {
        volatile char* p = static_cast<char*>(data);
        for (size_t i = 0; i < length; ++i) {
            p[i] = 0;
        }
    }
}

std::string AdvancedCrypto::generate_key_id() const {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

    auto random_bytes = generate_random_bytes(8);
    std::string random_str = bytes_to_hex(random_bytes);

    return "key_" + std::to_string(timestamp) + "_" + random_str.substr(0, 8);
}

// SecureString implementation
SecureString::SecureString(const std::string& str) : size_(str.size()) {
    data_ = std::make_unique<char[]>(size_ + 1);
    std::memcpy(data_.get(), str.c_str(), size_);
    data_[size_] = '\0';
}

SecureString::~SecureString() {
    if (data_) {
        volatile char* p = data_.get();
        for (size_t i = 0; i < size_; ++i) {
            p[i] = 0;
        }
    }
}

std::string SecureString::get() const {
    return std::string(data_.get(), size_);
}

bool SecureString::empty() const {
    return size_ == 0;
}

size_t SecureString::size() const {
    return size_;
}

} // namespace security
} // namespace aimux