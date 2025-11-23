#include "aimux/ab_testing/ab_testing_framework.hpp"
#include "aimux/metrics/metrics_collector.hpp"
#include <algorithm>
#include <cmath>
#include <random>
#include <sstream>
#include <iomanip>
#include <numeric>
#include <future>
#include <unordered_set>

namespace aimux {
namespace ab_testing {

// TestVariant implementation
nlohmann::json TestVariant::to_json() const {
    nlohmann::json j;
    j["name"] = name;
    j["description"] = description;
    j["traffic_percentage"] = traffic_percentage;
    j["configuration"] = configuration;
    j["is_control"] = is_control;
    j["metrics_baseline"] = metrics_baseline;
    if (plugin_config) {
        j["plugin_config"] = *plugin_config;
    }
    return j;
}

TestVariant TestVariant::from_json(const nlohmann::json& j) {
    TestVariant variant;
    variant.name = j.at("name").get<std::string>();
    variant.description = j.value("description", "");
    variant.traffic_percentage = j.at("traffic_percentage").get<double>();
    variant.configuration = j.value("configuration", nlohmann::json::object());
    variant.is_control = j.value("is_control", false);
    variant.metrics_baseline = j.value("metrics_baseline", std::unordered_map<std::string, double>{});

    if (j.contains("plugin_config")) {
        variant.plugin_config = j.at("plugin_config");
    }

    return variant;
}

bool TestVariant::validate() const {
    if (name.empty()) return false;
    if (traffic_percentage < 0.0 || traffic_percentage > 100.0) return false;
    if (traffic_percentage == 0.0 && !is_control) return false;
    return true;
}

// Experiment implementation
nlohmann::json Experiment::to_json() const {
    nlohmann::json j;
    j["id"] = id;
    j["name"] = name;
    j["description"] = description;
    j["status"] = static_cast<int>(status);

    j["created_at"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        created_at.time_since_epoch()).count();
    j["started_at"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        started_at.time_since_epoch()).count();
    j["ended_at"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        ended_at.time_since_epoch()).count();

    if (planned_end_time) {
        j["planned_end_time"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            planned_end_time->time_since_epoch()).count();
    }

    j["minimum_run_time_hours"] = minimum_run_time.count();
    j["maximum_run_time_hours"] = maximum_run_time.count();

    nlohmann::json variants_json;
    for (const auto& variant : variants) {
        variants_json.push_back(variant.to_json());
    }
    j["variants"] = variants_json;

    j["split_strategy"] = static_cast<int>(split_strategy);
    j["target_plugins"] = target_plugins;
    j["target_providers"] = target_providers;

    j["success_thresholds"] = success_thresholds;
    j["primary_metric"] = primary_metric;
    j["secondary_metrics"] = secondary_metrics;

    j["auto_rollback_enabled"] = auto_rollback_enabled;
    j["rollback_thresholds"] = rollback_thresholds;
    j["rollback_check_interval_minutes"] = rollback_check_interval.count();
    j["rollback_grace_period_minutes"] = rollback_grace_period.count();

    j["significance_level"] = significance_level;
    j["statistical_power"] = statistical_power;
    j["test_type"] = static_cast<int>(test_type);

    j["metadata"] = metadata;

    return j;
}

Experiment Experiment::from_json(const nlohmann::json& j) {
    Experiment experiment;
    experiment.id = j.at("id").get<std::string>();
    experiment.name = j.at("name").get<std::string>();
    experiment.description = j.value("description", "");
    experiment.status = static_cast<ExperimentStatus>(j.at("status").get<int>());

    experiment.created_at = std::chrono::system_clock::from_time_t(
        j.at("created_at").get<int64_t>() / 1000);
    experiment.started_at = std::chrono::system_clock::from_time_t(
        j.at("started_at").get<int64_t>() / 1000);
    experiment.ended_at = std::chrono::system_clock::from_time_t(
        j.at("ended_at").get<int64_t>() / 1000);

    if (j.contains("planned_end_time")) {
        experiment.planned_end_time = std::chrono::system_clock::from_time_t(
            j.at("planned_end_time").get<int64_t>() / 1000);
    }

    experiment.minimum_run_time = std::chrono::hours{
        j.value("minimum_run_time_hours", 24)};
    experiment.maximum_run_time = std::chrono::hours{
        j.value("maximum_run_time_hours", 168)};

    for (const auto& variant_json : j.at("variants")) {
        experiment.variants.push_back(TestVariant::from_json(variant_json));
    }

    experiment.split_strategy = static_cast<TrafficSplitStrategy>(
        j.value("split_strategy", static_cast<int>(TrafficSplitStrategy::RANDOM)));
    experiment.target_plugins = j.value("target_plugins", std::vector<std::string>{});
    experiment.target_providers = j.value("target_providers", std::vector<std::string>{});

    experiment.success_thresholds = j.value("success_thresholds", std::unordered_map<std::string, double>{});
    experiment.primary_metric = j.value("primary_metric", "");
    experiment.secondary_metrics = j.value("secondary_metrics", std::vector<std::string>{});

    experiment.auto_rollback_enabled = j.value("auto_rollback_enabled", true);
    experiment.rollback_thresholds = j.value("rollback_thresholds", std::unordered_map<std::string, double>{});
    experiment.rollback_check_interval = std::chrono::minutes{
        j.value("rollback_check_interval_minutes", 10)};
    experiment.rollback_grace_period = std::chrono::minutes{
        j.value("rollback_grace_period_minutes", 30)};

    experiment.significance_level = j.value("significance_level", 0.05);
    experiment.statistical_power = j.value("statistical_power", 0.8);
    experiment.test_type = static_cast<StatisticalTest>(
        j.value("test_type", static_cast<int>(StatisticalTest::T_TEST)));

    experiment.metadata = j.value("metadata", std::unordered_map<std::string, std::string>{});

    return experiment;
}

bool Experiment::validate() const {
    if (id.empty() || name.empty()) return false;
    if (variants.empty()) return false;

    // Check that at least one variant is marked as control
    bool has_control = std::any_of(variants.begin(), variants.end(),
        [](const TestVariant& v) { return v.is_control; });
    if (!has_control) return false;

    // Validate traffic percentages sum to 100%
    double total_percentage = std::accumulate(variants.begin(), variants.end(), 0.0,
        [](double sum, const TestVariant& v) { return sum + v.traffic_percentage; });
    if (std::abs(total_percentage - 100.0) > 0.01) return false;

    // Validate each variant
    for (const auto& variant : variants) {
        if (!variant.validate()) return false;
    }

    // Validate configuration
    if (significance_level <= 0.0 || significance_level >= 1.0) return false;
    if (statistical_power <= 0.0 || statistical_power >= 1.0) return false;
    if (primary_metric.empty()) return false;

    return true;
}

// VariantResults implementation
nlohmann::json VariantResults::to_json() const {
    nlohmann::json j;
    j["variant_name"] = variant_name;
    j["experiment_id"] = experiment_id;

    j["total_participants"] = total_participants;
    j["completed_sessions"] = completed_sessions;
    j["completion_rate"] = completion_rate;

    j["primary_metric_value"] = primary_metric_value;
    j["primary_metric_std_dev"] = primary_metric_std_dev;

    j["secondary_metrics"] = secondary_metrics;
    j["secondary_metrics_std_dev"] = secondary_metrics_std_dev;

    j["p_value"] = p_value;
    j["confidence_interval_lower"] = confidence_interval_lower;
    j["confidence_interval_upper"] = confidence_interval_upper;
    j["effect_size"] = effect_size;
    j["statistically_significant"] = statistically_significant;

    j["avg_response_time_ms"] = avg_response_time.count();
    j["p95_response_time_ms"] = p95_response_time.count();
    j["success_rate"] = success_rate;
    j["error_rate"] = error_rate;

    j["last_updated"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        last_updated.time_since_epoch()).count();

    return j;
}

// ExperimentResults implementation
nlohmann::json ExperimentResults::to_json() const {
    nlohmann::json j;
    j["experiment_id"] = experiment_id;
    j["generated_at"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        generated_at.time_since_epoch()).count();
    j["final_status"] = static_cast<int>(final_status);

    nlohmann::json variant_results_json;
    for (const auto& [name, results] : variant_results) {
        variant_results_json[name] = results.to_json();
    }
    j["variant_results"] = variant_results_json;

    j["winning_variant"] = winning_variant;
    j["has_clear_winner"] = has_clear_winner;

    j["overall_p_value"] = overall_p_value;
    j["statistical_power_achieved"] = statistical_power_achieved;
    j["multiple_comparison_adjustments"] = multiple_comparison_adjustments;

    j["recommend_deploy"] = recommend_deploy;
    j["recommend_extend_experiment"] = recommend_extend_experiment;
    j["recommended_action"] = recommended_action;
    j["concerns"] = concerns;

    j["sample_size_adequate"] = sample_size_adequate;
    j["test_assumptions_met"] = test_assumptions_met;
    j["data_quality_score"] = data_quality_score;

    return j;
}

// FrameworkAlert implementation
nlohmann::json ABTestingFramework::FrameworkAlert::to_json() const {
    nlohmann::json j;
    j["severity"] = static_cast<int>(severity);
    j["experiment_id"] = experiment_id;
    j["variant_name"] = variant_name;
    j["metric_name"] = metric_name;
    j["message"] = message;
    j["current_value"] = current_value;
    j["threshold_value"] = threshold_value;
    j["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        timestamp.time_since_epoch()).count();
    return j;
}

// TrafficSplitter implementation
thread_local std::mt19937 TrafficSplitter::rng_(std::random_device{}());

TrafficSplitter::TrafficSplitter(const Experiment& experiment)
    : current_experiment_(experiment) {}

std::string TrafficSplitter::assign_variant(const std::string& user_id,
                                          const std::string& session_id,
                                          const std::unordered_map<std::string, std::string>& context) {
    std::lock_guard<std::mutex> lock(assignment_mutex_);

    switch (current_experiment_.split_strategy) {
        case TrafficSplitStrategy::RANDOM:
        case TrafficSplitStrategy::WEIGHTED_RANDOM:
            return assign_weighted_random_variant();

        case TrafficSplitStrategy::ROUND_ROBIN:
            {
                auto& variant = current_experiment_.variants[round_robin_counter_ % current_experiment_.variants.size()];
                round_robin_counter_++;
                return variant.name;
            }

        case TrafficSplitStrategy::STICKY_SESSION:
            return assign_sticky_variant(user_id);

        case TrafficSplitStrategy::HASH_BASED:
            return assign_hash_based_variant(context);

        default:
            return assign_weighted_random_variant();
    }
}

std::string TrafficSplitter::assign_sticky_variant(const std::string& user_id) {
    auto it = user_assignments_.find(user_id);
    if (it != user_assignments_.end()) {
        return it->second;
    }

    // New assignment
    std::string variant = assign_weighted_random_variant();
    user_assignments_[user_id] = variant;
    return variant;
}

std::string TrafficSplitter::assign_hash_based_variant(const std::unordered_map<std::string, std::string>& attributes) {
    if (attributes.empty()) {
        return assign_weighted_random_variant();
    }

    // Create a deterministic hash from attributes
    std::stringstream ss;
    for (const auto& [key, value] : attributes) {
        ss << key << "=" << value << "&";
    }
    std::string hash_input = ss.str();
    std::string hash = generate_assignment_hash(hash_input);

    // Use hash to select variant
    auto weights = get_variant_weights();
    double hash_value = std::stoul(hash.substr(0, 8), nullptr, 16) / static_cast<double>(0xFFFFFFFF);
    double cumulative = 0.0;

    for (const auto& [variant_name, weight] : weights) {
        cumulative += weight;
        if (hash_value <= cumulative) {
            return variant_name;
        }
    }

    return weights.back().first;
}

std::string TrafficSplitter::assign_weighted_random_variant() {
    auto weights = get_variant_weights();
    double random_value = uniform_dist_(rng_);
    double cumulative = 0.0;

    for (const auto& [variant_name, weight] : weights) {
        cumulative += weight;
        if (random_value <= cumulative) {
            return variant_name;
        }
    }

    return weights.back().first;
}

void TrafficSplitter::update_experiment(const Experiment& experiment) {
    std::lock_guard<std::mutex> lock(assignment_mutex_);
    current_experiment_ = experiment;
}

std::unordered_map<std::string, size_t> TrafficSplitter::get_assignment_counts() const {
    std::lock_guard<std::mutex> lock(assignment_mutex_);
    std::unordered_map<std::string, size_t> counts;

    for (const auto& [user_id, variant] : user_assignments_) {
        counts[variant]++;
    }

    return counts;
}

double TrafficSplitter::get_split_accuracy() const {
    auto actual_counts = get_assignment_counts();
    auto expected_weights = get_variant_weights();
    std::transform(expected_weights.begin(), expected_weights.end(), expected_weights.begin(),
        [](auto pair) { return std::make_pair(pair.first, pair.second / 100.0); });

    size_t total_assignments = std::accumulate(actual_counts.begin(), actual_counts.end(), 0,
        [](size_t sum, const auto& pair) { return sum + pair.second; });

    if (total_assignments == 0) return 1.0;

    double total_variance = 0.0;
    for (const auto& [variant, weight] : expected_weights) {
        double expected_ratio = weight;
        double actual_ratio = static_cast<double>(actual_counts[variant]) / total_assignments;
        double variance = std::abs(expected_ratio - actual_ratio);
        total_variance += variance;
    }

    return 1.0 - (total_variance / expected_weights.size());
}

std::string TrafficSplitter::generate_assignment_hash(const std::string& input) const {
    // Simple hash implementation - in production, use a proper hash function
    std::hash<std::string> hasher;
    size_t hash_value = hasher(input);
    std::stringstream ss;
    ss << std::hex << hash_value;
    return ss.str();
}

std::vector<std::pair<std::string, double>> TrafficSplitter::get_variant_weights() const {
    std::vector<std::pair<std::string, double>> weights;
    for (const auto& variant : current_experiment_.variants) {
        weights.emplace_back(variant.name, variant.traffic_percentage);
    }
    return weights;
}

// StatisticalAnalyzer implementation
StatisticalAnalyzer::TestResult StatisticalAnalyzer::perform_t_test(
    const std::vector<double>& control_values,
    const std::vector<double>& variant_values,
    double alpha) {

    TestResult result;
    result.test_description = "Two-sample t-test";

    if (control_values.size() < 2 || variant_values.size() < 2) {
        result.significant = false;
        result.p_value = 1.0;
        return result;
    }

    // Calculate means
    double control_mean = std::accumulate(control_values.begin(), control_values.end(), 0.0) / control_values.size();
    double variant_mean = std::accumulate(variant_values.begin(), variant_values.end(), 0.0) / variant_values.size();

    // Calculate variances
    double control_var = 0.0, variant_var = 0.0;
    for (double val : control_values) {
        control_var += (val - control_mean) * (val - control_mean);
    }
    for (double val : variant_values) {
        variant_var += (val - variant_mean) * (val - variant_mean);
    }
    control_var /= (control_values.size() - 1);
    variant_var /= (variant_values.size() - 1);

    // Calculate t-statistic
    double pooled_variance = ((control_values.size() - 1) * control_var + (variant_values.size() - 1) * variant_var) /
                           (control_values.size() + variant_values.size() - 2);
    double standard_error = std::sqrt(pooled_variance * (1.0 / control_values.size() + 1.0 / variant_values.size()));
    result.test_statistic = (variant_mean - control_mean) / standard_error;

    // Degrees of freedom
    int df = control_values.size() + variant_values.size() - 2;

    // Calculate p-value (simplified - would use t-distribution in production)
    result.p_value = 2.0 * (1.0 - normal_cdf(std::abs(result.test_statistic)));
    result.confidence_interval_lower = variant_mean - 1.96 * standard_error;
    result.confidence_interval_upper = variant_mean + 1.96 * standard_error;

    result.effect_size = (variant_mean - control_mean) / std::sqrt(pooled_variance);
    result.power = calculate_achieved_power(control_values, variant_values, result.effect_size, alpha);
    result.significant = result.p_value < alpha;

    return result;
}

StatisticalAnalyzer::TestResult StatisticalAnalyzer::perform_z_test(
    double control_mean, double control_var, size_t control_n,
    double variant_mean, double variant_var, size_t variant_n,
    double alpha) {

    TestResult result;
    result.test_description = "Two-sample z-test";

    if (control_n < 30 || variant_n < 30) {
        result.significant = false;
        result.p_value = 1.0;
        return result;
    }

    double standard_error = std::sqrt(control_var / control_n + variant_var / variant_n);
    result.test_statistic = (variant_mean - control_mean) / standard_error;

    result.p_value = 2.0 * (1.0 - normal_cdf(std::abs(result.test_statistic)));
    result.confidence_interval_lower = variant_mean - 1.96 * standard_error;
    result.confidence_interval_upper = variant_mean + 1.96 * standard_error;

    double pooled_std_dev = std::sqrt((control_var + variant_var) / 2.0);
    result.effect_size = (variant_mean - control_mean) / pooled_std_dev;

    result.significant = result.p_value < alpha;

    return result;
}

StatisticalAnalyzer::TestResult StatisticalAnalyzer::perform_chi_square_test(
    const std::vector<size_t>& control_counts,
    const std::vector<size_t>& variant_counts,
    double alpha) {

    TestResult result;
    result.test_description = "Chi-square test";

    if (control_counts.size() != variant_counts.size() || control_counts.empty()) {
        result.significant = false;
        result.p_value = 1.0;
        return result;
    }

    // Calculate expected counts
    std::vector<double> expected_counts;
    size_t total_control = std::accumulate(control_counts.begin(), control_counts.end(), 0);
    size_t total_variant = std::accumulate(variant_counts.begin(), variant_counts.end(), 0);
    size_t grand_total = total_control + total_variant;

    for (size_t i = 0; i < control_counts.size(); ++i) {
        double row_total = control_counts[i] + variant_counts[i];
        expected_counts.push_back(row_total * total_variant / static_cast<double>(grand_total));
    }

    // Calculate chi-square statistic
    double chi_square = 0.0;
    for (size_t i = 0; i < variant_counts.size(); ++i) {
        if (expected_counts[i] > 0) {
            chi_square += std::pow(variant_counts[i] - expected_counts[i], 2) / expected_counts[i];
        }
    }

    result.test_statistic = chi_square;

    // Calculate p-value (simplified - would use chi-square distribution in production)
    result.p_value = 1.0 - normal_cdf(chi_square);
    result.significant = result.p_value < alpha;

    return result;
}

double StatisticalAnalyzer::calculate_required_sample_size(double effect_size,
                                                         double alpha,
                                                         double power,
                                                         StatisticalTest test) {
    // Simplified sample size calculation
    if (test == StatisticalTest::T_TEST) {
        double z_alpha = inverse_normal_cdf(1.0 - alpha / 2.0);
        double z_beta = inverse_normal_cdf(power);
        return 2.0 * std::pow(z_alpha + z_beta, 2) / (effect_size * effect_size);
    }
    return 100.0; // Default fallback
}

double StatisticalAnalyzer::calculate_achieved_power(const std::vector<double>& control_values,
                                                   const std::vector<double>& variant_values,
                                                   double effect_size,
                                                   double alpha) {
    size_t n = std::min(control_values.size(), variant_values.size());
    double z_alpha = inverse_normal_cdf(1.0 - alpha / 2.0);
    double z = std::abs(effect_size) * std::sqrt(n / 2.0) - z_alpha;
    return normal_cdf(z);
}

double StatisticalAnalyzer::calculate_cohens_d(const std::vector<double>& control_values,
                                               const std::vector<double>& variant_values) {
    if (control_values.empty() || variant_values.empty()) return 0.0;

    double control_mean = std::accumulate(control_values.begin(), control_values.end(), 0.0) / control_values.size();
    double variant_mean = std::accumulate(variant_values.begin(), variant_values.end(), 0.0) / variant_values.size();

    // Calculate pooled standard deviation
    double control_var = 0.0, variant_var = 0.0;
    for (double val : control_values) {
        control_var += (val - control_mean) * (val - control_mean);
    }
    for (double val : variant_values) {
        variant_var += (val - variant_mean) * (val - variant_mean);
    }

    control_var /= (control_values.size() - 1);
    variant_var /= (variant_values.size() - 1);

    double pooled_std_dev = std::sqrt(((control_values.size() - 1) * control_var + (variant_values.size() - 1) * variant_var) /
                                    (control_values.size() + variant_values.size() - 2));

    return pooled_std_dev > 0 ? (variant_mean - control_mean) / pooled_std_dev : 0.0;
}

double StatisticalAnalyzer::normal_cdf(double x) const {
    // Approximation of normal CDF
    return 0.5 * (1.0 + std::erf(x / std::sqrt(2.0)));
}

double StatisticalAnalyzer::inverse_normal_cdf(double p) const {
    // Approximation of inverse normal CDF (Beasley-Springer-Moro algorithm)
    if (p <= 0.0 || p >= 1.0) return 0.0;

    static const double a[4] = {2.50662823884, -18.61500062529, 41.39119773534, -25.44106049637};
    static const double b[4] = {-8.47351093090, 23.08336743743, -21.06224101826, 3.13082909833};
    static const double c[9] = {0.3374754822726147, 0.9761690190917186, 0.1607979714918209,
                               0.0276438810333863, 0.0038405729373609, 0.0003951896511919,
                               0.0000321767881768, 0.0000002888167364, 0.0000003960315187};

    double y = p - 0.5;
    if (std::abs(y) < 0.42) {
        double r = y * y;
        double x = y * (((a[3] * r + a[2]) * r + a[1]) * r + a[0]) /
                   ((((b[3] * r + b[2]) * r + b[1]) * r + b[0]) * r + 1.0);
        return x;
    } else {
        double r = (y < 0) ? p : (1.0 - p);
        r = std::sqrt(-std::log(r));
        double x = (((c[8] * r + c[7]) * r + c[6]) * r + c[5]) * r + c[4];
        x = (((c[3] * r + c[2]) * r + c[1]) * r + c[0]) / x;
        return (y < 0) ? -x : x;
    }
}

// ABTestingFramework implementation
ABTestingFramework::ABTestingFramework(std::shared_ptr<aimux::metrics::MetricsCollector> metrics_collector)
    : metrics_collector_(metrics_collector),
      statistical_analyzer_(std::make_unique<StatisticalAnalyzer>()) {
    start_monitoring();
}

ABTestingFramework::~ABTestingFramework() {
    stop_monitoring();
}

std::string ABTestingFramework::create_experiment(const Experiment& experiment) {
    if (!validate_experiment(experiment)) {
        return "";
    }

    std::string experiment_id = generate_experiment_id();
    Experiment new_experiment = experiment;
    new_experiment.id = experiment_id;
    new_experiment.created_at = std::chrono::system_clock::now();

    {
        std::lock_guard<std::mutex> lock(experiments_mutex_);
        experiments_[experiment_id] = new_experiment;
    }

    // Create traffic splitter
    {
        std::lock_guard<std::mutex> lock(splitters_mutex_);
        traffic_splitters_[experiment_id] = std::make_unique<TrafficSplitter>(new_experiment);
    }

    return experiment_id;
}

bool ABTestingFramework::update_experiment(const std::string& experiment_id, const Experiment& experiment) {
    if (!validate_experiment(experiment)) {
        return false;
    }

    std::lock_guard<std::mutex> lock(experiments_mutex_);
    auto it = experiments_.find(experiment_id);
    if (it == experiments_.end()) {
        return false;
    }

    // Only allow updates to draft or paused experiments
    if (it->second.status == ExperimentStatus::RUNNING) {
        return false;
    }

    Experiment updated_experiment = experiment;
    updated_experiment.id = experiment_id;
    updated_experiment.created_at = it->second.created_at;
    experiments_[experiment_id] = updated_experiment;

    // Update traffic splitter
    {
        std::lock_guard<std::mutex> splitters_lock(splitters_mutex_);
        traffic_splitters_[experiment_id]->update_experiment(updated_experiment);
    }

    return true;
}

bool ABTestingFramework::delete_experiment(const std::string& experiment_id) {
    // Can only delete completed or terminated experiments
    {
        std::lock_guard<std::mutex> lock(experiments_mutex_);
        auto it = experiments_.find(experiment_id);
        if (it == experiments_.end()) {
            return false;
        }

        if (it->second.status == ExperimentStatus::RUNNING ||
            it->second.status == ExperimentStatus::PAUSED) {
            return false;
        }

        experiments_.erase(it);
    }

    // Clean up associated data
    {
        std::lock_guard<std::mutex> lock(splitters_mutex_);
        traffic_splitters_.erase(experiment_id);
    }

    {
        std::lock_guard<std::mutex> lock(participation_mutex_);
        experiment_participations_.erase(experiment_id);
    }

    return true;
}

std::optional<Experiment> ABTestingFramework::get_experiment(const std::string& experiment_id) const {
    std::lock_guard<std::mutex> lock(experiments_mutex_);
    auto it = experiments_.find(experiment_id);
    if (it != experiments_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<Experiment> ABTestingFramework::list_experiments() const {
    std::lock_guard<std::mutex> lock(experiments_mutex_);
    std::vector<Experiment> experiments;
    for (const auto& [id, experiment] : experiments_) {
        experiments.push_back(experiment);
    }
    return experiments;
}

std::vector<Experiment> ABTestingFramework::list_active_experiments() const {
    std::lock_guard<std::mutex> lock(experiments_mutex_);
    std::vector<Experiment> active_experiments;
    for (const auto& [id, experiment] : experiments_) {
        if (experiment.status == ExperimentStatus::RUNNING) {
            active_experiments.push_back(experiment);
        }
    }
    return active_experiments;
}

bool ABTestingFramework::start_experiment(const std::string& experiment_id) {
    std::lock_guard<std::mutex> lock(experiments_mutex_);
    auto it = experiments_.find(experiment_id);
    if (it == experiments_.end()) {
        return false;
    }

    if (!can_start_experiment(it->second)) {
        return false;
    }

    it->second.status = ExperimentStatus::RUNNING;
    it->second.started_at = std::chrono::system_clock::now();

    if (!it->second.planned_end_time) {
        it->second.planned_end_time = it->second.started_at + it->second.maximum_run_time;
    }

    return true;
}

std::string ABTestingFramework::get_variant_for_request(const std::string& user_id,
                                                      const std::string& session_id,
                                                      const std::unordered_map<std::string, std::string>& context) {
    // Find active experiments that match the request context
    std::vector<std::pair<std::string, Experiment>> matching_experiments;

    {
        std::lock_guard<std::mutex> lock(experiments_mutex_);
        for (const auto& [id, experiment] : experiments_) {
            if (experiment.status == ExperimentStatus::RUNNING) {
                matching_experiments.emplace_back(id, experiment);
            }
        }
    }

    // Use the first matching experiment (could implement prioritization logic)
    if (!matching_experiments.empty()) {
        auto [experiment_id, experiment] = matching_experiments[0];

        std::lock_guard<std::mutex> splitters_lock(splitters_mutex_);
        auto splitter_it = traffic_splitters_.find(experiment_id);
        if (splitter_it != traffic_splitters_.end()) {
            std::string variant = splitter_it->second->assign_variant(user_id, session_id, context);
            record_participation(experiment_id, user_id, session_id, variant);
            return variant;
        }
    }

    return ""; // No experiment
}

void ABTestingFramework::record_participation(const std::string& experiment_id,
                                            const std::string& user_id,
                                            const std::string& session_id,
                                            const std::string& variant_name) {
    ParticipationAssignment assignment;
    assignment.experiment_id = experiment_id;
    assignment.user_id = user_id;
    assignment.session_id = session_id;
    assignment.variant_name = variant_name;
    assignment.assigned_at = std::chrono::system_clock::now();

    {
        std::lock_guard<std::mutex> lock(participation_mutex_);
        experiment_participations_[experiment_id].push_back(assignment);
    }

    // Record metrics
    if (metrics_collector_) {
        metrics_collector_->record_counter("ab_test_participations_total", 1.0,
                                         {
                                             {"experiment_id", experiment_id},
                                             {"variant", variant_name}
                                         });
    }
}

std::string ABTestingFramework::generate_experiment_id() const {
    static std::atomic<size_t> counter{0};
    size_t value = counter++;
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

    std::stringstream ss;
    ss << "exp_" << timestamp << "_" << std::setfill('0') << std::setw(6) << value;
    return ss.str();
}

bool ABTestingFramework::validate_experiment(const Experiment& experiment) const {
    return experiment.validate();
}

bool ABTestingFramework::can_start_experiment(const Experiment& experiment) const {
    if (experiment.status != ExperimentStatus::DRAFT && experiment.status != ExperimentStatus::PAUSED) {
        return false;
    }

    if (experiment.variants.empty()) {
        return false;
    }

    return true;
}

void ABTestingFramework::run_monitoring_loop() {
    while (monitoring_active_) {
        try {
            // Check for alerts
            {
                std::lock_guard<std::mutex> lock(experiments_mutex_);
                for (const auto& [id, experiment] : experiments_) {
                    if (experiment.status == ExperimentStatus::RUNNING) {
                        auto alerts = check_experiment_alerts(experiment);
                        for (const auto& alert : alerts) {
                            if (config_.enable_auto_rollback && should_trigger_rollback(experiment, alert)) {
                                rollback_experiment(id);
                            }
                        }
                    }
                }
            }

            // Update experiment results
            {
                std::lock_guard<std::mutex> lock(experiments_mutex_);
                for (const auto& [id, experiment] : experiments_) {
                    if (experiment.status == ExperimentStatus::RUNNING) {
                        update_experiment_results(id);
                        check_rollback_conditions(id);
                    }
                }
            }

            // Cleanup old experiments
            cleanup_completed_experiments();

        } catch (const std::exception& e) {
            // Log error but continue monitoring
        }

        std::this_thread::sleep_for(std::chrono::seconds{10});
    }
}

void ABTestingFramework::start_monitoring() {
    if (monitoring_active_) return;

    monitoring_active_ = true;
    monitoring_thread_ = std::thread(&ABTestingFramework::run_monitoring_loop, this);
}

void ABTestingFramework::stop_monitoring() {
    if (!monitoring_active_) return;

    monitoring_active_ = false;
    if (monitoring_thread_.joinable()) {
        monitoring_thread_.join();
    }
}

void ABTestingFramework::update_experiment_results(const std::string& experiment_id) {
    // This would update results by querying metrics
    // Implementation would depend on the specific metrics available
}

std::vector<ABTestingFramework::FrameworkAlert>
ABTestingFramework::check_experiment_alerts(const Experiment& experiment) const {
    std::vector<FrameworkAlert> alerts;
    auto now = std::chrono::system_clock::now();

    // Check if we're past the grace period
    if (now - experiment.started_at < experiment.rollback_grace_period) {
        return alerts;
    }

    // This would implement specific alert checking logic
    // For now, return empty alerts

    return alerts;
}

bool ABTestingFramework::should_trigger_rollback(const Experiment& experiment, const FrameworkAlert& alert) const {
    return alert.severity >= FrameworkAlert::ERROR && experiment.auto_rollback_enabled;
}

void ABTestingFramework::cleanup_completed_experiments() {
    auto now = std::chrono::system_clock::now();
    std::vector<std::string> to_remove;

    {
        std::lock_guard<std::mutex> lock(experiments_mutex_);
        for (const auto& [id, experiment] : experiments_) {
            if ((experiment.status == ExperimentStatus::COMPLETED ||
                 experiment.status == ExperimentStatus::ROLLED_BACK ||
                 experiment.status == ExperimentStatus::TERMINATED)) {
                if (now - experiment.ended_at > config_.experiment_cleanup_delay) {
                    to_remove.push_back(id);
                }
            }
        }
    }

    for (const auto& id : to_remove) {
        delete_experiment(id);
    }
}

nlohmann::json ABTestingFramework::get_status() const {
    nlohmann::json status;
    status["monitoring_active"] = monitoring_active_.load();
    status["total_experiments"] = experiments_.size();

    {
        std::lock_guard<std::mutex> lock(experiments_mutex_);
        size_t running_count = 0, completed_count = 0, failed_count = 0;
        for (const auto& [id, experiment] : experiments_) {
            switch (experiment.status) {
                case ExperimentStatus::RUNNING: running_count++; break;
                case ExperimentStatus::COMPLETED: completed_count++; break;
                case ExperimentStatus::ROLLED_BACK:
                case ExperimentStatus::TERMINATED: failed_count++; break;
                default: break;
            }
        }
        status["running_experiments"] = running_count;
        status["completed_experiments"] = completed_count;
        status["failed_experiments"] = failed_count;
    }

    {
        std::lock_guard<std::mutex> lock(alerts_mutex_);
        status["recent_alerts"] = recent_alerts_.size();
    }

    return status;
}

// Utility functions
namespace utils {
    std::string generate_session_id() {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(0, 15);

        std::stringstream ss;
        for (int i = 0; i < 32; ++i) {
            ss << std::hex << dis(gen);
        }
        return "sess_" + ss.str();
    }

    std::string hash_user_id(const std::string& user_id) {
        std::hash<std::string> hasher;
        size_t hash_value = hasher(user_id);
        std::stringstream ss;
        ss << std::hex << hash_value;
        return ss.str();
    }

    bool is_statistically_significant(double p_value, double alpha) {
        return p_value < alpha;
    }

    double calculate_effect_size(double control_mean, double variant_mean, double pooled_std_dev) {
        return pooled_std_dev > 0 ? (variant_mean - control_mean) / pooled_std_dev : 0.0;
    }

    nlohmann::json create_experiment_summary(const Experiment& experiment, const ExperimentResults& results) {
        nlohmann::json summary;
        summary["experiment"] = experiment.to_json();
        summary["results"] = results.to_json();
        return summary;
    }
}

// ABTestingFactory implementation
std::unique_ptr<ABTestingFramework> ABTestingFactory::create_framework(
    std::shared_ptr<aimux::metrics::MetricsCollector> metrics_collector) {
    return std::make_unique<ABTestingFramework>(metrics_collector);
}

std::unique_ptr<TrafficSplitter> ABTestingFactory::create_splitter(const Experiment& experiment) {
    return std::make_unique<TrafficSplitter>(experiment);
}

std::unique_ptr<StatisticalAnalyzer> ABTestingFactory::create_analyzer() {
    return std::make_unique<StatisticalAnalyzer>();
}

} // namespace ab_testing
} // namespace aimux