/**
 * Router module exports and interfaces
 *
 * This module provides the simplified HTTP proxy router implementation for MVP.
 * Future versions will replace this with the native C++ router implementation.
 */
import { SimpleRouter, type SimpleRouterConfig } from './simple-router';
import { RouterManager, type RouterManagerConfig, type RouterMetrics, type RouterStatus } from './router-manager';
export { SimpleRouter, RouterManager };
export type { SimpleRouterConfig, RouterManagerConfig, RouterMetrics, RouterStatus };
export declare function createRouter(providerRegistry: any, routingEngine: any, config?: Partial<SimpleRouterConfig>): SimpleRouter;
export declare function createRouterManager(providerRegistry: any, routingEngine: any, config?: RouterManagerConfig): RouterManager;
export declare const DEFAULT_ROUTER_CONFIG: SimpleRouterConfig;
export declare const DEFAULT_MANAGER_CONFIG: RouterManagerConfig;
export declare class RouterUtils {
    /**
     * Validate router port is available
     */
    static validatePort(port: number, host?: string): Promise<boolean>;
    /**
     * Find an available port starting from the given port
     */
    static findAvailablePort(startPort: number, host?: string): Promise<number>;
    /**
     * Test router connectivity
     */
    static testConnectivity(port: number, host?: string): Promise<boolean>;
    /**
     * Get router system information
     */
    static getSystemInfo(): {
        nodeVersion: string;
        platform: string;
        arch: string;
        memory: {
            total: number;
            free: number;
            used: number;
        };
    };
    /**
     * Validate router configuration
     */
    static validateConfig(config: Partial<SimpleRouterConfig>): {
        isValid: boolean;
        errors: string[];
    };
}
export declare const ROUTER_CONSTANTS: {
    DEFAULT_PORT: number;
    DEFAULT_HOST: string;
    DEFAULT_TIMEOUT: number;
    DEFAULT_MAX_REQUEST_SIZE: number;
    DEFAULT_HEALTH_CHECK_INTERVAL: number;
    MAX_RESTART_ATTEMPTS: number;
    RESTART_DELAY: number;
    METRICS_INTERVAL: number;
    MAX_ERRORS_STORED: number;
};
export declare enum RouterEvents {
    ROUTER_STARTED = "router:started",
    ROUTER_STOPPED = "router:stopped",
    ROUTER_RESTARTED = "router:restarted",
    ROUTER_ERROR = "router:error",
    ROUTER_METRICS_UPDATED = "router:metrics-updated",
    ROUTER_MAX_RESTART_ATTEMPTS = "router:max-restart-attempts",
    ROUTER_PROVIDER_UPDATED = "router:provider:updated",
    ROUTER_PROVIDER_REMOVED = "router:provider:removed"
}
export declare enum RouterErrorTypes {
    STARTUP_FAILED = "startup_failed",
    CONFIGURATION_ERROR = "configuration_error",
    NETWORK_ERROR = "network_error",
    PROVIDER_ERROR = "provider_error",
    TIMEOUT_ERROR = "timeout_error",
    VALIDATION_ERROR = "validation_error"
}
export declare class RouterError extends Error {
    type: RouterErrorTypes;
    provider?: string | undefined;
    originalError?: Error | undefined;
    constructor(message: string, type?: RouterErrorTypes, provider?: string | undefined, originalError?: Error | undefined);
}
export declare function createRouterError(message: string, type?: RouterErrorTypes, provider?: string, originalError?: Error): RouterError;
export type { ProviderRegistry } from '../providers/provider-registry';
export type { RoutingEngine } from '../core/routing-engine';
export type { BaseProvider } from '../providers/base-provider';
export type { AIRequest, AIResponse, ProviderConfig, ProviderCapabilities, ProviderHealth, ProviderError, } from '../providers/types';
/**
 * Router module version information
 */
export declare const VERSION = "1.0.0-mvp";
/**
 * Module metadata
 */
export declare const MODULE_INFO: {
    name: string;
    version: string;
    description: string;
    author: string;
    license: string;
    repository: string;
    type: string;
    capabilities: string[];
    future: string[];
};
export declare const ROUTER_EXPORTS: {
    SimpleRouter: typeof SimpleRouter;
    RouterManager: typeof RouterManager;
    createRouter: typeof createRouter;
    createRouterManager: typeof createRouterManager;
    RouterUtils: typeof RouterUtils;
    RouterError: typeof RouterError;
    createRouterError: typeof createRouterError;
    DEFAULT_ROUTER_CONFIG: SimpleRouterConfig;
    DEFAULT_MANAGER_CONFIG: RouterManagerConfig;
    ROUTER_CONSTANTS: {
        DEFAULT_PORT: number;
        DEFAULT_HOST: string;
        DEFAULT_TIMEOUT: number;
        DEFAULT_MAX_REQUEST_SIZE: number;
        DEFAULT_HEALTH_CHECK_INTERVAL: number;
        MAX_RESTART_ATTEMPTS: number;
        RESTART_DELAY: number;
        METRICS_INTERVAL: number;
        MAX_ERRORS_STORED: number;
    };
    RouterEvents: typeof RouterEvents;
    RouterErrorTypes: typeof RouterErrorTypes;
    VERSION: string;
    MODULE_INFO: {
        name: string;
        version: string;
        description: string;
        author: string;
        license: string;
        repository: string;
        type: string;
        capabilities: string[];
        future: string[];
    };
};
//# sourceMappingURL=index.d.ts.map