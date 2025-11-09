// Utility tests for helpers and common functions

describe('Utility Functions', () => {
  describe('Model Identification', () => {
    it('should extract provider from model ID', () => {
      function extractProvider(modelId: string): string {
        const [provider] = modelId.split(':', 1);
        return provider || (modelId ? 'unknown' : '');
      }

      expect(extractProvider('minimax:claude-3-sonnet')).toBe('minimax');
      expect(extractProvider('zai:gpt-4-vision')).toBe('zai');
      expect(extractProvider('claude-3-standalone')).toBe('claude-3-standalone'); // No provider prefix
      expect(extractProvider('')).toBe('');
    });

    it('should normalize model names across providers', () => {
      function normalizeModelName(modelId: string, provider: string): string {
        const baseModel = modelId.includes(':')
          ? modelId.split(':', 2)[1]
          : modelId;

        return `${provider}:${baseModel}`;
      }

      expect(normalizeModelName('claude-3-sonnet', 'minimax')).toBe('minimax:claude-3-sonnet');
      expect(normalizeModelName('minimax:claude-3-sonnet', 'minimax')).toBe('minimax:claude-3-sonnet');
      expect(normalizeModelName('gpt-4', 'zai')).toBe('zai:gpt-4');
    });

    it('should detect model capabilities from model name', () => {
      function detectCapabilities(modelId: string): string[] {
        const capabilities: string[] = [];
        const name = modelId.toLowerCase();

        if (name.includes('vision') || name.includes('image')) {
          capabilities.push('vision');
        }
        if (name.includes('thinking') || name.includes('reasoning')) {
          capabilities.push('thinking');
        }
        if (capabilities.length === 0) {
          capabilities.push('tools'); // Default capability
        }

        return capabilities;
      }

      expect(detectCapabilities('claude-3-vision')).toContain('vision');
      expect(detectCapabilities('claude-3-thinking')).toContain('thinking');
      expect(detectCapabilities('gpt-4')).toContain('tools');
    });
  });

  describe('Configuration Helpers', () => {
    it('should merge provider configurations correctly', () => {
      function mergeProviderConfigs(base: any, override: any): any {
        return {
          ...base,
          providers: {
            ...base.providers,
            ...override.providers,
          },
          routingRules: {
            ...base.routingRules,
            ...override.routingRules,
          },
        };
      }

      const baseConfig = {
        providers: {
          minimax: { name: 'Minimax', apiKey: 'key1', enabled: true },
          zai: { name: 'Z.AI', apiKey: 'key2', enabled: true },
        },
        routingRules: {
          thinking: 'minimax',
        },
      };

      const overrideConfig = {
        providers: {
          zai: { enabled: false }, // Disable zai
          synthetic: { name: 'Synthetic', apiKey: 'key3', enabled: true },
        },
        routingRules: {
          vision: 'zai', // Add vision routing
        },
      };

      const merged = mergeProviderConfigs(baseConfig, overrideConfig);

      expect(merged.providers.minimax.enabled).toBe(true);
      expect(merged.providers.zai.enabled).toBe(false); // Overridden
      expect(merged.providers.synthetic.enabled).toBe(true); // Added
      expect(merged.routingRules.thinking).toBe('minimax'); // Preserved
      expect(merged.routingRules.vision).toBe('zai'); // Added
    });

    it('should validate provider completeness', () => {
      function validateProvider(provider: any): boolean {
        return !!(
          provider.name &&
          provider.apiKey !== undefined &&
          typeof provider.enabled === 'boolean'
        );
      }

      const validProvider = {
        name: 'Test Provider',
        apiKey: 'test-key',
        enabled: true,
      };

      const invalidProvider = {
        name: 'Invalid Provider',
        // Missing apiKey
        enabled: true,
      };

      expect(validateProvider(validProvider)).toBe(true);
      expect(validateProvider(invalidProvider)).toBe(false);
    });
  });

  describe('Error Handling Utilities', () => {
    it('should categorize errors appropriately', () => {
      function categorizeError(error: any): string {
        if (error.code === 'ENOTFOUND') return 'network';
        if (error.response?.status === 401) return 'authentication';
        if (error.response?.status >= 500) return 'server';
        if (error.response?.status >= 400) return 'client';
        return 'unknown';
      }

      expect(categorizeError({ code: 'ENOTFOUND' })).toBe('network');
      expect(categorizeError({ response: { status: 401 } })).toBe('authentication');
      expect(categorizeError({ response: { status: 500 } })).toBe('server');
      expect(categorizeError({ response: { status: 400 } })).toBe('client');
    });

    it('should determine retry eligibility', () => {
      function shouldRetry(errorCategory: string, attemptCount: number): boolean {
        const retryableCategories = ['network', 'server'];
        const maxRetries = 3;

        return retryableCategories.includes(errorCategory) && attemptCount < maxRetries;
      }

      expect(shouldRetry('network', 1)).toBe(true);
      expect(shouldRetry('server', 2)).toBe(true);
      expect(shouldRetry('authentication', 1)).toBe(false); // Don't retry auth errors
      expect(shouldRetry('network', 3)).toBe(false); // Max retries reached
    });
  });

  describe('Performance Monitoring', () => {
    it('should calculate provider performance metrics', () => {
      interface RequestMetrics {
        successCount: number;
        errorCount: number;
        totalLatency: number;
        requestCount: number;
      }

      function calculatePerformance(metrics: RequestMetrics) {
        const totalRequests = metrics.successCount + metrics.errorCount;
        return {
          successRate: totalRequests > 0 ? metrics.successCount / totalRequests : 0,
          averageLatency: metrics.requestCount > 0 ? metrics.totalLatency / metrics.requestCount : 0,
          reliability: metrics.successCount > 0 ?
            1 - (metrics.errorCount / metrics.successCount) : 0,
        };
      }

      const testMetrics: RequestMetrics = {
        successCount: 95,
        errorCount: 5,
        totalLatency: 15000,
        requestCount: 100,
      };

      const performance = calculatePerformance(testMetrics);

      expect(performance.successRate).toBe(0.95);
      expect(performance.averageLatency).toBe(150);
      expect(performance.reliability).toBeCloseTo(0.947, 2);
    });

    it('should rank providers by performance', () => {
      interface ProviderPerformance {
        name: string;
        successRate: number;
        averageLatency: number;
        costPerToken: number;
      }

      function scoreProvider(perf: ProviderPerformance): number {
        // Higher success rate and lower latency = better score
        // Factor in cost as well
        const successScore = perf.successRate * 100;
        const latencyScore = Math.max(0, 100 - perf.averageLatency / 10); // Penalize high latency
        const costScore = Math.max(0, 100 - perf.costPerToken * 1000); // Penalize high cost

        return (successScore + latencyScore + costScore) / 3;
      }

      const providers: ProviderPerformance[] = [
        { name: 'minimax', successRate: 0.95, averageLatency: 150, costPerToken: 0.001 },
        { name: 'zai', successRate: 0.88, averageLatency: 120, costPerToken: 0.002 },
        { name: 'synthetic', successRate: 0.92, averageLatency: 180, costPerToken: 0.0005 },
      ];

      const ranked = providers
        .map(p => ({ ...p, score: scoreProvider(p) }))
        .sort((a, b) => b.score - a.score);

      expect(ranked[0].name).toBe('minimax'); // Should rank highest
      expect(ranked[2].name).toBe('synthetic'); // Should rank lowest despite lowest cost
    });
  });
});