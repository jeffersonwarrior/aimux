import { AxiosResponse } from 'axios';
import { BaseProvider } from './base-provider';
import { ProviderCapabilities, EnhancedModelInfo, AIRequest, AIResponse, ProviderHealth, ProviderConfig, AuthenticationResult, ProviderError } from './types';
/**
 * Z.AI Provider Implementation
 *
 * Provides integration with Z.AI API, supporting:
 * - Vision capabilities (image analysis)
 * - Tool calling (function execution)
 * - Up to 100K tokens context
 * - No thinking capabilities
 */
export declare class ZAIProvider extends BaseProvider {
    readonly name = "Z.AI";
    readonly id = "z-ai";
    capabilities: ProviderCapabilities;
    private httpClient;
    private readonly DEFAULT_BASE_URL;
    private readonly DEFAULT_MODELS_URL;
    constructor();
    /**
     * Authenticate with Z.AI API
     */
    authenticate(apiKey?: string): Promise<AuthenticationResult>;
    /**
     * Fetch available models from Z.AI
     * Returns hardcoded Z.AI models since no models endpoint exists
     */
    fetchModels(forceRefresh?: boolean): Promise<EnhancedModelInfo[]>;
    /**
     * Validate API key format and basic connectivity
     */
    validateApiKey(apiKey: string): Promise<boolean>;
    /**
     * Make an AI request to Z.AI
     */
    makeRequest(request: AIRequest): Promise<AIResponse>;
    /**
     * Perform health check on the provider
     */
    healthCheck(performFullCheck?: boolean): Promise<ProviderHealth>;
    /**
     * Normalize Z.AI models to our standard format
     */
    protected normalizeModels(models: any[]): EnhancedModelInfo[];
    /**
     * Transform standard AI request to Z.AI Anthropic-compatible format
     */
    protected transformRequest(request: AIRequest): any;
    /**
     * Transform Z.AI response to standard AI response format
     */
    protected transformResponse(response: AxiosResponse, startTime: number): AIResponse;
    /**
     * Handle provider-specific errors
     */
    protected handleError(error: any): ProviderError;
    /**
     * Get default configuration for Z.AI
     */
    getDefaultConfig(): Partial<ProviderConfig>;
    /**
     * Validate provider configuration
     */
    validateConfig(config: ProviderConfig): boolean;
    /**
     * Clean up resources
     */
    cleanup(): Promise<void>;
    /**
     * Update rate limiting information from response headers
     */
    private updateRateLimitFromHeaders;
}
//# sourceMappingURL=zai-provider.d.ts.map