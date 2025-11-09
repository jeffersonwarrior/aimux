"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.FailoverManager = exports.ErrorCategory = void 0;
const types_1 = require("../providers/types");
const logger_1 = require("../utils/logger");
const routing_utils_1 = require("./routing-utils");
/**
 * Error classification for retry decisions
 */
var ErrorCategory;
(function (ErrorCategory) {
    /** Temporary network issues, server overload */
    ErrorCategory["RETRYABLE"] = "retryable";
    /** Rate limiting, temporary provider issues */
    ErrorCategory["TEMPORARY"] = "temporary";
    /** Authentication, invalid requests */
    ErrorCategory["CLIENT_ERROR"] = "client_error";
    /** Permanent provider issues, outages */
    ErrorCategory["PERMANENT"] = "permanent";
    /** Unknown error type */
    ErrorCategory["UNKNOWN"] = "unknown";
})(ErrorCategory || (exports.ErrorCategory = ErrorCategory = {}));
/**
 * Circuit breaker state
 */
var CircuitState;
(function (CircuitState) {
    CircuitState["CLOSED"] = "closed";
    CircuitState["OPEN"] = "open";
    CircuitState["HALF_OPEN"] = "half_open";
})(CircuitState || (CircuitState = {}));
/**
 * Failover manager for handling provider outages and intelligent retry logic
 * Implements circuit breaker pattern, exponential backoff, and provider health monitoring
 */
class FailoverManager {
    providerRegistry;
    routingEngine;
    logger = (0, logger_1.getLogger)();
    config;
    circuitBreakers = new Map();
    providerFailureHistory = new Map();
    constructor(providerRegistry, routingEngine, config = {}) {
        this.providerRegistry = providerRegistry;
        this.routingEngine = routingEngine;
        this.config = {
            maxRetriesPerProvider: 3,
            maxTotalRetries: 10,
            initialRetryDelay: 1000,
            maxRetryDelay: 10000,
            backoffMultiplier: 2,
            enableJitter: true,
            jitterFactor: 0.1,
            enableCircuitBreaker: true,
            circuitBreakerThreshold: 5,
            circuitBreakerTimeout: 60000, // 1 minute
            healthCheckInterval: 5000, // 5 seconds
            enableIntelligentFailover: true,
            ...config,
        };
        this.logger.info('FailoverManager initialized', {
            maxRetriesPerProvider: this.config.maxRetriesPerProvider,
            maxTotalRetries: this.config.maxTotalRetries,
            enableCircuitBreaker: this.config.enableCircuitBreaker,
            enableIntelligentFailover: this.config.enableIntelligentFailover,
        });
    }
    /**
     * Handle failover for a failed AI request
     * @param request The original AI request
     * @param failedProviders Array of provider IDs that have already failed
     * @param originalError The error that triggered the failover
     * @returns Promise resolving to AI response or throwing if all providers fail
     */
    async handleFailover(request, failedProviders = [], originalError) {
        const startTime = Date.now();
        const attempts = [];
        const requirements = (0, routing_utils_1.analyzeRequestRequirements)(request);
        this.logger.info('Starting failover process', {
            requestId: this.generateRequestId(),
            requestType: requirements.type,
            failedProviders,
            originalError: originalError?.message,
        });
        // Get failed provider error categories
        if (originalError) {
            const errorCategory = this.classifyError(originalError);
            this.logger.debug('Classified original error', {
                errorCategory,
                errorMessage: originalError.message,
            });
            // If error is non-retryable, fail immediately
            if (errorCategory === ErrorCategory.CLIENT_ERROR ||
                errorCategory === ErrorCategory.PERMANENT) {
                throw new types_1.ProviderError(`Non-retryable error: ${originalError.message}`, 'unknown', undefined, originalError);
            }
        }
        try {
            let response = null;
            let attemptCount = 0;
            const currentFailedProviders = [...failedProviders];
            while (attemptCount < this.config.maxTotalRetries && !response) {
                attemptCount++;
                // Select next provider for failover
                const nextProvider = this.selectFailoverProvider(request, requirements, currentFailedProviders, attempts, attemptCount);
                if (!nextProvider) {
                    this.logger.warn('No suitable providers available for failover', {
                        attemptCount,
                        failedProviders: currentFailedProviders,
                    });
                    break;
                }
                // Check circuit breaker
                if (this.config.enableCircuitBreaker && this.isCircuitOpen(nextProvider.id)) {
                    this.logger.debug('Provider circuit breaker is open, skipping', {
                        providerId: nextProvider.id,
                        providerName: nextProvider.name,
                    });
                    currentFailedProviders.push(nextProvider.id);
                    continue;
                }
                // Calculate retry delay
                const retryDelay = this.calculateRetryDelay(attemptCount, attempts);
                const attempt = {
                    attemptNumber: attemptCount,
                    providerId: nextProvider.id,
                    providerName: nextProvider.name,
                    retryDelay,
                    startTime: Date.now(),
                    success: false,
                };
                attempts.push(attempt);
                // Wait before retry (except for first attempt)
                if (attemptCount > 1 && retryDelay > 0) {
                    this.logger.debug('Waiting before retry', {
                        attemptNumber: attemptCount,
                        providerId: nextProvider.id,
                        delay: retryDelay,
                    });
                    await this.sleep(retryDelay);
                }
                try {
                    this.logger.info('Attempting provider failover', {
                        attemptNumber: attemptCount,
                        providerId: nextProvider.id,
                        providerName: nextProvider.name,
                    });
                    // Make the request
                    response = await this.makeProviderRequest(nextProvider, request);
                    // Success!
                    attempt.success = true;
                    attempt.duration = Date.now() - attempt.startTime;
                    // Mark success for circuit breaker
                    if (this.config.enableCircuitBreaker) {
                        this.recordProviderSuccess(nextProvider.id);
                    }
                    // Update routing engine with success metrics
                    this.routingEngine.updateProviderPerformance(nextProvider.id, attempt.duration, true);
                    this.logger.info('Failover successful', {
                        attemptNumber: attemptCount,
                        providerId: nextProvider.id,
                        providerName: nextProvider.name,
                        responseTime: attempt.duration,
                        totalFailoverTime: Date.now() - startTime,
                    });
                    // Mark response metadata
                    response.metadata = {
                        ...response.metadata,
                        fallback_used: true,
                        routing_decision: `failover:${nextProvider.id}`,
                        failover_attempts: attemptCount,
                    };
                }
                catch (error) {
                    const providerError = error instanceof types_1.ProviderError
                        ? error
                        : new types_1.ProviderError(error instanceof Error ? error.message : String(error), nextProvider.id, undefined, error);
                    attempt.error = providerError;
                    attempt.success = false;
                    attempt.duration = Date.now() - attempt.startTime;
                    attempt.errorCategory = this.classifyError(providerError);
                    this.logger.warn('Provider failover attempt failed', {
                        attemptNumber: attemptCount,
                        providerId: nextProvider.id,
                        providerName: nextProvider.name,
                        errorMessage: providerError.message,
                        errorCategory: attempt.errorCategory,
                        responseTime: attempt.duration,
                    });
                    // Record failure for circuit breaker
                    if (this.config.enableCircuitBreaker) {
                        this.recordProviderFailure(nextProvider.id, providerError);
                    }
                    // Update routing engine with failure metrics
                    this.routingEngine.updateProviderPerformance(nextProvider.id, attempt.duration, false, attempt.errorCategory);
                    // Add to failed providers list
                    currentFailedProviders.push(nextProvider.id);
                    // Check if error is non-retryable for this provider
                    if (attempt.errorCategory === ErrorCategory.CLIENT_ERROR ||
                        attempt.errorCategory === ErrorCategory.PERMANENT) {
                        this.logger.info('Skipping further retries for this provider due to non-retryable error', {
                            providerId: nextProvider.id,
                            errorCategory: attempt.errorCategory,
                        });
                        continue;
                    }
                }
            }
            if (!response) {
                const failoverError = new types_1.ProviderError(`All providers failed after ${attemptCount} failover attempts. Last error: ${attempts[attempts.length - 1]?.error?.message || 'Unknown error'}`, 'failover-manager', undefined, attempts[attempts.length - 1]?.error);
                this.logger.error('Failover failed - all providers exhausted', {
                    totalAttempts: attemptCount,
                    totalFailoverTime: Date.now() - startTime,
                    attempts: attempts.map(a => ({
                        providerId: a.providerId,
                        success: a.success,
                        errorCategory: a.errorCategory,
                        retryDelay: a.retryDelay,
                    })),
                });
                throw failoverError;
            }
            return response;
        }
        catch (error) {
            this.logger.error('Critical error in failover manager', {
                error: error instanceof Error ? error.message : String(error),
                totalFailoverTime: Date.now() - startTime,
                attempts: attempts.length,
            });
            throw error;
        }
    }
    /**
     * Select the next provider for failover
     */
    selectFailoverProvider(request, requirements, failedProviders, previousAttempts, attemptCount) {
        if (this.config.enableIntelligentFailover) {
            return this.selectIntelligentProvider(request, requirements, failedProviders, previousAttempts);
        }
        else {
            return this.selectFallbackProvider(request, requirements, failedProviders);
        }
    }
    /**
     * Intelligent provider selection considering performance, health, and error patterns
     */
    selectIntelligentProvider(request, requirements, failedProviders, previousAttempts) {
        // Get all available providers
        const availableProviders = this.providerRegistry
            .getProvidersForRequest(request)
            .filter(provider => !failedProviders.includes(provider.id));
        if (availableProviders.length === 0) {
            return null;
        }
        // Score each provider based on various factors
        const scoredProviders = availableProviders.map(provider => {
            let score = 0;
            // Base compatibility score
            score += (0, routing_utils_1.calculateProviderCompatibility)(provider, requirements);
            // Health status bonus/penalty
            const health = provider.getLastHealthCheck();
            if (health) {
                if (health.status === 'healthy')
                    score += 5;
                else if (health.status === 'degraded')
                    score += 2;
                else
                    score -= 5;
            }
            // Circuit breaker penalty
            if (this.isCircuitOpen(provider.id)) {
                score -= 20;
            }
            // Recent failure penalty
            const recentFailures = this.getRecentFailureCount(provider.id);
            score -= recentFailures * 2;
            // Error type analysis - penalize providers with similar recent errors
            const similarErrorPatterns = this.getSimilarErrorPatternCount(provider.id, previousAttempts);
            score -= similarErrorPatterns * 3;
            // Performance metrics bonus
            const performanceMetrics = this.routingEngine.getProviderPerformanceMetrics()[provider.id];
            if (performanceMetrics) {
                score += (performanceMetrics.successRate / 100) * 2;
                score += Math.max(0, 3 - performanceMetrics.averageResponseTime / 1000);
            }
            // Provider priority bonus
            const config = provider.getConfig();
            if (config?.priority) {
                score += config.priority / 10;
            }
            return { provider, score };
        });
        // Sort by score (highest first) and return the best
        scoredProviders.sort((a, b) => b.score - a.score);
        return scoredProviders[0]?.provider || null;
    }
    /**
     * Simple fallback provider selection
     */
    selectFallbackProvider(request, requirements, failedProviders) {
        return this.providerRegistry.selectBestProvider(request, failedProviders);
    }
    /**
     * Make a request to a specific provider with error handling
     */
    async makeProviderRequest(provider, request) {
        try {
            // Pre-request health check if configured
            if (this.config.healthCheckInterval > 0 && Math.random() < 0.1) {
                // 10% chance
                const health = await provider.healthCheck(false);
                if (health.status === 'unhealthy') {
                    throw new types_1.ProviderError(`Provider ${provider.name} is unhealthy: ${health.error_message || 'Unknown reason'}`, provider.id);
                }
            }
            return await provider.makeRequest(request);
        }
        catch (error) {
            // Ensure we throw a ProviderError
            if (error instanceof types_1.ProviderError) {
                throw error;
            }
            throw new types_1.ProviderError(error instanceof Error ? error.message : String(error), provider.id, undefined, error);
        }
    }
    /**
     * Classify error for retry decisions
     */
    classifyError(error) {
        // ProviderError with status code
        if (error instanceof types_1.ProviderError) {
            const statusCode = error.statusCode;
            if (statusCode) {
                if (statusCode >= 500) {
                    return ErrorCategory.RETRYABLE; // Server errors
                }
                else if (statusCode === 429) {
                    return ErrorCategory.TEMPORARY; // Rate limiting
                }
                else if (statusCode >= 400 && statusCode < 500) {
                    return ErrorCategory.CLIENT_ERROR; // Client errors
                }
            }
        }
        // Network errors
        const message = error.message.toLowerCase();
        if (message.includes('timeout') ||
            message.includes('network') ||
            message.includes('connection') ||
            message.includes('econnreset') ||
            message.includes('enotfound')) {
            return ErrorCategory.RETRYABLE;
        }
        if (message.includes('rate limit') ||
            message.includes('too many requests') ||
            message.includes('quota exceeded')) {
            return ErrorCategory.TEMPORARY;
        }
        if (message.includes('unauthorized') ||
            message.includes('forbidden') ||
            message.includes('invalid api key') ||
            message.includes('authentication')) {
            return ErrorCategory.CLIENT_ERROR;
        }
        if (message.includes('service unavailable') ||
            message.includes('maintenance') ||
            message.includes('outage')) {
            return ErrorCategory.TEMPORARY;
        }
        // Default to retryable for unknown errors
        return ErrorCategory.UNKNOWN;
    }
    /**
     * Calculate retry delay with exponential backoff and jitter
     */
    calculateRetryDelay(attemptCount, previousAttempts) {
        // Exponential backoff
        let delay = this.config.initialRetryDelay * Math.pow(this.config.backoffMultiplier, attemptCount - 1);
        // Cap at maximum delay
        delay = Math.min(delay, this.config.maxRetryDelay);
        // Add jitter if enabled
        if (this.config.enableJitter) {
            const jitterRange = delay * this.config.jitterFactor;
            const jitter = (Math.random() * 2 - 1) * jitterRange; // ±jitterRange
            delay += jitter;
        }
        return Math.max(0, Math.round(delay));
    }
    /**
     * Circuit breaker management
     */
    isCircuitOpen(providerId) {
        if (!this.config.enableCircuitBreaker) {
            return false;
        }
        const breaker = this.circuitBreakers.get(providerId);
        if (!breaker) {
            return false;
        }
        switch (breaker.state) {
            case CircuitState.OPEN:
                // Check if timeout has passed
                if (Date.now() >= breaker.nextAttemptTime) {
                    breaker.state = CircuitState.HALF_OPEN;
                    this.logger.debug('Circuit breaker transitioning to half-open', {
                        providerId,
                    });
                    return false;
                }
                return true;
            case CircuitState.HALF_OPEN:
                return false; // Allow one request through
            default:
                return false;
        }
    }
    /**
     * Record provider success for circuit breaker
     */
    recordProviderSuccess(providerId) {
        if (!this.config.enableCircuitBreaker) {
            return;
        }
        const breaker = this.circuitBreakers.get(providerId);
        if (!breaker) {
            return;
        }
        if (breaker.state === CircuitState.HALF_OPEN) {
            breaker.state = CircuitState.CLOSED;
            breaker.failureCount = 0;
            this.logger.info('Circuit breaker closed after successful request', {
                providerId,
            });
        }
    }
    /**
     * Record provider failure for circuit breaker
     */
    recordProviderFailure(providerId, error) {
        if (!this.config.enableCircuitBreaker) {
            return;
        }
        let breaker = this.circuitBreakers.get(providerId);
        if (!breaker) {
            breaker = {
                state: CircuitState.CLOSED,
                failureCount: 0,
                lastFailureTime: 0,
                nextAttemptTime: 0,
            };
            this.circuitBreakers.set(providerId, breaker);
        }
        breaker.failureCount++;
        breaker.lastFailureTime = Date.now();
        // Check if we should open the circuit
        if (breaker.state === CircuitState.CLOSED &&
            breaker.failureCount >= this.config.circuitBreakerThreshold) {
            breaker.state = CircuitState.OPEN;
            breaker.nextAttemptTime = Date.now() + this.config.circuitBreakerTimeout;
            this.logger.warn('Circuit breaker opened due to repeated failures', {
                providerId,
                failureCount: breaker.failureCount,
                threshold: this.config.circuitBreakerThreshold,
                timeout: this.config.circuitBreakerTimeout,
                error: error.message,
            });
        }
        else if (breaker.state === CircuitState.HALF_OPEN) {
            // Close on first failure in half-open state
            breaker.state = CircuitState.OPEN;
            breaker.nextAttemptTime = Date.now() + this.config.circuitBreakerTimeout;
            this.logger.warn('Circuit breaker re-opened after half-open failure', {
                providerId,
                error: error.message,
            });
        }
        // Track failure history
        const failures = this.providerFailureHistory.get(providerId) || [];
        failures.push(Date.now());
        // Keep only recent failures (last hour)
        const oneHourAgo = Date.now() - 3600000;
        this.providerFailureHistory.set(providerId, failures.filter(time => time > oneHourAgo));
    }
    /**
     * Get recent failure count for a provider
     */
    getRecentFailureCount(providerId) {
        const failures = this.providerFailureHistory.get(providerId) || [];
        const fiveMinutesAgo = Date.now() - 300000; // 5 minutes
        return failures.filter(time => time > fiveMinutesAgo).length;
    }
    /**
     * Get count of similar error patterns
     */
    getSimilarErrorPatternCount(providerId, attempts) {
        const providerAttempts = attempts.filter(a => a.providerId === providerId);
        const recentErrors = providerAttempts
            .slice(-3)
            .map(a => a.errorCategory)
            .filter(Boolean);
        // Count consecutive similar errors
        let similarCount = 0;
        for (let i = recentErrors.length - 1; i >= 0; i--) {
            if (recentErrors[i] === recentErrors[recentErrors.length - 1]) {
                similarCount++;
            }
            else {
                break;
            }
        }
        return similarCount;
    }
    /**
     * Sleep utility for delays
     */
    sleep(ms) {
        return new Promise(resolve => setTimeout(resolve, ms));
    }
    /**
     * Generate request ID for logging
     */
    generateRequestId() {
        return `fo_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`;
    }
    /**
     * Get failover statistics
     */
    getFailoverStatistics() {
        const activeCircuitBreakers = Array.from(this.circuitBreakers.entries())
            .filter(([, breaker]) => breaker.state !== CircuitState.CLOSED)
            .map(([providerId]) => providerId);
        const providerFailureCounts = {};
        for (const [providerId, failures] of this.providerFailureHistory.entries()) {
            providerFailureCounts[providerId] = failures.length;
        }
        return {
            activeCircuitBreakers,
            providerFailureCounts,
            circuitBreakerConfig: {
                enabled: this.config.enableCircuitBreaker,
                threshold: this.config.circuitBreakerThreshold,
                timeout: this.config.circuitBreakerTimeout,
            },
        };
    }
    /**
     * Reset circuit breaker for a provider
     */
    resetCircuitBreaker(providerId) {
        const breaker = this.circuitBreakers.get(providerId);
        if (breaker) {
            breaker.state = CircuitState.CLOSED;
            breaker.failureCount = 0;
            breaker.lastFailureTime = 0;
            breaker.nextAttemptTime = 0;
            this.logger.info('Circuit breaker manually reset', { providerId });
        }
    }
    /**
     * Update failover configuration
     */
    updateConfig(config) {
        this.config = { ...this.config, ...config };
        this.logger.info('Failover configuration updated', { config: this.config });
    }
    /**
     * Get current failover configuration
     */
    getConfig() {
        return { ...this.config };
    }
    /**
     * Clear all failover state and circuits
     */
    clearState() {
        this.circuitBreakers.clear();
        this.providerFailureHistory.clear();
        this.logger.info('Failover manager state cleared');
    }
}
exports.FailoverManager = FailoverManager;
//# sourceMappingURL=failover-manager.js.map