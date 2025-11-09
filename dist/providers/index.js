"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.DEFAULT_RETRY_CONFIG = exports.DEFAULT_REQUEST_TIMEOUT = exports.PROVIDER_SYSTEM_VERSION = exports.ProviderError = exports.SyntheticProvider = exports.ZAIProvider = exports.MinimaxM2Provider = exports.ProviderRegistry = exports.BaseProvider = void 0;
exports.createProviderRegistry = createProviderRegistry;
exports.validateProviderConfig = validateProviderConfig;
exports.createAIRequest = createAIRequest;
exports.createMessage = createMessage;
exports.createContentWithImage = createContentWithImage;
exports.createTool = createTool;
exports.determineRequestType = determineRequestType;
exports.delay = delay;
exports.retryWithBackoff = retryWithBackoff;
// Core provider classes
const base_provider_1 = require("./base-provider");
Object.defineProperty(exports, "BaseProvider", { enumerable: true, get: function () { return base_provider_1.BaseProvider; } });
const provider_registry_1 = require("./provider-registry");
Object.defineProperty(exports, "ProviderRegistry", { enumerable: true, get: function () { return provider_registry_1.ProviderRegistry; } });
// Provider implementations
const minimax_provider_1 = require("./minimax-provider");
Object.defineProperty(exports, "MinimaxM2Provider", { enumerable: true, get: function () { return minimax_provider_1.MinimaxM2Provider; } });
const zai_provider_1 = require("./zai-provider");
Object.defineProperty(exports, "ZAIProvider", { enumerable: true, get: function () { return zai_provider_1.ZAIProvider; } });
const synthetic_provider_1 = require("./synthetic-provider");
Object.defineProperty(exports, "SyntheticProvider", { enumerable: true, get: function () { return synthetic_provider_1.SyntheticProvider; } });
// Error classes
const types_1 = require("./types");
Object.defineProperty(exports, "ProviderError", { enumerable: true, get: function () { return types_1.ProviderError; } });
/**
 * Create and configure a new provider registry
 * @returns A configured ProviderRegistry instance
 */
function createProviderRegistry() {
    const registry = new provider_registry_1.ProviderRegistry();
    // Start health monitoring by default
    registry.startHealthMonitoring(30000); // 30 seconds
    return registry;
}
/**
 * Validate provider configuration against schema
 * @param config Configuration to validate
 * @returns True if valid, throws error if invalid
 */
function validateProviderConfig(config) {
    if (!config || typeof config !== 'object') {
        throw new Error('Provider config must be an object');
    }
    if (typeof config.name !== 'string' || !config.name.trim()) {
        throw new Error('Provider name is required and must be a non-empty string');
    }
    if (typeof config.apiKey !== 'string' || !config.apiKey.trim()) {
        throw new Error('Provider API key is required and must be a non-empty string');
    }
    if (typeof config.enabled !== 'boolean') {
        throw new Error('Provider enabled status must be a boolean');
    }
    if (!config.capabilities || typeof config.capabilities !== 'object') {
        throw new Error('Provider capabilities are required');
    }
    return true;
}
/**
 * Create a standard AI request object
 * @param params Request parameters
 * @returns A standardized AI request
 */
function createAIRequest(params) {
    return {
        model: params.model,
        messages: params.messages,
        max_tokens: params.max_tokens,
        temperature: params.temperature,
        top_p: params.top_p,
        stream: params.stream || false,
        tools: params.tools,
        tool_choice: params.tool_choice,
        metadata: params.metadata || {},
    };
}
/**
 * Create a standard message object
 * @param role Message role
 * @param content Message content
 * @param name Optional message name
 * @returns A standardized message object
 */
function createMessage(role, content, name) {
    const message = {
        role,
        content,
    };
    if (name) {
        message.name = name;
    }
    return message;
}
/**
 * Create a text message with optional image URL
 * @param content Text content
 * @param imageUrl Optional image URL
 * @returns Content array for message
 */
function createContentWithImage(content, imageUrl, imageDetail) {
    if (!imageUrl) {
        return content;
    }
    return [
        {
            type: 'text',
            text: content,
        },
        {
            type: 'image_url',
            image_url: {
                url: imageUrl,
                detail: imageDetail || 'auto',
            },
        },
    ];
}
/**
 * Create a tool definition
 * @param name Tool name
 * @param description Tool description
 * @param parameters Tool parameters schema
 * @returns A tool definition object
 */
function createTool(name, description, parameters) {
    return {
        type: 'function',
        function: {
            name,
            description,
            parameters,
        },
    };
}
/**
 * Determine request type based on message content and other factors
 * @param request The AI request to analyze
 * @returns The determined request type
 */
function determineRequestType(request) {
    // Check for tools
    if (request.tools && request.tools.length > 0) {
        return 'tools';
    }
    // Check for vision content
    for (const message of request.messages) {
        if (Array.isArray(message.content)) {
            for (const part of message.content) {
                if (part.type === 'image_url') {
                    return 'vision';
                }
            }
        }
    }
    // Check for thinking indicators (simple heuristic)
    const textContent = request.messages
        .map((msg) => Array.isArray(msg.content) ? msg.content.map((p) => p.text || '').join('') : msg.content)
        .join(' ')
        .toLowerCase();
    const thinkingKeywords = [
        'think step by step',
        'let me think',
        'reasoning',
        'analyze',
        'consider',
        'evaluate',
        'step by step',
        'thought process',
    ];
    if (thinkingKeywords.some(keyword => textContent.includes(keyword))) {
        return 'thinking';
    }
    return 'regular';
}
/**
 * Provider system version
 */
exports.PROVIDER_SYSTEM_VERSION = '1.0.0';
/**
 * Default timeout for provider requests (in milliseconds)
 */
exports.DEFAULT_REQUEST_TIMEOUT = 30000;
/**
 * Default retry configuration
 */
exports.DEFAULT_RETRY_CONFIG = {
    maxRetries: 3,
    initialDelayMs: 1000,
    maxDelayMs: 10000,
    backoffMultiplier: 2,
};
/**
 * Utility to delay execution
 * @param ms Milliseconds to delay
 * @returns Promise that resolves after delay
 */
function delay(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
}
/**
 * Retry a function with exponential backoff
 * @param fn Function to retry
 * @param config Retry configuration
 * @returns Promise resolving to function result
 */
async function retryWithBackoff(fn, config = {}) {
    const finalConfig = { ...exports.DEFAULT_RETRY_CONFIG, ...config };
    let lastError;
    for (let attempt = 0; attempt <= finalConfig.maxRetries; attempt++) {
        try {
            return await fn();
        }
        catch (error) {
            lastError = error;
            if (attempt === finalConfig.maxRetries) {
                break;
            }
            const delayMs = Math.min(finalConfig.initialDelayMs * Math.pow(finalConfig.backoffMultiplier, attempt), finalConfig.maxDelayMs);
            console.warn(`Attempt ${attempt + 1} failed, retrying in ${delayMs}ms:`, error);
            await delay(delayMs);
        }
    }
    throw lastError;
}
//# sourceMappingURL=index.js.map