import {
  ProviderConfig,
  ConfigRoutingConfig,
  MultiProviderConfig,
  ProviderConfigSchema,
  RoutingConfigSchema,
  FallbackConfigSchema,
  MultiProviderConfigSchema,
} from './types';

/**
 * Configuration validation utilities
 */

export class ConfigValidator {
  /**
   * Validate a provider configuration
   */
  static validateProvider(config: any): { valid: boolean; errors: string[] } {
    const result = ProviderConfigSchema.safeParse(config);
    if (result.success) {
      return { valid: true, errors: [] };
    }

    return {
      valid: false,
      errors: result.error.errors.map((err: any) => `${err.path.join('.')}: ${err.message}`),
    };
  }

  /**
   * Validate routing configuration
   */
  static validateRouting(config: any): { valid: boolean; errors: string[] } {
    const result = RoutingConfigSchema.safeParse(config);
    if (result.success) {
      return { valid: true, errors: [] };
    }

    return {
      valid: false,
      errors: result.error.errors.map((err: any) => `${err.path.join('.')}: ${err.message}`),
    };
  }

  /**
   * Validate fallback configuration
   */
  static validateFallback(config: any): { valid: boolean; errors: string[] } {
    const result = FallbackConfigSchema.safeParse(config);
    if (result.success) {
      return { valid: true, errors: [] };
    }

    return {
      valid: false,
      errors: result.error.errors.map((err: any) => `${err.path.join('.')}: ${err.message}`),
    };
  }

  /**
   * Validate complete multi-provider configuration
   */
  static validateMultiProvider(config: any): { valid: boolean; errors: string[] } {
    // Import schema here to avoid circular dependency
    const result = MultiProviderConfigSchema.safeParse(config);
    if (result.success) {
      return { valid: true, errors: [] };
    }

    return {
      valid: false,
      errors: result.error.errors.map((err: any) => `${err.path.join('.')}: ${err.message}`),
    };
  }
}

/**
 * Provider capability matching utilities
 */

export class ProviderCapabilityMatcher {
  /**
   * Check if a provider supports a specific capability
   */
  static supportsCapability(
    provider: ProviderConfig,
    capability: 'thinking' | 'vision' | 'tools'
  ): boolean {
    return Boolean(provider.capabilities?.includes(capability));
  }

  /**
   * Get providers that support specific capabilities
   */
  static getProvidersForCapability(
    providers: Record<string, ProviderConfig>,
    capability: 'thinking' | 'vision' | 'tools'
  ): string[] {
    return Object.entries(providers)
      .filter(([_, config]) => config.enabled && this.supportsCapability(config, capability))
      .map(([id, _]) => id);
  }

  /**
   * Get all enabled providers sorted by priority (lower number = higher priority)
   */
  static getProvidersByPriority(providers: Record<string, ProviderConfig>): string[] {
    return Object.entries(providers)
      .filter(([_, config]) => config.enabled)
      .sort(([_, a], [__, b]) => a.priority - b.priority)
      .map(([id, _]) => id);
  }

  /**
   * Find best provider for a specific request type based on routing strategy
   */
  static findBestProvider(
    providers: Record<string, ProviderConfig>,
    routing: ConfigRoutingConfig,
    requestType: 'thinking' | 'vision' | 'tools' | 'default'
  ): string | null {
    const enabledProviders = Object.entries(providers).filter(([_, config]) => config.enabled);

    // First, check for specific provider mapping
    const specificProvider = this.getSpecificProviderForRequest(routing, requestType);
    if (specificProvider && providers[specificProvider]?.enabled) {
      return specificProvider;
    }

    // Apply routing strategy
    switch (routing.strategy) {
      case 'capability':
        return this.findProviderByCapability(enabledProviders, requestType);
      case 'cost':
        return this.findProviderByCost(enabledProviders);
      case 'speed':
        return this.findProviderBySpeed(enabledProviders);
      case 'reliability':
        return this.findProviderByReliability(enabledProviders);
      default:
        return this.findProviderByCapability(enabledProviders, requestType);
    }
  }

  private static getSpecificProviderForRequest(
    routing: ConfigRoutingConfig,
    requestType: 'thinking' | 'vision' | 'tools' | 'default'
  ): string | null {
    switch (requestType) {
      case 'thinking':
        return routing.thinkingProvider || null;
      case 'vision':
        return routing.visionProvider || null;
      case 'tools':
        return routing.toolsProvider || null;
      default:
        return routing.defaultProvider;
    }
  }

  private static findProviderByCapability(
    providers: [string, ProviderConfig][],
    requestType: 'thinking' | 'vision' | 'tools' | 'default'
  ): string | null {
    if (requestType === 'default') {
      // For default requests, use priority ordering
      providers.sort(([_, a], [__, b]) => a.priority - b.priority);
      return providers[0]?.[0] || null;
    }

    // Find providers that support the required capability
    const capableProviders = providers.filter(([_, config]) =>
      config.capabilities?.includes(requestType)
    );

    if (capableProviders.length === 0) {
      // Fallback to any enabled provider
      providers.sort(([_, a], [__, b]) => a.priority - b.priority);
      return providers[0]?.[0] || null;
    }

    // Sort by priority among capable providers
    capableProviders.sort(([_, a], [__, b]) => a.priority - b.priority);
    return capableProviders[0]?.[0] || null;
  }

  private static findProviderByCost(providers: [string, ProviderConfig][]): string | null {
    // For now, use priority as cost indicator (lower priority could mean lower cost)
    // In a real implementation, this would use actual cost data
    providers.sort(([_, a], [__, b]) => a.priority - b.priority);
    return providers[0]?.[0] || null;
  }

  private static findProviderBySpeed(providers: [string, ProviderConfig][]): string | null {
    // For now, use priority as speed indicator (lower priority could mean faster)
    // In a real implementation, this would use latency metrics
    providers.sort(([_, a], [__, b]) => a.priority - b.priority);
    return providers[0]?.[0] || null;
  }

  private static findProviderByReliability(providers: [string, ProviderConfig][]): string | null {
    // For now, use priority as reliability indicator (lower priority could mean more reliable)
    // In a real implementation, this would use uptime/availability metrics
    providers.sort(([_, a], [__, b]) => a.priority - b.priority);
    return providers[0]?.[0] || null;
  }
}

/**
 * Configuration migration utilities
 */

export class ConfigMigrator {
  /**
   * Create default multi-provider configuration
   */
  static createDefaultMultiProviderConfig(): MultiProviderConfig {
    return {
      providers: {
        'synthetic-new': {
          name: 'Synthetic.New',
          apiKey: '',
          baseUrl: 'https://api.synthetic.new',
          anthropicBaseUrl: 'https://api.synthetic.new/anthropic',
          modelsApiUrl: 'https://api.synthetic.new/openai/v1/models',
          enabled: true,
          priority: 5,
          timeout: 30000,
          maxRetries: 3,
        },
      },
      routing: {
        defaultProvider: 'synthetic-new',
        enableFailover: true,
        strategy: 'capability',
        loadBalancing: false,
      },
      fallback: {
        enabled: true,
        retryDelay: 1000,
        maxFailures: 3,
        healthCheck: true,
        healthCheckInterval: 60000,
      },
    };
  }

  /**
   * Migrate from legacy single-provider config to multi-provider
   */
  static migrateLegacyConfig(legacyConfig: any): MultiProviderConfig {
    const defaultConfig = this.createDefaultMultiProviderConfig();

    // Migrate synthetic-new provider from legacy config
    if (legacyConfig.apiKey || legacyConfig.baseUrl) {
      defaultConfig.providers['synthetic-new'] = {
        ...defaultConfig.providers['synthetic-new']!,
        apiKey: legacyConfig.apiKey || '',
        baseUrl: legacyConfig.baseUrl || defaultConfig.providers['synthetic-new']!.baseUrl,
        anthropicBaseUrl:
          legacyConfig.anthropicBaseUrl ||
          defaultConfig.providers['synthetic-new']!.anthropicBaseUrl,
        modelsApiUrl:
          legacyConfig.modelsApiUrl || defaultConfig.providers['synthetic-new']!.modelsApiUrl,
      };
    }

    // Migrate legacy providers
    if (legacyConfig.providers) {
      Object.entries(legacyConfig.providers).forEach(([key, provider]: [string, any]) => {
        if (key !== 'synthetic-new' && typeof provider === 'object') {
          defaultConfig.providers[key as keyof typeof defaultConfig.providers] = {
            name: provider.name || key,
            apiKey: provider.apiKey || '',
            baseUrl: provider.baseUrl,
            anthropicBaseUrl: provider.anthropicBaseUrl,
            modelsApiUrl: provider.modelsApiUrl,
            models: provider.models,
            capabilities: provider.capabilities,
            enabled: provider.enabled !== false,
            priority: provider.priority || 5,
            timeout: provider.timeout || 30000,
            maxRetries: provider.maxRetries || 3,
          };
        }
      });
    }

    // Migrate routing rules
    if (legacyConfig.routingRules) {
      if (legacyConfig.routingRules.thinking) {
        defaultConfig.routing.thinkingProvider = legacyConfig.routingRules.thinking;
      }
      if (legacyConfig.routingRules.vision) {
        defaultConfig.routing.visionProvider = legacyConfig.routingRules.vision;
      }
      if (legacyConfig.routingRules.tools) {
        defaultConfig.routing.toolsProvider = legacyConfig.routingRules.tools;
      }
    }

    return defaultConfig;
  }

  /**
   * Merge two multi-provider configurations
   */
  static mergeMultiProviderConfigs(
    base: MultiProviderConfig,
    override: Partial<MultiProviderConfig>
  ): MultiProviderConfig {
    const merged: MultiProviderConfig = {
      providers: { ...base.providers },
      routing: { ...base.routing, ...override.routing },
      fallback: { ...base.fallback, ...override.fallback },
    };

    // Merge providers
    if (override.providers) {
      Object.entries(override.providers).forEach(([key, provider]) => {
        if (provider) {
          merged.providers[key as keyof typeof merged.providers] = {
            ...merged.providers[key as keyof typeof merged.providers],
            ...provider,
          };
        }
      });
    }

    return merged;
  }
}

/**
 * Configuration health check utilities
 */

export class ConfigHealthChecker {
  /**
   * Check if configuration is healthy and ready for use
   */
  static async checkConfigHealth(config: MultiProviderConfig): Promise<{
    healthy: boolean;
    issues: string[];
    recommendations: string[];
  }> {
    const issues: string[] = [];
    const recommendations: string[] = [];

    // Check if at least one provider is enabled
    const enabledProviders = Object.values(config.providers).filter(p => p.enabled);
    if (enabledProviders.length === 0) {
      issues.push('No providers are enabled');
      recommendations.push('Enable at least one provider');
    }

    // Check if enabled providers have API keys
    const providersWithoutKeys = enabledProviders.filter(p => !p.apiKey);
    if (providersWithoutKeys.length > 0) {
      issues.push(`${providersWithoutKeys.length} provider(s) missing API keys`);
      recommendations.push('Configure API keys for enabled providers');
    }

    // Check routing configuration
    const defaultProvider = config.routing.defaultProvider;
    if (!config.providers[defaultProvider as keyof typeof config.providers]) {
      issues.push(`Default provider "${defaultProvider}" is not configured`);
      recommendations.push('Configure the default provider or choose a different one');
    }

    // Check fallback configuration
    if (config.fallback.enabled && enabledProviders.length <= 1) {
      recommendations.push(
        'Consider enabling failover for better reliability when using multiple providers'
      );
    }

    return {
      healthy: issues.length === 0,
      issues,
      recommendations,
    };
  }

  /**
   * Get configuration summary
   */
  static getConfigSummary(config: MultiProviderConfig): {
    totalProviders: number;
    enabledProviders: number;
    configuredProviders: number;
    routingStrategy: string;
    failoverEnabled: boolean;
    healthChecksEnabled: boolean;
  } {
    const totalProviders = Object.keys(config.providers).length;
    const enabledProviders = Object.values(config.providers).filter(p => p.enabled).length;
    const configuredProviders = Object.values(config.providers).filter(
      p => p.enabled && p.apiKey
    ).length;

    return {
      totalProviders,
      enabledProviders,
      configuredProviders,
      routingStrategy: config.routing.strategy,
      failoverEnabled: config.routing.enableFailover,
      healthChecksEnabled: config.fallback.healthCheck,
    };
  }
}
