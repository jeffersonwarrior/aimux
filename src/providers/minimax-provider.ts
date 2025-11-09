import axios, { AxiosInstance, AxiosRequestConfig, AxiosResponse } from 'axios';
import { BaseProvider } from './base-provider';
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

/**
 * Minimax M2 Provider Implementation
 *
 * Provides integration with Minimax M2 API, supporting:
 * - Thinking capabilities (enhanced reasoning)
 * - Vision capabilities (image analysis)
 * - Tool calling (function execution)
 * - Up to 200K tokens context
 */
export class MinimaxM2Provider extends BaseProvider {
  public readonly name = 'Minimax M2';
  public readonly id = 'minimax-m2';

  public capabilities: ProviderCapabilities = {
    thinking: true,
    vision: false, // MiniMax M2 does not support image inputs
    tools: true,
    maxTokens: 200000,
    streaming: true,
    systemMessages: true,
    temperature: true,
    topP: true,
    maxOutputTokens: 8000,
  };

  private httpClient: AxiosInstance;
  private readonly DEFAULT_BASE_URL = 'https://api.minimax.io/anthropic';
  private readonly DEFAULT_MODELS_URL = undefined; // No models endpoint available

  constructor() {
    super();
    this.httpClient = axios.create({
      timeout: 60000, // 60 seconds timeout
      headers: {
        'Content-Type': 'application/json',
      },
    });

    // Add request interceptor for rate limiting and error handling
    this.httpClient.interceptors.request.use(
      async config => {
        // Check rate limiting before making request
        if (!this.canMakeRequest()) {
          throw new ProviderError('Rate limit exceeded', this.name, 429);
        }

        // Add API key if configured
        if (this.config?.apiKey) {
          config.headers.Authorization = `Bearer ${this.config.apiKey}`;
        }

        return config;
      },
      error => Promise.reject(error)
    );

    // Add response interceptor for rate limit handling
    this.httpClient.interceptors.response.use(
      response => {
        // Update rate limiting info from response headers
        this.updateRateLimitFromHeaders(response.headers);
        return response;
      },
      error => {
        if (error.response?.status === 429) {
          this.updateRateLimitFromHeaders(error.response.headers);
        }
        return Promise.reject(error);
      }
    );
  }

  /**
   * Authenticate with Minimax M2 API
   */
  public async authenticate(apiKey?: string): Promise<AuthenticationResult> {
    try {
      const keyToUse = apiKey || this.config?.apiKey;
      if (!keyToUse) {
        return {
          success: false,
          error: 'API key is required for authentication',
        };
      }

      // Make a minimal messages request to validate the API key
      const response = await this.httpClient.post(
        `${this.config?.baseUrl || this.DEFAULT_BASE_URL}/messages`,
        {
          model: 'MiniMax-M2',
          max_tokens: 1,
          messages: [{ role: 'user', content: 'hi' }],
        },
        {
          headers: {
            Authorization: `Bearer ${keyToUse}`,
          },
          timeout: 10000,
        }
      );

      if (response.status === 200) {
        // Update config if new API key was provided
        if (apiKey && apiKey !== this.config?.apiKey) {
          this.config = { ...this.config!, apiKey };
        }

        return {
          success: true,
          details: {
            verified_capabilities: ['thinking', 'tools'], // vision not supported
            limits: {
              requests_per_minute: 60,
              tokens_per_minute: 1000000,
            },
          },
        };
      } else {
        return {
          success: false,
          error: `Authentication failed with status: ${response.status}`,
        };
      }
    } catch (error: any) {
      return {
        success: false,
        error: error.response?.data?.error?.message || error.message || 'Authentication request failed',
      };
    }
  }

  /**
   * Fetch available models from Minimax M2
   * Returns hardcoded models since MiniMax doesn't provide a models endpoint
   */
  public async fetchModels(forceRefresh: boolean = false): Promise<EnhancedModelInfo[]> {
    const cacheKey = this.getCacheKey();

    // Check cache first unless force refresh is requested
    if (!forceRefresh && this.modelCache.has(cacheKey)) {
      return this.modelCache.get(cacheKey)!;
    }

    // Return hardcoded MiniMax M2 models
    const hardcodedModels = [
      {
        id: 'MiniMax-M2',
        object: 'model',
        created: Math.floor(Date.now() / 1000),
        owned_by: 'minimax',
      },
      {
        id: 'MiniMax-M2-Stable',
        object: 'model',
        created: Math.floor(Date.now() / 1000),
        owned_by: 'minimax',
      },
    ];

    const models = this.normalizeModels(hardcodedModels);
    this.modelCache.set(cacheKey, models);

    return models;
  }

  /**
   * Validate API key format and basic connectivity
   */
  public async validateApiKey(apiKey: string): Promise<boolean> {
    if (!apiKey || typeof apiKey !== 'string' || apiKey.length < 20) {
      return false;
    }

    try {
      const response = await this.httpClient.post(
        `${this.config?.baseUrl || this.DEFAULT_BASE_URL}/messages`,
        {
          model: 'MiniMax-M2',
          max_tokens: 1,
          messages: [{ role: 'user', content: 'hi' }],
        },
        {
          headers: {
            Authorization: `Bearer ${apiKey}`,
          },
          timeout: 10000, // Shorter timeout for validation
        }
      );

      return response.status === 200;
    } catch {
      return false;
    }
  }

  /**
   * Make an AI request to Minimax M2
   */
  public async makeRequest(request: AIRequest): Promise<AIResponse> {
    if (!this.config?.apiKey) {
      throw new ProviderError('Provider not configured with API key', this.name);
    }

    const startTime = Date.now();

    try {
      // Transform the request to Minimax M2 format
      const minimaxRequest = this.transformRequest(request);

      const response: AxiosResponse = await this.httpClient.post(
        `${this.config?.baseUrl || this.DEFAULT_BASE_URL}/messages`,
        minimaxRequest,
        {
          timeout: this.config?.timeout || 120000, // 2 minutes default
          signal: request.metadata?.timeout
            ? AbortSignal.timeout(request.metadata.timeout)
            : undefined,
        }
      );

      // Update rate limiting
      const estimatedTokens = this.estimateTokens(request);
      this.updateRateLimit(estimatedTokens);

      // Transform response to standard format
      return this.transformResponse(response, startTime);
    } catch (error: any) {
      throw this.handleError(error);
    }
  }

  /**
   * Perform health check on the provider
   */
  public async healthCheck(performFullCheck: boolean = false): Promise<ProviderHealth> {
    const startTime = Date.now();

    try {
      if (!this.config?.apiKey) {
        return {
          status: 'unhealthy',
          last_check: new Date(),
          error_message: 'No API key configured',
        };
      }

      // Basic connectivity check using messages endpoint
      const response = await this.httpClient.post(
        `${this.config?.baseUrl || this.DEFAULT_BASE_URL}/messages`,
        {
          model: 'MiniMax-M2',
          max_tokens: 1,
          messages: [{ role: 'user', content: 'ping' }],
        },
        {
          timeout: 10000,
          headers: {
            Authorization: `Bearer ${this.config.apiKey}`,
          },
        }
      );

      const responseTime = Date.now() - startTime;

      if (response.status === 200) {
        const health: ProviderHealth = {
          status: 'healthy',
          response_time: responseTime,
          last_check: new Date(),
          uptime_percentage: 99.9, // Placeholder
          capabilities_status: {
            thinking: 'available',
            vision: 'unavailable',
            tools: 'available',
          },
        };

        // Perform full check if requested
        if (performFullCheck) {
          try {
            // Try a minimal chat completion request
            await this.makeRequest({
              model: 'minimax-claude-instant-1',
              messages: [{ role: 'user', content: 'ping' }],
              max_tokens: 1,
            });
          } catch (error: any) {
            health.status = 'degraded';
            health.error_message = `Chat completion failed: ${error.message}`;
          }
        }

        this.lastHealthCheck = health;
        return health;
      } else {
        const health: ProviderHealth = {
          status: 'unhealthy',
          response_time: responseTime,
          last_check: new Date(),
          error_message: `HTTP ${response.status}: ${response.statusText}`,
        };
        this.lastHealthCheck = health;
        return health;
      }
    } catch (error: any) {
      const health: ProviderHealth = {
        status: 'unhealthy',
        response_time: Date.now() - startTime,
        last_check: new Date(),
        error_message: error.message || 'Health check failed',
      };
      this.lastHealthCheck = health;
      return health;
    }
  }

  /**
   * Normalize Minimax M2 models to our standard format
   */
  protected normalizeModels(models: any[]): EnhancedModelInfo[] {
    return models.map((model: any) => ({
      id: model.id,
      name: model.id === 'MiniMax-M2' ? 'MiniMax M2' : 'MiniMax M2 Stable',
      object: model.object || 'model',
      created: model.created || Date.now() / 1000,
      owned_by: model.owned_by || 'minimax',
      provider: 'minimax-m2',
      capabilities: {
        ...this.capabilities,
      },
      context_length: 200000, // MiniMax M2 supports up to 200K context
      max_output_length: 8000,
      pricing: {
        prompt: '0.001', // Placeholder pricing
        completion: '0.002',
        image: undefined, // Not supported
      },
      specialized: {
        thinking: true, // MiniMax M2 has agentic capabilities and reasoning
        coding: true, // Good for coding tasks
        reasoning: true,
        creative: model.id === 'MiniMax-M2', // M2 might be more creative than Stable
      },
    }));
  }

  /**
   * Transform standard AI request to MiniMax Anthropic-compatible format
   */
  protected transformRequest(request: AIRequest): any {
    const minimaxRequest: any = {
      model: request.model.replace('minimax-', ''), // Remove prefix if present
      messages: request.messages.map(msg => {
        const transformed: any = {
          role: msg.role,
        };

        if (typeof msg.content === 'string') {
          transformed.content = msg.content;
        } else if (Array.isArray(msg.content)) {
          // Handle mixed content, but filter out images since not supported
          transformed.content = msg.content
            .filter(part => part.type !== 'image_url') // Remove images
            .map(part => {
              if (part.type === 'text') {
                return { type: 'text', text: part.text };
              }
              return part;
            });

          // If all content was images, provide a fallback text
          if (transformed.content.length === 0) {
            transformed.content = [{ type: 'text', text: 'I cannot process images. Please provide text-only input.' }];
          }
        }

        if (msg.name) {
          transformed.name = msg.name;
        }

        return transformed;
      }),
      stream: request.stream || false,
    };

    // Add required parameters
    if (request.max_tokens !== undefined) {
      minimaxRequest.max_tokens = request.max_tokens;
    } else {
      minimaxRequest.max_tokens = 8000; // Default for MiniMax M2
    }

    // Add optional parameters
    if (request.temperature !== undefined) {
      minimaxRequest.temperature = request.temperature;
    }

    if (request.top_p !== undefined) {
      minimaxRequest.top_p = request.top_p;
    }

    if (request.tools && request.tools.length > 0) {
      minimaxRequest.tools = request.tools.map(tool => ({
        type: tool.type,
        function: {
          name: tool.function.name,
          description: tool.function.description,
          parameters: tool.function.parameters,
        },
      }));
    }

    if (request.tool_choice) {
      minimaxRequest.tool_choice = request.tool_choice;
    }

    return minimaxRequest;
  }

  /**
   * Transform Minimax M2 response to standard AI response format
   */
  protected transformResponse(response: AxiosResponse, startTime: number): AIResponse {
    const data = response.data;
    const responseTime = Date.now() - startTime;

    return {
      id: data.id || `minimax-${Date.now()}`,
      object: data.object || 'chat.completion',
      created: data.created || Math.floor(Date.now() / 1000),
      model: data.model || 'unknown',
      choices: (data.choices || []).map((choice: any) => ({
        index: choice.index || 0,
        message: choice.message || {
          role: 'assistant',
          content: choice.text || '',
        },
        finish_reason: choice.finish_reason || 'stop',
      })),
      usage: data.usage
        ? {
            prompt_tokens: data.usage.prompt_tokens,
            completion_tokens: data.usage.completion_tokens,
            total_tokens: data.usage.total_tokens,
          }
        : undefined,
      provider: this.name,
      response_time: responseTime,
      metadata: {
        cached: response.headers['x-cached'] === 'true',
        routing_decision: 'minimax-m2',
        fallback_used: false,
        failover_attempts: 0,
      },
    };
  }

  /**
   * Handle provider-specific errors
   */
  protected handleError(error: any): ProviderError {
    if (error.response) {
      // API returned an error response
      const statusCode = error.response.status;
      const message = error.response.data?.error?.message || error.response.statusText;

      return new ProviderError(`Minimax M2 API error: ${message}`, this.name, statusCode, error);
    } else if (error.code === 'ECONNABORTED') {
      // Timeout error
      return new ProviderError('Minimax M2 request timeout', this.name, 408, error);
    } else if (error.code === 'ENOTFOUND' || error.code === 'ECONNREFUSED') {
      // Network error
      return new ProviderError('Minimax M2 network error', this.name, 503, error);
    } else {
      // Other errors
      return new ProviderError(
        `Minimax M2 provider error: ${error.message}`,
        this.name,
        500,
        error
      );
    }
  }

  /**
   * Get default configuration for Minimax M2
   */
  public getDefaultConfig(): Partial<ProviderConfig> {
    return {
      name: this.name,
      baseUrl: this.DEFAULT_BASE_URL,
      modelsUrl: this.DEFAULT_MODELS_URL,
      timeout: 120000,
      maxRetries: 3,
      retryDelay: 1000,
      capabilities: this.capabilities,
      enabled: false,
      priority: 1,
    };
  }

  /**
   * Validate provider configuration
   */
  public validateConfig(config: ProviderConfig): boolean {
    return !!(
      config.name &&
      config.apiKey &&
      typeof config.apiKey === 'string' &&
      config.apiKey.length >= 20
    );
  }

  /**
   * Clean up resources
   */
  public async cleanup(): Promise<void> {
    // Clear any pending requests
    this.modelCache.clear();
    this.lastHealthCheck = undefined;
  }

  /**
   * Update rate limiting information from response headers
   */
  private updateRateLimitFromHeaders(headers: any): void {
    const requestsPerMinute = parseInt(headers['x-ratelimit-limit-requests']) || 60;
    const requestsRemaining =
      parseInt(headers['x-ratelimit-remaining-requests']) || requestsPerMinute;
    const tokensPerMinute = parseInt(headers['x-ratelimit-limit-tokens']) || 1000000;
    const tokensRemaining = parseInt(headers['x-ratelimit-remaining-tokens']) || tokensPerMinute;
    const resetTimeMs = parseInt(headers['x-ratelimit-reset']) || 60000;

    this.rateLimitInfo = {
      requestsPerMinute,
      tokensPerMinute,
      requestsRemaining,
      tokensRemaining,
      resetTime: new Date(Date.now() + resetTimeMs),
    };
  }
}
