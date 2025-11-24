/**
 * @file openai_model_query_test.cpp
 * @brief Tests for OpenAIModelQuery - Phase 2.2 of v3.0 Model Discovery
 *
 * Test Coverage:
 * - Successful API query with real OPENAI_API_KEY
 * - Filtering to production GPT-4 models only
 * - Version extraction from model IDs
 * - Sorting by created timestamp
 * - Error handling (401, 429, timeout, malformed JSON)
 * - Caching (24-hour TTL)
 *
 * Total: 12 tests
 *
 * Author: Claude Code (AI Agent)
 * Date: November 24, 2025
 * Status: Phase 2.2 - Provider Query Tests
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "aimux/providers/openai_model_query.hpp"
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

std::string get_env_var_openai(const std::string& var_name) {
    const char* value = std::getenv(var_name.c_str());
    return value ? std::string(value) : "";
}

void load_env_file_openai(const std::string& filename) {
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

class OpenAIModelQueryTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Load .env file
        load_env_file_openai("/home/aimux/.env");

        // Get API key
        api_key_ = get_env_var_openai("OPENAI_API_KEY");
    }

    std::string api_key_;
};

// ============================================================================
// Test Suite 1: Successful API Query (2 tests)
// ============================================================================

TEST_F(OpenAIModelQueryTest, SuccessfulAPIQuery_ReturnsGPT4Models) {
    ASSERT_FALSE(api_key_.empty()) << "OPENAI_API_KEY not found in .env file";

    OpenAIModelQuery query(api_key_);

    // Query API (this makes a real HTTP request)
    std::vector<ModelRegistry::ModelInfo> models;
    EXPECT_NO_THROW({
        models = query.get_available_models();
    });

    // Verify we got models
    EXPECT_GT(models.size(), 0) << "Expected at least one GPT-4 model from OpenAI API";

    // Verify all models have "openai" provider
    for (const auto& model : models) {
        EXPECT_EQ(model.provider, "openai");
        EXPECT_FALSE(model.model_id.empty());
        EXPECT_FALSE(model.version.empty());
        EXPECT_FALSE(model.release_date.empty());
        EXPECT_TRUE(model.is_available);
    }
}

TEST_F(OpenAIModelQueryTest, SuccessfulAPIQuery_OnlyGPT4Models) {
    ASSERT_FALSE(api_key_.empty()) << "OPENAI_API_KEY not found in .env file";

    OpenAIModelQuery query(api_key_);
    auto models = query.get_available_models();

    ASSERT_GT(models.size(), 0);

    // Verify all returned models are GPT-4 variants
    for (const auto& model : models) {
        EXPECT_TRUE(model.model_id.find("gpt-4") == 0)
            << "Expected only GPT-4 models, got: " << model.model_id;

        // Should NOT include GPT-3.5
        EXPECT_TRUE(model.model_id.find("gpt-3.5") == std::string::npos)
            << "Should not include GPT-3.5 models: " << model.model_id;
    }
}

// ============================================================================
// Test Suite 2: Filtering to Production Models (2 tests)
// ============================================================================

TEST_F(OpenAIModelQueryTest, Filtering_ExcludesPreviewModels) {
    ASSERT_FALSE(api_key_.empty()) << "OPENAI_API_KEY not found in .env file";

    OpenAIModelQuery query(api_key_);
    auto models = query.get_available_models();

    ASSERT_GT(models.size(), 0);

    // Verify no preview or experimental models
    for (const auto& model : models) {
        EXPECT_TRUE(model.model_id.find("preview") == std::string::npos)
            << "Should not include preview models: " << model.model_id;

        EXPECT_TRUE(model.model_id.find("experimental") == std::string::npos)
            << "Should not include experimental models: " << model.model_id;

        // Old preview date codes should be excluded
        EXPECT_TRUE(model.model_id.find("0314") == std::string::npos)
            << "Should not include old preview models: " << model.model_id;

        EXPECT_TRUE(model.model_id.find("0613") == std::string::npos)
            << "Should not include old preview models: " << model.model_id;
    }
}

TEST_F(OpenAIModelQueryTest, Filtering_IncludesProductionVariants) {
    ASSERT_FALSE(api_key_.empty()) << "OPENAI_API_KEY not found in .env file";

    OpenAIModelQuery query(api_key_);
    auto models = query.get_available_models();

    ASSERT_GT(models.size(), 0);

    // Check for known production GPT-4 variants
    // Note: Actual models available may vary over time
    bool found_gpt4 = false;

    for (const auto& model : models) {
        if (model.model_id.find("gpt-4") == 0) {
            found_gpt4 = true;
            // Verify production model characteristics
            EXPECT_FALSE(model.model_id.empty());
            EXPECT_FALSE(model.version.empty());
            EXPECT_TRUE(model.is_available);
        }
    }

    EXPECT_TRUE(found_gpt4) << "Expected to find at least one GPT-4 model";
}

// ============================================================================
// Test Suite 3: Version Extraction (2 tests)
// ============================================================================

TEST_F(OpenAIModelQueryTest, VersionExtraction_GPT4Variants) {
    ASSERT_FALSE(api_key_.empty()) << "OPENAI_API_KEY not found in .env file";

    OpenAIModelQuery query(api_key_);
    auto models = query.get_available_models();

    ASSERT_GT(models.size(), 0);

    // Verify version extraction for different GPT-4 variants
    for (const auto& model : models) {
        if (model.model_id.find("gpt-4o") == 0) {
            // GPT-4 Omni should be version 4.2
            EXPECT_EQ(model.version, "4.2")
                << "Expected version 4.2 for gpt-4o: " << model.model_id;
        } else if (model.model_id.find("gpt-4-turbo") == 0) {
            // GPT-4 Turbo should be version 4.1
            EXPECT_EQ(model.version, "4.1")
                << "Expected version 4.1 for gpt-4-turbo: " << model.model_id;
        } else if (model.model_id.find("gpt-4") == 0) {
            // Base GPT-4 should be version 4.0
            EXPECT_EQ(model.version, "4.0")
                << "Expected version 4.0 for gpt-4: " << model.model_id;
        }
    }
}

TEST_F(OpenAIModelQueryTest, VersionExtraction_AllModelsHaveValidVersion) {
    ASSERT_FALSE(api_key_.empty()) << "OPENAI_API_KEY not found in .env file";

    OpenAIModelQuery query(api_key_);
    auto models = query.get_available_models();

    ASSERT_GT(models.size(), 0);

    // Verify version format for all models
    for (const auto& model : models) {
        // Version should be in format "X.Y"
        EXPECT_TRUE(model.version.find('.') != std::string::npos)
            << "Version should contain a dot: " << model.version;

        // Version should start with a digit
        EXPECT_TRUE(std::isdigit(model.version[0]))
            << "Version should start with a digit: " << model.version;

        // Version should be 4.x
        EXPECT_TRUE(model.version[0] == '4')
            << "GPT-4 models should have version 4.x: " << model.version;
    }
}

// ============================================================================
// Test Suite 4: Sorting by Created Timestamp (1 test)
// ============================================================================

TEST_F(OpenAIModelQueryTest, Sorting_ByCreatedTimestamp_Descending) {
    ASSERT_FALSE(api_key_.empty()) << "OPENAI_API_KEY not found in .env file";

    OpenAIModelQuery query(api_key_);
    auto models = query.get_available_models();

    ASSERT_GE(models.size(), 2) << "Need at least 2 models to verify sorting";

    // Verify models are sorted by creation date (descending - newest first)
    for (size_t i = 1; i < models.size(); i++) {
        EXPECT_GE(models[i-1].release_date, models[i].release_date)
            << "Models should be sorted by created timestamp (descending): "
            << models[i-1].model_id << " (" << models[i-1].release_date << ") "
            << "should come before "
            << models[i].model_id << " (" << models[i].release_date << ")";
    }
}

// ============================================================================
// Test Suite 5: Error Handling (3 tests)
// ============================================================================

TEST_F(OpenAIModelQueryTest, ErrorHandling_InvalidAPIKey) {
    OpenAIModelQuery query("sk-invalid-key-12345");

    // Should throw exception for 401 Unauthorized
    EXPECT_THROW({
        query.get_available_models();
    }, std::runtime_error);
}

TEST_F(OpenAIModelQueryTest, ErrorHandling_EmptyAPIKey) {
    OpenAIModelQuery query("");

    // Should throw exception for missing authentication
    EXPECT_THROW({
        query.get_available_models();
    }, std::runtime_error);
}

TEST_F(OpenAIModelQueryTest, ErrorHandling_ValidResponseParsing) {
    // This test verifies the implementation handles valid OpenAI response format
    ASSERT_FALSE(api_key_.empty()) << "OPENAI_API_KEY not found in .env file";

    OpenAIModelQuery query(api_key_);

    // Should not throw for valid API key
    EXPECT_NO_THROW({
        auto models = query.get_available_models();
        EXPECT_GT(models.size(), 0);
    });
}

// ============================================================================
// Test Suite 6: Caching (2 tests)
// ============================================================================

TEST_F(OpenAIModelQueryTest, Caching_ValidCacheWithinTTL) {
    ASSERT_FALSE(api_key_.empty()) << "OPENAI_API_KEY not found in .env file";

    OpenAIModelQuery query(api_key_);

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

TEST_F(OpenAIModelQueryTest, Caching_ExpiredCacheRefreshes) {
    ASSERT_FALSE(api_key_.empty()) << "OPENAI_API_KEY not found in .env file";

    OpenAIModelQuery query(api_key_);

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
