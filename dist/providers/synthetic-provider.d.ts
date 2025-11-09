import { AxiosResponse } from 'axios';
import { BaseProvider } from './base-provider';
import { ProviderCapabilities, EnhancedModelInfo, AIRequest, AIResponse, ProviderHealth, ProviderConfig, AuthenticationResult, ProviderError } from './types';
/**
 * Synthetic.New Provider Implementation
 *
 * Provides integration with Synthetic.New API, supporting:
 * - Thinking capabilities (enhanced reasoning)
 * - Tool calling (function execution)
 * - Up to 200K tokens context
 * - No vision capabilities
 */
export declare class SyntheticProvider extends BaseProvider {
    readonly name = "Synthetic.New";
    readonly id = "synthetic-new";
    capabilities: ProviderCapabilities;
    private httpClient;
    private readonly DEFAULT_BASE_URL;
    private readonly DEFAULT_ANTHROPIC_URL;
    private readonly DEFAULT_MODELS_URL;
    constructor();
    /**
     * Authenticate with Synthetic.New API
     */
    authenticate(apiKey?: string): Promise<AuthenticationResult>;
    /**
     * Fetch available models from Synthetic.New
     */
    fetchModels(forceRefresh?: boolean): Promise<EnhancedModelInfo[]>;
    /**
     * Validate API key format and basic connectivity
     */
    validateApiKey(apiKey: string): Promise<boolean>;
    /**
     * Make an AI request to Synthetic.New
     */
    makeRequest(request: AIRequest): Promise<AIResponse>;
    /**
     * Perform health check on the provider
     */
    healthCheck(performFullCheck?: boolean): Promise<ProviderHealth>;
    /**
     * Get the appropriate endpoint for the request type
     */
    private getEndpointForRequest;
    /**
     * Normalize Synthetic.New models to our standard format
     */
    protected normalizeModels(models: any[]): EnhancedModelInfo[];
    /**
     * Transform standard AI request to Synthetic.New format
     */
    protected transformRequest(request: AIRequest): any;
    /**
     * Transform Synthetic.New response to standard AI response format
     */
    protected transformResponse(response: AxiosResponse, startTime: number): AIResponse;
    /**
     * Handle provider-specific errors
     */
    protected handleError(error: any): ProviderError;
    /**
     * Get default configuration for Synthetic.New
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
//# sourceMappingURL=synthetic-provider.d.ts.map