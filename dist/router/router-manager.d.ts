import { EventEmitter } from 'events';
import { ProviderRegistry } from '../providers/provider-registry';
import { RoutingEngine } from '../core/routing-engine';
import { SimpleRouterConfig } from './simple-router';
import { ProviderConfig } from '../providers/types';
export interface RouterManagerConfig {
    router?: Partial<SimpleRouterConfig>;
    autoRestart?: boolean;
    restartDelay?: number;
    maxRestartAttempts?: number;
    enableMetrics?: boolean;
    metricsInterval?: number;
}
export interface RouterMetrics {
    uptime: number;
    requestsHandled: number;
    requestsSuccessful: number;
    requestsFailed: number;
    averageResponseTime: number;
    providerUsage: Record<string, {
        requests: number;
        responseTime: number;
        errorRate: number;
    }>;
}
export interface RouterStatus {
    isRunning: boolean;
    startTime?: Date;
    uptime?: number;
    config: SimpleRouterConfig;
    metrics: RouterMetrics;
    providerCount: number;
    lastRestart?: Date;
    restartAttempts: number;
    errors: Array<{
        timestamp: Date;
        message: string;
        provider?: string;
    }>;
}
export declare class RouterManager extends EventEmitter {
    private router?;
    private providerRegistry;
    private routingEngine;
    private config;
    private isManaged;
    private startTime?;
    private metrics;
    private restartAttempts;
    private restartTimer?;
    private metricsTimer?;
    private errors;
    constructor(providerRegistry: ProviderRegistry, routingEngine: RoutingEngine, config?: RouterManagerConfig);
    /**
     * Initialize the router manager
     */
    initialize(): Promise<void>;
    /**
     * Start the router
     */
    start(): Promise<void>;
    /**
     * Stop the router
     */
    stop(): Promise<void>;
    /**
     * Restart the router
     */
    restart(): Promise<void>;
    /**
     * Add or update a provider configuration
     */
    updateProvider(providerConfig: ProviderConfig): Promise<void>;
    /**
     * Remove a provider
     */
    removeProvider(providerName: string): Promise<void>;
    /**
     * Get router status
     */
    getStatus(): RouterStatus;
    /**
     * Get detailed metrics
     */
    getMetrics(): RouterMetrics;
    /**
     * Check if router is running
     */
    isRunning(): boolean;
    /**
     * Perform health check
     */
    healthCheck(): Promise<{
        router: boolean;
        providers: Record<string, any>;
        overall: boolean;
    }>;
    /**
     * Set up error handling
     */
    private setupErrorHandling;
    /**
     * Schedule automatic restart
     */
    private scheduleRestart;
    /**
     * Start metrics collection
     */
    private startMetricsCollection;
    /**
     * Update metrics
     */
    private updateMetrics;
    /**
     * Record an error
     */
    private recordError;
    /**
     * Update request metrics (called by SimpleRouter)
     */
    updateRequestMetrics(providerName: string, success: boolean, responseTime: number): void;
    /**
     * Cleanup resources
     */
    cleanup(): Promise<void>;
}
//# sourceMappingURL=router-manager.d.ts.map