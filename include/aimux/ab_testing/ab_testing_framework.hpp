#pragma once

#include <chrono>
#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <random>
#include <nlohmann/json.hpp>
#include "aimux/metrics/metrics_collector.hpp"

namespace aimux {
namespace ab_testing {

/**
 * @brief Traffic splitting strategies for A/B testing
 */
enum class TrafficSplitStrategy {
    RANDOM,           // Random assignment based on percentages
    ROUND_ROBIN,      // Alternating assignment
    STICKY_SESSION,   // User-based consistent assignment
    HASH_BASED,       // Consistent hash of request attributes
    WEIGHTED_RANDOM   // Random with weighted percentages
};

/**
 * @brief Statistical test types
 */
enum class StatisticalTest {
    T_TEST,           // Two-sample t-test
    Z_TEST,           // Two-sample z-test
    CHI_SQUARE,       // Chi-square test
    MANN_WHITNEY,     // Mann-Whitney U test
    KOLMOGOROV_SMIRNOV // Kolmogorov-Smirnov test
};

/**
 * @brief Experiment status
 */
enum class ExperimentStatus {
    DRAFT,            // Not yet active
    RUNNING,          // Currently active
    PAUSED,           // Temporarily stopped
    COMPLETED,        // Finished with results
    ROLLED_BACK,      // Changes reverted
    TERMINATED        // Early termination
};

/**
 * @brief A/B test variant configuration
 */
struct TestVariant {
    std::string name;
    std::string description;
    double traffic_percentage = 0.0;
    nlohmann::json configuration;
    bool is_control = false;

    std::unordered_map<std::string, double> metrics_baseline;
    std::optional<nlohmann::json> plugin_config;

    nlohmann::json to_json() const;
    static TestVariant from_json(const nlohmann::json& j);
    bool validate() const;
};

/**
 * @brief A/B testing experiment definition
 */
struct Experiment {
    std::string id;
    std::string name;
    std::string description;
    ExperimentStatus status = ExperimentStatus::DRAFT;

    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point started_at;
    std::chrono::system_clock::time_point ended_at;
    std::optional<std::chrono::system_clock::time_point> planned_end_time;
    std::chrono::hours minimum_run_time{24}; // Minimum time before statistical significance
    std::chrono::hours maximum_run_time{168}; // Maximum run time (1 week)

    std::vector<TestVariant> variants;
    TrafficSplitStrategy split_strategy = TrafficSplitStrategy::RANDOM;
    std::vector<std::string> target_plugins;
    std::vector<std::string> target_providers;

    // Success criteria
    std::unordered_map<std::string, double> success_thresholds;
    std::string primary_metric;
    std::vector<std::string> secondary_metrics;

    // Rollback configuration
    bool auto_rollback_enabled = true;
    std::unordered_map<std::string, double> rollback_thresholds;
    std::chrono::minutes rollback_check_interval{10};
    std::chrono::minutes rollback_grace_period{30};

    // Statistical configuration
    double significance_level = 0.05;
    double statistical_power = 0.8;
    StatisticalTest test_type = StatisticalTest::T_TEST;

    std::unordered_map<std::string, std::string> metadata;

    nlohmann::json to_json() const;
    static Experiment from_json(const nlohmann::json& j);
    bool validate() const;
};

/**
 * @brief Experiment participation and assignment
 */
struct ParticipationAssignment {
    std::string experiment_id;
    std::string user_id;
    std::string session_id;
    std::string variant_name;
    std::chrono::system_clock::time_point assigned_at;
    std::unordered_map<std::string, std::string> assignment_context;
};

/**
 * @brief Result metrics for a variant
 */
struct VariantResults {
    std::string variant_name;
    std::string experiment_id;

    // Participation counts
    size_t total_participants = 0;
    size_t completed_sessions = 0;
    double completion_rate = 0.0;

    // Primary metrics
    double primary_metric_value = 0.0;
    double primary_metric_std_dev = 0.0;

    // Secondary metrics
    std::unordered_map<std::string, double> secondary_metrics;
    std::unordered_map<std::string, double> secondary_metrics_std_dev;

    // Statistical significance
    double p_value = 0.0;
    double confidence_interval_lower = 0.0;
    double confidence_interval_upper = 0.0;
    double effect_size = 0.0;
    bool statistically_significant = false;

    // Performance summary
    std::chrono::milliseconds avg_response_time{0};
    std::chrono::milliseconds p95_response_time{0};
    double success_rate = 0.0;
    double error_rate = 0.0;
    std::chrono::system_clock::time_point last_updated;

    nlohmann::json to_json() const;
};

/**
 * @brief Complete experiment results
 */
struct ExperimentResults {
    std::string experiment_id;
    std::chrono::system_clock::time_point generated_at;
    ExperimentStatus final_status;

    std::unordered_map<std::string, VariantResults> variant_results;
    std::string winning_variant;
    bool has_clear_winner = false;

    // Statistical analysis
    double overall_p_value = 0.0;
    double statistical_power_achieved = 0.0;
    std::unordered_map<std::string, double> multiple_comparison_adjustments;

    // Recommendations
    bool recommend_deploy = false;
    bool recommend_extend_experiment = false;
    std::string recommended_action;
    std::vector<std::string> concerns;

    // Quality metrics
    bool sample_size_adequate = false;
    bool test_assumptions_met = true;
    double data_quality_score = 0.0;

    nlohmann::json to_json() const;
};

/**
 * @brief Traffic splitter for A/B testing
 *
 * Routes incoming requests to appropriate test variants based on configured
 * splitting strategy. Supports various assignment methods and maintains
 * consistency for sticky sessions.
 */
class TrafficSplitter {
public:
    explicit TrafficSplitter(const Experiment& experiment);

    // Assignment methods
    std::string assign_variant(const std::string& user_id,
                             const std::string& session_id,
                             const std::unordered_map<std::string, std::string>& context = {});

    std::string assign_sticky_variant(const std::string& user_id);
    std::string assign_hash_based_variant(const std::unordered_map<std::string, std::string>& attributes);
    std::string assign_weighted_random_variant();

    // Management
    void update_experiment(const Experiment& experiment);
    Experiment get_experiment() const { return current_experiment_; }

    // Statistics
    std::unordered_map<std::string, size_t> get_assignment_counts() const;
    double get_split_accuracy() const;

private:
    Experiment current_experiment_;
    mutable std::mutex assignment_mutex_;

    // Assignment state
    std::unordered_map<std::string, std::string> user_assignments_;
    std::unordered_map<std::string, std::string> session_assignments_;
    std::atomic<size_t> round_robin_counter_{0};

    // Random number generation
    thread_local static std::mt19937 rng_;
    std::uniform_real_distribution<double> uniform_dist_{0.0, 1.0};

    std::string generate_assignment_hash(const std::string& input) const;
    std::vector<std::pair<std::string, double>> get_variant_weights() const;
};

/**
 * @brief Statistical analysis engine for A/B tests
 *
 * Performs statistical significance testing, power analysis, and
 * effect size calculation. Supports multiple statistical tests
 * and adapts to different data distributions.
 */
class StatisticalAnalyzer {
public:
    /**
     * @brief Result of statistical test
     */
    struct TestResult {
        double test_statistic = 0.0;
        double p_value = 0.0;
        double confidence_interval_lower = 0.0;
        double confidence_interval_upper = 0.0;
        double effect_size = 0.0;
        double power = 0.0;
        bool significant = false;
        std::string test_description;
    };

    // Statistical tests
    TestResult perform_t_test(const std::vector<double>& control_values,
                             const std::vector<double>& variant_values,
                             double alpha = 0.05);

    TestResult perform_z_test(double control_mean, double control_var, size_t control_n,
                             double variant_mean, double variant_var, size_t variant_n,
                             double alpha = 0.05);

    TestResult perform_chi_square_test(const std::vector<size_t>& control_counts,
                                      const std::vector<size_t>& variant_counts,
                                      double alpha = 0.05);

    TestResult perform_mann_whitney_test(const std::vector<double>& control_values,
                                        const std::vector<double>& variant_values,
                                        double alpha = 0.05);

    TestResult perform_kolmogorov_smirnov_test(const std::vector<double>& control_values,
                                              const std::vector<double>& variant_values,
                                              double alpha = 0.05);

    // Power analysis
    double calculate_required_sample_size(double effect_size,
                                         double alpha = 0.05,
                                         double power = 0.8,
                                         StatisticalTest test = StatisticalTest::T_TEST);

    double calculate_achieved_power(const std::vector<double>& control_values,
                                   const std::vector<double>& variant_values,
                                   double effect_size,
                                   double alpha = 0.05);

    // Multiple comparison correction
    std::unordered_map<std::string, double> apply_bonferroni_correction(
        const std::unordered_map<std::string, double>& p_values,
        double family_wise_alpha = 0.05);

    std::unordered_map<std::string, double> apply_false_discovery_rate_correction(
        const std::unordered_map<std::string, double>& p_values,
        double q_value = 0.05);

    // Effect size calculations
    double calculate_cohens_d(const std::vector<double>& control_values,
                             const std::vector<double>& variant_values);

    double calculate_cliffs_delta(const std::vector<double>& control_values,
                                 const std::vector<double>& variant_values);

private:
    double normal_cdf(double x) const;
    double inverse_normal_cdf(double p) const;

    std::vector<double> rank_values(const std::vector<double>& values) const;
    std::vector<double> remove_outliers(const std::vector<double>& values,
                                       double threshold = 1.5) const;
};

/**
 * @brief Main A/B testing framework
 *
 * Orchestrates experiment management, traffic splitting, data collection,
 * and statistical analysis. Provides comprehensive A/B testing capabilities
 * for prettifier plugins with automatic rollback and optimization suggestions.
 */
class ABTestingFramework {
public:
    explicit ABTestingFramework(std::shared_ptr<aimux::metrics::MetricsCollector> metrics_collector);
    ~ABTestingFramework();

    // Experiment management
    std::string create_experiment(const Experiment& experiment);
    bool update_experiment(const std::string& experiment_id, const Experiment& experiment);
    bool delete_experiment(const std::string& experiment_id);

    std::optional<Experiment> get_experiment(const std::string& experiment_id) const;
    std::vector<Experiment> list_experiments() const;
    std::vector<Experiment> list_active_experiments() const;

    // Experiment control
    bool start_experiment(const std::string& experiment_id);
    bool pause_experiment(const std::string& experiment_id);
    bool resume_experiment(const std::string& experiment_id);
    bool stop_experiment(const std::string& experiment_id);
    bool rollback_experiment(const std::string& experiment_id);

    // Participant assignment
    std::string get_variant_for_request(const std::string& user_id,
                                      const std::string& session_id,
                                      const std::unordered_map<std::string, std::string>& context = {});

    void record_participation(const std::string& experiment_id,
                            const std::string& user_id,
                            const std::string& session_id,
                            const std::string& variant_name);

    // Results and analysis
    ExperimentResults get_experiment_results(const std::string& experiment_id) const;
    std::unordered_map<std::string, VariantResults> get_variant_results(const std::string& experiment_id) const;

    // Statistical analysis
    StatisticalAnalyzer::TestResult compare_variants(const std::string& experiment_id,
                                                    const std::string& control_variant,
                                                    const std::string& test_variant) const;

    bool is_experiment_significant(const std::string& experiment_id) const;
    std::string get_winning_variant(const std::string& experiment_id) const;

    // Monitoring and alerts
    struct AlertConfig {
        double performance_degradation_threshold = 0.1;  // 10% degradation
        double error_rate_increase_threshold = 0.05;    // 5% increase
        double response_time_increase_threshold = 0.2;  // 20% increase
        std::chrono::minutes alert_check_interval{5};
        bool auto_pause_on_alert = true;
    };

    void set_alert_config(const AlertConfig& config);
    AlertConfig get_alert_config() const { return alert_config_; }

    struct FrameworkAlert {
        enum Severity { INFO, WARNING, ERROR, CRITICAL };
        Severity severity;
        std::string experiment_id;
        std::string variant_name;
        std::string metric_name;
        std::string message;
        double current_value;
        double threshold_value;
        std::chrono::system_clock::time_point timestamp;

        nlohmann::json to_json() const;
    };

    std::vector<FrameworkAlert> check_for_alerts() const;
    void clear_alerts();

    // Configuration
    struct FrameworkConfig {
        std::chrono::seconds results_update_interval{30};
        std::chrono::hours experiment_cleanup_delay{24};
        bool enable_auto_rollback = true;
        bool enable_statistical_monitoring = true;
        size_t max_concurrent_experiments = 10;
        std::string default_statistical_test = "t_test";
        double default_significance_level = 0.05;
        double default_statistical_power = 0.8;
    };

    void update_config(const FrameworkConfig& config);
    FrameworkConfig get_config() const { return config_; }

    // Status and health
    nlohmann::json get_status() const;
    nlohmann::json get_performance_metrics() const;

private:
    std::shared_ptr<aimux::metrics::MetricsCollector> metrics_collector_;
    FrameworkConfig config_;
    AlertConfig alert_config_;

    // Experiment storage
    mutable std::mutex experiments_mutex_;
    std::unordered_map<std::string, Experiment> experiments_;

    // Traffic splitters for active experiments
    mutable std::mutex splitters_mutex_;
    std::unordered_map<std::string, std::unique_ptr<TrafficSplitter>> traffic_splitters_;

    // Participation tracking
    mutable std::mutex participation_mutex_;
    std::unordered_map<std::string, std::vector<ParticipationAssignment>> experiment_participations_;

    // Alert management
    mutable std::mutex alerts_mutex_;
    std::vector<FrameworkAlert> recent_alerts_;
    std::unordered_map<std::string, std::chrono::system_clock::time_point> last_alert_times_;

    // Background processing
    std::thread monitoring_thread_;
    std::atomic<bool> monitoring_active_{false};

    // Statistical analysis
    std::unique_ptr<StatisticalAnalyzer> statistical_analyzer_;

    // Internal methods
    void start_monitoring();
    void stop_monitoring();
    void run_monitoring_loop();

    std::string generate_experiment_id() const;
    bool validate_experiment(const Experiment& experiment) const;
    bool can_start_experiment(const Experiment& experiment) const;

    void update_experiment_results(const std::string& experiment_id);
    void check_rollback_conditions(const std::string& experiment_id);

    std::vector<FrameworkAlert> check_experiment_alerts(const Experiment& experiment) const;
    bool should_trigger_rollback(const Experiment& experiment, const FrameworkAlert& alert) const;

    void cleanup_completed_experiments();

    // Metrics integration
    void record_experiment_metrics(const std::string& experiment_id, const std::string& variant_name);
    std::vector<double> extract_metric_values(const std::string& experiment_id,
                                            const std::string& variant_name,
                                            const std::string& metric_name,
                                            const std::chrono::system_clock::time_point& start,
                                            const std::chrono::system_clock::time_point& end) const;
};

/**
 * @brief Factory for creating A/B testing components
 */
class ABTestingFactory {
public:
    static std::unique_ptr<ABTestingFramework> create_framework(
        std::shared_ptr<aimux::metrics::MetricsCollector> metrics_collector);

    static std::unique_ptr<TrafficSplitter> create_splitter(const Experiment& experiment);
    static std::unique_ptr<StatisticalAnalyzer> create_analyzer();
};

// Utility functions
namespace utils {
    std::string generate_session_id();
    std::string hash_user_id(const std::string& user_id);
    bool is_statistically_significant(double p_value, double alpha = 0.05);
    double calculate_effect_size(double control_mean, double variant_mean, double pooled_std_dev);
    nlohmann::json create_experiment_summary(const Experiment& experiment, const ExperimentResults& results);
}

} // namespace ab_testing
} // namespace aimux