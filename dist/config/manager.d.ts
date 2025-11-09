import { AppConfig, ProviderConfig, ConfigRoutingConfig, FallbackConfig, MultiProviderConfig } from './types';
export declare class ConfigManager {
    private configDir;
    private configPath;
    private _config;
    constructor(configDir?: string);
    get config(): AppConfig;
    private ensureConfigDir;
    private loadConfig;
    /**
     * Migrate legacy configuration to multi-provider format
     */
    private migrateConfig;
    /**
     * Async version of saveConfig for internal use
     */
    private saveConfigAsync;
    saveConfig(config?: AppConfig): Promise<boolean>;
    updateConfig(updates: Partial<AppConfig>): Promise<boolean>;
    hasApiKey(): boolean;
    getApiKey(): string;
    setApiKey(apiKey: string): Promise<boolean>;
    getSelectedModel(): string;
    setSelectedModel(model: string): Promise<boolean>;
    getCacheDuration(): number;
    setCacheDuration(hours: number): Promise<boolean>;
    isCacheValid(cacheFile: string): Promise<boolean>;
    isFirstRun(): boolean;
    markFirstRunCompleted(): Promise<boolean>;
    hasSavedModel(): boolean;
    getSavedModel(): string;
    setSavedModel(model: string): Promise<boolean>;
    hasSavedThinkingModel(): boolean;
    getSavedThinkingModel(): string;
    setSavedThinkingModel(model: string): Promise<boolean>;
    /**
     * Get multi-provider configuration
     */
    getMultiProviderConfig(): MultiProviderConfig | null;
    /**
     * Check if multi-provider mode is enabled
     */
    isMultiProviderEnabled(): boolean;
    /**
     * Get configuration for a specific provider
     */
    getProviderConfig(providerId: string): ProviderConfig | null;
    /**
     * Set or update provider configuration
     */
    setProviderConfig(providerId: string, config: Partial<ProviderConfig>): Promise<boolean>;
    /**
     * Get routing configuration
     */
    getRoutingConfig(): ConfigRoutingConfig | null;
    /**
     * Update routing configuration
     */
    updateRoutingConfig(routingConfig: Partial<ConfigRoutingConfig>): Promise<boolean>;
    /**
     * Get fallback configuration
     */
    getFallbackConfig(): FallbackConfig | null;
    /**
     * Update fallback configuration
     */
    updateFallbackConfig(fallbackConfig: Partial<FallbackConfig>): Promise<boolean>;
    /**
     * Get list of configured providers
     */
    getConfiguredProviders(): {
        id: string;
        config: ProviderConfig;
    }[];
    /**
     * Get provider for specific request type based on routing rules
     */
    getProviderForRequestType(requestType: 'default' | 'thinking' | 'vision' | 'tools'): string | null;
    /**
     * Enable or disable a provider
     */
    setProviderEnabled(providerId: string, enabled: boolean): Promise<boolean>;
    /**
     * Validate provider configuration
     */
    validateProviderConfig(providerConfig: any): {
        valid: boolean;
        errors: string[];
    };
    /**
     * Import configuration from synclaude (migration utility)
     */
    importFromSynclaude(): Promise<boolean>;
}
//# sourceMappingURL=manager.d.ts.map