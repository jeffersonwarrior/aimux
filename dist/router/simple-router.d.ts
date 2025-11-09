import { ProviderRegistry } from '../providers/provider-registry';
import { RoutingEngine } from '../core/routing-engine';
export interface SimpleRouterConfig {
    port: number;
    host: string;
    enableLogging: boolean;
    maxRequestSize: number;
    timeout: number;
    enableHealthCheck: boolean;
    healthCheckInterval: number;
}
export declare class SimpleRouter {
    private server;
    private isRunning;
    private providerRegistry;
    private routingEngine;
    private config;
    private healthCheckTimer?;
    constructor(providerRegistry: ProviderRegistry, routingEngine: RoutingEngine, config?: Partial<SimpleRouterConfig>);
    /**
     * Start the HTTP proxy server
     */
    start(): Promise<void>;
    /**
     * Stop the HTTP proxy server
     */
    stop(): Promise<void>;
    /**
     * Handle incoming HTTP requests
     */
    private handleRequest;
    /**
     * Parse request body with size limit
     */
    private parseRequestBody;
    /**
     * Analyze request to determine routing requirements
     */
    private analyzeRequest;
    /**
     * Forward request to selected provider
     */
    private forwardRequest;
    /**
     * Build target URL for provider
     */
    private buildTargetUrl;
    /**
     * Normalize response from provider
     */
    private normalizeResponse;
    /**
     * Start health check timer
     */
    private startHealthCheck;
    /**
     * Perform health check on all providers
     */
    private performHealthCheck;
    /**
     * Get router status
     */
    getStatus(): {
        isRunning: boolean;
        config: SimpleRouterConfig;
        uptime?: number;
        providerCount: number;
    };
}
//# sourceMappingURL=simple-router.d.ts.map