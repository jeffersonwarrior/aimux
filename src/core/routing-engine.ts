import { BaseProvider } from '../providers/base-provider';
import { ProviderRegistry } from '../providers/provider-registry';
import {
  AIRequest,
  AIResponse,
  ProviderCapabilities,
  ProviderError,
  ProviderHealth,
} from '../providers/types';
import { getLogger } from '../utils/logger';
import {
  analyzeRequestRequirements,
  determineRequestType,
  estimateRequestComplexity,
  normalizeCapabilityRequirements,
  calculateProviderCompatibility,
  RequestRequirements,
} from './routing-utils';

/**
 * Routing engine configuration options
 */
export interface RoutingEngineConfig {
  /** Enable performance-based routing */
  enablePerformanceRouting: boolean;
  /** Enable cost-based routing */
  enableCostRouting: boolean;
  /** Enable health-based routing */
  enableHealthRouting: boolean;
  /** Default fallback behavior */
  enableFallback: boolean;
  /** Maximum number of providers to try before giving up */
  maxProviderAttempts: number;
  /** Provider selection preferences by capability */
  capabilityPreferences: {
    [key in keyof ProviderCapabilities]?: string[];
  };
  /** Custom routing rules */
  customRules?: RoutingRule[];
}

/**
 * Custom routing rule interface
 */
export interface RoutingRule {
  id: string;
  name: string;
  condition: (request: AIRequest) => boolean;
  providerSelector: (providers: BaseProvider[]) => BaseProvider | null;
  priority: number; // Lower number = higher priority
  enabled: boolean;
}

/**
 * Routing decision context
 */
export interface RoutingContext {
  originalRequest: AIRequest;
  requirements: RequestRequirements;
  candidateProviders: BaseProvider[];
  excludedProviders: string[];
  attemptCount: number;
  startTime: number;
  routingDecision?: string;
  selectedProvider?: BaseProvider;
}

/**
 * Routing engine for intelligent provider selection and request routing
 * Implements capability-based routing with fallback, performance optimization, and custom rules
 */
export class RoutingEngine {
  private logger = getLogger();
  private config: RoutingEngineConfig;
  private providerPerformanceCache: Map<string, PerformanceMetrics> = new Map();
  private routingHistory: RoutingHistoryEntry[] = [];
  private maxHistorySize = 1000;

  constructor(
    private providerRegistry: ProviderRegistry,
    config: Partial<RoutingEngineConfig> = {}
  ) {
    this.config = {
      enablePerformanceRouting: true,
      enableCostRouting: false,
      enableHealthRouting: true,
      enableFallback: true,
      maxProviderAttempts: 3,
      capabilityPreferences: {},
      customRules: [],
      ...config,
    };

    this.logger.info('RoutingEngine initialized', {
      enablePerformanceRouting: this.config.enablePerformanceRouting,
      enableHealthRouting: this.config.enableHealthRouting,
      enableFallback: this.config.enableFallback,
      maxProviderAttempts: this.config.maxProviderAttempts,
    });
  }

  /**
   * Select the best provider for a given AI request
   * @param request The AI request to route
   * @param excludeProviders Optional array of provider IDs to exclude
   * @returns Selected provider or null if no suitable provider found
   */
  public selectProvider(request: AIRequest, excludeProviders: string[] = []): BaseProvider | null {
    const startTime = Date.now();

    try {
      // Analyze request requirements
      const requirements = analyzeRequestRequirements(request);

      // Get candidate providers
      const candidateProviders = this.getCandidateProviders(
        request,
        requirements,
        excludeProviders
      );

      if (candidateProviders.length === 0) {
        this.logger.warn('No suitable providers found for request', {
          requestType: requirements.type,
          requiredCapabilities: requirements.capabilities,
          excludeProviders,
        });
        return null;
      }

      // Create routing context
      const context: RoutingContext = {
        originalRequest: request,
        requirements,
        candidateProviders,
        excludedProviders: excludeProviders,
        attemptCount: 1,
        startTime,
      };

      // Select provider using configured strategy
      const selectedProvider = this.selectProviderWithStrategy(context);

      // Record routing decision
      this.recordRoutingDecision(context, selectedProvider);

      if (selectedProvider) {
        this.logger.debug('Provider selected successfully', {
          providerId: selectedProvider.id,
          providerName: selectedProvider.name,
          requestType: requirements.type,
          routingTime: Date.now() - startTime,
          decisionReason: context.routingDecision,
        });
      }

      return selectedProvider;
    } catch (error) {
      this.logger.error('Error in provider selection', {
        error: error instanceof Error ? error.message : String(error),
        requestModel: request.model,
        excludeProviders,
      });
      return null;
    }
  }

  /**
   * Get providers that can handle the request based on requirements
   */
  private getCandidateProviders(
    request: AIRequest,
    requirements: RequestRequirements,
    excludeProviders: string[]
  ): BaseProvider[] {
    // Get basic candidates from registry
    let candidates = this.providerRegistry
      .getProvidersForRequest(request)
      .filter(provider => !excludeProviders.includes(provider.id));

    // Filter by required capabilities
    if (requirements.capabilities.length > 0) {
      candidates = candidates.filter(provider =>
        requirements.capabilities.every(capability => provider.hasCapability(capability))
      );
    }

    // Apply performance filtering if enabled
    if (this.config.enablePerformanceRouting) {
      candidates = this.filterByPerformance(candidates);
    }

    // Apply health filtering if enabled
    if (this.config.enableHealthRouting) {
      candidates = this.filterByHealth(candidates);
    }

    return candidates;
  }

  /**
   * Select provider using the configured strategy (rules, capabilities, performance)
   */
  private selectProviderWithStrategy(context: RoutingContext): BaseProvider | null {
    // 1. Try custom routing rules first (highest priority)
    if (this.config.customRules && this.config.customRules.length > 0) {
      const ruleProvider = this.selectProviderByCustomRules(context);
      if (ruleProvider) {
        context.routingDecision = `custom-rule:${ruleProvider.id}`;
        return ruleProvider;
      }
    }

    // 2. Try capability-based routing
    const capabilityProvider = this.selectProviderByCapabilities(context);
    if (capabilityProvider) {
      context.routingDecision = `capability:${capabilityProvider.id}`;
      return capabilityProvider;
    }

    // 3. Try performance-based routing
    if (this.config.enablePerformanceRouting) {
      const performanceProvider = this.selectProviderByPerformance(context.candidateProviders);
      if (performanceProvider) {
        context.routingDecision = `performance:${performanceProvider.id}`;
        return performanceProvider;
      }
    }

    // 4. Fall back to priority-based selection
    const priorityProvider = context.candidateProviders[0];
    if (priorityProvider) {
      context.routingDecision = `priority:${priorityProvider.id}`;
      return priorityProvider;
    }

    return null;
  }

  /**
   * Select provider using custom routing rules
   */
  private selectProviderByCustomRules(context: RoutingContext): BaseProvider | null {
    const sortedRules = (this.config.customRules || [])
      .filter(rule => rule.enabled)
      .sort((a, b) => a.priority - b.priority);

    for (const rule of sortedRules) {
      try {
        if (rule.condition(context.originalRequest)) {
          const provider = rule.providerSelector(context.candidateProviders);
          if (provider && context.candidateProviders.includes(provider)) {
            this.logger.debug('Custom routing rule matched', {
              ruleId: rule.id,
              ruleName: rule.name,
              selectedProvider: provider.id,
            });
            return provider;
          }
        }
      } catch (error) {
        this.logger.warn('Error executing custom routing rule', {
          ruleId: rule.id,
          error: error instanceof Error ? error.message : String(error),
        });
      }
    }

    return null;
  }

  /**
   * Select provider based on capability requirements and preferences
   */
  private selectProviderByCapabilities(context: RoutingContext): BaseProvider | null {
    const { requirements, candidateProviders } = context;

    // For thinking requests, prioritize providers with thinking capability
    if (requirements.requiresThinking) {
      const preferredProviders = this.config.capabilityPreferences.thinking || [];
      for (const providerId of preferredProviders) {
        const provider = candidateProviders.find(p => p.id === providerId);
        if (provider && provider.hasCapability('thinking')) {
          return provider;
        }
      }

      // Fall back to any thinking-capable provider
      const thinkingProvider = candidateProviders.find(p => p.hasCapability('thinking'));
      if (thinkingProvider) {
        return thinkingProvider;
      }
    }

    // For vision requests, prioritize providers with vision capability
    if (requirements.requiresVision) {
      const preferredProviders = this.config.capabilityPreferences.vision || [];
      for (const providerId of preferredProviders) {
        const provider = candidateProviders.find(p => p.id === providerId);
        if (provider && provider.hasCapability('vision')) {
          return provider;
        }
      }

      // Fall back to any vision-capable provider
      const visionProvider = candidateProviders.find(p => p.hasCapability('vision'));
      if (visionProvider) {
        return visionProvider;
      }
    }

    // For tools requests, prioritize providers with tools capability
    if (requirements.requiresTools) {
      const preferredProviders = this.config.capabilityPreferences.tools || [];
      for (const providerId of preferredProviders) {
        const provider = candidateProviders.find(p => p.id === providerId);
        if (provider && provider.hasCapability('tools')) {
          return provider;
        }
      }

      // Fall back to any tools-capable provider
      const toolsProvider = candidateProviders.find(p => p.hasCapability('tools'));
      if (toolsProvider) {
        return toolsProvider;
      }
    }

    // For regular requests, use general capability preferences or default priority
    const preferredProviders = this.config.capabilityPreferences.streaming || [];
    for (const providerId of preferredProviders) {
      const provider = candidateProviders.find(p => p.id === providerId);
      if (provider) {
        return provider;
      }
    }

    // Return highest priority provider as default
    return candidateProviders[0] || null;
  }

  /**
   * Select provider based on performance metrics
   */
  private selectProviderByPerformance(providers: BaseProvider[]): BaseProvider | null {
    if (providers.length === 0) return null;
    if (providers.length === 1) return providers[0] || null;

    // Sort by performance score (lower is better)
    const sortedProviders = [...providers].sort((a, b) => {
      const aMetrics = this.providerPerformanceCache.get(a.id);
      const bMetrics = this.providerPerformanceCache.get(b.id);

      if (!aMetrics && !bMetrics) return 0;
      if (!aMetrics) return 1; // Place unknown performance provider at end
      if (!bMetrics) return -1;

      // Compare by average response time, then success rate
      const aScore = aMetrics.averageResponseTime / (aMetrics.successRate / 100);
      const bScore = bMetrics.averageResponseTime / (bMetrics.successRate / 100);

      return aScore - bScore;
    });

    return sortedProviders[0] || null;
  }

  /**
   * Filter providers by performance metrics
   */
  private filterByPerformance(providers: BaseProvider[]): BaseProvider[] {
    return providers.filter(provider => {
      const metrics = this.providerPerformanceCache.get(provider.id);
      if (!metrics) return true; // Include unknown providers

      // Filter out providers with very poor performance
      return metrics.successRate > 50; // At least 50% success rate
    });
  }

  /**
   * Filter providers by health status
   */
  private filterByHealth(providers: BaseProvider[]): BaseProvider[] {
    return providers.filter(provider => {
      const health = provider.getLastHealthCheck();
      if (!health) return true; // Include unknown health providers

      // Filter out unhealthy providers
      return health.status !== 'unhealthy';
    });
  }

  /**
   * Update performance metrics for a provider
   */
  public updateProviderPerformance(
    providerId: string,
    responseTime: number,
    success: boolean,
    errorType?: string
  ): void {
    const existing = this.providerPerformanceCache.get(providerId) || {
      totalRequests: 0,
      successfulRequests: 0,
      failedRequests: 0,
      averageResponseTime: 0,
      successRate: 0,
      lastUpdated: new Date(),
      errorTypes: {},
    };

    existing.totalRequests++;
    existing.lastUpdated = new Date();

    if (success) {
      existing.successfulRequests++;
    } else {
      existing.failedRequests++;
      if (errorType) {
        existing.errorTypes[errorType] = (existing.errorTypes[errorType] || 0) + 1;
      }
    }

    // Update average response time (exponential moving average)
    const alpha = 0.3; // Smoothing factor
    if (existing.averageResponseTime === 0) {
      existing.averageResponseTime = responseTime;
    } else {
      existing.averageResponseTime =
        alpha * responseTime + (1 - alpha) * existing.averageResponseTime;
    }

    // Calculate success rate
    existing.successRate = (existing.successfulRequests / existing.totalRequests) * 100;

    this.providerPerformanceCache.set(providerId, existing);

    this.logger.debug('Provider performance updated', {
      providerId,
      responseTime,
      success,
      successRate: existing.successRate,
      averageResponseTime: existing.averageResponseTime,
    });
  }

  /**
   * Record routing decision for analytics
   */
  private recordRoutingDecision(
    context: RoutingContext,
    selectedProvider: BaseProvider | null
  ): void {
    const entry: RoutingHistoryEntry = {
      timestamp: new Date(),
      requestId: this.generateRequestId(),
      requestType: context.requirements.type,
      requiredCapabilities: context.requirements.capabilities,
      candidateProviderCount: context.candidateProviders.length,
      selectedProviderId: selectedProvider?.id,
      routingDecision: context.routingDecision || 'none',
      routingTime: Date.now() - context.startTime,
      success: selectedProvider !== null,
    };

    this.routingHistory.push(entry);

    // Limit history size
    if (this.routingHistory.length > this.maxHistorySize) {
      this.routingHistory = this.routingHistory.slice(-this.maxHistorySize);
    }
  }

  /**
   * Generate a unique request ID
   */
  private generateRequestId(): string {
    return `req_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`;
  }

  /**
   * Get routing statistics and history
   */
  public getRoutingStatistics(): {
    totalRoutings: number;
    successfulRoutings: number;
    averageRoutingTime: number;
    providerUsageCounts: Record<string, number>;
    capabilityUsageCounts: Record<string, number>;
    recentFailures: RoutingHistoryEntry[];
  } {
    const totalRoutings = this.routingHistory.length;
    const successfulRoutings = this.routingHistory.filter(entry => entry.success).length;
    const averageRoutingTime =
      totalRoutings > 0
        ? this.routingHistory.reduce((sum, entry) => sum + entry.routingTime, 0) / totalRoutings
        : 0;

    const providerUsageCounts: Record<string, number> = {};
    const capabilityUsageCounts: Record<string, number> = {};

    for (const entry of this.routingHistory) {
      if (entry.selectedProviderId) {
        providerUsageCounts[entry.selectedProviderId] =
          (providerUsageCounts[entry.selectedProviderId] || 0) + 1;
      }

      for (const capability of entry.requiredCapabilities) {
        capabilityUsageCounts[capability] = (capabilityUsageCounts[capability] || 0) + 1;
      }
    }

    const recentFailures = this.routingHistory.filter(entry => !entry.success).slice(-10);

    return {
      totalRoutings,
      successfulRoutings,
      averageRoutingTime,
      providerUsageCounts,
      capabilityUsageCounts,
      recentFailures,
    };
  }

  /**
   * Get performance metrics for all providers
   */
  public getProviderPerformanceMetrics(): Record<string, PerformanceMetrics> {
    const metrics: Record<string, PerformanceMetrics> = {};
    const entries = Array.from(this.providerPerformanceCache.entries());
    for (const [providerId, providerMetrics] of entries) {
      metrics[providerId] = { ...providerMetrics };
    }
    return metrics;
  }

  /**
   * Clear routing history and performance cache
   */
  public clearCache(): void {
    this.routingHistory = [];
    this.providerPerformanceCache.clear();
    this.logger.info('Routing engine cache cleared');
  }

  /**
   * Update routing configuration
   */
  public updateConfig(config: Partial<RoutingEngineConfig>): void {
    this.config = { ...this.config, ...config };
    this.logger.info('Routing configuration updated', { config: this.config });
  }

  /**
   * Get current routing configuration
   */
  public getConfig(): RoutingEngineConfig {
    return { ...this.config };
  }
}

/**
 * Performance metrics interface
 */
export interface PerformanceMetrics {
  totalRequests: number;
  successfulRequests: number;
  failedRequests: number;
  averageResponseTime: number;
  successRate: number;
  lastUpdated: Date;
  errorTypes: Record<string, number>;
}

/**
 * Routing history entry interface
 */
interface RoutingHistoryEntry {
  timestamp: Date;
  requestId: string;
  requestType: string;
  requiredCapabilities: string[];
  candidateProviderCount: number;
  selectedProviderId?: string;
  routingDecision: string;
  routingTime: number;
  success: boolean;
}
