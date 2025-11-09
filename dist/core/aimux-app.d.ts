export interface AimuxAppOptions {
    verbose?: boolean;
    quiet?: boolean;
    additionalArgs?: string[];
    thinkingModel?: string;
    defaultProvider?: string;
    thinkingProvider?: string;
}
/**
 * Multi-provider AI application that manages multiple AI providers and routing
 */
export interface AimuxAppMode {
    type: 'bridge' | 'routing';
    enableRouting?: boolean;
}
export declare class AimuxApp {
    private configManager;
    private ui;
    private launcher;
    private modelManager;
    private providerRegistry;
    private routingEngine;
    private failoverManager;
    private mode;
    constructor(mode?: AimuxAppMode);
    setupLogging(options: AimuxAppOptions): Promise<void>;
    getConfig(): {
        apiKey: string;
        baseUrl: string;
        anthropicBaseUrl: string;
        modelsApiUrl: string;
        cacheDurationHours: number;
        selectedModel: string;
        selectedThinkingModel: string;
        firstRunCompleted: boolean;
        providers?: Record<string, {
            apiKey: string;
            name: string;
            enabled: boolean;
            priority: number;
            timeout: number;
            maxRetries: number;
            baseUrl?: string | undefined;
            anthropicBaseUrl?: string | undefined;
            modelsApiUrl?: string | undefined;
            models?: string[] | undefined;
            capabilities?: ("thinking" | "vision" | "tools")[] | undefined;
        }> | undefined;
        multiProvider?: {
            providers: {
                'minimax-m2'?: {
                    apiKey: string;
                    name: string;
                    enabled: boolean;
                    priority: number;
                    timeout: number;
                    maxRetries: number;
                    baseUrl?: string | undefined;
                    anthropicBaseUrl?: string | undefined;
                    modelsApiUrl?: string | undefined;
                    models?: string[] | undefined;
                    capabilities?: ("thinking" | "vision" | "tools")[] | undefined;
                } | undefined;
                'z-ai'?: {
                    apiKey: string;
                    name: string;
                    enabled: boolean;
                    priority: number;
                    timeout: number;
                    maxRetries: number;
                    baseUrl?: string | undefined;
                    anthropicBaseUrl?: string | undefined;
                    modelsApiUrl?: string | undefined;
                    models?: string[] | undefined;
                    capabilities?: ("thinking" | "vision" | "tools")[] | undefined;
                } | undefined;
                'synthetic-new'?: {
                    apiKey: string;
                    name: string;
                    enabled: boolean;
                    priority: number;
                    timeout: number;
                    maxRetries: number;
                    baseUrl?: string | undefined;
                    anthropicBaseUrl?: string | undefined;
                    modelsApiUrl?: string | undefined;
                    models?: string[] | undefined;
                    capabilities?: ("thinking" | "vision" | "tools")[] | undefined;
                } | undefined;
            };
            routing: {
                defaultProvider: string;
                strategy: "capability" | "cost" | "speed" | "reliability";
                enableFailover: boolean;
                loadBalancing: boolean;
                thinkingProvider?: string | undefined;
                visionProvider?: string | undefined;
                toolsProvider?: string | undefined;
            };
            fallback: {
                enabled: boolean;
                retryDelay: number;
                maxFailures: number;
                healthCheck: boolean;
                healthCheckInterval: number;
            };
        } | undefined;
        routingRules?: Record<string, string> | undefined;
    };
    private getModelManager;
    /**
     * Run with default provider
     */
    run(options?: AimuxAppOptions): Promise<void>;
    /**
     * Run with specific provider
     */
    runWithProvider(providerId: string, options?: AimuxAppOptions): Promise<void>;
    /**
     * Run in hybrid mode with different providers for different request types
     */
    runHybrid(options: {
        thinkingProvider: string;
        defaultProvider: string;
        verbose?: boolean;
        quiet?: boolean;
        additionalArgs?: string[];
    }): Promise<void>;
    /**
     * Interactive provider setup for multiple providers
     */
    setupProviders(): Promise<void>;
    /**
     * Launch interactive provider setup
     */
    launchProviderSetup(): Promise<void>;
    /**
     * Launch interactive provider status
     */
    launchProviderStatus(): Promise<void>;
    /**
     * Setup a single provider
     */
    setupSingleProvider(providerId: string): Promise<void>;
    /**
     * List all configured providers
     */
    listProviders(): Promise<void>;
    /**
     * Test all configured providers
     */
    testProviders(): Promise<void>;
    /**
     * Test a specific provider
     */
    testProvider(providerId: string): Promise<boolean>;
    /**
     * Setup routing configuration
     */
    private setupRouting;
    /**
     * Launch Claude Code with specified provider
     */
    private launchWithProvider;
    /**
     * Auto-configure models for a specific provider
     */
    private autoConfigureModelsForProvider;
    /**
     * Get default models for a specific provider
     */
    private getDefaultModelsForProvider;
    /**
     * Get provider information
     */
    private getProviderInfo;
    /**
     * Show current configuration
     */
    showConfig(): Promise<void>;
    /**
     * Run system health check
     */
    doctor(): Promise<void>;
    interactiveModelSelection(): Promise<void>;
    listModels(): Promise<void>;
    clearCache(): Promise<void>;
    cacheInfo(): Promise<void>;
}
//# sourceMappingURL=aimux-app.d.ts.map