#pragma once

#include <string>
#include <memory>
#include <openssl/ssl.h>

namespace aimux {
namespace network {

/**
 * @brief SSL verification modes
 */
enum SSLVerifyMode {
    SSL_VERIFY_MODE_NONE = 0,
    SSL_VERIFY_MODE_PEER = 1,
    SSL_VERIFY_MODE_FAIL_IF_NO_PEER_CERT = 2,
    SSL_VERIFY_MODE_CLIENT_ONCE = 4
};

/**
 * @brief SSL protocol flags
 */
enum SSLProtocol {
    SSL_PROTOCOL_TLS1_2 = 1,
    SSL_PROTOCOL_TLS1_3 = 2
};

/**
 * @brief SSL configuration for secure connections
 */
class SSLConfig {
public:
    SSLConfig();
    ~SSLConfig();
    
    // Load configuration from file
    bool load_from_file(const std::string& config_file);
    
    // Validate configuration
    bool validate_configuration() const;
    
    // Create SSL context
    SSL_CTX* create_ssl_context() const;
    
    // Getters
    SSLVerifyMode get_verify_mode() const { return verify_mode_; }
    int get_verify_depth() const { return verify_depth_; }
    const std::string& get_cipher_list() const { return cipher_list_; }
    int get_protocols() const { return protocols_; }
    const std::string& get_certificate_file() const { return certificate_file_; }
    const std::string& get_private_key_file() const { return private_key_file_; }
    const std::string& get_ca_cert_file() const { return ca_cert_file_; }
    
    // Setters
    void set_verify_mode(SSLVerifyMode mode) { verify_mode_ = mode; }
    void set_verify_depth(int depth) { verify_depth_ = depth; }
    void set_cipher_list(const std::string& ciphers) { cipher_list_ = ciphers; }
    void set_protocols(int protocols) { protocols_ = protocols; }
    void set_certificate_file(const std::string& file) { certificate_file_ = file; }
    void set_private_key_file(const std::string& file) { private_key_file_ = file; }
    void set_ca_cert_file(const std::string& file) { ca_cert_file_ = file; }
    
    // Export configuration
    std::string get_configuration_string() const;

private:
    SSLVerifyMode verify_mode_;
    int verify_depth_;
    std::string cipher_list_;
    int protocols_;
    std::string certificate_file_;
    std::string private_key_file_;
    std::string ca_cert_file_;
    std::string crl_file_;
    int session_timeout_;
    int session_cache_size_;
    
    std::string verify_mode_to_string() const;
    std::string protocols_to_string() const;
};

/**
 * @brief SSL configuration manager
 */
class SSLConfigManager {
public:
    static SSLConfigManager& instance();
    
    bool load_configuration(const std::string& config_file);
    SSLConfig* get_config() const;

private:
    SSLConfigManager() = default;
    std::unique_ptr<SSLConfig> config_;
};

} // namespace network
} // namespace aimux
