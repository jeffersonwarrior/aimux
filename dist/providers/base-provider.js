"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.BaseProvider = void 0;
/**
 * Abstract base class for all AI providers
 * Defines the common interface that all providers must implement
 */
class BaseProvider {
    /**
     * Provider configuration
     */
    config;
    /**
     * Whether the provider is enabled and ready to use
     */
    enabled = false;
    /**
     * Last health check result
     */
    lastHealthCheck;
    /**
     * Cache for models to avoid repeated fetching
     */
    modelCache = new Map();
    /**
     * Rate limiting information
     */
    rateLimitInfo;
    /**
     * Configure the provider with settings
     * @param config Provider configuration
     */
    configure(config) {
        this.config = config;
        this.enabled = config.enabled;
        this.capabilities = config.capabilities;
    }
    /**
     * Get the current provider configuration
     * @returns Current provider config or undefined if not configured
     */
    getConfig() {
        return this.config;
    }
    /**
     * Check if the provider has a specific capability
     * @param capability The capability to check
     * @returns True if provider supports the capability
     */
    hasCapability(capability) {
        return this.capabilities[capability] === true;
    }
    /**
     * Check if the provider can handle a specific request
     * @param request The AI request to evaluate
     * @returns True if provider can handle the request
     */
    canHandleRequest(request) {
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
        const hasVision = request.messages.some(msg => Array.isArray(msg.content) && msg.content.some(part => part.type === 'image_url'));
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
    getModel(modelId) {
        const cacheKey = this.getCacheKey();
        const models = this.modelCache.get(cacheKey) || [];
        return models.find(model => model.id === modelId);
    }
    /**
     * Rate limiting helper - check if a request can be made
     * @returns True if request can proceed, false if rate limited
     */
    canMakeRequest() {
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
    updateRateLimit(tokensUsed) {
        if (this.rateLimitInfo) {
            this.rateLimitInfo.requestsRemaining = Math.max(0, this.rateLimitInfo.requestsRemaining - 1);
            this.rateLimitInfo.tokensRemaining = Math.max(0, this.rateLimitInfo.tokensRemaining - tokensUsed);
        }
    }
    /**
     * Estimate tokens needed for a request (rough approximation)
     * @param request The AI request
     * @returns Estimated number of tokens
     */
    estimateTokens(request) {
        let tokens = 0;
        for (const message of request.messages) {
            if (typeof message.content === 'string') {
                tokens += Math.ceil(message.content.length / 4); // Rough estimate: 1 token ≈ 4 characters
            }
            else if (Array.isArray(message.content)) {
                for (const part of message.content) {
                    if (part.type === 'text' && part.text) {
                        tokens += Math.ceil(part.text.length / 4);
                    }
                    else if (part.type === 'image_url') {
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
    getCacheKey() {
        return `${this.id}-${this.config?.apiKey?.slice(-8) || 'no-key'}`;
    }
    /**
     * Clear cached models
     */
    clearModelCache() {
        this.modelCache.clear();
    }
    /**
     * Get last health check result
     * @returns Last health check or undefined
     */
    getLastHealthCheck() {
        return this.lastHealthCheck;
    }
    /**
     * Get provider information summary
     * @returns Provider information object
     */
    getInfo() {
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
    getPerformanceMetrics() {
        // This method should be implemented by concrete provider classes
        // to track their own performance metrics
        return undefined;
    }
}
exports.BaseProvider = BaseProvider;
//# sourceMappingURL=base-provider.js.map