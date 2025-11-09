import { z } from 'zod';

// Provider-specific configuration schema
export const ProviderConfigSchema = z.object({
  name: z.string().describe('Display name of the provider'),
  apiKey: z.string().default('').describe('API key for the provider'),
  baseUrl: z.string().optional().describe('Custom base URL for the provider'),
  anthropicBaseUrl: z.string().optional().describe('Anthropic-compatible endpoint'),
  modelsApiUrl: z.string().optional().describe('OpenAI-compatible models endpoint'),
  models: z.array(z.string()).optional().describe('Available models for this provider'),
  capabilities: z
    .array(z.enum(['thinking', 'vision', 'tools']))
    .optional()
    .describe('Provider capabilities'),
  enabled: z.boolean().default(true).describe('Whether this provider is enabled'),
  priority: z.number().min(1).max(10).default(5).describe('Provider priority for failover'),
  timeout: z.number().min(1000).max(300000).default(30000).describe('Request timeout in ms'),
  maxRetries: z.number().min(0).max(5).default(3).describe('Maximum retry attempts'),
});

// Routing configuration schema
export const RoutingConfigSchema = z.object({
  defaultProvider: z
    .string()
    .default('synthetic-new')
    .describe('Default provider for all requests'),
  thinkingProvider: z.string().optional().describe('Provider for thinking-capable requests'),
  visionProvider: z.string().optional().describe('Provider for vision-capable requests'),
  toolsProvider: z.string().optional().describe('Provider for tool-using requests'),
  strategy: z
    .enum(['capability', 'cost', 'speed', 'reliability'])
    .default('capability')
    .describe('Routing strategy'),
  enableFailover: z.boolean().default(true).describe('Enable automatic failover'),
  loadBalancing: z.boolean().default(false).describe('Enable load balancing across providers'),
});

// Fallback configuration schema
export const FallbackConfigSchema = z.object({
  enabled: z.boolean().default(true).describe('Enable fallback mechanism'),
  retryDelay: z.number().min(100).max(10000).default(1000).describe('Delay between retries in ms'),
  maxFailures: z
    .number()
    .min(1)
    .max(10)
    .default(3)
    .describe('Max failures before switching providers'),
  healthCheck: z.boolean().default(true).describe('Enable periodic health checks'),
  healthCheckInterval: z
    .number()
    .min(30000)
    .max(300000)
    .default(60000)
    .describe('Health check interval in ms'),
});

// Multi-provider configuration schema
export const MultiProviderConfigSchema = z.object({
  providers: z
    .object({
      'minimax-m2': ProviderConfigSchema.optional(),
      'z-ai': ProviderConfigSchema.optional(),
      'synthetic-new': ProviderConfigSchema.optional(),
    })
    .describe('Multi-provider configurations'),
  routing: RoutingConfigSchema.describe('Routing configuration'),
  fallback: FallbackConfigSchema.describe('Fallback configuration'),
});

// Legacy AppConfig for backward compatibility
export const AppConfigSchema = z.object({
  // Legacy single-provider fields (keep for backward compatibility)
  apiKey: z.string().default('').describe('Default API key (fallback)'),
  baseUrl: z.string().default('https://api.synthetic.new').describe('Default API base URL'),
  anthropicBaseUrl: z
    .string()
    .default('https://api.synthetic.new/anthropic')
    .describe('Anthropic-compatible API endpoint'),
  modelsApiUrl: z
    .string()
    .default('https://api.synthetic.new/openai/v1/models')
    .describe('OpenAI-compatible models endpoint'),

  // Common fields
  cacheDurationHours: z
    .number()
    .int()
    .min(1)
    .max(168)
    .default(24)
    .describe('Model cache duration in hours'),
  selectedModel: z.string().default('').describe('Last selected model'),
  selectedThinkingModel: z.string().default('').describe('Last selected thinking model'),
  firstRunCompleted: z
    .boolean()
    .default(false)
    .describe('Whether first-time setup has been completed'),

  // Enhanced multi-provider configuration
  multiProvider: MultiProviderConfigSchema.optional().describe('Multi-provider configuration'),

  // Legacy fields for migration
  providers: z
    .record(ProviderConfigSchema)
    .optional()
    .describe('Legacy providers configuration (deprecated)'),
  routingRules: z.record(z.string()).optional().describe('Legacy routing rules (deprecated)'),
});

export type AppConfig = z.infer<typeof AppConfigSchema>;
export type ProviderConfig = z.infer<typeof ProviderConfigSchema>;
export type ConfigRoutingConfig = z.infer<typeof RoutingConfigSchema>;
export type FallbackConfig = z.infer<typeof FallbackConfigSchema>;
export type MultiProviderConfig = z.infer<typeof MultiProviderConfigSchema>;

// Backward compatibility alias
export type RoutingConfig = ConfigRoutingConfig;

export class ConfigValidationError extends Error {
  constructor(
    message: string,
    public override cause?: unknown
  ) {
    super(message);
    this.name = 'ConfigValidationError';
  }
}

export class ConfigLoadError extends Error {
  constructor(
    message: string,
    public override cause?: unknown
  ) {
    super(message);
    this.name = 'ConfigLoadError';
  }
}

export class ConfigSaveError extends Error {
  constructor(
    message: string,
    public override cause?: unknown
  ) {
    super(message);
    this.name = 'ConfigSaveError';
  }
}
