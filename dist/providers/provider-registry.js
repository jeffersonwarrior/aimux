"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.ProviderRegistry = void 0;
/**
 * Registry for managing multiple AI providers
 * Handles provider registration, selection, and delegation
 */
class ProviderRegistry {
    providers = new Map();
    capabilityMap = new Map();
    priorityOrder = [];
    healthCheckInterval;
    healthCheckIntervalMs = 30000; // 30 seconds
    /**
     * Register a new provider
     * @param provider The provider to register
     * @throws Error if provider with same ID already exists
     */
    register(provider) {
        if (this.providers.has(provider.id)) {
            throw new Error(`Provider with ID '${provider.id}' is already registered`);
        }
        this.providers.set(provider.id, provider);
        this.updateCapabilityMap(provider);
        this.updatePriorityOrder();
        console.log(`Registered provider: ${provider.name} (${provider.id})`);
    }
    /**
     * Unregister a provider
     * @param providerId The ID of the provider to unregister
     * @returns True if provider was removed, false if not found
     */
    async unregister(providerId) {
        const provider = this.providers.get(providerId);
        if (!provider) {
            return false;
        }
        // Clean up provider resources
        await provider.cleanup();
        this.providers.delete(providerId);
        this.rebuildCapabilityMap();
        this.updatePriorityOrder();
        console.log(`Unregistered provider: ${provider.name} (${providerId})`);
        return true;
    }
    /**
     * Get a provider by ID
     * @param name The provider ID or name
     * @returns The provider or null if not found
     */
    getProvider(idOrName) {
        // Try direct ID lookup first
        let provider = this.providers.get(idOrName);
        // If not found, try name lookup
        if (!provider) {
            for (const p of this.providers.values()) {
                if (p.id === idOrName || p.name === idOrName) {
                    provider = p;
                    break;
                }
            }
        }
        return provider || null;
    }
    /**
     * Get all registered providers
     * @returns Array of all providers
     */
    getAllProviders() {
        return Array.from(this.providers.values());
    }
    /**
     * Get all enabled providers
     * @returns Array of enabled providers
     */
    getEnabledProviders() {
        return Array.from(this.providers.values()).filter(provider => provider.enabled);
    }
    /**
     * Get providers that support a specific capability
     * @param capability The capability to check
     * @returns Array of providers that support the capability
     */
    getProvidersByCapability(capability) {
        const providerNames = this.capabilityMap.get(capability);
        if (!providerNames) {
            return [];
        }
        return Array.from(providerNames)
            .map(name => this.providers.get(name))
            .filter((provider) => provider !== undefined && provider.enabled);
    }
    /**
     * Get providers that can handle a specific request
     * @param request The AI request to evaluate
     * @returns Array of providers that can handle the request, ordered by priority
     */
    getProvidersForRequest(request) {
        return this.getEnabledProviders()
            .filter(provider => provider.canHandleRequest(request))
            .sort((a, b) => {
            const aPriority = a.getConfig()?.priority || 0;
            const bPriority = b.getConfig()?.priority || 0;
            return bPriority - aPriority; // Higher priority first
        });
    }
    /**
     * Select the best provider for a request based on capabilities and priority
     * @param request The AI request
     * @param excludeProviders Optional array of provider IDs to exclude
     * @returns The best provider or null if none available
     */
    selectBestProvider(request, excludeProviders = []) {
        const eligibleProviders = this.getProvidersForRequest(request).filter(provider => !excludeProviders.includes(provider.id));
        if (eligibleProviders.length === 0) {
            return null;
        }
        // For now, return the highest priority provider
        // In the future, this could incorporate health, latency, cost, etc.
        return eligibleProviders[0] || null;
    }
    /**
     * Check health of all enabled providers
     * @param performFullCheck Whether to perform comprehensive checks
     * @returns Promise resolving to health status of all providers
     */
    async checkAllProvidersHealth(performFullCheck = false) {
        const healthResults = {};
        const enabledProviders = this.getEnabledProviders();
        // Check all providers in parallel
        const healthPromises = enabledProviders.map(async (provider) => {
            try {
                const health = await provider.healthCheck(performFullCheck);
                healthResults[provider.id] = health;
            }
            catch (error) {
                healthResults[provider.id] = {
                    status: 'unhealthy',
                    last_check: new Date(),
                    error_message: error instanceof Error ? error.message : String(error),
                };
            }
        });
        await Promise.allSettled(healthPromises);
        return healthResults;
    }
    /**
     * Get the total number of registered providers
     * @returns Number of providers
     */
    getProviderCount() {
        return this.providers.size;
    }
    /**
     * Get the number of enabled providers
     * @returns Number of enabled providers
     */
    getEnabledProviderCount() {
        return this.getEnabledProviders().length;
    }
    /**
     * Start automatic health checking for all providers
     * @param intervalMs Interval in milliseconds between health checks
     */
    startHealthMonitoring(intervalMs = 30000) {
        if (this.healthCheckInterval) {
            this.stopHealthMonitoring();
        }
        this.healthCheckIntervalMs = intervalMs;
        this.healthCheckInterval = setInterval(async () => {
            try {
                await this.checkAllProvidersHealth(false); // Light check by default
            }
            catch (error) {
                console.error('Health monitoring error:', error);
            }
        }, this.healthCheckIntervalMs);
        console.log(`Started health monitoring with ${intervalMs}ms interval`);
    }
    /**
     * Stop automatic health checking
     */
    stopHealthMonitoring() {
        if (this.healthCheckInterval) {
            clearInterval(this.healthCheckInterval);
            this.healthCheckInterval = undefined;
            console.log('Stopped health monitoring');
        }
    }
    /**
     * Get provider IDs sorted by priority
     * @returns Array of provider IDs sorted by priority
     */
    getProviderPriorityOrder() {
        return [...this.priorityOrder];
    }
    /**
     * Set custom priority order for providers
     * @param providerIdOrder Array of provider IDs in desired priority order
     * @throws Error if any provider ID is not registered
     */
    setProviderPriorityOrder(providerIdOrder) {
        // Validate all provider IDs exist
        for (const id of providerIdOrder) {
            if (!this.providers.has(id)) {
                throw new Error(`Provider with ID '${id}' is not registered`);
            }
        }
        this.priorityOrder = [...providerIdOrder];
        console.log('Updated provider priority order:', this.priorityOrder);
    }
    /**
     * Get statistics about providers and their capabilities
     * @returns Provider statistics object
     */
    getStatistics() {
        const providersByCapability = {};
        // Initialize all capabilities with zero
        const allCapabilities = Array.from(this.capabilityMap.keys());
        for (const capability of allCapabilities) {
            providersByCapability[capability] = 0;
        }
        // Count providers by capability
        for (const provider of this.getEnabledProviders()) {
            for (const capability of allCapabilities) {
                if (provider.hasCapability(capability) &&
                    providersByCapability[capability] !== undefined) {
                    providersByCapability[capability]++;
                }
            }
        }
        // Count healthy providers
        let healthyProviders = 0;
        let totalResponseTime = 0;
        let responseTimeCount = 0;
        for (const provider of this.getEnabledProviders()) {
            const health = provider.getLastHealthCheck();
            if (health?.status === 'healthy') {
                healthyProviders++;
            }
            if (health?.response_time) {
                totalResponseTime += health.response_time;
                responseTimeCount++;
            }
        }
        return {
            totalProviders: this.getProviderCount(),
            enabledProviders: this.getEnabledProviderCount(),
            providersByCapability,
            averageResponseTime: responseTimeCount > 0 ? totalResponseTime / responseTimeCount : undefined,
            healthyProviders,
        };
    }
    /**
     * Clear all registered providers
     * Cleans up all provider resources
     */
    async clear() {
        this.stopHealthMonitoring();
        const cleanupPromises = Array.from(this.providers.values()).map(provider => provider
            .cleanup()
            .catch(error => console.error(`Error cleaning up provider ${provider.id}:`, error)));
        await Promise.allSettled(cleanupPromises);
        this.providers.clear();
        this.capabilityMap.clear();
        this.priorityOrder = [];
        console.log('Cleared all providers from registry');
    }
    /**
     * Check if registry is empty
     * @returns True if no providers are registered
     */
    isEmpty() {
        return this.providers.size === 0;
    }
    /**
     * Get provider information summary
     * @returns Array of provider information objects
     */
    getProviderInfos() {
        return Array.from(this.providers.values()).map(provider => provider.getInfo());
    }
    /**
     * Update the capability mapping for a provider
     * @param provider The provider to update
     */
    updateCapabilityMap(provider) {
        const capabilities = Object.keys(provider.capabilities);
        for (const capability of capabilities) {
            if (provider.capabilities[capability]) {
                if (!this.capabilityMap.has(capability)) {
                    this.capabilityMap.set(capability, new Set());
                }
                this.capabilityMap.get(capability).add(provider.id);
            }
        }
    }
    /**
     * Rebuild the entire capability map
     */
    rebuildCapabilityMap() {
        this.capabilityMap.clear();
        for (const provider of this.providers.values()) {
            this.updateCapabilityMap(provider);
        }
    }
    /**
     * Update priority order based on provider configurations
     */
    updatePriorityOrder() {
        const providers = Array.from(this.providers.values());
        this.priorityOrder = providers
            .map(p => ({ id: p.id, priority: p.getConfig()?.priority || 0 }))
            .sort((a, b) => b.priority - a.priority)
            .map(p => p.id);
    }
}
exports.ProviderRegistry = ProviderRegistry;
//# sourceMappingURL=provider-registry.js.map