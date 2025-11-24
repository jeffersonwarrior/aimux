/**
 * @file env_utils.hpp
 * @brief Environment variable utilities for loading .env files
 *
 * Author: Claude Code (AI Agent)
 * Date: November 24, 2025
 */

#pragma once

#include <string>
#include <fstream>
#include <iostream>
#include <cstdlib>

namespace aimux {
namespace core {

/**
 * @brief Load environment variables from a .env file
 * @param filename Path to the .env file
 *
 * Format: KEY=VALUE (one per line)
 * Comments start with #
 * Empty lines are skipped
 * Existing environment variables are not overwritten
 */
inline void load_env_file(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Warning: Could not open " << filename << std::endl;
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        // Skip comments and empty lines
        if (line.empty() || line[0] == '#') continue;

        size_t eq_pos = line.find('=');
        if (eq_pos != std::string::npos) {
            std::string key = line.substr(0, eq_pos);
            std::string value = line.substr(eq_pos + 1);
            setenv(key.c_str(), value.c_str(), 0);  // Don't overwrite existing env vars
        }
    }
}

} // namespace core
} // namespace aimux
