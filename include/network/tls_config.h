#ifndef AIMUX_TLS_CONFIG_H
#define AIMUX_TLS_CONFIG_H

#include <string>
#include <memory>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <filesystem>

namespace aimux {
namespace network {

/**
 * TLS configuration for secure HTTPS connections
 * Handles SSL/TLS setup, certificate validation, and secure communication
 */
class TLSConfig {
public:
    TLSConfig();
    ~TLSConfig();

    // Initialization
    bool initialize();
    bool loadCertificates(const std::string& certFile, const std::string& keyFile);
    bool loadCACertificate(const std::string& caFile);
    bool generateSelfSignedCertificate(const std::string& certFile, const std::string& keyFile);

    // Configuration
    void setVerifyMode(int mode);
    void setCipherList(const std::string& ciphers);
    void setProtocolVersions(int minVersion, int maxVersion);

    // SSL context management
    SSL_CTX* getSSLContext() const { return sslContext_; }
    bool isConfigured() const { return sslContext_ != nullptr; }

    // Certificate validation
    bool validateCertificate(const std::string& certFile);
    bool checkPrivateKeyMatch(const std::string& certFile, const std::string& keyFile);
    std::string getCertificateInfo(const std::string& certFile);

    // Security settings
    void enableHSTS();
    void enableHPKP();
    void setSecurityHeaders();

    // Default production configuration
    static TLSConfig createProductionConfig();

private:
    // OpenSSL context
    SSL_CTX* sslContext_;
    bool initialized_;

    // Configuration
    std::string certFile_;
    std::string keyFile_;
    std::string caFile_;
    int verifyMode_;
    std::string cipherList_;

    // Helper functions
    bool setupSSLContext();
    void cleanupSSLContext();
    bool validateCertificateFile(const std::string& file);
    std::string getOpenSSLError();
};

/**
 * HTTPS server wrapper with TLS support
 */
class HTTPSServer {
public:
    HTTPSServer(const TLSConfig& tlsConfig);
    ~HTTPSServer();

    bool start(int port);
    void stop();
    bool isRunning() const { return running_; }

    // Request handling (simplified interface)
    void registerHandler(const std::string& path, std::function<void(const HTTPRequest&, HTTPResponse&)> handler);

private:
    TLSConfig tlsConfig_;
    int port_;
    bool running_;
    std::unique_ptr<std::thread> serverThread_;

    void serverLoop();
    void handleHTTPRequest(const HTTPRequest& request, HTTPResponse& response);
};

/**
 * TLS validation utilities
 */
namespace utils {
    bool isSecureURL(const std::string& url);
    std::string getCertificateFingerprint(const std::string& certFile);
    bool verifyCertificateChain(const std::string& certFile, const std::string& caFile);
    std::string getDefaultCipherList();
    bool isTLSVersionSupported(int version);
}

} // namespace network
} // namespace aimux

#endif // AIMUX_TLS_CONFIG_H