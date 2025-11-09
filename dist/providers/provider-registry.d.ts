import { BaseProvider } from './base-provider';
import { ProviderCapabilities, AIRequest, ProviderHealth } from './types';
/**
 * Registry for managing multiple AI providers
 * Handles provider registration, selection, and delegation
 */
export declare class ProviderRegistry {
    private providers;
    private capabilityMap;
    private priorityOrder;
    private healthCheckInterval?;
    private healthCheckIntervalMs;
    /**
     * Register a new provider
     * @param provider The provider to register
     * @throws Error if provider with same ID already exists
     */
    register(provider: BaseProvider): void;
    /**
     * Unregister a provider
     * @param providerId The ID of the provider to unregister
     * @returns True if provider was removed, false if not found
     */
    unregister(providerId: string): Promise<boolean>;
    /**
     * Get a provider by ID
     * @param name The provider ID or name
     * @returns The provider or null if not found
     */
    getProvider(idOrName: string): BaseProvider | null;
    /**
     * Get all registered providers
     * @returns Array of all providers
     */
    getAllProviders(): BaseProvider[];
    /**
     * Get all enabled providers
     * @returns Array of enabled providers
     */
    getEnabledProviders(): BaseProvider[];
    /**
     * Get providers that support a specific capability
     * @param capability The capability to check
     * @returns Array of providers that support the capability
     */
    getProvidersByCapability(capability: keyof ProviderCapabilities): BaseProvider[];
    /**
     * Get providers that can handle a specific request
     * @param request The AI request to evaluate
     * @returns Array of providers that can handle the request, ordered by priority
     */
    getProvidersForRequest(request: AIRequest): BaseProvider[];
    /**
     * Select the best provider for a request based on capabilities and priority
     * @param request The AI request
     * @param excludeProviders Optional array of provider IDs to exclude
     * @returns The best provider or null if none available
     */
    selectBestProvider(request: AIRequest, excludeProviders?: string[]): BaseProvider | null;
    /**
     * Check health of all enabled providers
     * @param performFullCheck Whether to perform comprehensive checks
     * @returns Promise resolving to health status of all providers
     */
    checkAllProvidersHealth(performFullCheck?: boolean): Promise<Record<string, ProviderHealth>>;
    /**
     * Get the total number of registered providers
     * @returns Number of providers
     */
    getProviderCount(): number;
    /**
     * Get the number of enabled providers
     * @returns Number of enabled providers
     */
    getEnabledProviderCount(): number;
    /**
     * Start automatic health checking for all providers
     * @param intervalMs Interval in milliseconds between health checks
     */
    startHealthMonitoring(intervalMs?: number): void;
    /**
     * Stop automatic health checking
     */
    stopHealthMonitoring(): void;
    /**
     * Get provider IDs sorted by priority
     * @returns Array of provider IDs sorted by priority
     */
    getProviderPriorityOrder(): string[];
    /**
     * Set custom priority order for providers
     * @param providerIdOrder Array of provider IDs in desired priority order
     * @throws Error if any provider ID is not registered
     */
    setProviderPriorityOrder(providerIdOrder: string[]): void;
    /**
     * Get statistics about providers and their capabilities
     * @returns Provider statistics object
     */
    getStatistics(): {
        totalProviders: number;
        enabledProviders: number;
        providersByCapability: Record<string, number>;
        averageResponseTime?: number;
        healthyProviders: number;
    };
    /**
     * Clear all registered providers
     * Cleans up all provider resources
     */
    clear(): Promise<void>;
    /**
     * Check if registry is empty
     * @returns True if no providers are registered
     */
    isEmpty(): boolean;
    /**
     * Get provider information summary
     * @returns Array of provider information objects
     */
    getProviderInfos(): ReturnType<BaseProvider['getInfo']>[];
    /**
     * Update the capability mapping for a provider
     * @param provider The provider to update
     */
    private updateCapabilityMap;
    /**
     * Rebuild the entire capability map
     */
    private rebuildCapabilityMap;
    /**
     * Update priority order based on provider configurations
     */
    private updatePriorityOrder;
}
//# sourceMappingURL=provider-registry.d.ts.map