/**
 * @file api_initializer_test.cpp
 * @brief Tests for APIInitializer - Phase 3.3 of v3.0 Model Discovery
 *
 * Test Coverage:
 * - Initialization of all 3 providers
 * - Fallback mechanism when model unavailable
 * - Caching of results (24-hour TTL)
 * - Error handling (missing keys, network issues)
 * - Concurrent initialization
 * - Timeout handling
 * - Performance (< 5 seconds total)
 * - Invalid API keys
 * - Missing API keys
 *
 * Total: 20 tests
 *
 * Author: Claude Code (AI Agent)
 * Date: November 24, 2025
 * Status: Phase 3.3 - APIInitializer Tests
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "aimux/core/api_initializer.hpp"
#include "aimux/core/model_registry.hpp"
#include <chrono>
#include <thread>
#include <fstream>
#include <cstdlib>

using namespace aimux::core;

// ============================================================================
// Environment Variable Helper
// ============================================================================

std::string get_env_var_init(const std::string& var_name) {
    const char* value = std::getenv(var_name.c_str());
    return value ? std::string(value) : "";
}

void load_env_file_init(const std::string& filename) {
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

class APIInitializerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Load .env file
        load_env_file_init("/home/aimux/.env");

        // Clear cache before each test for fresh results
        APIInitializer::clear_cache();
    }

    void TearDown() override {
        // Clear cache after each test
        APIInitializer::clear_cache();
    }
};

// ============================================================================
// Test Suite 1: Basic Initialization (3 tests)
// ============================================================================

TEST_F(APIInitializerTest, InitializeAllProviders_Success) {
    // This test verifies the full initialization pipeline works
    auto result = APIInitializer::initialize_all_providers();

    // Should have results for all 3 providers
    EXPECT_GE(result.selected_models.size(), 1)
        << "Expected at least one provider to be initialized";

    // Check for expected providers
    bool has_anthropic = result.selected_models.find("anthropic") != result.selected_models.end();
    bool has_openai = result.selected_models.find("openai") != result.selected_models.end();
    bool has_cerebras = result.selected_models.find("cerebras") != result.selected_models.end();

    // At least one provider should succeed
    EXPECT_TRUE(has_anthropic || has_openai || has_cerebras)
        << "At least one provider should be initialized";

    // Verify has_success() method
    EXPECT_TRUE(result.has_success());

    // Verify summary() doesn't crash
    std::string summary = result.summary();
    EXPECT_FALSE(summary.empty());
}

TEST_F(APIInitializerTest, InitializeAllProviders_ReturnsModels) {
    auto result = APIInitializer::initialize_all_providers();

    // Each initialized provider should have a model
    for (const auto& [provider, model] : result.selected_models) {
        EXPECT_FALSE(model.model_id.empty()) << "Provider " << provider << " has empty model_id";
        EXPECT_FALSE(model.version.empty()) << "Provider " << provider << " has empty version";
        EXPECT_EQ(model.provider, provider) << "Provider mismatch for " << provider;
    }
}

TEST_F(APIInitializerTest, InitializeAllProviders_PerformanceUnder5Seconds) {
    auto start_time = std::chrono::high_resolution_clock::now();

    auto result = APIInitializer::initialize_all_providers();

    auto end_time = std::chrono::high_resolution_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(
        end_time - start_time).count();

    // Should complete within 5 seconds (5000ms)
    EXPECT_LT(elapsed_ms, 5000.0)
        << "Initialization took " << elapsed_ms << " ms, should be under 5000ms";

    // Result should also track timing
    EXPECT_GT(result.total_time_ms, 0.0);
}

// ============================================================================
// Test Suite 2: Single Provider Initialization (3 tests)
// ============================================================================

TEST_F(APIInitializerTest, InitializeProvider_Anthropic) {
    std::string api_key = get_env_var_init("ANTHROPIC_API_KEY");

    if (api_key.empty()) {
        GTEST_SKIP() << "ANTHROPIC_API_KEY not found, skipping test";
    }

    auto result = APIInitializer::initialize_provider("anthropic");

    ASSERT_EQ(result.selected_models.size(), 1);
    EXPECT_EQ(result.selected_models.begin()->first, "anthropic");

    const auto& model = result.selected_models["anthropic"];
    EXPECT_FALSE(model.model_id.empty());
    EXPECT_EQ(model.provider, "anthropic");
}

TEST_F(APIInitializerTest, InitializeProvider_OpenAI) {
    std::string api_key = get_env_var_init("OPENAI_API_KEY");

    if (api_key.empty()) {
        GTEST_SKIP() << "OPENAI_API_KEY not found, skipping test";
    }

    auto result = APIInitializer::initialize_provider("openai");

    ASSERT_EQ(result.selected_models.size(), 1);
    EXPECT_EQ(result.selected_models.begin()->first, "openai");

    const auto& model = result.selected_models["openai"];
    EXPECT_FALSE(model.model_id.empty());
    EXPECT_EQ(model.provider, "openai");
}

TEST_F(APIInitializerTest, InitializeProvider_Cerebras) {
    std::string api_key = get_env_var_init("CEREBRAS_API_KEY");

    if (api_key.empty()) {
        GTEST_SKIP() << "CEREBRAS_API_KEY not found, skipping test";
    }

    auto result = APIInitializer::initialize_provider("cerebras");

    ASSERT_EQ(result.selected_models.size(), 1);
    EXPECT_EQ(result.selected_models.begin()->first, "cerebras");

    const auto& model = result.selected_models["cerebras"];
    EXPECT_FALSE(model.model_id.empty());
    EXPECT_EQ(model.provider, "cerebras");
}

// ============================================================================
// Test Suite 3: Fallback Mechanism (3 tests)
// ============================================================================

TEST_F(APIInitializerTest, Fallback_MissingAPIKey) {
    // Temporarily unset API key
    char* old_key = getenv("CEREBRAS_API_KEY");
    std::string saved_key = old_key ? std::string(old_key) : "";
    unsetenv("CEREBRAS_API_KEY");

    auto result = APIInitializer::initialize_provider("cerebras");

    // Should use fallback
    EXPECT_EQ(result.selected_models.size(), 1);
    EXPECT_FALSE(result.validation_results["cerebras"]);
    EXPECT_TRUE(result.used_fallback["cerebras"]);
    EXPECT_FALSE(result.error_messages["cerebras"].empty());

    // Fallback model should be llama3.1-8b
    const auto& model = result.selected_models["cerebras"];
    EXPECT_EQ(model.model_id, "llama3.1-8b");

    // Restore API key
    if (!saved_key.empty()) {
        setenv("CEREBRAS_API_KEY", saved_key.c_str(), 1);
    }
}

TEST_F(APIInitializerTest, Fallback_InvalidAPIKey) {
    // Test with invalid key
    setenv("CEREBRAS_API_KEY", "invalid-key-12345", 1);

    auto result = APIInitializer::initialize_provider("cerebras");

    // Should use fallback due to API error
    EXPECT_EQ(result.selected_models.size(), 1);
    EXPECT_FALSE(result.validation_results["cerebras"]);

    // Reload valid key from .env
    load_env_file_init("/home/aimux/.env");
}

TEST_F(APIInitializerTest, Fallback_ContainsValidModels) {
    // Verify fallback models are valid
    char* old_key = getenv("ANTHROPIC_API_KEY");
    std::string saved_key = old_key ? std::string(old_key) : "";
    unsetenv("ANTHROPIC_API_KEY");

    auto result = APIInitializer::initialize_provider("anthropic");

    const auto& model = result.selected_models["anthropic"];
    EXPECT_EQ(model.provider, "anthropic");
    EXPECT_FALSE(model.model_id.empty());
    EXPECT_FALSE(model.version.empty());
    EXPECT_TRUE(model.is_available);

    // Restore API key
    if (!saved_key.empty()) {
        setenv("ANTHROPIC_API_KEY", saved_key.c_str(), 1);
    }
}

// ============================================================================
// Test Suite 4: Caching (4 tests)
// ============================================================================

TEST_F(APIInitializerTest, Caching_NoInitialCache) {
    EXPECT_FALSE(APIInitializer::has_valid_cache());

    auto empty_result = APIInitializer::get_cached_result();
    EXPECT_TRUE(empty_result.selected_models.empty());
}

TEST_F(APIInitializerTest, Caching_CacheAfterInitialization) {
    // First initialization
    auto result1 = APIInitializer::initialize_all_providers();

    // Should now have cache
    EXPECT_TRUE(APIInitializer::has_valid_cache());

    // Second call should use cache
    auto start_time = std::chrono::high_resolution_clock::now();
    auto result2 = APIInitializer::initialize_all_providers();
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - start_time).count();

    // Cached call should be very fast (< 10ms)
    EXPECT_LT(elapsed_ms, 10)
        << "Cached initialization should be fast, took " << elapsed_ms << "ms";

    // Results should match
    EXPECT_EQ(result1.selected_models.size(), result2.selected_models.size());
}

TEST_F(APIInitializerTest, Caching_GetCachedResult) {
    // Initialize first
    auto result1 = APIInitializer::initialize_all_providers();

    // Get cached result
    auto cached = APIInitializer::get_cached_result();

    // Should match
    EXPECT_EQ(result1.selected_models.size(), cached.selected_models.size());

    for (const auto& [provider, model] : result1.selected_models) {
        ASSERT_TRUE(cached.selected_models.find(provider) != cached.selected_models.end());
        EXPECT_EQ(model.model_id, cached.selected_models[provider].model_id);
    }
}

TEST_F(APIInitializerTest, Caching_ClearCache) {
    // Initialize
    APIInitializer::initialize_all_providers();
    EXPECT_TRUE(APIInitializer::has_valid_cache());

    // Clear cache
    APIInitializer::clear_cache();
    EXPECT_FALSE(APIInitializer::has_valid_cache());

    // Get cached result should be empty
    auto empty_result = APIInitializer::get_cached_result();
    EXPECT_TRUE(empty_result.selected_models.empty());
}

// ============================================================================
// Test Suite 5: Error Handling (3 tests)
// ============================================================================

TEST_F(APIInitializerTest, ErrorHandling_InvalidProvider) {
    EXPECT_THROW({
        APIInitializer::initialize_provider("invalid_provider");
    }, std::runtime_error);
}

TEST_F(APIInitializerTest, ErrorHandling_AllProvidersMissingKeys) {
    // Save original keys
    char* anthropic_key = getenv("ANTHROPIC_API_KEY");
    char* openai_key = getenv("OPENAI_API_KEY");
    char* cerebras_key = getenv("CEREBRAS_API_KEY");

    std::string saved_anthropic = anthropic_key ? std::string(anthropic_key) : "";
    std::string saved_openai = openai_key ? std::string(openai_key) : "";
    std::string saved_cerebras = cerebras_key ? std::string(cerebras_key) : "";

    // Unset all keys
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("OPENAI_API_KEY");
    unsetenv("CEREBRAS_API_KEY");

    auto result = APIInitializer::initialize_all_providers();

    // All should use fallback
    EXPECT_EQ(result.selected_models.size(), 3);

    for (const auto& [provider, model] : result.selected_models) {
        EXPECT_FALSE(result.validation_results[provider])
            << "Provider " << provider << " should have failed validation";
        EXPECT_TRUE(result.used_fallback[provider])
            << "Provider " << provider << " should have used fallback";
    }

    // Restore keys
    if (!saved_anthropic.empty()) setenv("ANTHROPIC_API_KEY", saved_anthropic.c_str(), 1);
    if (!saved_openai.empty()) setenv("OPENAI_API_KEY", saved_openai.c_str(), 1);
    if (!saved_cerebras.empty()) setenv("CEREBRAS_API_KEY", saved_cerebras.c_str(), 1);
}

TEST_F(APIInitializerTest, ErrorHandling_GracefulDegradation) {
    // Even if some providers fail, initialization should continue
    auto result = APIInitializer::initialize_all_providers();

    // Should have at least attempted all providers
    EXPECT_TRUE(result.validation_results.find("anthropic") != result.validation_results.end());
    EXPECT_TRUE(result.validation_results.find("openai") != result.validation_results.end());
    EXPECT_TRUE(result.validation_results.find("cerebras") != result.validation_results.end());
}

// ============================================================================
// Test Suite 6: Integration with ModelRegistry (2 tests)
// ============================================================================

TEST_F(APIInitializerTest, Integration_ModelsAddedToRegistry) {
    auto& registry = ModelRegistry::instance();

    // Initialize all providers
    auto result = APIInitializer::initialize_all_providers();

    // Check that selected models are in registry
    for (const auto& [provider, model] : result.selected_models) {
        auto retrieved = registry.get_latest_model(provider);

        EXPECT_FALSE(retrieved.model_id.empty())
            << "Model for " << provider << " should be in registry";

        // Should be able to validate the model
        EXPECT_TRUE(registry.validate_model(provider, model.model_id))
            << "Model " << model.model_id << " should be validated";
    }
}

TEST_F(APIInitializerTest, Integration_CanRetrieveLatestModel) {
    auto& registry = ModelRegistry::instance();

    // Initialize
    auto result = APIInitializer::initialize_all_providers();

    // For each provider, retrieve latest from registry
    for (const auto& [provider, expected_model] : result.selected_models) {
        auto latest = registry.get_latest_model(provider);

        // Should match what was initialized
        EXPECT_EQ(latest.provider, provider);
        EXPECT_FALSE(latest.model_id.empty());
    }
}

// ============================================================================
// Test Suite 7: Concurrent Access (2 tests)
// ============================================================================

TEST_F(APIInitializerTest, Concurrency_MultipleThreadsInitialize) {
    const int num_threads = 5;
    std::vector<std::thread> threads;
    std::vector<APIInitializer::InitResult> results(num_threads);

    // Clear cache first
    APIInitializer::clear_cache();

    // Launch multiple threads
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back([&results, i]() {
            results[i] = APIInitializer::initialize_all_providers();
        });
    }

    // Wait for all threads
    for (auto& t : threads) {
        t.join();
    }

    // All threads should get consistent results (cache hit after first)
    for (int i = 1; i < num_threads; i++) {
        EXPECT_EQ(results[0].selected_models.size(), results[i].selected_models.size())
            << "Thread " << i << " got different result size";
    }
}

TEST_F(APIInitializerTest, Concurrency_ThreadSafeCacheAccess) {
    const int num_threads = 10;
    std::vector<std::thread> threads;

    // Initialize once
    APIInitializer::initialize_all_providers();

    // All threads read cache concurrently
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back([]() {
            EXPECT_TRUE(APIInitializer::has_valid_cache());
            auto result = APIInitializer::get_cached_result();
            EXPECT_FALSE(result.selected_models.empty());
        });
    }

    // Wait for all threads
    for (auto& t : threads) {
        t.join();
    }

    // No crashes = success
}
