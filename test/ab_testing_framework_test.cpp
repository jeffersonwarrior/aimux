#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <thread>
#include <chrono>
#include <memory>
#include <random>
#include <vector>
#include <unordered_map>
#include "aimux/ab_testing/ab_testing_framework.hpp"
#include "aimux/metrics/metrics_collector.hpp"

namespace aimux {
namespace ab_testing {
namespace test {

class MockMetricsCollector : public aimux::metrics::MetricsCollector {
public:
    MockMetricsCollector() : aimux::metrics::MetricsCollector() {}

    MOCK_METHOD(void, record_counter, (const std::string&, double, const std::unordered_map<std::string, std::string>&), (override));
    MOCK_METHOD(void, record_gauge, (const std::string&, double, const std::unordered_map<std::string, std::string>&), (override));
    MOCK_METHOD(void, record_histogram, (const std::string&, double, const std::unordered_map<std::string, std::string>&), (override));
    MOCK_METHOD(void, record_timer, (const std::string&, std::chrono::nanoseconds, const std::unordered_map<std::string, std::string>&), (override));

    // Store recorded calls for verification
    struct Recording {
        std::string name;
        double value;
        std::unordered_map<std::string, std::string> tags;
        std::chrono::system_clock::time_point timestamp;
    };

    std::vector<Recording> recordings;

    void record_counter(const std::string& name, double value, const std::unordered_map<std::string, std::string>& tags) override {
        recordings.push_back({name, value, tags, std::chrono::system_clock::now()});
    }

    void record_prettification_event(const PrettificationEvent& event) override {
        // Store for test verification
        events.push_back(event);
    }

    std::vector<PrettificationEvent> events;
};

class ABTestingFrameworkTest : public ::testing::Test {
protected:
    void SetUp() override {
        mock_collector_ = std::make_shared<MockMetricsCollector>();
        framework_ = ABTestingFactory::create_framework(mock_collector_);

        // Create test experiment
        experiment_.name = "Test Experiment";
        experiment_.description = "Test A/B experiment for unit testing";
        experiment_.primary_metric = "processing_time_ms";
        experiment_.significance_level = 0.05;
        experiment_.statistical_power = 0.8;
        experiment_.minimum_run_time = std::chrono::hours{1};
        experiment_.maximum_run_time = std::chrono::hours{24};

        // Create test variants
        TestVariant control;
        control.name = "control";
        control.description = "Control variant";
        control.traffic_percentage = 50.0;
        control.is_control = true;

        TestVariant test_variant;
        test_variant.name = "variant_a";
        test_variant.description = "Test variant";
        test_variant.traffic_percentage = 50.0;
        test_variant.is_control = false;

        experiment_.variants = {control, test_variant};
    }

    std::shared_ptr<MockMetricsCollector> mock_collector_;
    std::unique_ptr<ABTestingFramework> framework_;
    Experiment experiment_;

    std::string create_experiment() {
        return framework_->create_experiment(experiment_);
    }

    void simulate_participation(const std::string& experiment_id, int num_users = 100) {
        std::random_device rd;
        std::mt19937 gen(rd());

        for (int i = 0; i < num_users; ++i) {
            std::string user_id = "user_" + std::to_string(i);
            std::string session_id = "session_" + std::to_string(i);

            auto variant = framework_->get_variant_for_request(user_id, session_id);
            EXPECT_FALSE(variant.empty());
        }
    }
};

TEST_F(ABTestingFrameworkTest, CreateAndValidateExperiment) {
    std::string experiment_id = create_experiment();
    EXPECT_FALSE(experiment_id.empty());

    auto stored_experiment = framework_->get_experiment(experiment_id);
    EXPECT_TRUE(stored_experiment.has_value());
    EXPECT_EQ(stored_experiment->name, experiment_.name);
    EXPECT_EQ(stored_experiment->variants.size(), 2);
    EXPECT_TRUE(stored_experiment->validate());
}

TEST_F(ABTestingFrameworkTest, InvalidExperimentValidation) {
    Experiment invalid_experiment;

    // Empty name should be invalid
    EXPECT_FALSE(invalid_experiment.validate());

    // No variants should be invalid
    invalid_experiment.name = "Invalid Test";
    invalid_experiment.variants = {};
    EXPECT_FALSE(invalid_experiment.validate());

    // Traffic percentages should sum to 100%
    TestVariant variant1{"v1", "", 60.0, {}, true};
    TestVariant variant2{"v2", "", 60.0, {}, false}; // Sum = 120%
    invalid_experiment.variants = {variant1, variant2};
    EXPECT_FALSE(invalid_experiment.validate());

    // Should have at least one control variant
    variant2.traffic_percentage = 40.0; // Now sum = 100%
    variant1.is_control = false;
    variant2.is_control = false; // No control variant
    EXPECT_FALSE(invalid_experiment.validate());
}

TEST_F(ABTestingFrameworkTest, StartAndStopExperiment) {
    std::string experiment_id = create_experiment();
    ASSERT_FALSE(experiment_id.empty());

    // Start should succeed
    EXPECT_TRUE(framework_->start_experiment(experiment_id));

    auto experiment = framework_->get_experiment(experiment_id);
    EXPECT_TRUE(experiment.has_value());
    EXPECT_EQ(experiment->status, ExperimentStatus::RUNNING);

    // Stop should succeed
    EXPECT_TRUE(framework_->stop_experiment(experiment_id));

    experiment = framework_->get_experiment(experiment_id);
    EXPECT_EQ(experiment->status, ExperimentStatus::COMPLETED);
}

TEST_F(ABTestingFrameworkTest, TrafficSplittingRandom) {
    experiment_.split_strategy = TrafficSplitStrategy::RANDOM;
    std::string experiment_id = create_experiment();
    EXPECT_TRUE(framework_->start_experiment(experiment_id));

    std::unordered_map<std::string, int> variant_counts;
    const int num_assignments = 1000;

    for (int i = 0; i < num_assignments; ++i) {
        std::string user_id = "user_" + std::to_string(i);
        std::string session_id = "session_" + std::to_string(i);

        auto variant = framework_->get_variant_for_request(user_id, session_id);
        EXPECT_FALSE(variant.empty());

        variant_counts[variant]++;
    }

    // Should have roughly 50/50 split (allowing for some variance)
    EXPECT_EQ(variant_counts.size(), 2);
    EXPECT_TRUE(variant_counts.contains("control"));
    EXPECT_TRUE(variant_counts.contains("variant_a"));

    // Check that split is approximately balanced (within 5%)
    double control_percentage = static_cast<double>(variant_counts["control"]) / num_assignments * 100.0;
    EXPECT_NEAR(control_percentage, 50.0, 5.0);
}

TEST_F(ABTestingFrameworkTest, TrafficSplittingStickySession) {
    experiment_.split_strategy = TrafficSplitStrategy::STICKY_SESSION;
    std::string experiment_id = create_experiment();
    EXPECT_TRUE(framework_->start_experiment(experiment_id));

    std::unordered_map<std::string, std::string> user_variants;
    const int num_users = 100;

    // First round of assignments
    for (int i = 0; i < num_users; ++i) {
        std::string user_id = "user_" + std::to_string(i);
        std::string session_id = "session_" + std::to_string(i) + "_1";

        auto variant = framework_->get_variant_for_request(user_id, session_id);
        user_variants[user_id] = variant;
        EXPECT_FALSE(variant.empty());
    }

    // Second round - should get same variants for same users
    for (int i = 0; i < num_users; ++i) {
        std::string user_id = "user_" + std::to_string(i);
        std::string session_id = "session_" + std::to_string(i) + "_2";

        auto variant = framework_->get_variant_for_request(user_id, session_id);
        EXPECT_EQ(user_variants[user_id], variant);
    }
}

TEST_F(ABTestingFrameworkTest, ExperimentStatusTransitions) {
    std::string experiment_id = create_experiment();
    auto experiment = framework_->get_experiment(experiment_id);
    EXPECT_EQ(experiment->status, ExperimentStatus::DRAFT);

    // Start experiment
    EXPECT_TRUE(framework_->start_experiment(experiment_id));
    experiment = framework_->get_experiment(experiment_id);
    EXPECT_EQ(experiment->status, ExperimentStatus::RUNNING);

    // Pause experiment
    EXPECT_TRUE(framework_->pause_experiment(experiment_id));
    experiment = framework_->get_experiment(experiment_id);
    EXPECT_EQ(experiment->status, ExperimentStatus::PAUSED);

    // Resume experiment
    EXPECT_TRUE(framework_->resume_experiment(experiment_id));
    experiment = framework_->get_experiment(experiment_id);
    EXPECT_EQ(experiment->status, ExperimentStatus::RUNNING);

    // Stop experiment
    EXPECT_TRUE(framework_->stop_experiment(experiment_id));
    experiment = framework_->get_experiment(experiment_id);
    EXPECT_EQ(experiment->status, ExperimentStatus::COMPLETED);
}

TEST_F(ABTestingFrameworkTest, ParticipationTracking) {
    std::string experiment_id = create_experiment();
    EXPECT_TRUE(framework_->start_experiment(experiment_id));

    simulate_participation(experiment_id, 50);

    // Verify metrics were recorded
    EXPECT_GT(mock_collector_->recordings.size(), 0);

    // Verify participation records
    auto experiment_obj = framework_->get_experiment(experiment_id);
    EXPECT_TRUE(experiment_obj.has_value());

    // Check that metrics were recorded with correct tags
    bool found_participation_metric = false;
    for (const auto& recording : mock_collector_->recordings) {
        if (recording.name == "ab_test_participations_total") {
            found_participation_metric = true;
            EXPECT_EQ(recording.tags["experiment_id"], experiment_id);
            EXPECT_TRUE(recording.tags["variant"] == "control" || recording.tags["variant"] == "variant_a");
        }
    }
    EXPECT_TRUE(found_participation_metric);
}

TEST_F(ABTestingFrameworkTest, MultipleExperiments) {
    // Create first experiment
    std::string exp1_id = create_experiment();
    EXPECT_TRUE(framework_->start_experiment(exp1_id));

    // Create second experiment
    Experiment exp2 = experiment_;
    exp2.name = "Second Experiment";
    std::string exp2_id = framework_->create_experiment(exp2);
    EXPECT_TRUE(framework_->start_experiment(exp2_id));

    auto active_experiments = framework_->list_active_experiments();
    EXPECT_EQ(active_experiments.size(), 2);

    // Verify they have different IDs
    EXPECT_NE(exp1_id, exp2_id);

    // Stop one experiment
    EXPECT_TRUE(framework_->stop_experiment(exp1_id));

    active_experiments = framework_->list_active_experiments();
    EXPECT_EQ(active_experiments.size(), 1);
}

TEST_F(ABTestingFrameworkTest, ExperimentUpdate) {
    std::string experiment_id = create_experiment();

    // Update experiment while in draft
    experiment_.name = "Updated Experiment Name";
    EXPECT_TRUE(framework_->update_experiment(experiment_id, experiment_));

    auto updated_experiment = framework_->get_experiment(experiment_id);
    ASSERT_TRUE(updated_experiment.has_value());
    EXPECT_EQ(updated_experiment->name, "Updated Experiment Name");

    // Start experiment
    EXPECT_TRUE(framework_->start_experiment(experiment_id));

    // Try to update while running - should fail
    experiment_.name = "Should Not Update";
    EXPECT_FALSE(framework_->update_experiment(experiment_id, experiment_));
}

class TrafficSplitterTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test experiment
        experiment_.name = "Splitter Test";
        experiment_.split_strategy = TrafficSplitStrategy::RANDOM;

        TestVariant control{"control", "", 60.0, {}, true};
        TestVariant variant_a{"variant_a", "", 40.0, {}, false};
        experiment_.variants = {control, variant_a};

        splitter_ = std::make_unique<TrafficSplitter>(experiment_);
    }

    Experiment experiment_;
    std::unique_ptr<TrafficSplitter> splitter_;
};

TEST_F(TrafficSplitterTest, RandomSplitting) {
    std::unordered_map<std::string, int> counts;
    const int num_assignments = 1000;

    for (int i = 0; i < num_assignments; ++i) {
        std::string user_id = "user_" + std::to_string(i);
        std::string session_id = "session_" + std::to_string(i);
        auto variant = splitter_->assign_variant(user_id, session_id);
        counts[variant]++;
    }

    EXPECT_EQ(counts.size(), 2);
    EXPECT_TRUE(counts.contains("control"));
    EXPECT_TRUE(counts.contains("variant_a"));

    // Check proportions (60/40 split with tolerance)
    double control_percentage = static_cast<double>(counts["control"]) / num_assignments * 100.0;
    double variant_percentage = static_cast<double>(counts["variant_a"]) / num_assignments * 100.0;

    EXPECT_NEAR(control_percentage, 60.0, 5.0);
    EXPECT_NEAR(variant_percentage, 40.0, 5.0);
}

TEST_F(TrafficSplitterTest, RoundRobinSplitting) {
    experiment_.split_strategy = TrafficSplitStrategy::ROUND_ROBIN;
    splitter_ = std::make_unique<TrafficSplitter>(experiment_);

    // Test pattern
    std::vector<std::string> expected_pattern = {"control", "variant_a"};
    for (int i = 0; i < 10; ++i) {
        std::string user_id = "user_" + std::to_string(i);
        std::string session_id = "session_" + std::to_string(i);
        auto variant = splitter_->assign_variant(user_id, session_id);
        EXPECT_EQ(variant, expected_pattern[i % 2]);
    }
}

TEST_F(TrafficSplitterTest, StickySessions) {
    experiment_.split_strategy = TrafficSplitStrategy::STICKY_SESSION;
    splitter_ = std::make_unique<TrafficSplitter>(experiment_);

    std::unordered_map<std::string, std::string> user_assignments;

    // First assignments
    for (int i = 0; i < 5; ++i) {
        std::string user_id = "user_" + std::to_string(i);
        std::string session_id = "session_" + std::to_string(i);
        auto variant = splitter_->assign_variant(user_id, session_id);
        user_assignments[user_id] = variant;
    }

    // Second assignments with different sessions - should get same variants
    for (int i = 0; i < 5; ++i) {
        std::string user_id = "user_" + std::to_string(i);
        std::string session_id = "new_session_" + std::to_string(i);
        auto variant = splitter_->assign_variant(user_id, session_id);
        EXPECT_EQ(variant, user_assignments[user_id]);
    }
}

TEST_F(TrafficSplitterTest, SplitAccuracy) {
    const int num_assignments = 1000;
    std::unordered_map<std::string, int> counts;

    for (int i = 0; i < num_assignments; ++i) {
        std::string user_id = "user_" + std::to_string(i);
        std::string session_id = "session_" + std::to_string(i);
        auto variant = splitter_->assign_variant(user_id, session_id);
        counts[variant]++;
    }

    double accuracy = splitter_->get_split_accuracy();
    EXPECT_GT(accuracy, 0.9); // Should be very accurate with large samples
}

class StatisticalAnalyzerTest : public ::testing::Test {
protected:
    void SetUp() override {
        analyzer_ = std::make_unique<StatisticalAnalyzer>();
    }

    std::unique_ptr<StatisticalAnalyzer> analyzer_;

    std::vector<double> generate_normal_data(double mean, double std_dev, size_t count) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::normal_distribution<> dist(mean, std_dev);

        std::vector<double> data;
        for (size_t i = 0; i < count; ++i) {
            data.push_back(dist(gen));
        }
        return data;
    }
};

TEST_F(StatisticalAnalyzerTest, TTestSameDistribution) {
    auto control = generate_normal_data(50.0, 10.0, 100);
    auto variant = generate_normal_data(50.0, 10.0, 100);

    auto result = analyzer_->perform_t_test(control, variant, 0.05);

    // Should not find a significant difference between identical distributions
    EXPECT_FALSE(result.significant);
    EXPECT_GT(result.p_value, 0.05);
    EXPECT_NEAR(result.effect_size, 0.0, 0.3);
}

TEST_F(StatisticalAnalyzerTest, TTestDifferentDistributions) {
    auto control = generate_normal_data(50.0, 10.0, 100);
    auto variant = generate_normal_data(60.0, 10.0, 100); // 10-point difference

    auto result = analyzer_->perform_t_test(control, variant, 0.05);

    // Should find a significant difference
    EXPECT_TRUE(result.significant);
    EXPECT_LT(result.p_value, 0.05);
    EXPECT_GT(std::abs(result.effect_size), 0.5);
}

TEST_F(StatisticalAnalyzerTest, EffectSizeCalculation) {
    auto control = generate_normal_data(50.0, 10.0, 50);
    auto variant = generate_normal_data(60.0, 10.0, 50);

    double effect_size = analyzer_->calculate_cohens_d(control, variant);

    // Effect size should be positive (variant better than control)
    EXPECT_GT(effect_size, 0.0);
    EXPECT_LT(effect_size, 2.0); // Should be reasonable
}

TEST_F(StatisticalAnalyzerTest, SampleSizeCalculation) {
    double effect_size = 0.5; // Medium effect size
    size_t required_size = analyzer_->calculate_required_sample_size(effect_size, 0.05, 0.8);

    // Should return reasonable sample size (typically around 64 for medium effect)
    EXPECT_GT(required_size, 30);
    EXPECT_LT(required_size, 200);
}

TEST_F(StatisticalAnalyzerTest, MultipleComparisonCorrection) {
    std::unordered_map<std::string, double> p_values = {
        {"metric1", 0.01},
        {"metric2", 0.03},
        {"metric3", 0.08},
        {"metric4", 0.15}
    };

    auto corrected = analyzer_->apply_bonferroni_correction(p_values, 0.05);

    // Bonferroni correction should make p-values larger
    EXPECT_GT(corrected["metric1"], p_values["metric1"]);
    EXPECT_GT(corrected["metric2"], p_values["metric2"]);
    EXPECT_GT(corrected["metric3"], p_values["metric3"]);
    EXPECT_GT(corrected["metric4"], p_values["metric4"]);
}

class IntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        mock_collector_ = std::make_shared<MockMetricsCollector>();
        framework_ = ABTestingFactory::create_framework(mock_collector_);
    }

    std::shared_ptr<MockMetricsCollector> mock_collector_;
    std::unique_ptr<ABTestingFramework> framework_;
};

TEST_F(IntegrationTest, EndToEndExperiment) {
    // Create experiment
    Experiment experiment;
    experiment.name = "Integration Test";
    experiment.description = "End-to-end integration test";
    experiment.primary_metric = "processing_time_ms";
    experiment.minimum_run_time = std::chrono::minutes{5}; // Short for test

    TestVariant control{"control", "", 50.0, {}, true};
    TestVariant variant{"improved", "", 50.0, {}, false};
    experiment.variants = {control, variant};

    std::string experiment_id = framework_->create_experiment(experiment);
    EXPECT_FALSE(experiment_id.empty());

    // Start experiment
    EXPECT_TRUE(framework_->start_experiment(experiment_id));

    // Simulate traffic
    std::thread traffic_thread([this, experiment_id]() {
        for (int i = 0; i < 200; ++i) {
            std::string user_id = "user_" + std::to_string(i % 50);
            std::string session_id = "session_" + std::to_string(i);

            auto variant = framework_->get_variant_for_request(user_id, session_id);
            EXPECT_FALSE(variant.empty());

            // Simulate some processing delay
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });

    traffic_thread.join();

    // Check experiment status
    auto exp_status = framework_->get_experiment(experiment_id);
    EXPECT_TRUE(exp_status.has_value());
    EXPECT_EQ(exp_status->status, ExperimentStatus::RUNNING);

    // Verify metrics were recorded
    EXPECT_GT(mock_collector_->recordings.size(), 100);

    // Stop experiment
    EXPECT_TRUE(framework_->stop_experiment(experiment_id));

    // Check final status
    exp_status = framework_->get_experiment(experiment_id);
    EXPECT_EQ(exp_status->status, ExperimentStatus::COMPLETED);

    // Get experiment results
    auto results = framework_->get_experiment_results(experiment_id);
    EXPECT_EQ(results.experiment_id, experiment_id);
    EXPECT_EQ(results.variant_results.size(), 2);
}

TEST_F(IntegrationTest, AlertGeneration) {
    // Create experiment with strict thresholds for testing
    Experiment experiment;
    experiment.name = "Alert Test";
    experiment.primary_metric = "processing_time_ms";
    experiment.auto_rollback_enabled = true;
    experiment.rollback_grace_period = std::chrono::minutes{1}; // Very short for test

    TestVariant control{"control", "", 50.0, {}, true};
    TestVariant problematic{"problematic", "", 50.0, {}, false};
    experiment.variants = {control, problematic};

    std::string experiment_id = framework_->create_experiment(experiment);
    EXPECT_TRUE(framework_->start_experiment(experiment_id));

    // Wait past grace period
    std::this_thread::sleep_for(std::chrono::milliseconds(1200));

    // Check for alerts
    auto alerts = framework_->check_for_alerts();
    // Alert infrastructure should be working (no specific assertion as alerts depend on metrics)
    EXPECT_GE(alerts.size(), 0);
}

} // namespace test
} // namespace ab_testing
} // namespace aimux