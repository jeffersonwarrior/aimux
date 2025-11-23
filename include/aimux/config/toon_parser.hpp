#pragma once

#include <string>
#include <nlohmann/json.hpp>

namespace aimux {
namespace config {

/**
 * @brief Result of TOON parsing operation
 */
struct ToonParseResult {
    bool success = false;
    nlohmann::json config;
    std::string error;
};

/**
 * @brief TOON (Tabular Object-Oriented Notation) configuration parser
 * Lightweight configuration format optimized for AI provider configurations
 */
class ToonParser {
public:
    ToonParser();
    
    // Parse TOON from file
    ToonParseResult parse_file(const std::string& filename);
    
    // Parse TOON from string
    ToonParseResult parse_string(const std::string& content);
    
    // Convert JSON to TOON format
    std::string json_to_toon(const nlohmann::json& json, int indent = 0);
    
    // Validate TOON syntax
    bool validate_toon(const std::string& content, std::string& error);

private:
    // Parse TOON string to JSON
    nlohmann::json parse_toon_to_json(const std::string& content);
    
    // Parse individual TOON value
    nlohmann::json parse_toon_value(const std::string& value);
    
    // Convert JSON to TOON recursively
    void json_to_toon_recursive(const nlohmann::json& json, 
                               std::ostringstream& toon, 
                               int indent, 
                               const std::string& prefix);
    
    // Convert JSON value to TOON string
    std::string toon_value_to_string(const nlohmann::json& value);
};

} // namespace config
} // namespace aimux
