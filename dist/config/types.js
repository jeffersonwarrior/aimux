"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.ConfigSaveError = exports.ConfigLoadError = exports.ConfigValidationError = exports.AppConfigSchema = exports.MultiProviderConfigSchema = exports.FallbackConfigSchema = exports.RoutingConfigSchema = exports.ProviderConfigSchema = void 0;
const zod_1 = require("zod");
// Provider-specific configuration schema
exports.ProviderConfigSchema = zod_1.z.object({
    name: zod_1.z.string().describe('Display name of the provider'),
    apiKey: zod_1.z.string().default('').describe('API key for the provider'),
    baseUrl: zod_1.z.string().optional().describe('Custom base URL for the provider'),
    anthropicBaseUrl: zod_1.z.string().optional().describe('Anthropic-compatible endpoint'),
    modelsApiUrl: zod_1.z.string().optional().describe('OpenAI-compatible models endpoint'),
    models: zod_1.z.array(zod_1.z.string()).optional().describe('Available models for this provider'),
    capabilities: zod_1.z
        .array(zod_1.z.enum(['thinking', 'vision', 'tools']))
        .optional()
        .describe('Provider capabilities'),
    enabled: zod_1.z.boolean().default(true).describe('Whether this provider is enabled'),
    priority: zod_1.z.number().min(1).max(10).default(5).describe('Provider priority for failover'),
    timeout: zod_1.z.number().min(1000).max(300000).default(30000).describe('Request timeout in ms'),
    maxRetries: zod_1.z.number().min(0).max(5).default(3).describe('Maximum retry attempts'),
});
// Routing configuration schema
exports.RoutingConfigSchema = zod_1.z.object({
    defaultProvider: zod_1.z
        .string()
        .default('synthetic-new')
        .describe('Default provider for all requests'),
    thinkingProvider: zod_1.z.string().optional().describe('Provider for thinking-capable requests'),
    visionProvider: zod_1.z.string().optional().describe('Provider for vision-capable requests'),
    toolsProvider: zod_1.z.string().optional().describe('Provider for tool-using requests'),
    strategy: zod_1.z
        .enum(['capability', 'cost', 'speed', 'reliability'])
        .default('capability')
        .describe('Routing strategy'),
    enableFailover: zod_1.z.boolean().default(true).describe('Enable automatic failover'),
    loadBalancing: zod_1.z.boolean().default(false).describe('Enable load balancing across providers'),
});
// Fallback configuration schema
exports.FallbackConfigSchema = zod_1.z.object({
    enabled: zod_1.z.boolean().default(true).describe('Enable fallback mechanism'),
    retryDelay: zod_1.z.number().min(100).max(10000).default(1000).describe('Delay between retries in ms'),
    maxFailures: zod_1.z
        .number()
        .min(1)
        .max(10)
        .default(3)
        .describe('Max failures before switching providers'),
    healthCheck: zod_1.z.boolean().default(true).describe('Enable periodic health checks'),
    healthCheckInterval: zod_1.z
        .number()
        .min(30000)
        .max(300000)
        .default(60000)
        .describe('Health check interval in ms'),
});
// Multi-provider configuration schema
exports.MultiProviderConfigSchema = zod_1.z.object({
    providers: zod_1.z
        .object({
        'minimax-m2': exports.ProviderConfigSchema.optional(),
        'z-ai': exports.ProviderConfigSchema.optional(),
        'synthetic-new': exports.ProviderConfigSchema.optional(),
    })
        .describe('Multi-provider configurations'),
    routing: exports.RoutingConfigSchema.describe('Routing configuration'),
    fallback: exports.FallbackConfigSchema.describe('Fallback configuration'),
});
// Legacy AppConfig for backward compatibility
exports.AppConfigSchema = zod_1.z.object({
    // Legacy single-provider fields (keep for backward compatibility)
    apiKey: zod_1.z.string().default('').describe('Default API key (fallback)'),
    baseUrl: zod_1.z.string().default('https://api.synthetic.new').describe('Default API base URL'),
    anthropicBaseUrl: zod_1.z
        .string()
        .default('https://api.synthetic.new/anthropic')
        .describe('Anthropic-compatible API endpoint'),
    modelsApiUrl: zod_1.z
        .string()
        .default('https://api.synthetic.new/openai/v1/models')
        .describe('OpenAI-compatible models endpoint'),
    // Common fields
    cacheDurationHours: zod_1.z
        .number()
        .int()
        .min(1)
        .max(168)
        .default(24)
        .describe('Model cache duration in hours'),
    selectedModel: zod_1.z.string().default('').describe('Last selected model'),
    selectedThinkingModel: zod_1.z.string().default('').describe('Last selected thinking model'),
    firstRunCompleted: zod_1.z
        .boolean()
        .default(false)
        .describe('Whether first-time setup has been completed'),
    // Enhanced multi-provider configuration
    multiProvider: exports.MultiProviderConfigSchema.optional().describe('Multi-provider configuration'),
    // Legacy fields for migration
    providers: zod_1.z
        .record(exports.ProviderConfigSchema)
        .optional()
        .describe('Legacy providers configuration (deprecated)'),
    routingRules: zod_1.z.record(zod_1.z.string()).optional().describe('Legacy routing rules (deprecated)'),
});
class ConfigValidationError extends Error {
    cause;
    constructor(message, cause) {
        super(message);
        this.cause = cause;
        this.name = 'ConfigValidationError';
    }
}
exports.ConfigValidationError = ConfigValidationError;
class ConfigLoadError extends Error {
    cause;
    constructor(message, cause) {
        super(message);
        this.cause = cause;
        this.name = 'ConfigLoadError';
    }
}
exports.ConfigLoadError = ConfigLoadError;
class ConfigSaveError extends Error {
    cause;
    constructor(message, cause) {
        super(message);
        this.cause = cause;
        this.name = 'ConfigSaveError';
    }
}
exports.ConfigSaveError = ConfigSaveError;
//# sourceMappingURL=types.js.map