import { AppConfigSchema } from '../src/config';

describe('Multi-Provider Configuration', () => {
  describe('Provider Management', () => {
    it('should validate a multi-provider configuration', () => {
      const multiProviderConfig = {
        apiKey: 'fallback-key',
        baseUrl: 'https://api.fallback.com',
        providers: {
          minimax: {
            name: 'Minimax M2',
            apiKey: 'minimax-key',
            baseUrl: 'https://api.minimax.chat',
            capabilities: ['thinking', 'vision', 'tools'],
            enabled: true,
          },
          zai: {
            name: 'Z.AI',
            apiKey: 'zai-key',
            baseUrl: 'https://api.z.ai',
            capabilities: ['vision', 'tools'],
            enabled: true,
          },
          synthetic: {
            name: 'Synthetic.New',
            apiKey: 'synthetic-key',
            baseUrl: 'https://api.synthetic.new',
            capabilities: ['thinking', 'tools'],
            enabled: true,
          },
        },
        routingRules: {
          thinking: 'minimax',
          vision: 'zai',
          tools: 'synthetic',
          default: 'minimax',
        },
      };

      const result = AppConfigSchema.safeParse(multiProviderConfig);
      expect(result.success).toBe(true);

      if (result.success) {
        expect(Object.keys(result.data.providers)).toEqual(['minimax', 'zai', 'synthetic']);
        expect(result.data.providers?.minimax.capabilities).toEqual(['thinking', 'vision', 'tools']);
        expect(result.data.routingRules?.thinking).toBe('minimax');
      }
    });

    it('should validate configuration with disabled providers', () => {
      const configWithDisabled = {
        apiKey: 'fallback-key',
        providers: {
          minimax: {
            name: 'Minimax M2',
            apiKey: 'minimax-key',
            enabled: true,
          },
          zai: {
            name: 'Z.AI',
            apiKey: '',
            enabled: false, // Disabled provider
          },
        },
      };

      const result = AppConfigSchema.safeParse(configWithDisabled);
      expect(result.success).toBe(true);

      if (result.success) {
        expect(result.data.providers?.minimax.enabled).toBe(true);
        expect(result.data.providers?.zai.enabled).toBe(false);
      }
    });

    it('should handle empty providers configuration', () => {
      const emptyProvidersConfig = {
        apiKey: 'test-key',
        providers: {},
        routingRules: {},
      };

      const result = AppConfigSchema.safeParse(emptyProvidersConfig);
      expect(result.success).toBe(true);
      expect(result.data.providers).toEqual({});
      expect(result.data.routingRules).toEqual({});
    });
  });

  describe('Capability-Based Provider Selection', () => {
    it('should validate provider capabilities', () => {
      const providerWithCapabilities = {
        name: 'Test Provider',
        apiKey: 'test-key',
        capabilities: ['thinking', 'vision', 'tools'], // All valid capabilities
        enabled: true,
      };

      const config = {
        providers: {
          test: providerWithCapabilities,
        },
      };

      const result = AppConfigSchema.safeParse(config);
      expect(result.success).toBe(true);
    });

    it('should handle providers without explicit capabilities', () => {
      const providerWithoutCapabilities = {
        name: 'Basic Provider',
        apiKey: 'test-key',
        enabled: true,
        // No capabilities specified - should still be valid
      };

      const config = {
        providers: {
          basic: providerWithoutCapabilities,
        },
      };

      const result = AppConfigSchema.safeParse(config);
      expect(result.success).toBe(true);
    });
  });

  describe('Routing Rules Validation', () => {
    it('should validate routing rules configuration', () => {
      const routingConfig = {
        providers: {
          minimax: { name: 'Minimax', apiKey: 'key', enabled: true },
          zai: { name: 'Z.AI', apiKey: 'key', enabled: true },
        },
        routingRules: {
          thinking: 'minimax',
          vision: 'zai',
          default: 'minimax',
        },
      };

      const result = AppConfigSchema.safeParse(routingConfig);
      expect(result.success).toBe(true);
    });

    it('should handle empty routing rules', () => {
      const configWithEmptyRouting = {
        apiKey: 'test-key',
        providers: {
          minimax: { name: 'Minimax', apiKey: 'key', enabled: true },
        },
        routingRules: {},
      };

      const result = AppConfigSchema.safeParse(configWithEmptyRouting);
      expect(result.success).toBe(true);
      expect(result.data.routingRules).toEqual({});
    });
  });
});