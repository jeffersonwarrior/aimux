import { BaseProvider } from '../providers/base-provider';
import { ProviderRegistry } from '../providers/provider-registry';
import { AIRequest, ProviderCapabilities } from '../providers/types';
import { RequestRequirements } from './routing-utils';
/**
 * Routing engine configuration options
 */
export interface RoutingEngineConfig {
    /** Enable performance-based routing */
    enablePerformanceRouting: boolean;
    /** Enable cost-based routing */
    enableCostRouting: boolean;
    /** Enable health-based routing */
    enableHealthRouting: boolean;
    /** Default fallback behavior */
    enableFallback: boolean;
    /** Maximum number of providers to try before giving up */
    maxProviderAttempts: number;
    /** Provider selection preferences by capability */
    capabilityPreferences: {
        [key in keyof ProviderCapabilities]?: string[];
    };
    /** Custom routing rules */
    customRules?: RoutingRule[];
}
/**
 * Custom routing rule interface
 */
export interface RoutingRule {
    id: string;
    name: string;
    condition: (request: AIRequest) => boolean;
    providerSelector: (providers: BaseProvider[]) => BaseProvider | null;
    priority: number;
    enabled: boolean;
}
/**
 * Routing decision context
 */
export interface RoutingContext {
    originalRequest: AIRequest;
    requirements: RequestRequirements;
    candidateProviders: BaseProvider[];
    excludedProviders: string[];
    attemptCount: number;
    startTime: number;
    routingDecision?: string;
    selectedProvider?: BaseProvider;
}
/**
 * Routing engine for intelligent provider selection and request routing
 * Implements capability-based routing with fallback, performance optimization, and custom rules
 */
export declare class RoutingEngine {
    private providerRegistry;
    private logger;
    private config;
    private providerPerformanceCache;
    private routingHistory;
    private maxHistorySize;
    constructor(providerRegistry: ProviderRegistry, config?: Partial<RoutingEngineConfig>);
    /**
     * Select the best provider for a given AI request
     * @param request The AI request to route
     * @param excludeProviders Optional array of provider IDs to exclude
     * @returns Selected provider or null if no suitable provider found
     */
    selectProvider(request: AIRequest, excludeProviders?: string[]): BaseProvider | null;
    /**
     * Get providers that can handle the request based on requirements
     */
    private getCandidateProviders;
    /**
     * Select provider using the configured strategy (rules, capabilities, performance)
     */
    private selectProviderWithStrategy;
    /**
     * Select provider using custom routing rules
     */
    private selectProviderByCustomRules;
    /**
     * Select provider based on capability requirements and preferences
     */
    private selectProviderByCapabilities;
    /**
     * Select provider based on performance metrics
     */
    private selectProviderByPerformance;
    /**
     * Filter providers by performance metrics
     */
    private filterByPerformance;
    /**
     * Filter providers by health status
     */
    private filterByHealth;
    /**
     * Update performance metrics for a provider
     */
    updateProviderPerformance(providerId: string, responseTime: number, success: boolean, errorType?: string): void;
    /**
     * Record routing decision for analytics
     */
    private recordRoutingDecision;
    /**
     * Generate a unique request ID
     */
    private generateRequestId;
    /**
     * Get routing statistics and history
     */
    getRoutingStatistics(): {
        totalRoutings: number;
        successfulRoutings: number;
        averageRoutingTime: number;
        providerUsageCounts: Record<string, number>;
        capabilityUsageCounts: Record<string, number>;
        recentFailures: RoutingHistoryEntry[];
    };
    /**
     * Get performance metrics for all providers
     */
    getProviderPerformanceMetrics(): Record<string, PerformanceMetrics>;
    /**
     * Clear routing history and performance cache
     */
    clearCache(): void;
    /**
     * Update routing configuration
     */
    updateConfig(config: Partial<RoutingEngineConfig>): void;
    /**
     * Get current routing configuration
     */
    getConfig(): RoutingEngineConfig;
}
/**
 * Performance metrics interface
 */
export interface PerformanceMetrics {
    totalRequests: number;
    successfulRequests: number;
    failedRequests: number;
    averageResponseTime: number;
    successRate: number;
    lastUpdated: Date;
    errorTypes: Record<string, number>;
}
/**
 * Routing history entry interface
 */
interface RoutingHistoryEntry {
    timestamp: Date;
    requestId: string;
    requestType: string;
    requiredCapabilities: string[];
    candidateProviderCount: number;
    selectedProviderId?: string;
    routingDecision: string;
    routingTime: number;
    success: boolean;
}
export {};
//# sourceMappingURL=routing-engine.d.ts.map