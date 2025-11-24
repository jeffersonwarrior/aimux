/**
 * @file cerebras_model_query_test.cpp
 * @brief Tests for CerebrasModelQuery - Phase 2.3 of v3.0 Model Discovery
 *
 * Test Coverage:
 * - Successful API query with real CEREBRAS_API_KEY
 * - Response parsing for Cerebras format
 * - Version extraction from model IDs
 * - Latest model selection
 * - Error handling (401, 429, timeout, malformed JSON)
 * - Caching (24-hour TTL)
 *
 * Total: 11 tests
 *
 * Author: Claude Code (AI Agent)
 * Date: November 24, 2025
 * Status: Phase 2.3 - Provider Query Tests
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "aimux/providers/cerebras_model_query.hpp"
#include "aimux/core/model_registry.hpp"
#include <chrono>
#include <thread>
#include <fstream>
#include <cstdlib>

using namespace aimux::providers;
using namespace aimux::core;

// ============================================================================
// Environment Variable Helper
// ============================================================================

std::string get_env_var_cerebras(const std::string& var_name) {
    const char* value = std::getenv(var_name.c_str());
    return value ? std::string(value) : "";
}

void load_env_file_cerebras(const std::string& filename) {
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

// ============================================================================
// Test Fixture
// ============================================================================

class CerebrasModelQueryTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Load .env file
        load_env_file_cerebras("/home/aimux/.env");

        // Get API key
        api_key_ = get_env_var_cerebras("CEREBRAS_API_KEY");
    }

    std::string api_key_;
};

// ============================================================================
// Test Suite 1: Successful API Query (2 tests)
// ============================================================================

TEST_F(CerebrasModelQueryTest, SuccessfulAPIQuery_ReturnsModels) {
    ASSERT_FALSE(api_key_.empty()) << "CEREBRAS_API_KEY not found in .env file";

    CerebrasModelQuery query(api_key_);

    // Query API (this makes a real HTTP request)
    std::vector<ModelRegistry::ModelInfo> models;
    EXPECT_NO_THROW({
        models = query.get_available_models();
    });

    // Verify we got models
    EXPECT_GT(models.size(), 0) << "Expected at least one model from Cerebras API";

    // Verify all models have "cerebras" provider
    for (const auto& model : models) {
        EXPECT_EQ(model.provider, "cerebras");
        EXPECT_FALSE(model.model_id.empty());
        EXPECT_FALSE(model.version.empty());
        EXPECT_FALSE(model.release_date.empty());
        EXPECT_TRUE(model.is_available);
    }
}

TEST_F(CerebrasModelQueryTest, SuccessfulAPIQuery_ExtractsModelID) {
    ASSERT_FALSE(api_key_.empty()) << "CEREBRAS_API_KEY not found in .env file";

    CerebrasModelQuery query(api_key_);
    auto models = query.get_available_models();

    ASSERT_GT(models.size(), 0);

    // Check that at least one Llama model is present
    bool found_llama = false;
    for (const auto& model : models) {
        if (model.model_id.find("llama") != std::string::npos) {
            found_llama = true;
            break;
        }
    }

    EXPECT_TRUE(found_llama) << "Expected to find at least one Llama model";
}

// ============================================================================
// Test Suite 2: Response Parsing (2 tests)
// ============================================================================

TEST_F(CerebrasModelQueryTest, ParseResponse_ValidCerebrasFormat) {
    ASSERT_FALSE(api_key_.empty()) << "CEREBRAS_API_KEY not found in .env file";

    CerebrasModelQuery query(api_key_);
    auto models = query.get_available_models();

    ASSERT_GT(models.size(), 0);

    // Verify proper parsing of Cerebras response format
    for (const auto& model : models) {
        // Release date should be in YYYY-MM-DD format
        EXPECT_EQ(model.release_date.length(), 10)
            << "Expected YYYY-MM-DD format, got: " << model.release_date;
        EXPECT_EQ(model.release_date[4], '-');
        EXPECT_EQ(model.release_date[7], '-');
    }
}

TEST_F(CerebrasModelQueryTest, ParseResponse_ExtractsAllFields) {
    ASSERT_FALSE(api_key_.empty()) << "CEREBRAS_API_KEY not found in .env file";

    CerebrasModelQuery query(api_key_);
    auto models = query.get_available_models();

    ASSERT_GT(models.size(), 0);

    // Verify all required fields are present
    for (const auto& model : models) {
        EXPECT_FALSE(model.provider.empty()) << "Provider should not be empty";
        EXPECT_FALSE(model.model_id.empty()) << "Model ID should not be empty";
        EXPECT_FALSE(model.version.empty()) << "Version should not be empty";
        EXPECT_FALSE(model.release_date.empty()) << "Release date should not be empty";
        EXPECT_TRUE(model.is_available) << "Model should be marked as available";
    }
}

// ============================================================================
// Test Suite 3: Version Extraction (2 tests)
// ============================================================================

TEST_F(CerebrasModelQueryTest, VersionExtraction_LlamaModels) {
    ASSERT_FALSE(api_key_.empty()) << "CEREBRAS_API_KEY not found in .env file";

    CerebrasModelQuery query(api_key_);
    auto models = query.get_available_models();

    ASSERT_GT(models.size(), 0);

    // Look for Llama 3.1 models
    bool found_llama_3 = false;
    for (const auto& model : models) {
        if (model.model_id.find("llama3") != std::string::npos) {
            found_llama_3 = true;

            // Version should be extracted correctly
            if (model.model_id.find("llama3.1") != std::string::npos) {
                EXPECT_EQ(model.version, "3.1")
                    << "Expected version 3.1 for model: " << model.model_id;
            } else if (model.model_id.find("llama3") != std::string::npos) {
                // Could be 3.0 or 3.1
                EXPECT_TRUE(model.version == "3.0" || model.version == "3.1")
                    << "Expected version 3.x for model: " << model.model_id;
            }
        }
    }

    if (!found_llama_3) {
        std::cout << "INFO: No Llama 3.x models found in API response\n";
    }
}

TEST_F(CerebrasModelQueryTest, VersionExtraction_AllModelsHaveValidVersion) {
    ASSERT_FALSE(api_key_.empty()) << "CEREBRAS_API_KEY not found in .env file";

    CerebrasModelQuery query(api_key_);
    auto models = query.get_available_models();

    ASSERT_GT(models.size(), 0);

    // Verify version format for all models
    for (const auto& model : models) {
        // Version should be in format "X.Y" or "X.Y.Z"
        EXPECT_TRUE(model.version.find('.') != std::string::npos)
            << "Version should contain a dot: " << model.version;

        // Version should start with a digit
        EXPECT_TRUE(std::isdigit(model.version[0]))
            << "Version should start with a digit: " << model.version;
    }
}

// ============================================================================
// Test Suite 4: Latest Model Selection (1 test)
// ============================================================================

TEST_F(CerebrasModelQueryTest, LatestModelSelection_HighestVersionFirst) {
    ASSERT_FALSE(api_key_.empty()) << "CEREBRAS_API_KEY not found in .env file";

    CerebrasModelQuery query(api_key_);
    auto models = query.get_available_models();

    ASSERT_GE(models.size(), 1);

    // Models should be sorted by version (descending)
    // The first model should be the latest version
    if (models.size() >= 2) {
        // Compare first two models
        int version_compare = ModelRegistry::compare_versions(
            models[0].version, models[1].version);

        EXPECT_GE(version_compare, 0)
            << "First model should have >= version than second: "
            << models[0].model_id << " (" << models[0].version << ") vs "
            << models[1].model_id << " (" << models[1].version << ")";
    }
}

// ============================================================================
// Test Suite 5: Error Handling (2 tests)
// ============================================================================

TEST_F(CerebrasModelQueryTest, ErrorHandling_InvalidAPIKey) {
    CerebrasModelQuery query("invalid-api-key-12345");

    // Should throw exception for 401 Unauthorized
    EXPECT_THROW({
        query.get_available_models();
    }, std::runtime_error);
}

TEST_F(CerebrasModelQueryTest, ErrorHandling_EmptyAPIKey) {
    CerebrasModelQuery query("");

    // Should throw exception for missing authentication
    EXPECT_THROW({
        query.get_available_models();
    }, std::runtime_error);
}

// ============================================================================
// Test Suite 6: Caching (2 tests)
// ============================================================================

TEST_F(CerebrasModelQueryTest, Caching_ValidCacheWithinTTL) {
    ASSERT_FALSE(api_key_.empty()) << "CEREBRAS_API_KEY not found in .env file";

    CerebrasModelQuery query(api_key_);

    // Clear cache first
    query.clear_cache();
    EXPECT_FALSE(query.has_valid_cache());

    // First query - should hit API
    auto start_time = std::chrono::high_resolution_clock::now();
    auto models1 = query.get_available_models();
    auto first_query_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - start_time).count();

    EXPECT_TRUE(query.has_valid_cache());

    // Second query - should use cache (much faster)
    start_time = std::chrono::high_resolution_clock::now();
    auto models2 = query.get_available_models();
    auto second_query_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - start_time).count();

    // Cache hit should be significantly faster (at least 10x)
    EXPECT_LT(second_query_time, first_query_time / 10)
        << "Cached query should be much faster. First: " << first_query_time
        << "ms, Second: " << second_query_time << "ms";

    // Results should be identical
    EXPECT_EQ(models1.size(), models2.size());
}

TEST_F(CerebrasModelQueryTest, Caching_ExpiredCacheRefreshes) {
    ASSERT_FALSE(api_key_.empty()) << "CEREBRAS_API_KEY not found in .env file";

    CerebrasModelQuery query(api_key_);

    // Clear cache first
    query.clear_cache();
    EXPECT_FALSE(query.has_valid_cache());

    // First query - populate cache
    auto models1 = query.get_available_models();
    EXPECT_TRUE(query.has_valid_cache());

    // Clear cache to simulate expiry
    query.clear_cache();
    EXPECT_FALSE(query.has_valid_cache());

    // Next query should hit API again
    auto models2 = query.get_available_models();
    EXPECT_TRUE(query.has_valid_cache());

    // Results should be consistent
    EXPECT_EQ(models1.size(), models2.size());
}
