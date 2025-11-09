import { AxiosResponse } from 'axios';
import { BaseProvider } from './base-provider';
import { ProviderCapabilities, EnhancedModelInfo, AIRequest, AIResponse, ProviderHealth, ProviderConfig, AuthenticationResult, ProviderError } from './types';
/**
 * Minimax M2 Provider Implementation
 *
 * Provides integration with Minimax M2 API, supporting:
 * - Thinking capabilities (enhanced reasoning)
 * - Vision capabilities (image analysis)
 * - Tool calling (function execution)
 * - Up to 200K tokens context
 */
export declare class MinimaxM2Provider extends BaseProvider {
    readonly name = "Minimax M2";
    readonly id = "minimax-m2";
    capabilities: ProviderCapabilities;
    private httpClient;
    private readonly DEFAULT_BASE_URL;
    private readonly DEFAULT_MODELS_URL;
    constructor();
    /**
     * Authenticate with Minimax M2 API
     */
    authenticate(apiKey?: string): Promise<AuthenticationResult>;
    /**
     * Fetch available models from Minimax M2
     * Returns hardcoded models since MiniMax doesn't provide a models endpoint
     */
    fetchModels(forceRefresh?: boolean): Promise<EnhancedModelInfo[]>;
    /**
     * Validate API key format and basic connectivity
     */
    validateApiKey(apiKey: string): Promise<boolean>;
    /**
     * Make an AI request to Minimax M2
     */
    makeRequest(request: AIRequest): Promise<AIResponse>;
    /**
     * Perform health check on the provider
     */
    healthCheck(performFullCheck?: boolean): Promise<ProviderHealth>;
    /**
     * Normalize Minimax M2 models to our standard format
     */
    protected normalizeModels(models: any[]): EnhancedModelInfo[];
    /**
     * Transform standard AI request to MiniMax Anthropic-compatible format
     */
    protected transformRequest(request: AIRequest): any;
    /**
     * Transform Minimax M2 response to standard AI response format
     */
    protected transformResponse(response: AxiosResponse, startTime: number): AIResponse;
    /**
     * Handle provider-specific errors
     */
    protected handleError(error: any): ProviderError;
    /**
     * Get default configuration for Minimax M2
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
//# sourceMappingURL=minimax-provider.d.ts.map