import {
  ProviderCapabilities,
  EnhancedModelInfo,
  AIRequest,
  AIResponse,
  ProviderHealth,
  ProviderConfig,
  AuthenticationResult,
  ProviderError,
} from './types';
import { ModelInfo } from '../models/types';

/**
 * Abstract base class for all AI providers
 * Defines the common interface that all providers must implement
 */
export abstract class BaseProvider {
  /**
   * Human-readable name of the provider
   */
  public abstract name: string;

  /**
   * Unique identifier for the provider (e.g., 'minimax-m2', 'z-ai', 'synthetic-new')
   */
  public abstract id: string;

  /**
   * Capabilities supported by this provider
   */
  public abstract capabilities: ProviderCapabilities;

  /**
   * Provider configuration
   */
  protected config?: ProviderConfig;

  /**
   * Whether the provider is enabled and ready to use
   */
  public enabled: boolean = false;

  /**
   * Last health check result
   */
  protected lastHealthCheck?: ProviderHealth;

  /**
   * Cache for models to avoid repeated fetching
   */
  protected modelCache: Map<string, EnhancedModelInfo[]> = new Map();

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
  public abstract authenticate(apiKey?: string): Promise<AuthenticationResult>;

  /**
   * Fetch available models from the provider
   * @param forceRefresh Whether to bypass cache and fetch fresh data
   * @returns Promise resolving to array of enhanced model information
   */
  public abstract fetchModels(forceRefresh?: boolean): Promise<EnhancedModelInfo[]>;

  /**
   * Validate an API key format and basic connectivity
   * @param apiKey The API key to validate
   * @returns Promise resolving to true if valid, false otherwise
   */
  public abstract validateApiKey(apiKey: string): Promise<boolean>;

  /**
   * Make an AI request to the provider
   * @param request The AI request to make
   * @returns Promise resolving to AI response
   * @throws ProviderError if the request fails
   */
  public abstract makeRequest(request: AIRequest): Promise<AIResponse>;

  /**
   * Check the health and availability of the provider
   * @param performFullCheck Whether to perform a comprehensive check
   * @returns Promise resolving to provider health status
   */
  public abstract healthCheck(performFullCheck?: boolean): Promise<ProviderHealth>;

  /**
   * Configure the provider with settings
   * @param config Provider configuration
   */
  public configure(config: ProviderConfig): void {
    this.config = config;
    this.enabled = config.enabled;
    this.capabilities = config.capabilities;
  }

  /**
   * Get the current provider configuration
   * @returns Current provider config or undefined if not configured
   */
  public getConfig(): ProviderConfig | undefined {
    return this.config;
  }

  /**
   * Check if the provider has a specific capability
   * @param capability The capability to check
   * @returns True if provider supports the capability
   */
  public hasCapability(capability: keyof ProviderCapabilities): boolean {
    return this.capabilities[capability] === true;
  }

  /**
   * Check if the provider can handle a specific request
   * @param request The AI request to evaluate
   * @returns True if provider can handle the request
   */
  public canHandleRequest(request: AIRequest): boolean {
    // Check if provider is configured and enabled
    if (!this.config || !this.enabled) {
      return false;
    }

    // Check basic capabilities
    if (request.stream && !this.capabilities.streaming) {
      return false;
    }

    // Check for system messages in the messages array
    const hasSystemMessage = request.messages.some(msg => msg.role === 'system');
    if (hasSystemMessage && !this.capabilities.systemMessages) {
      return false;
    }

    // Check for vision content
    const hasVision = request.messages.some(
      msg => Array.isArray(msg.content) && msg.content.some(part => part.type === 'image_url')
    );
    if (hasVision && !this.capabilities.vision) {
      return false;
    }

    // Check for tools
    if (request.tools && request.tools.length > 0 && !this.capabilities.tools) {
      return false;
    }

    // Check token limits
    const estimatedTokens = this.estimateTokens(request);
    if (estimatedTokens > this.capabilities.maxTokens) {
      return false;
    }

    return true;
  }

  /**
   * Get a model by its ID from the cached models
   * @param modelId The model ID to find
   * @returns The enhanced model info or undefined if not found
   */
  public getModel(modelId: string): EnhancedModelInfo | undefined {
    const cacheKey = this.getCacheKey();
    const models = this.modelCache.get(cacheKey) || [];
    return models.find(model => model.id === modelId);
  }

  /**
   * Rate limiting helper - check if a request can be made
   * @returns True if request can proceed, false if rate limited
   */
  protected canMakeRequest(): boolean {
    if (!this.rateLimitInfo) {
      return true;
    }

    const now = new Date();
    if (now >= this.rateLimitInfo.resetTime) {
      // Reset rate limit
      this.rateLimitInfo.requestsRemaining = this.rateLimitInfo.requestsPerMinute;
      this.rateLimitInfo.tokensRemaining = this.rateLimitInfo.tokensPerMinute;
      this.rateLimitInfo.resetTime = new Date(now.getTime() + 60000); // 1 minute from now
      return true;
    }

    return this.rateLimitInfo.requestsRemaining > 0;
  }

  /**
   * Update rate limiting info after a request
   * @param tokensUsed Number of tokens consumed by the request
   */
  protected updateRateLimit(tokensUsed: number): void {
    if (this.rateLimitInfo) {
      this.rateLimitInfo.requestsRemaining = Math.max(0, this.rateLimitInfo.requestsRemaining - 1);
      this.rateLimitInfo.tokensRemaining = Math.max(
        0,
        this.rateLimitInfo.tokensRemaining - tokensUsed
      );
    }
  }

  /**
   * Estimate tokens needed for a request (rough approximation)
   * @param request The AI request
   * @returns Estimated number of tokens
   */
  protected estimateTokens(request: AIRequest): number {
    let tokens = 0;

    for (const message of request.messages) {
      if (typeof message.content === 'string') {
        tokens += Math.ceil(message.content.length / 4); // Rough estimate: 1 token ≈ 4 characters
      } else if (Array.isArray(message.content)) {
        for (const part of message.content) {
          if (part.type === 'text' && part.text) {
            tokens += Math.ceil(part.text.length / 4);
          } else if (part.type === 'image_url') {
            tokens += 85; // Approximate tokens for an image
          }
        }
      }
    }

    // Add buffer for system messages and function definitions
    if (request.tools) {
      tokens += request.tools.length * 50; // Rough estimate per tool
    }

    return tokens;
  }

  /**
   * Get cache key for model storage
   * @returns Cache key string
   */
  protected getCacheKey(): string {
    return `${this.id}-${this.config?.apiKey?.slice(-8) || 'no-key'}`;
  }

  /**
   * Clear cached models
   */
  public clearModelCache(): void {
    this.modelCache.clear();
  }

  /**
   * Get last health check result
   * @returns Last health check or undefined
   */
  public getLastHealthCheck(): ProviderHealth | undefined {
    return this.lastHealthCheck;
  }

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
  public abstract getDefaultConfig(): Partial<ProviderConfig>;

  /**
   * Validate provider-specific configuration
   * @param config Configuration to validate
   * @returns True if configuration is valid
   */
  public abstract validateConfig(config: ProviderConfig): boolean;

  /**
   * Clean up resources and perform any provider-specific shutdown tasks
   */
  public abstract cleanup(): Promise<void>;

  /**
   * Get provider information summary
   * @returns Provider information object
   */
  public getInfo(): {
    name: string;
    id: string;
    capabilities: ProviderCapabilities;
    enabled: boolean;
    configured: boolean;
    lastHealthCheck?: ProviderHealth;
    modelCount?: number;
  } {
    const cacheKey = this.getCacheKey();
    const modelCount = this.modelCache.get(cacheKey)?.length || 0;

    return {
      name: this.name,
      id: this.id,
      capabilities: this.capabilities,
      enabled: this.enabled,
      configured: !!this.config,
      lastHealthCheck: this.lastHealthCheck,
      modelCount,
    };
  }

  /**
   * Get performance metrics for the provider
   * @returns Performance metrics object or undefined if not available
   */
  public getPerformanceMetrics():
    | {
        totalRequests: number;
        successfulRequests: number;
        failedRequests: number;
        averageResponseTime: number;
        successRate: number;
        lastUpdated: Date;
        errorTypes: Record<string, number>;
      }
    | undefined {
    // This method should be implemented by concrete provider classes
    // to track their own performance metrics
    return undefined;
  }
}
