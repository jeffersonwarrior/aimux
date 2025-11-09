import axios, { AxiosInstance, AxiosResponse } from 'axios';
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
 * Synthetic.New Provider Implementation
 *
 * Provides integration with Synthetic.New API, supporting:
 * - Thinking capabilities (enhanced reasoning)
 * - Tool calling (function execution)
 * - Up to 200K tokens context
 * - No vision capabilities
 */
export class SyntheticProvider extends BaseProvider {
  public readonly name = 'Synthetic.New';
  public readonly id = 'synthetic-new';

  public capabilities: ProviderCapabilities = {
    thinking: true,
    vision: false,
    tools: true,
    maxTokens: 200000,
    streaming: true,
    systemMessages: true,
    temperature: true,
    topP: true,
    maxOutputTokens: 8000,
  };

  private httpClient: AxiosInstance;
  private readonly DEFAULT_BASE_URL = 'https://api.synthetic.new';
  private readonly DEFAULT_ANTHROPIC_URL = 'https://api.synthetic.new/anthropic';
  private readonly DEFAULT_MODELS_URL = 'https://api.synthetic.new/openai/v1/models';

  constructor() {
    super();
    this.httpClient = axios.create({
      timeout: 60000, // 60 seconds timeout
      headers: {
        'Content-Type': 'application/json',
        'User-Agent': 'aimux/2.0.0',
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
   * Authenticate with Synthetic.New API
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

      // Make a simple request to validate the API key
      const response = await this.httpClient.get(
        `${this.config?.baseUrl || this.DEFAULT_MODELS_URL}`,
        {
          headers: {
            Authorization: `Bearer ${keyToUse}`,
          },
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
            plan: 'Synthetic.New Standard',
            limits: {
              requests_per_minute: 200,
              tokens_per_minute: 2000000,
              daily_requests: 10000,
            },
            verified_capabilities: ['thinking', 'tools'],
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
        error: error.message || 'Authentication request failed',
      };
    }
  }

  /**
   * Fetch available models from Synthetic.New
   */
  public async fetchModels(forceRefresh: boolean = false): Promise<EnhancedModelInfo[]> {
    const cacheKey = this.getCacheKey();

    // Check cache first unless force refresh is requested
    if (!forceRefresh && this.modelCache.has(cacheKey)) {
      return this.modelCache.get(cacheKey)!;
    }

    try {
      const response = await this.httpClient.get(
        `${this.config?.baseUrl || this.DEFAULT_MODELS_URL}`,
        {
          headers: {
            Authorization: `Bearer ${this.config?.apiKey}`,
          },
        }
      );

      if (response.status !== 200) {
        throw new ProviderError(
          `Failed to fetch models: HTTP ${response.status}`,
          this.name,
          response.status
        );
      }

      const models = this.normalizeModels(response.data.data || []);
      this.modelCache.set(cacheKey, models);

      return models;
    } catch (error: any) {
      throw this.handleError(error);
    }
  }

  /**
   * Validate API key format and basic connectivity
   */
  public async validateApiKey(apiKey: string): Promise<boolean> {
    if (!apiKey || typeof apiKey !== 'string' || apiKey.length < 20) {
      return false;
    }

    try {
      const response = await this.httpClient.get(
        `${this.config?.baseUrl || this.DEFAULT_MODELS_URL}`,
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
   * Make an AI request to Synthetic.New
   */
  public async makeRequest(request: AIRequest): Promise<AIResponse> {
    if (!this.config?.apiKey) {
      throw new ProviderError('Provider not configured with API key', this.name);
    }

    const startTime = Date.now();

    try {
      // Transform the request to Synthetic.New format
      const syntheticRequest = this.transformRequest(request);

      // Use the appropriate endpoint based on the request type
      const endpoint = this.getEndpointForRequest(request);

      const response: AxiosResponse = await this.httpClient.post(endpoint, syntheticRequest, {
        timeout: this.config?.timeout || 120000, // 2 minutes default
        signal: request.metadata?.timeout
          ? AbortSignal.timeout(request.metadata.timeout)
          : undefined,
      });

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

      // Basic connectivity check
      const response = await this.httpClient.get(
        `${this.config?.baseUrl || this.DEFAULT_MODELS_URL}`,
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
          uptime_percentage: 99.7, // Placeholder
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
              model: 'claude-3-haiku-20240307',
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
   * Get the appropriate endpoint for the request type
   */
  private getEndpointForRequest(request: AIRequest): string {
    const baseUrl = this.config?.baseUrl || this.DEFAULT_BASE_URL;
    const anthropicUrl = this.DEFAULT_ANTHROPIC_URL;

    // Use Anthropic-compatible endpoint for Claude models
    if (request.model.toLowerCase().includes('claude')) {
      return `${anthropicUrl}/v1/messages`;
    }

    // Use OpenAI-compatible endpoint for other models
    return `${baseUrl}/v1/chat/completions`;
  }

  /**
   * Normalize Synthetic.New models to our standard format
   */
  protected normalizeModels(models: any[]): EnhancedModelInfo[] {
    return models.map((model: any) => ({
      id: model.id,
      name:
        model.id?.replace(/-/g, ' ').replace(/\b\w/g, (l: string) => l.toUpperCase()) || model.id,
      object: model.object || 'model',
      created: model.created || Date.now() / 1000,
      owned_by: model.owned_by || 'synthetic-new',
      provider: 'synthetic-new',
      capabilities: {
        ...this.capabilities,
        // Adjust capabilities per model if needed
        thinking: model.id?.includes('claude') || model.id?.includes('thinking'),
        vision: false, // Synthetic.New doesn't support vision
        tools: !model.id?.includes('simple'),
      },
      context_length:
        model.context_length || (model.id?.includes('claude-3-opus') ? 200000 : 100000),
      max_output_length: model.max_output_tokens || 8000,
      pricing: {
        prompt: model.pricing?.prompt || '0.0005',
        completion: model.pricing?.completion || '0.0015',
        request: model.pricing?.request || '0.001',
      },
      specialized: {
        thinking: model.id?.includes('claude-3-opus') || model.id?.includes('thinking'),
        coding: model.id?.includes('claude-3-sonnet') || model.id?.includes('coding'),
        reasoning: model.id?.includes('claude'),
        creative: model.id?.includes('claude-3-haiku') || model.id?.includes('creative'),
      },
    }));
  }

  /**
   * Transform standard AI request to Synthetic.New format
   */
  protected transformRequest(request: AIRequest): any {
    const isClaudeModel = request.model.toLowerCase().includes('claude');

    let syntheticRequest: any;

    if (isClaudeModel) {
      // Anthropic-compatible format for Claude models
      syntheticRequest = {
        model: request.model,
        messages: request.messages.map(msg => ({
          role: msg.role,
          content:
            typeof msg.content === 'string' ? { type: 'text', text: msg.content } : msg.content,
        })),
        max_tokens: request.max_tokens || 1024,
        stream: request.stream || false,
      };

      // Add optional parameters for Claude
      if (request.temperature !== undefined) {
        syntheticRequest.temperature = request.temperature;
      }

      if (request.top_p !== undefined) {
        syntheticRequest.top_p = request.top_p;
      }

      if (request.stop) {
        syntheticRequest.stop_sequences = Array.isArray(request.stop)
          ? request.stop
          : [request.stop];
      }

      // Tools support for Claude
      if (request.tools && request.tools.length > 0) {
        syntheticRequest.tools = request.tools.map(tool => ({
          name: tool.function.name,
          description: tool.function.description,
          input_schema: tool.function.parameters,
        }));

        if (request.tool_choice) {
          if (request.tool_choice === 'auto') {
            syntheticRequest.tool_choice = { type: 'auto' };
          } else if (request.tool_choice === 'required') {
            syntheticRequest.tool_choice = { type: 'any' };
          } else if (typeof request.tool_choice === 'object') {
            syntheticRequest.tool_choice = {
              type: 'tool',
              name: request.tool_choice.function.name,
            };
          }
        }
      }
    } else {
      // OpenAI-compatible format for other models
      syntheticRequest = {
        model: request.model,
        messages: request.messages,
        stream: request.stream || false,
      };

      // Add optional parameters for OpenAI-compatible models
      if (request.max_tokens !== undefined) {
        syntheticRequest.max_tokens = request.max_tokens;
      }

      if (request.temperature !== undefined) {
        syntheticRequest.temperature = request.temperature;
      }

      if (request.top_p !== undefined) {
        syntheticRequest.top_p = request.top_p;
      }

      if (request.stop) {
        syntheticRequest.stop = request.stop;
      }

      // Tools support for OpenAI-compatible models
      if (request.tools && request.tools.length > 0) {
        syntheticRequest.tools = request.tools;
      }

      if (request.tool_choice) {
        syntheticRequest.tool_choice = request.tool_choice;
      }
    }

    return syntheticRequest;
  }

  /**
   * Transform Synthetic.New response to standard AI response format
   */
  protected transformResponse(response: AxiosResponse, startTime: number): AIResponse {
    const data = response.data;
    const responseTime = Date.now() - startTime;

    // Handle different response formats
    if (data.content && Array.isArray(data.content)) {
      // Anthropic Claude format
      const content = data.content.find((item: any) => item.type === 'text');

      return {
        id: data.id || `synthetic-${Date.now()}`,
        object: 'chat.completion',
        created: Math.floor(Date.now() / 1000),
        model: data.model || 'unknown',
        choices: [
          {
            index: 0,
            message: {
              role: 'assistant',
              content: content?.text || '',
              tool_calls: data.content
                .filter((item: any) => item.type === 'tool_use')
                .map((tool: any) => ({
                  id: tool.id,
                  type: 'function',
                  function: {
                    name: tool.name,
                    arguments: JSON.stringify(tool.input),
                  },
                })),
            },
            finish_reason: data.stop_reason || 'stop',
          },
        ],
        usage: data.usage
          ? {
              prompt_tokens: data.usage.input_tokens,
              completion_tokens: data.usage.output_tokens,
              total_tokens: data.usage.input_tokens + data.usage.output_tokens,
            }
          : undefined,
        provider: this.name,
        response_time: responseTime,
        metadata: {
          cached: response.headers['x-cached'] === 'true',
          routing_decision: 'synthetic-new',
          fallback_used: false,
          failover_attempts: 0,
        },
      };
    } else {
      // OpenAI-compatible format
      return {
        id: data.id || `synthetic-${Date.now()}`,
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
          routing_decision: 'synthetic-new',
          fallback_used: false,
          failover_attempts: 0,
        },
      };
    }
  }

  /**
   * Handle provider-specific errors
   */
  protected handleError(error: any): ProviderError {
    if (error.response) {
      // API returned an error response
      const statusCode = error.response.status;
      const message =
        error.response.data?.error?.message ||
        error.response.data?.message ||
        error.response.data?.error?.type ||
        error.response.statusText;

      return new ProviderError(`Synthetic.New API error: ${message}`, this.name, statusCode, error);
    } else if (error.code === 'ECONNABORTED') {
      // Timeout error
      return new ProviderError('Synthetic.New request timeout', this.name, 408, error);
    } else if (error.code === 'ENOTFOUND' || error.code === 'ECONNREFUSED') {
      // Network error
      return new ProviderError('Synthetic.New network error', this.name, 503, error);
    } else {
      // Other errors
      return new ProviderError(
        `Synthetic.New provider error: ${error.message}`,
        this.name,
        500,
        error
      );
    }
  }

  /**
   * Get default configuration for Synthetic.New
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
      priority: 3, // Highest priority for cost effectiveness
      customization: {
        // Synthetic.New specific customizations can go here
        preferredFormat: 'anthropic',
      },
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
    const requestsPerMinute = parseInt(headers['x-ratelimit-limit-requests']) || 200;
    const requestsRemaining =
      parseInt(headers['x-ratelimit-remaining-requests']) || requestsPerMinute;
    const tokensPerMinute = parseInt(headers['x-ratelimit-limit-tokens']) || 2000000;
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
