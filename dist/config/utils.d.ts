import { ProviderConfig, ConfigRoutingConfig, MultiProviderConfig } from './types';
/**
 * Configuration validation utilities
 */
export declare class ConfigValidator {
    /**
     * Validate a provider configuration
     */
    static validateProvider(config: any): {
        valid: boolean;
        errors: string[];
    };
    /**
     * Validate routing configuration
     */
    static validateRouting(config: any): {
        valid: boolean;
        errors: string[];
    };
    /**
     * Validate fallback configuration
     */
    static validateFallback(config: any): {
        valid: boolean;
        errors: string[];
    };
    /**
     * Validate complete multi-provider configuration
     */
    static validateMultiProvider(config: any): {
        valid: boolean;
        errors: string[];
    };
}
/**
 * Provider capability matching utilities
 */
export declare class ProviderCapabilityMatcher {
    /**
     * Check if a provider supports a specific capability
     */
    static supportsCapability(provider: ProviderConfig, capability: 'thinking' | 'vision' | 'tools'): boolean;
    /**
     * Get providers that support specific capabilities
     */
    static getProvidersForCapability(providers: Record<string, ProviderConfig>, capability: 'thinking' | 'vision' | 'tools'): string[];
    /**
     * Get all enabled providers sorted by priority (lower number = higher priority)
     */
    static getProvidersByPriority(providers: Record<string, ProviderConfig>): string[];
    /**
     * Find best provider for a specific request type based on routing strategy
     */
    static findBestProvider(providers: Record<string, ProviderConfig>, routing: ConfigRoutingConfig, requestType: 'thinking' | 'vision' | 'tools' | 'default'): string | null;
    private static getSpecificProviderForRequest;
    private static findProviderByCapability;
    private static findProviderByCost;
    private static findProviderBySpeed;
    private static findProviderByReliability;
}
/**
 * Configuration migration utilities
 */
export declare class ConfigMigrator {
    /**
     * Create default multi-provider configuration
     */
    static createDefaultMultiProviderConfig(): MultiProviderConfig;
    /**
     * Migrate from legacy single-provider config to multi-provider
     */
    static migrateLegacyConfig(legacyConfig: any): MultiProviderConfig;
    /**
     * Merge two multi-provider configurations
     */
    static mergeMultiProviderConfigs(base: MultiProviderConfig, override: Partial<MultiProviderConfig>): MultiProviderConfig;
}
/**
 * Configuration health check utilities
 */
export declare class ConfigHealthChecker {
    /**
     * Check if configuration is healthy and ready for use
     */
    static checkConfigHealth(config: MultiProviderConfig): Promise<{
        healthy: boolean;
        issues: string[];
        recommendations: string[];
    }>;
    /**
     * Get configuration summary
     */
    static getConfigSummary(config: MultiProviderConfig): {
        totalProviders: number;
        enabledProviders: number;
        configuredProviders: number;
        routingStrategy: string;
        failoverEnabled: boolean;
        healthChecksEnabled: boolean;
    };
}
//# sourceMappingURL=utils.d.ts.map