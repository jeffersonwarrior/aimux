import { ProviderRegistry } from '../providers/provider-registry';
import { AIRequest, AIResponse } from '../providers/types';
import { RoutingEngine } from './routing-engine';
/**
 * Failover configuration options
 */
export interface FailoverConfig {
    /** Maximum number of retry attempts per provider */
    maxRetriesPerProvider: number;
    /** Maximum total retry attempts across all providers */
    maxTotalRetries: number;
    /** Initial delay between retries (in milliseconds) */
    initialRetryDelay: number;
    /** Maximum delay between retries (in milliseconds) */
    maxRetryDelay: number;
    /** Exponential backoff multiplier */
    backoffMultiplier: number;
    /** Enable jitter in retry delays */
    enableJitter: boolean;
    /** Jitter factor (0-1, percentage of delay to randomize) */
    jitterFactor: number;
    /** Enable circuit breaker pattern */
    enableCircuitBreaker: boolean;
    /** Circuit breaker failure threshold */
    circuitBreakerThreshold: number;
    /** Circuit breaker timeout (in milliseconds) */
    circuitBreakerTimeout: number;
    /** Provider health check interval during failover */
    healthCheckInterval: number;
    /** Enable intelligent provider selection during failover */
    enableIntelligentFailover: boolean;
}
/**
 * Error classification for retry decisions
 */
export declare enum ErrorCategory {
    /** Temporary network issues, server overload */
    RETRYABLE = "retryable",
    /** Rate limiting, temporary provider issues */
    TEMPORARY = "temporary",
    /** Authentication, invalid requests */
    CLIENT_ERROR = "client_error",
    /** Permanent provider issues, outages */
    PERMANENT = "permanent",
    /** Unknown error type */
    UNKNOWN = "unknown"
}
/**
 * Failover attempt information
 */
export interface FailoverAttempt {
    attemptNumber: number;
    providerId: string;
    providerName: string;
    error?: Error;
    errorCategory?: ErrorCategory;
    retryDelay: number;
    startTime: number;
    duration?: number;
    success: boolean;
}
/**
 * Failover manager for handling provider outages and intelligent retry logic
 * Implements circuit breaker pattern, exponential backoff, and provider health monitoring
 */
export declare class FailoverManager {
    private providerRegistry;
    private routingEngine;
    private logger;
    private config;
    private circuitBreakers;
    private providerFailureHistory;
    constructor(providerRegistry: ProviderRegistry, routingEngine: RoutingEngine, config?: Partial<FailoverConfig>);
    /**
     * Handle failover for a failed AI request
     * @param request The original AI request
     * @param failedProviders Array of provider IDs that have already failed
     * @param originalError The error that triggered the failover
     * @returns Promise resolving to AI response or throwing if all providers fail
     */
    handleFailover(request: AIRequest, failedProviders?: string[], originalError?: Error): Promise<AIResponse>;
    /**
     * Select the next provider for failover
     */
    private selectFailoverProvider;
    /**
     * Intelligent provider selection considering performance, health, and error patterns
     */
    private selectIntelligentProvider;
    /**
     * Simple fallback provider selection
     */
    private selectFallbackProvider;
    /**
     * Make a request to a specific provider with error handling
     */
    private makeProviderRequest;
    /**
     * Classify error for retry decisions
     */
    classifyError(error: Error): ErrorCategory;
    /**
     * Calculate retry delay with exponential backoff and jitter
     */
    private calculateRetryDelay;
    /**
     * Circuit breaker management
     */
    private isCircuitOpen;
    /**
     * Record provider success for circuit breaker
     */
    private recordProviderSuccess;
    /**
     * Record provider failure for circuit breaker
     */
    private recordProviderFailure;
    /**
     * Get recent failure count for a provider
     */
    private getRecentFailureCount;
    /**
     * Get count of similar error patterns
     */
    private getSimilarErrorPatternCount;
    /**
     * Sleep utility for delays
     */
    private sleep;
    /**
     * Generate request ID for logging
     */
    private generateRequestId;
    /**
     * Get failover statistics
     */
    getFailoverStatistics(): {
        activeCircuitBreakers: string[];
        providerFailureCounts: Record<string, number>;
        circuitBreakerConfig: any;
    };
    /**
     * Reset circuit breaker for a provider
     */
    resetCircuitBreaker(providerId: string): void;
    /**
     * Update failover configuration
     */
    updateConfig(config: Partial<FailoverConfig>): void;
    /**
     * Get current failover configuration
     */
    getConfig(): FailoverConfig;
    /**
     * Clear all failover state and circuits
     */
    clearState(): void;
}
//# sourceMappingURL=failover-manager.d.ts.map