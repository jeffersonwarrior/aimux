#ifndef AIMUX_VERSION_H
#define AIMUX_VERSION_H

#include <string>

namespace aimux {
    // Build information constants with CMake substitution
    constexpr const char* VERSION_MAJOR = "2";
    constexpr const char* VERSION_MINOR = "0";
    constexpr const char* VERSION_PATCH = "0";
    constexpr const char* VERSION_FULL = "2.0.0";
    constexpr const char* GIT_VERSION = "";
    constexpr const char* GIT_COMMIT = "";
    constexpr const char* BUILD_DATE = "";
    constexpr const char* BUILD_TYPE = "Debug";

    // Utility function for version string
    inline std::string getFullVersionString() {
        return std::string("aimux/") + VERSION_FULL +
               " (" + GIT_VERSION + "-" + GIT_COMMIT.substr(0, 7) + ")";
    }

    inline std::string getBuildInfo() {
        return std::string("Build: ") + BUILD_TYPE + " " + BUILD_DATE;
    }

    // Check if this is a production build
    inline bool isProductionBuild() {
        return std::string(BUILD_TYPE) == "Release";
    }
}

#endif // AIMUX_VERSION_H
