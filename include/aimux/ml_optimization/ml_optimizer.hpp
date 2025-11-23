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
#include <functional>
#include <nlohmann/json.hpp>
#include "aimux/metrics/metrics_collector.hpp"

namespace aimux {
namespace ml_optimization {

/**
 * @brief Types of feedback from users and systems
 */
enum class FeedbackType {
    POSITIVE,         // User explicitly liked the formatting
    NEGATIVE,         // User explicitly disliked the formatting
    CORRECTION,       // User provided corrected formatting
    PERFORMANCE_GOOD, // System measured good performance metrics
    PERFORMANCE_POOR, // System measured poor performance metrics
    AUTOMATIC,        // Automated feedback from system metrics
    EXPLICIT          // Explicit user feedback
};

/**
 * @brief Priority levels for ML training and model updates
 */
enum class TrainingPriority {
    IMMEDIATE,    // High priority - critical model updates
    HIGH,         // Important improvements
    NORMAL,       // Regular training cycle
    LOW,          // Background optimization
    MAINTENANCE   // Housekeeping and cleanup
};

/**
 * @brief Machine learning model types for optimization
 */
enum class ModelType {
    NEURAL_NETWORK,    // Deep learning model
    RANDOM_FOREST,     // Ensemble decision trees
    GRADIENT_BOOSTING, // Gradientboosted trees
    LINEAR_REGRESSION, // Linear statistical model
    CLUSTERING,        // Unsupervised clustering
    ENSEMBLE           // Combination of multiple models
};

/**
 * @brief Format optimization request
 */
struct OptimizationRequest {
    std::string request_id;
    std::string plugin_name;
    std::string provider_name;
    std::string model_name;
    std::string input_content;
    std::string original_format;
    std::string target_format;
    std::unordered_map<std::string, std::string> context;
    std::chrono::system_clock::time_point timestamp;

    nlohmann::json to_json() const;
    static OptimizationRequest from_json(const nlohmann::json& j);
};

/**
 * @brief Optimization result with confidence scores
 */
struct OptimizationResult {
    std::string request_id;
    std::string plugin_name;
    std::string optimized_content;
    std::string chosen_format;
    double confidence_score = 0.0;
    std::vector<std::string> alternative_formats;
    std::unordered_map<std::string, double> format_scores;

    // Prediction information
    double processing_time_prediction_ms = 0.0;
    double quality_score = 0.0;
    std::unordered_map<std::string, double> feature_importance;

    // Model information
    std::string model_version;
    std::chrono::system_clock::time_point model_trained_at;

    bool success = false;
    std::string error_message;
    std::chrono::milliseconds processing_time{0};

    nlohmann::json to_json() const;
    static OptimizationResult from_json(const nlohmann::json& j);
};

/**
 * @brief User feedback on formatting quality
 */
struct UserFeedback {
    std::string feedback_id;
    std::string request_id;
    std::string user_id;
    std::string session_id;
    FeedbackType type;
    std::string feedback_text;
    std::string corrected_content;  // For correction feedback
    std::vector<std::string> preferred_formats;
    std::unordered_map<std::string, double> quality_ratings;

    std::chrono::system_clock::time_point timestamp;
    std::unordered_map<std::string, std::string> metadata;

    nlohmann::json to_json() const;
    static UserFeedback from_json(const nlohmann::json& j);
};

/**
 * @brief ML model configuration and metadata
 */
struct ModelConfig {
    std::string model_id;
    std::string name;
    std::string description;
    ModelType type;
    std::string plugin_name;

    // Training parameters
    std::chrono::hours retraining_interval{24};
    size_t min_training_samples = 1000;
    size_t max_training_samples = 100000;
    double validation_split = 0.2;
    double early_stopping_patience = 5;

    // Performance targets
    double target_accuracy = 0.85;
    double target_precision = 0.80;
    double target_recall = 0.80;
    double max_inference_time_ms = 50.0;

    // Model architecture (simplified representation)
    nlohmann::json architecture_config;
    std::unordered_map<std::string, double> hyperparameters;

    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point last_trained;
    std::chrono::system_clock::time_point last_evaluated;

    // Training history
    std::vector<double> training_accuracy_history;
    std::vector<double> validation_accuracy_history;
    std::vector<std::chrono::system_clock::time_point> training_timestamps;

    nlohmann::json to_json() const;
    static ModelConfig from_json(const nlohmann::json& j);
};

/**
 * @brief Training dataset management
 */
class TrainingDataManager {
public:
    /**
     * @brief Data sample for training
     */
    struct TrainingSample {
        std::string input_content;
        std::string optimal_output;
        std::string plugin_name;
        std::string provider_name;
        std::vector<std::string> features;
        double quality_score = 0.0;
        std::unordered_map<std::string, std::vector<FeedbackType>> feedback_history;

        std::chrono::system_clock::time_point created_at;
    };

    explicit TrainingDataManager(const std::string& storage_path = "");
    virtual ~TrainingDataManager() = default;

    // Data management
    bool add_sample(const TrainingSample& sample);
    bool add_samples(const std::vector<TrainingSample>& samples);
    std::vector<TrainingSample> get_training_data(const std::string& plugin_name,
                                                  size_t limit = 10000) const;

    // Feedback integration
    bool incorporate_feedback(const UserFeedback& feedback);
    bool update_sample_quality(const std::string& sample_id, double new_quality);

    // Dataset statistics
    struct DatasetStats {
        size_t total_samples = 0;
        size_t samples_per_plugin = 0;
        double avg_quality_score = 0.0;
        std::chrono::system_clock::time_point oldest_sample;
        std::chrono::system_clock::time_point newest_sample;
    };

    DatasetStats get_dataset_stats(const std::string& plugin_name = "") const;

    // Data quality management
    bool cleanup_old_samples(std::chrono::hours max_age = std::chrono::hours{720}); // 30 days
    bool remove_duplicates();
    std::vector<TrainingSample> validate_data_quality() const;

    // Export/Import
    bool export_dataset(const std::string& file_path, const std::string& plugin_name = "") const;
    bool import_dataset(const std::string& file_path);

protected:
    std::unordered_map<std::string, std::vector<TrainingSample>> plugin_datasets_;
    std::string storage_path_;
    mutable std::mutex data_mutex_;

    virtual bool persist_data() const;
    virtual bool load_data();

private:
    std::string generate_sample_id(const TrainingSample& sample) const;
    double calculate_sample_quality(const TrainingSample& sample) const;
};

/**
 * @brief Abstract base class for ML optimization models
 */
class MLOptimizationModel {
public:
    explicit MLOptimizationModel(const ModelConfig& config);
    virtual ~MLOptimizationModel() = default;

    // Core optimization methods
    virtual OptimizationResult optimize_format(const OptimizationRequest& request) = 0;
    virtual bool train_model(const std::vector<TrainingDataManager::TrainingSample>& training_data,
                           const std::vector<TrainingDataManager::TrainingSample>& validation_data) = 0;

    // Model management
    virtual bool save_model(const std::string& model_path) const = 0;
    virtual bool load_model(const std::string& model_path) = 0;
    virtual nlohmann::json get_model_metadata() const = 0;

    // Evaluation metrics
    struct EvaluationResults {
        double accuracy = 0.0;
        double precision = 0.0;
        double recall = 0.0;
        double f1_score = 0.0;
        double mean_absolute_error = 0.0;
        double mean_squared_error = 0.0;
        double inference_time_ms = 0.0;
        double model_size_mb = 0.0;

        std::chrono::system_clock::time_point evaluated_at;
    };

 virtual EvaluationResults evaluate_model(const std::vector<TrainingDataManager::TrainingSample>& test_data) const = 0;

    // Feedback integration
    virtual bool incorporate_feedback(const UserFeedback& feedback) = 0;
    virtual bool update_model_incremental(const std::vector<UserFeedback>& feedback_batch) = 0;

    // Accessibility
    const ModelConfig& get_config() const { return config_; }
    bool is_trained() const { return model_trained_; }
    std::chrono::system_clock::time_point last_trained_at() const { return last_trained_; }

protected:
    ModelConfig config_;
    bool model_trained_ = false;
    std::chrono::system_clock::time_point last_trained_;

    // Feature extraction
    virtual std::vector<double> extract_features(const OptimizationRequest& request) const;
    virtual std::vector<double> extract_features_from_content(const std::string& content) const;

    // Utility methods
    std::vector<std::string> tokenize_content(const std::string& content) const;
    double calculate_text_similarity(const std::string& text1, const std::string& text2) const;
    std::unordered_map<std::string, double> calculate_content_statistics(const std::string& content) const;

private:
    mutable std::mutex model_mutex_;
};

/**
 * @brief Neural network implementation for format optimization
 */
class NeuralNetworkModel : public MLOptimizationModel {
public:
    explicit NeuralNetworkModel(const ModelConfig& config);
    ~NeuralNetworkModel() override = default;

    OptimizationResult optimize_format(const OptimizationRequest& request) override;
    bool train_model(const std::vector<TrainingDataManager::TrainingSample>& training_data,
                    const std::vector<TrainingDataManager::TrainingSample>& validation_data) override;

    bool save_model(const std::string& model_path) const override;
    bool load_model(const std::string& model_path) override;
    nlohmann::json get_model_metadata() const override;

    EvaluationResults evaluate_model(const std::vector<TrainingDataManager::TrainingSample>& test_data) const override;

    bool incorporate_feedback(const UserFeedback& feedback) override;
    bool update_model_incremental(const std::vector<UserFeedback>& feedback_batch) override;

private:
    // Simplified neural network representation
    struct NeuralNetwork {
        std::vector<std::vector<double>> weights;
        std::vector<std::vector<double>> biases;
        std::vector<int> layer_sizes;
    };

    NeuralNetwork network_;
    mutable std::mutex training_mutex_;

    // Neural network operations
    std::vector<double> forward_propagation(const std::vector<double>& input) const;
    void backward_propagation(const std::vector<double>& input,
                             const std::vector<double>& target,
                             double learning_rate = 0.01);
    double sigmoid(double x) const;
    double sigmoid_derivative(double x) const;
    std::vector<double> sigmoid_vector(const std::vector<double>& vec) const;

    // Training utilities
    std::vector<std::vector<double>> prepare_training_data(
        const std::vector<TrainingDataManager::TrainingSample>& samples) const;
    double calculate_loss(const std::vector<double>& predicted,
                         const std::vector<double>& target) const;
};

/**
 * @brief Ensemble model combining multiple models
 */
class EnsembleModel : public MLOptimizationModel {
public:
    explicit EnsembleModel(const ModelConfig& config);
    ~EnsembleModel() override = default;

    OptimizationResult optimize_format(const OptimizationRequest& request) override;
    bool train_model(const std::vector<TrainingDataManager::TrainingSample>& training_data,
                    const std::vector<TrainingDataManager::TrainingSample>& validation_data) override;

    bool save_model(const std::string& model_path) const override;
    bool load_model(const std::string& model_path) override;
    nlohmann::json get_model_metadata() const override;

    EvaluationResults evaluate_model(const std::vector<TrainingDataManager::TrainingSample>& test_data) const override;

    bool incorporate_feedback(const UserFeedback& feedback) override;
    bool update_model_incremental(const std::vector<UserFeedback>& feedback_batch) override;

    // Ensemble management
    void add_submodel(std::unique_ptr<MLOptimizationModel> model, double weight = 1.0);
    void remove_submodel(const std::string& model_id);
    void update_submodel_weight(const std::string& model_id, double new_weight);

    std::vector<std::string> get_submodel_ids() const;
    std::unordered_map<std::string, double> get_submodel_weights() const;

private:
    struct SubmodelInfo {
        std::unique_ptr<MLOptimizationModel> model;
        double weight;
        double recent_performance;
        std::chrono::system_clock::time_point last_used;
    };

    std::unordered_map<std::string, SubmodelInfo> submodels_;
    mutable std::mutex ensemble_mutex_;

    std::string select_best_model(const OptimizationRequest& request) const;
    void update_model_performance(const std::string& model_id, double performance);
    void rebalance_weights();
};

/**
 * @brief Main ML optimization system
 *
 * Orchestrates machine learning model training, deployment, and optimization
 * for prettifier plugins. Provides automated learning pipeline with
 * feedback collection and continuous improvement.
 */
class MLOptimizer {
public:
    /**
     * @brief Configuration for the ML optimizer
     */
    struct OptimizerConfig {
        std::string models_storage_path = "./models";
        std::string data_storage_path = "./training_data";

        std::chrono::seconds model_check_interval{300};   // 5 minutes
        std::chrono::hours data_cleanup_interval{24};     // 24 hours
        std::chrono::minutes feedback_batch_size{10};     // 10 minutes

        bool enable_auto_retraining = true;
        bool enable_incremental_learning = true;
        bool enable_model_ensembling = true;
        bool enable_feature_importance = true;

        size_t max_concurrent_models = 5;
        double min_model_performance = 0.7;
        TrainingPriority default_training_priority = TrainingPriority::NORMAL;

        // Resource limits
        size_t max_memory_usage_mb = 1024;
        double max_cpu_usage_percent = 70.0;
        std::chrono::seconds max_training_time{3600}; // 1 hour
    };

    explicit MLOptimizer(std::shared_ptr<aimux::metrics::MetricsCollector> metrics_collector,
                        const OptimizerConfig& config);

    ~MLOptimizer();

    // Core optimization methods
    OptimizationResult optimize_format(const OptimizationRequest& request);
    std::vector<OptimizationResult> optimize_batch(const std::vector<OptimizationRequest>& requests);

    // Model management
    bool register_model(const std::string& plugin_name, const ModelConfig& config);
    bool unregister_model(const std::string& plugin_name);

    std::optional<ModelConfig> get_model_config(const std::string& plugin_name) const;
    std::vector<std::string> list_registered_plugins() const;
    std::unordered_map<std::string, ModelConfig> get_all_model_configs() const;

    // Training control
    bool train_model(const std::string& plugin_name, TrainingPriority priority = TrainingPriority::NORMAL);
    bool retrain_all_models(TrainingPriority priority = TrainingPriority::NORMAL);
    bool cancel_training(const std::string& plugin_name);

    // Feedback integration
    bool record_feedback(const UserFeedback& feedback);
    bool record_feedback_batch(const std::vector<UserFeedback>& feedback_batch);

    // Performance monitoring
    struct ModelPerformance {
        std::string plugin_name;
        double accuracy = 0.0;
        double avg_inference_time_ms = 0.0;
        double success_rate = 0.0;
        double user_satisfaction_score = 0.0;

        std::chrono::system_clock::time_point last_updated;
        size_t total_optimizations = 0;
        size_t total_feedback = 0;

        std::vector<double> recent_accuracy_trend;
        std::vector<double> recent_performance_trend;
    };

    ModelPerformance get_model_performance(const std::string& plugin_name) const;
    std::unordered_map<std::string, ModelPerformance> get_all_model_performance() const;

    // Automated learning pipeline
    bool start_automated_learning();
    bool stop_automated_learning();
    bool is_learning_active() const { return learning_active_; }

    // Data management
    std::shared_ptr<TrainingDataManager> get_data_manager() const { return data_manager_; }
    size_t get_training_dataset_size(const std::string& plugin_name = "") const;

    // Configuration and status
    void update_config(const OptimizerConfig& config);
    OptimizerConfig get_config() const { return config_; }

    nlohmann::json get_status() const;
    nlohmann::json get_optimization_statistics() const;

    // Callbacks
    using OptimizationCallback = std::function<void(const OptimizationResult&)>;
    using TrainingCallback = std::function<void(const std::string&, bool)>;

    void set_optimization_callback(OptimizationCallback callback) { optimization_callback_ = callback; }
    void set_training_callback(TrainingCallback callback) { training_callback_ = callback; }

private:
    OptimizerConfig config_;
    std::shared_ptr<aimux::metrics::MetricsCollector> metrics_collector_;

    // Model registry
    std::unordered_map<std::string, std::unique_ptr<MLOptimizationModel>> registered_models_;
    std::unordered_map<std::string, ModelConfig> model_configs_;
    mutable std::mutex models_mutex_;

    // Training data management
    std::shared_ptr<TrainingDataManager> data_manager_;

    // Training queue and management
    struct TrainingTask {
        std::string plugin_name;
        TrainingPriority priority;
        std::chrono::system_clock::time_point scheduled_at;
        std::function<bool()> training_function;
    };

    std::priority_queue<TrainingTask> training_queue_;
    std::thread training_worker_;
    std::atomic<bool> training_active_{false};
    std::condition_variable training_cv_;
    std::mutex training_mutex_;

    // Automated learning
    std::thread learning_thread_;
    std::atomic<bool> learning_active_{false};

    // Feedback processing
    std::vector<UserFeedback> feedback_batch_;
    std::mutex feedback_mutex_;
    std::thread feedback_processor_;
    std::atomic<bool> feedback_processing_active_{false};

    // Callbacks
    OptimizationCallback optimization_callback_;
    TrainingCallback training_callback_;

    // Internal methods
    std::unique_ptr<MLOptimizationModel> create_model(const ModelConfig& config);
    bool load_or_create_model(const std::string& plugin_name, const ModelConfig& config);

    void run_training_loop();
    void run_learning_loop();
    void run_feedback_processing_loop();

    bool should_retrain_model(const std::string& plugin_name) const;
    std::vector<TrainingDataManager::TrainingSample> prepare_training_data(const std::string& plugin_name) const;

    void record_optimization_metrics(const OptimizationResult& result);
    void record_training_metrics(const std::string& plugin_name, bool success, double duration_ms);

    std::string generate_model_cache_path(const std::string& plugin_name) const;
    bool persist_model_state() const;
    bool load_model_state();
};

/**
 * @brief Factory for creating ML components
 */
class MLOptimizationFactory {
public:
    static std::unique_ptr<MLOptimizer> create_optimizer(
        std::shared_ptr<aimux::metrics::MetricsCollector> metrics_collector,
        const MLOptimizer::OptimizerConfig& config = MLOptimizer::OptimizerConfig{});

    static std::unique_ptr<MLOptimizationModel> create_model(const ModelConfig& config);
    static std::shared_ptr<TrainingDataManager> create_data_manager(const std::string& storage_path = "");

    // Register custom model types
    using ModelFactory = std::function<std::unique_ptr<MLOptimizationModel>(const ModelConfig&)>;
    static void register_model_type(ModelType type, ModelFactory factory);

    static std::vector<ModelType> list_available_model_types();
};

// Utility functions
namespace utils {
    std::vector<double> extract_text_features(const std::string& text);
    double calculate_content_complexity(const std::string& content);
    std::vector<std::string> detect_code_languages(const std::string& content);
    bool is_json_content(const std::string& content);
    bool is_markdown_content(const std::string& content);
}

} // namespace ml_optimization
} // namespace aimux