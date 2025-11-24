#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "aimux/core/model_registry.hpp"
#include <thread>
#include <vector>
#include <fstream>

using namespace aimux::core;

// ============================================================================
// Test Suite 1: Version Comparison (5 tests)
// ============================================================================

TEST(ModelRegistryTest, VersionComparison_MajorVersion) {
    // Test major version comparison
    EXPECT_EQ(ModelRegistry::compare_versions("4.0", "3.5"), 1);   // 4.0 > 3.5
    EXPECT_EQ(ModelRegistry::compare_versions("3.5", "4.0"), -1);  // 3.5 < 4.0
    EXPECT_EQ(ModelRegistry::compare_versions("3.0", "3.0"), 0);   // 3.0 == 3.0
}

TEST(ModelRegistryTest, VersionComparison_MinorVersion) {
    // Test minor version comparison
    EXPECT_EQ(ModelRegistry::compare_versions("3.5", "3.0"), 1);   // 3.5 > 3.0
    EXPECT_EQ(ModelRegistry::compare_versions("3.0", "3.5"), -1);  // 3.0 < 3.5
    EXPECT_EQ(ModelRegistry::compare_versions("4.1", "4.0"), 1);   // 4.1 > 4.0
}

TEST(ModelRegistryTest, VersionComparison_PatchVersion) {
    // Test patch version comparison
    EXPECT_EQ(ModelRegistry::compare_versions("3.5.1", "3.5.0"), 1);   // 3.5.1 > 3.5.0
    EXPECT_EQ(ModelRegistry::compare_versions("3.5.0", "3.5.1"), -1);  // 3.5.0 < 3.5.1
    EXPECT_EQ(ModelRegistry::compare_versions("4.0.2", "4.0.1"), 1);   // 4.0.2 > 4.0.1
}

TEST(ModelRegistryTest, VersionComparison_PrereleaseVersions) {
    // Test prerelease version handling
    // Stable > prerelease
    EXPECT_EQ(ModelRegistry::compare_versions("3.5", "3.5-rc1"), 1);    // stable > rc
    EXPECT_EQ(ModelRegistry::compare_versions("3.5-rc1", "3.5"), -1);   // rc < stable

    // Prerelease comparison (lexicographic)
    EXPECT_EQ(ModelRegistry::compare_versions("3.5-rc2", "3.5-rc1"), 1);    // rc2 > rc1
    EXPECT_EQ(ModelRegistry::compare_versions("3.5-beta", "3.5-alpha"), 1); // beta > alpha
}

TEST(ModelRegistryTest, VersionComparison_EdgeCases) {
    // Test edge cases
    EXPECT_EQ(ModelRegistry::compare_versions("", ""), 0);           // empty == empty
    EXPECT_EQ(ModelRegistry::compare_versions("1.0", ""), 1);        // 1.0 > empty
    EXPECT_EQ(ModelRegistry::compare_versions("", "1.0"), -1);       // empty < 1.0

    // Missing patch version
    EXPECT_EQ(ModelRegistry::compare_versions("3.5", "3.5.0"), 0);   // 3.5 == 3.5.0
}

// ============================================================================
// Test Suite 2: Model Selection (3 tests)
// ============================================================================

TEST(ModelRegistryTest, SelectLatest_SingleModel) {
    std::vector<ModelRegistry::ModelInfo> models = {
        {"anthropic", "claude-3-sonnet", "3.0", "2024-03-01", true}
    };

    auto latest = ModelRegistry::select_latest(models);

    EXPECT_EQ(latest.model_id, "claude-3-sonnet");
    EXPECT_EQ(latest.version, "3.0");
}

TEST(ModelRegistryTest, SelectLatest_MultipleModels_ByVersion) {
    std::vector<ModelRegistry::ModelInfo> models = {
        {"anthropic", "claude-3-sonnet", "3.0", "2024-03-01", true},
        {"anthropic", "claude-3-5-sonnet", "3.5", "2024-06-01", true},
        {"anthropic", "claude-4-sonnet", "4.0", "2024-10-01", true}
    };

    auto latest = ModelRegistry::select_latest(models);

    EXPECT_EQ(latest.model_id, "claude-4-sonnet");
    EXPECT_EQ(latest.version, "4.0");
}

TEST(ModelRegistryTest, SelectLatest_SameVersion_ByReleaseDate) {
    std::vector<ModelRegistry::ModelInfo> models = {
        {"anthropic", "claude-3-5-sonnet-20240320", "3.5", "2024-03-20", true},
        {"anthropic", "claude-3-5-sonnet-20241022", "3.5", "2024-10-22", true},
        {"anthropic", "claude-3-5-sonnet-20240601", "3.5", "2024-06-01", true}
    };

    auto latest = ModelRegistry::select_latest(models);

    EXPECT_EQ(latest.model_id, "claude-3-5-sonnet-20241022");
    EXPECT_EQ(latest.release_date, "2024-10-22");
}

// ============================================================================
// Test Suite 3: Registry Operations (4 tests)
// ============================================================================

TEST(ModelRegistryTest, AddAndGetLatestModel) {
    auto& registry = ModelRegistry::instance();

    ModelRegistry::ModelInfo model1{"openai", "gpt-4", "4.0", "2024-01-01", true};
    ModelRegistry::ModelInfo model2{"openai", "gpt-4-turbo", "4.1", "2024-06-01", true};

    registry.add_model(model1);
    registry.add_model(model2);

    auto latest = registry.get_latest_model("openai");

    EXPECT_EQ(latest.model_id, "gpt-4-turbo");
    EXPECT_EQ(latest.version, "4.1");
}

TEST(ModelRegistryTest, ValidateModel_Success) {
    auto& registry = ModelRegistry::instance();

    ModelRegistry::ModelInfo model{"cerebras", "llama3.1-8b", "1.0", "2024-07-01", true};
    registry.add_model(model);

    EXPECT_TRUE(registry.validate_model("cerebras", "llama3.1-8b"));
}

TEST(ModelRegistryTest, ValidateModel_NotFound) {
    auto& registry = ModelRegistry::instance();

    EXPECT_FALSE(registry.validate_model("cerebras", "nonexistent-model"));
}

TEST(ModelRegistryTest, GetModelsForProvider) {
    auto& registry = ModelRegistry::instance();

    ModelRegistry::ModelInfo model1{"anthropic", "claude-3-sonnet", "3.0", "2024-03-01", true};
    ModelRegistry::ModelInfo model2{"anthropic", "claude-3-5-sonnet", "3.5", "2024-06-01", true};

    registry.add_model(model1);
    registry.add_model(model2);

    auto models = registry.get_models_for_provider("anthropic");

    EXPECT_GE(models.size(), 2);  // At least 2 models (may have more from other tests)

    // Check that our models are present
    bool found_3_0 = false;
    bool found_3_5 = false;
    for (const auto& m : models) {
        if (m.model_id == "claude-3-sonnet") found_3_0 = true;
        if (m.model_id == "claude-3-5-sonnet") found_3_5 = true;
    }
    EXPECT_TRUE(found_3_0);
    EXPECT_TRUE(found_3_5);
}

// ============================================================================
// Test Suite 4: Caching (2 tests)
// ============================================================================

TEST(ModelRegistryTest, CacheAndLoad) {
    auto& registry = ModelRegistry::instance();

    std::map<std::string, ModelRegistry::ModelInfo> models_to_cache = {
        {"anthropic", {"anthropic", "claude-3-5-sonnet-20241022", "3.5", "2024-10-22", true}},
        {"openai", {"openai", "gpt-4-turbo", "4.1", "2024-06-01", true}}
    };

    registry.cache_model_selection(models_to_cache);

    // Load from cache
    auto loaded_models = registry.load_cached_models();

    EXPECT_EQ(loaded_models.size(), 2);
    EXPECT_EQ(loaded_models["anthropic"].model_id, "claude-3-5-sonnet-20241022");
    EXPECT_EQ(loaded_models["openai"].model_id, "gpt-4-turbo");
}

TEST(ModelRegistryTest, RefreshFromCache) {
    auto& registry = ModelRegistry::instance();

    // Cache some models
    std::map<std::string, ModelRegistry::ModelInfo> models_to_cache = {
        {"cerebras", {"cerebras", "llama3.1-70b", "2.0", "2024-08-01", true}}
    };
    registry.cache_model_selection(models_to_cache);

    // Refresh should load from cache
    registry.refresh_available_models();

    auto latest = registry.get_latest_model("cerebras");
    EXPECT_EQ(latest.model_id, "llama3.1-70b");
}

// ============================================================================
// Test Suite 5: Thread Safety (1 test)
// ============================================================================

TEST(ModelRegistryTest, ThreadSafety_ConcurrentAccess) {
    auto& registry = ModelRegistry::instance();

    // Create multiple threads that add models concurrently
    std::vector<std::thread> threads;

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&registry, i]() {
            ModelRegistry::ModelInfo model{
                "test-provider",
                "test-model-" + std::to_string(i),
                "1." + std::to_string(i),
                "2024-01-01",
                true
            };
            registry.add_model(model);
        });
    }

    // Wait for all threads
    for (auto& t : threads) {
        t.join();
    }

    // Verify all models were added
    auto models = registry.get_models_for_provider("test-provider");
    EXPECT_GE(models.size(), 10);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
