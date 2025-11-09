"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.ZAIProvider = void 0;
const axios_1 = __importDefault(require("axios"));
const base_provider_1 = require("./base-provider");
const types_1 = require("./types");
/**
 * Z.AI Provider Implementation
 *
 * Provides integration with Z.AI API, supporting:
 * - Vision capabilities (image analysis)
 * - Tool calling (function execution)
 * - Up to 100K tokens context
 * - No thinking capabilities
 */
class ZAIProvider extends base_provider_1.BaseProvider {
    name = 'Z.AI';
    id = 'z-ai';
    capabilities = {
        thinking: false,
        vision: true,
        tools: true,
        maxTokens: 100000,
        streaming: true,
        systemMessages: true,
        temperature: true,
        topP: true,
        maxOutputTokens: 4000,
    };
    httpClient;
    DEFAULT_BASE_URL = 'https://api.z.ai/api/anthropic/v1';
    DEFAULT_MODELS_URL = undefined; // No models endpoint available
    constructor() {
        super();
        this.httpClient = axios_1.default.create({
            timeout: 60000, // 60 seconds timeout
            headers: {
                'Content-Type': 'application/json',
            },
        });
        // Add request interceptor for rate limiting and error handling
        this.httpClient.interceptors.request.use(async (config) => {
            // Check rate limiting before making request
            if (!this.canMakeRequest()) {
                throw new types_1.ProviderError('Rate limit exceeded', this.name, 429);
            }
            // Add API key if configured
            if (this.config?.apiKey) {
                config.headers.Authorization = `Bearer ${this.config.apiKey}`;
            }
            return config;
        }, error => Promise.reject(error));
        // Add response interceptor for rate limit handling
        this.httpClient.interceptors.response.use(response => {
            // Update rate limiting info from response headers
            this.updateRateLimitFromHeaders(response.headers);
            return response;
        }, error => {
            if (error.response?.status === 429) {
                this.updateRateLimitFromHeaders(error.response.headers);
            }
            return Promise.reject(error);
        });
    }
    /**
     * Authenticate with Z.AI API
     */
    async authenticate(apiKey) {
        try {
            const keyToUse = apiKey || this.config?.apiKey;
            if (!keyToUse) {
                return {
                    success: false,
                    error: 'API key is required for authentication',
                };
            }
            // Make a minimal messages request to validate the API key
            const response = await this.httpClient.post(`${this.config?.baseUrl || this.DEFAULT_BASE_URL}/messages`, {
                model: 'glm-4.6', // Default model from Z.AI Anthropic endpoint
                max_tokens: 1,
                messages: [{ role: 'user', content: 'hi' }],
            }, {
                headers: {
                    Authorization: `Bearer ${keyToUse}`,
                },
                timeout: 10000,
            });
            if (response.status === 200) {
                // Update config if new API key was provided
                if (apiKey && apiKey !== this.config?.apiKey) {
                    this.config = { ...this.config, apiKey };
                }
                return {
                    success: true,
                    details: {
                        verified_capabilities: ['vision', 'tools'],
                        limits: {
                            requests_per_minute: 100,
                            tokens_per_minute: 500000,
                        },
                    },
                };
            }
            else {
                return {
                    success: false,
                    error: `Authentication failed with status: ${response.status}`,
                };
            }
        }
        catch (error) {
            return {
                success: false,
                error: error.response?.data?.error?.message || error.message || 'Authentication request failed',
            };
        }
    }
    /**
     * Fetch available models from Z.AI
     * Returns hardcoded Z.AI models since no models endpoint exists
     */
    async fetchModels(forceRefresh = false) {
        const cacheKey = this.getCacheKey();
        // Check cache first unless force refresh is requested
        if (!forceRefresh && this.modelCache.has(cacheKey)) {
            return this.modelCache.get(cacheKey);
        }
        // Return hardcoded Z.AI models based on the environment variables provided
        const hardcodedModels = [
            {
                id: 'glm-4.6',
                object: 'model',
                created: Math.floor(Date.now() / 1000),
                owned_by: 'z-ai',
            },
            {
                id: 'glm-4.5-air',
                object: 'model',
                created: Math.floor(Date.now() / 1000),
                owned_by: 'z-ai',
            },
        ];
        const models = this.normalizeModels(hardcodedModels);
        this.modelCache.set(cacheKey, models);
        return models;
    }
    /**
     * Validate API key format and basic connectivity
     */
    async validateApiKey(apiKey) {
        if (!apiKey || typeof apiKey !== 'string' || apiKey.length < 20) {
            return false;
        }
        try {
            const response = await this.httpClient.post(`${this.config?.baseUrl || this.DEFAULT_BASE_URL}/messages`, {
                model: 'glm-4.6',
                max_tokens: 1,
                messages: [{ role: 'user', content: 'hi' }],
            }, {
                headers: {
                    Authorization: `Bearer ${apiKey}`,
                },
                timeout: 10000, // Shorter timeout for validation
            });
            return response.status === 200;
        }
        catch {
            return false;
        }
    }
    /**
     * Make an AI request to Z.AI
     */
    async makeRequest(request) {
        if (!this.config?.apiKey) {
            throw new types_1.ProviderError('Provider not configured with API key', this.name);
        }
        const startTime = Date.now();
        try {
            // Transform the request to Z.AI format
            const zaiRequest = this.transformRequest(request);
            const response = await this.httpClient.post(`${this.config?.baseUrl || this.DEFAULT_BASE_URL}/messages`, zaiRequest, {
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
        }
        catch (error) {
            throw this.handleError(error);
        }
    }
    /**
     * Perform health check on the provider
     */
    async healthCheck(performFullCheck = false) {
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
            const response = await this.httpClient.post(`${this.config?.baseUrl || this.DEFAULT_BASE_URL}/messages`, {
                model: 'glm-4.6',
                max_tokens: 1,
                messages: [{ role: 'user', content: 'ping' }],
            }, {
                timeout: 10000,
                headers: {
                    Authorization: `Bearer ${this.config.apiKey}`,
                },
            });
            const responseTime = Date.now() - startTime;
            if (response.status === 200) {
                const health = {
                    status: 'healthy',
                    response_time: responseTime,
                    last_check: new Date(),
                    uptime_percentage: 99.5, // Placeholder
                    capabilities_status: {
                        thinking: 'unavailable',
                        vision: 'available',
                        tools: 'available',
                    },
                };
                // Perform full check if requested
                if (performFullCheck) {
                    try {
                        // Try a minimal chat completion request
                        await this.makeRequest({
                            model: 'glm-4.6',
                            messages: [{ role: 'user', content: 'ping' }],
                            max_tokens: 1,
                        });
                    }
                    catch (error) {
                        health.status = 'degraded';
                        health.error_message = `Chat completion failed: ${error.message}`;
                    }
                }
                this.lastHealthCheck = health;
                return health;
            }
            else {
                const health = {
                    status: 'unhealthy',
                    response_time: responseTime,
                    last_check: new Date(),
                    error_message: `HTTP ${response.status}: ${response.statusText}`,
                };
                this.lastHealthCheck = health;
                return health;
            }
        }
        catch (error) {
            const health = {
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
     * Normalize Z.AI models to our standard format
     */
    normalizeModels(models) {
        return models.map((model) => ({
            id: model.id,
            name: model.id === 'glm-4.6' ? 'GLM-4.6' : 'GLM-4.5 Air',
            object: model.object || 'model',
            created: model.created || Date.now() / 1000,
            owned_by: model.owned_by || 'z-ai',
            provider: 'z-ai',
            capabilities: {
                ...this.capabilities,
                // Z.AI GLM models support vision and tools but not thinking
                thinking: false,
                vision: true, // GLM models support vision
                tools: true, // GLM models support tools
            },
            context_length: 100000, // GLM context length
            max_output_length: 4000,
            pricing: {
                prompt: '0.002', // Placeholder pricing
                completion: '0.004',
                image: '0.015',
            },
            specialized: {
                thinking: false,
                coding: model.id === 'glm-4.6', // 4.6 is better for coding
                reasoning: false,
                creative: model.id === 'glm-4.6', // 4.6 is more creative
            },
        }));
    }
    /**
     * Transform standard AI request to Z.AI Anthropic-compatible format
     */
    transformRequest(request) {
        const zaiRequest = {
            model: request.model.replace('zai-', '') || 'glm-4.6', // Default to glm-4.6
            messages: request.messages.map(msg => {
                const transformed = {
                    role: msg.role,
                };
                if (typeof msg.content === 'string') {
                    transformed.content = msg.content;
                }
                else if (Array.isArray(msg.content)) {
                    // Handle mixed content (text + images) - Z.AI supports vision
                    transformed.content = msg.content.map(part => {
                        if (part.type === 'text') {
                            return { type: 'text', text: part.text };
                        }
                        else if (part.type === 'image_url') {
                            return {
                                type: 'image_url',
                                image_url: {
                                    url: part.image_url.url,
                                    detail: part.image_url.detail || 'auto',
                                },
                            };
                        }
                        return part;
                    });
                }
                if (msg.name) {
                    transformed.name = msg.name;
                }
                // Handle tool calls and tool responses
                if (msg.tool_calls) {
                    transformed.tool_calls = msg.tool_calls;
                }
                if (msg.tool_call_id) {
                    transformed.tool_call_id = msg.tool_call_id;
                }
                return transformed;
            }),
            stream: request.stream || false,
        };
        // Add required parameters
        if (request.max_tokens !== undefined) {
            zaiRequest.max_tokens = request.max_tokens;
        }
        else {
            zaiRequest.max_tokens = 4000; // Default for Z.AI GLM models
        }
        // Add optional parameters
        if (request.temperature !== undefined) {
            zaiRequest.temperature = request.temperature;
        }
        if (request.top_p !== undefined) {
            zaiRequest.top_p = request.top_p;
        }
        if (request.stop) {
            zaiRequest.stop = request.stop;
        }
        // Tool calling support using Anthropic format
        if (request.tools && request.tools.length > 0) {
            zaiRequest.tools = request.tools.map(tool => ({
                type: tool.type,
                function: {
                    name: tool.function.name,
                    description: tool.function.description,
                    parameters: tool.function.parameters,
                },
            }));
        }
        if (request.tool_choice) {
            zaiRequest.tool_choice = request.tool_choice;
        }
        return zaiRequest;
    }
    /**
     * Transform Z.AI response to standard AI response format
     */
    transformResponse(response, startTime) {
        const data = response.data;
        const responseTime = Date.now() - startTime;
        // Handle Anthropic-style message response (Z.AI format)
        if (data.content && Array.isArray(data.content)) {
            const messageContent = data.content
                .filter((item) => item.type === 'text')
                .map((item) => item.text)
                .join('\n');
            return {
                id: data.id || `zai-${Date.now()}`,
                object: data.object || 'chat.completion',
                created: data.created || Math.floor(Date.now() / 1000),
                model: data.model || 'glm-4.6',
                choices: [{
                        index: 0,
                        message: {
                            role: data.role || 'assistant',
                            content: messageContent,
                        },
                        finish_reason: data.stop_reason || 'stop',
                    }],
                usage: data.usage
                    ? {
                        prompt_tokens: data.usage.input_tokens || data.usage.prompt_tokens,
                        completion_tokens: data.usage.output_tokens || data.usage.completion_tokens,
                        total_tokens: data.usage.total_tokens,
                    }
                    : undefined,
                provider: this.name,
                response_time: responseTime,
                metadata: {
                    cached: response.headers['x-cached'] === 'true',
                    routing_decision: 'z-ai',
                    fallback_used: false,
                    failover_attempts: 0,
                },
            };
        }
        // Fallback to OpenAI-style format if needed
        return {
            id: data.id || `zai-${Date.now()}`,
            object: data.object || 'chat.completion',
            created: data.created || Math.floor(Date.now() / 1000),
            model: data.model || 'glm-4.6',
            choices: (data.choices || []).map((choice) => ({
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
                routing_decision: 'z-ai',
                fallback_used: false,
                failover_attempts: 0,
            },
        };
    }
    /**
     * Handle provider-specific errors
     */
    handleError(error) {
        if (error.response) {
            // API returned an error response
            const statusCode = error.response.status;
            const message = error.response.data?.error?.message ||
                error.response.data?.message ||
                error.response.statusText;
            return new types_1.ProviderError(`Z.AI API error: ${message}`, this.name, statusCode, error);
        }
        else if (error.code === 'ECONNABORTED') {
            // Timeout error
            return new types_1.ProviderError('Z.AI request timeout', this.name, 408, error);
        }
        else if (error.code === 'ENOTFOUND' || error.code === 'ECONNREFUSED') {
            // Network error
            return new types_1.ProviderError('Z.AI network error', this.name, 503, error);
        }
        else {
            // Other errors
            return new types_1.ProviderError(`Z.AI provider error: ${error.message}`, this.name, 500, error);
        }
    }
    /**
     * Get default configuration for Z.AI
     */
    getDefaultConfig() {
        return {
            name: this.name,
            baseUrl: this.DEFAULT_BASE_URL,
            modelsUrl: this.DEFAULT_MODELS_URL,
            timeout: 120000,
            maxRetries: 3,
            retryDelay: 1000,
            capabilities: this.capabilities,
            enabled: false,
            priority: 2, // Lower priority than Minimax for cost reasons
        };
    }
    /**
     * Validate provider configuration
     */
    validateConfig(config) {
        return !!(config.name &&
            config.apiKey &&
            typeof config.apiKey === 'string' &&
            config.apiKey.length >= 20);
    }
    /**
     * Clean up resources
     */
    async cleanup() {
        // Clear any pending requests
        this.modelCache.clear();
        this.lastHealthCheck = undefined;
    }
    /**
     * Update rate limiting information from response headers
     */
    updateRateLimitFromHeaders(headers) {
        const requestsPerMinute = parseInt(headers['x-ratelimit-limit-requests']) || 100;
        const requestsRemaining = parseInt(headers['x-ratelimit-remaining-requests']) || requestsPerMinute;
        const tokensPerMinute = parseInt(headers['x-ratelimit-limit-tokens']) || 500000;
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
exports.ZAIProvider = ZAIProvider;
//# sourceMappingURL=zai-provider.js.map