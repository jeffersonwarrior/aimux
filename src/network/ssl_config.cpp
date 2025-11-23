#include "aimux/network/ssl_config.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <regex>
#include <openssl/ssl.h>
#include <openssl/err.h>

namespace aimux {
namespace network {

SSLConfig::SSLConfig() 
    : verify_mode_(SSL_VERIFY_MODE_PEER),
      verify_depth_(9),
      cipher_list_("HIGH:!aNULL:!MD5"),
      protocols_(SSL_PROTOCOL_TLS1_2 | SSL_PROTOCOL_TLS1_3),
      certificate_file_(""),
      private_key_file_(""),
      ca_cert_file_(""),
      crl_file_(""),
      session_timeout_(300),
      session_cache_size_(1000) {
    
    // Initialize OpenSSL
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
}

SSLConfig::~SSLConfig() {
    EVP_cleanup();
    ERR_free_strings();
}

bool SSLConfig::load_from_file(const std::string& config_file) {
    std::ifstream file(config_file);
    if (!file.is_open()) {
        std::cerr << "Cannot open SSL config file: " << config_file << "\n";
        return false;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        // Skip comments and empty lines
        line = std::regex_replace(line, std::regex("^\\s*#.*"), "");
        line = std::regex_replace(line, std::regex("^\\s*$"), "");
        if (line.empty()) continue;
        
        std::smatch match;
        if (std::regex_match(line, match, std::regex("^([^=]+)=(.*)$"))) {
            std::string key = std::regex_replace(match[1].str(), std::regex("^\\s+|\\s+$"), "");
            std::string value = std::regex_replace(match[2].str(), std::regex("^\\s+|\\s+$"), "");
            
            if (key == "verify_mode") {
                if (value == "none") verify_mode_ = SSL_VERIFY_MODE_NONE;
                else if (value == "peer") verify_mode_ = SSL_VERIFY_MODE_PEER;
                else if (value == "fail_if_no_peer_cert") verify_mode_ = SSL_VERIFY_MODE_FAIL_IF_NO_PEER_CERT;
                else if (value == "client_once") verify_mode_ = SSL_VERIFY_MODE_CLIENT_ONCE;
            } else if (key == "verify_depth") {
                verify_depth_ = std::stoi(value);
            } else if (key == "cipher_list") {
                cipher_list_ = value;
            } else if (key == "protocols") {
                protocols_ = 0;
                if (value.find("TLSv1.2") != std::string::npos) protocols_ |= SSL_PROTOCOL_TLS1_2;
                if (value.find("TLSv1.3") != std::string::npos) protocols_ |= SSL_PROTOCOL_TLS1_3;
            } else if (key == "certificate_file") {
                certificate_file_ = value;
            } else if (key == "private_key_file") {
                private_key_file_ = value;
            } else if (key == "ca_cert_file") {
                ca_cert_file_ = value;
            } else if (key == "crl_file") {
                crl_file_ = value;
            } else if (key == "session_timeout") {
                session_timeout_ = std::stoi(value);
            } else if (key == "session_cache_size") {
                session_cache_size_ = std::stoi(value);
            }
        }
    }
    
    return validate_configuration();
}

bool SSLConfig::validate_configuration() const {
    // Validate certificate files
    if (!certificate_file_.empty()) {
        std::ifstream file(certificate_file_);
        if (!file.is_open()) {
            std::cerr << "Certificate file not found: " << certificate_file_ << "\n";
            return false;
        }
    }
    
    if (!private_key_file_.empty()) {
        std::ifstream file(private_key_file_);
        if (!file.is_open()) {
            std::cerr << "Private key file not found: " << private_key_file_ << "\n";
            return false;
        }
    }
    
    if (!ca_cert_file_.empty()) {
        std::ifstream file(ca_cert_file_);
        if (!file.is_open()) {
            std::cerr << "CA certificate file not found: " << ca_cert_file_ << "\n";
            return false;
        }
    }
    
    // Validate cipher list
    if (!cipher_list_.empty()) {
        SSL_CTX* ctx = SSL_CTX_new(TLS_method());
        if (ctx) {
            if (!SSL_CTX_set_cipher_list(ctx, cipher_list_.c_str())) {
                std::cerr << "Invalid cipher list: " << cipher_list_ << "\n";
                SSL_CTX_free(ctx);
                return false;
            }
            SSL_CTX_free(ctx);
        }
    }
    
    return true;
}

SSL_CTX* SSLConfig::create_ssl_context() const {
    SSL_CTX* ctx = SSL_CTX_new(TLS_method());
    if (!ctx) {
        std::cerr << "Failed to create SSL context\n";
        return nullptr;
    }
    
    // Set SSL version
    long options = 0;
    if (!(protocols_ & SSL_PROTOCOL_TLS1_2)) options |= static_cast<long>(SSL_OP_NO_TLSv1_2);
    if (!(protocols_ & SSL_PROTOCOL_TLS1_3)) options |= static_cast<long>(SSL_OP_NO_TLSv1_3);
    SSL_CTX_set_options(ctx, static_cast<uint64_t>(options));
    
    // Set cipher list
    if (!cipher_list_.empty()) {
        if (!SSL_CTX_set_cipher_list(ctx, cipher_list_.c_str())) {
            std::cerr << "Failed to set cipher list: " << cipher_list_ << "\n";
            SSL_CTX_free(ctx);
            return nullptr;
        }
    }
    
    // Set verification mode
    SSL_CTX_set_verify(ctx, verify_mode_, nullptr);
    SSL_CTX_set_verify_depth(ctx, verify_depth_);
    
    // Load certificates
    if (!certificate_file_.empty() && !private_key_file_.empty()) {
        if (!SSL_CTX_use_certificate_file(ctx, certificate_file_.c_str(), SSL_FILETYPE_PEM)) {
            std::cerr << "Failed to load certificate: " << certificate_file_ << "\n";
            SSL_CTX_free(ctx);
            return nullptr;
        }
        
        if (!SSL_CTX_use_PrivateKey_file(ctx, private_key_file_.c_str(), SSL_FILETYPE_PEM)) {
            std::cerr << "Failed to load private key: " << private_key_file_ << "\n";
            SSL_CTX_free(ctx);
            return nullptr;
        }
        
        if (!SSL_CTX_check_private_key(ctx)) {
            std::cerr << "Private key does not match certificate\n";
            SSL_CTX_free(ctx);
            return nullptr;
        }
    }
    
    // Load CA certificates
    if (!ca_cert_file_.empty()) {
        if (!SSL_CTX_load_verify_locations(ctx, ca_cert_file_.c_str(), nullptr)) {
            std::cerr << "Failed to load CA certificate: " << ca_cert_file_ << "\n";
            SSL_CTX_free(ctx);
            return nullptr;
        }
    }
    
    // Configure session cache
    SSL_CTX_set_timeout(ctx, session_timeout_);
    SSL_CTX_set_session_cache_mode(ctx, SSL_SESS_CACHE_SERVER);
    
    return ctx;
}

std::string SSLConfig::get_configuration_string() const {
    std::ostringstream config;
    config << "# SSL Configuration\n";
    config << "verify_mode=" << verify_mode_to_string() << "\n";
    config << "verify_depth=" << verify_depth_ << "\n";
    config << "cipher_list=" << cipher_list_ << "\n";
    config << "protocols=" << protocols_to_string() << "\n";
    if (!certificate_file_.empty()) config << "certificate_file=" << certificate_file_ << "\n";
    if (!private_key_file_.empty()) config << "private_key_file=" << private_key_file_ << "\n";
    if (!ca_cert_file_.empty()) config << "ca_cert_file=" << ca_cert_file_ << "\n";
    if (!crl_file_.empty()) config << "crl_file=" << crl_file_ << "\n";
    config << "session_timeout=" << session_timeout_ << "\n";
    config << "session_cache_size=" << session_cache_size_ << "\n";
    
    return config.str();
}

std::string SSLConfig::verify_mode_to_string() const {
    switch (verify_mode_) {
        case SSL_VERIFY_MODE_NONE: return "none";
        case SSL_VERIFY_MODE_PEER: return "peer";
        case SSL_VERIFY_MODE_FAIL_IF_NO_PEER_CERT: return "fail_if_no_peer_cert";
        case SSL_VERIFY_MODE_CLIENT_ONCE: return "client_once";
        default: return "peer";
    }
}

std::string SSLConfig::protocols_to_string() const {
    std::vector<std::string> protocols;
    if (protocols_ & SSL_PROTOCOL_TLS1_2) protocols.push_back("TLSv1.2");
    if (protocols_ & SSL_PROTOCOL_TLS1_3) protocols.push_back("TLSv1.3");
    
    if (protocols.empty()) return "none";
    
    std::string result = protocols[0];
    for (size_t i = 1; i < protocols.size(); ++i) {
        result += " " + protocols[i];
    }
    return result;
}

SSLConfigManager& SSLConfigManager::instance() {
    static SSLConfigManager instance;
    return instance;
}

bool SSLConfigManager::load_configuration(const std::string& config_file) {
    config_ = std::make_unique<SSLConfig>();
    return config_->load_from_file(config_file);
}

SSLConfig* SSLConfigManager::get_config() const {
    return config_.get();
}

} // namespace network
} // namespace aimux
