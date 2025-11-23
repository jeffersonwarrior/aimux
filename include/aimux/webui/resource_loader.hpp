#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <vector>
#include <algorithm>

namespace aimux {
namespace webui {

/**
 * @brief Embedded resource entry containing data and metadata
 */
struct EmbeddedResource {
    std::string data;           // Raw file content
    std::string content_type;   // MIME type
    std::string etag;          // ETag for caching
    size_t size_bytes;         // Size in bytes

    EmbeddedResource() : size_bytes(0) {}
    EmbeddedResource(const std::string& d, const std::string& ct)
        : data(d), content_type(ct), size_bytes(d.size()) {
        etag = "\"" + std::to_string(std::hash<std::string>{}(d)) + "\"";
    }
};

/**
 * @brief Resource loader for embedded HTML/CSS/JS files
 *
 * This class provides compile-time resource embedding for a single binary deployment.
 * All web assets are embedded as string literals and served through Crow's framework.
 */
class ResourceLoader {
public:
    /**
     * @brief Get singleton instance
     */
    static ResourceLoader& getInstance();

    /**
     * @brief Get embedded resource by path
     * @param path Resource path (e.g., "/dashboard.html", "/dashboard.css")
     * @return Pointer to embedded resource or nullptr if not found
     */
    const EmbeddedResource* getResource(const std::string& path) const;

    /**
     * @brief Check if resource exists
     * @param path Resource path
     * @return True if resource exists
     */
    bool hasResource(const std::string& path) const;

    /**
     * @brief Get all available resource paths
     * @return Vector of resource paths
     */
    std::vector<std::string> getResourcePaths() const;

    /**
     * @brief Initialize all embedded resources
     * This method should be called once at startup
     */
    void initialize();

private:
    ResourceLoader() = default;
    ~ResourceLoader() = default;

    // Non-copyable, non-movable singleton
    ResourceLoader(const ResourceLoader&) = delete;
    ResourceLoader& operator=(const ResourceLoader&) = delete;
    ResourceLoader(ResourceLoader&&) = delete;
    ResourceLoader& operator=(ResourceLoader&&) = delete;

    // Embedded resources registry
    std::unordered_map<std::string, std::unique_ptr<EmbeddedResource>> resources_;

    // Resource initialization helpers
    void initializeDashboardHTML();
    void initializeDashboardCSS();
    void initializeDashboardJS();

    // Utility methods for resource creation
    void addResource(const std::string& path, const std::string& data, const std::string& content_type);
    std::string getContentType(const std::string& extension) const;
};

/**
 * @brief Resource embedding macros for compile-time inclusion
 *
 * These macros help with resource embedding at compile time.
 * They can be used with build systems that support resource compilation.
 */

// Macro to embed a file as a string literal (if supported by build system)
#define EMBED_RESOURCE(path, name) \
    namespace aimux { namespace webui { namespace resources { \
        extern const char name[]; \
        extern const size_t name##_size; \
    }}}

// Macro to get embedded resource data
#define GET_EMBEDDED_RESOURCE(name) \
    aimux::webui::resources::name, aimux::webui::resources::name##_size

} // namespace webui
} // namespace aimux