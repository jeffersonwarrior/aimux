import { ProviderCapabilities, EnhancedModelInfo, AIRequest, AIResponse, ProviderHealth, ProviderConfig, AuthenticationResult, ProviderError } from './types';
/**
 * Abstract base class for all AI providers
 * Defines the common interface that all providers must implement
 */
export declare abstract class BaseProvider {
    /**
     * Human-readable name of the provider
     */
    abstract name: string;
    /**
     * Unique identifier for the provider (e.g., 'minimax-m2', 'z-ai', 'synthetic-new')
     */
    abstract id: string;
    /**
     * Capabilities supported by this provider
     */
    abstract capabilities: ProviderCapabilities;
    /**
     * Provider configuration
     */
    protected config?: ProviderConfig;
    /**
     * Whether the provider is enabled and ready to use
     */
    enabled: boolean;
    /**
     * Last health check result
     */
    protected lastHealthCheck?: ProviderHealth;
    /**
     * Cache for models to avoid repeated fetching
     */
    protected modelCache: Map<string, EnhancedModelInfo[]>;
    /**
     * Rate limiting information
     */
    protected rateLimitInfo?: {
        requestsPerMinute: number;
        tokensPerMinute: number;
        requestsRemaining: number;
        tokensRemaining: number;
        resetTime: Date;
    };
    /**
     * Authenticate with the provider using stored or provided credentials
     * @param apiKey Optional API key to use instead of stored config
     * @returns Promise resolving to authentication result
     */
    abstract authenticate(apiKey?: string): Promise<AuthenticationResult>;
    /**
     * Fetch available models from the provider
     * @param forceRefresh Whether to bypass cache and fetch fresh data
     * @returns Promise resolving to array of enhanced model information
     */
    abstract fetchModels(forceRefresh?: boolean): Promise<EnhancedModelInfo[]>;
    /**
     * Validate an API key format and basic connectivity
     * @param apiKey The API key to validate
     * @returns Promise resolving to true if valid, false otherwise
     */
    abstract validateApiKey(apiKey: string): Promise<boolean>;
    /**
     * Make an AI request to the provider
     * @param request The AI request to make
     * @returns Promise resolving to AI response
     * @throws ProviderError if the request fails
     */
    abstract makeRequest(request: AIRequest): Promise<AIResponse>;
    /**
     * Check the health and availability of the provider
     * @param performFullCheck Whether to perform a comprehensive check
     * @returns Promise resolving to provider health status
     */
    abstract healthCheck(performFullCheck?: boolean): Promise<ProviderHealth>;
    /**
     * Configure the provider with settings
     * @param config Provider configuration
     */
    configure(config: ProviderConfig): void;
    /**
     * Get the current provider configuration
     * @returns Current provider config or undefined if not configured
     */
    getConfig(): ProviderConfig | undefined;
    /**
     * Check if the provider has a specific capability
     * @param capability The capability to check
     * @returns True if provider supports the capability
     */
    hasCapability(capability: keyof ProviderCapabilities): boolean;
    /**
     * Check if the provider can handle a specific request
     * @param request The AI request to evaluate
     * @returns True if provider can handle the request
     */
    canHandleRequest(request: AIRequest): boolean;
    /**
     * Get a model by its ID from the cached models
     * @param modelId The model ID to find
     * @returns The enhanced model info or undefined if not found
     */
    getModel(modelId: string): EnhancedModelInfo | undefined;
    /**
     * Rate limiting helper - check if a request can be made
     * @returns True if request can proceed, false if rate limited
     */
    protected canMakeRequest(): boolean;
    /**
     * Update rate limiting info after a request
     * @param tokensUsed Number of tokens consumed by the request
     */
    protected updateRateLimit(tokensUsed: number): void;
    /**
     * Estimate tokens needed for a request (rough approximation)
     * @param request The AI request
     * @returns Estimated number of tokens
     */
    protected estimateTokens(request: AIRequest): number;
    /**
     * Get cache key for model storage
     * @returns Cache key string
     */
    protected getCacheKey(): string;
    /**
     * Clear cached models
     */
    clearModelCache(): void;
    /**
     * Get last health check result
     * @returns Last health check or undefined
     */
    getLastHealthCheck(): ProviderHealth | undefined;
    /**
     * Normalize model information from provider format to our standard format
     * @param models Raw models from provider API
     * @returns Array of enhanced model info
     */
    protected abstract normalizeModels(models: any[]): EnhancedModelInfo[];
    /**
     * Transform request format to provider-specific format
     * @param request Standard AI request
     * @returns Provider-specific request object
     */
    protected abstract transformRequest(request: AIRequest): any;
    /**
     * Transform provider response to standard AI response format
     * @param response Provider response
     * @param startTime Request start time for calculating response time
     * @returns Standard AI response
     */
    protected abstract transformResponse(response: any, startTime: number): AIResponse;
    /**
     * Handle provider-specific errors and convert to standardized ProviderError
     * @param error Original error from provider
     * @returns ProviderError
     */
    protected abstract handleError(error: any): ProviderError;
    /**
     * Get provider-specific default configuration
     * @returns Default configuration object
     */
    abstract getDefaultConfig(): Partial<ProviderConfig>;
    /**
     * Validate provider-specific configuration
     * @param config Configuration to validate
     * @returns True if configuration is valid
     */
    abstract validateConfig(config: ProviderConfig): boolean;
    /**
     * Clean up resources and perform any provider-specific shutdown tasks
     */
    abstract cleanup(): Promise<void>;
    /**
     * Get provider information summary
     * @returns Provider information object
     */
    getInfo(): {
        name: string;
        id: string;
        capabilities: ProviderCapabilities;
        enabled: boolean;
        configured: boolean;
        lastHealthCheck?: ProviderHealth;
        modelCount?: number;
    };
    /**
     * Get performance metrics for the provider
     * @returns Performance metrics object or undefined if not available
     */
    getPerformanceMetrics(): {
        totalRequests: number;
        successfulRequests: number;
        failedRequests: number;
        averageResponseTime: number;
        successRate: number;
        lastUpdated: Date;
        errorTypes: Record<string, number>;
    } | undefined;
}
//# sourceMappingURL=base-provider.d.ts.map