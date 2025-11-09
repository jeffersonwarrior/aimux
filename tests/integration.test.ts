import { ConfigManager } from '../src/config';
import { mkdtemp, rm } from 'fs/promises';
import { join } from 'path';
import { tmpdir } from 'os';

describe('Integration Tests', () => {
  let configManager: ConfigManager;
  let tempDir: string;

  beforeEach(async () => {
    tempDir = await mkdtemp(join(tmpdir(), 'aimux-integration-'));
    configManager = new ConfigManager(join(tempDir, '.config', 'aimux'));
  });

  afterEach(async () => {
    try {
      await rm(tempDir, { recursive: true, force: true });
    } catch (error) {
      // Ignore cleanup errors
    }
  });

  describe('Multi-Provider Workflow', () => {
    it('should handle complete multi-provider setup workflow', async () => {
      // Step 1: Initialize with provider configurations
      const providerConfig = {
        providers: {
          minimax: {
            name: 'Minimax M2',
            apiKey: 'test-minimax-key',
            baseUrl: 'https://api.minimax.chat',
            capabilities: ['thinking', 'vision', 'tools'],
            enabled: true,
          },
          zai: {
            name: 'Z.AI',
            apiKey: 'test-zai-key',
            baseUrl: 'https://api.z.ai',
            capabilities: ['vision', 'tools'],
            enabled: true,
          },
        },
        routingRules: {
          thinking: 'minimax',
          vision: 'zai',
          default: 'minimax',
        },
        selectedModel: 'minimax:claude-3-vision',
        firstRunCompleted: true,
      };

      const success = await configManager.updateConfig(providerConfig);
      expect(success).toBe(true);

      // Step 2: Verify configuration persistence
      const newConfigManager = new ConfigManager(join(tempDir, '.config', 'aimux'));
      const config = newConfigManager.config;

      expect(config.providers).toBeDefined();
      expect(Object.keys(config.providers || {})).toEqual(['minimax', 'zai']);
      expect(config.providers?.minimax.apiKey).toBe('test-minimax-key');
      expect(config.providers?.zai.capabilities).toEqual(['vision', 'tools']);
      expect(config.routingRules?.thinking).toBe('minimax');
      expect(config.selectedModel).toBe('minimax:claude-3-vision');
      expect(config.firstRunCompleted).toBe(true);
    });

    it('should handle provider enabled/disabled state transitions', async () => {
      // Initial setup with both providers enabled
      const initialConfig = {
        providers: {
          minimax: {
            name: 'Minimax',
            apiKey: 'key1',
            enabled: true,
          },
          zai: {
            name: 'Z.AI',
            apiKey: 'key2',
            enabled: true,
          },
        },
        firstRunCompleted: true,
      };

      await configManager.updateConfig(initialConfig);

      // Disable one provider
      const updatedConfig = {
        providers: {
          ...configManager.config.providers,
          zai: {
            ...configManager.config.providers?.zai,
            enabled: false,
          },
        },
      };

      const success = await configManager.updateConfig(updatedConfig);
      expect(success).toBe(true);

      expect(configManager.config.providers?.minimax.enabled).toBe(true);
      expect(configManager.config.providers?.zai.enabled).toBe(false);
    });
  });

  describe('Hybrid Mode Configuration', () => {
    it('should support thinking and regular model selection', async () => {
      const hybridConfig = {
        selectedModel: 'zai:gpt-4-vision',
        selectedThinkingModel: 'minimax:claude-3-thinking',
        providers: {
          minimax: {
            name: 'Minimax',
            apiKey: 'thinking-key',
            capabilities: ['thinking'],
            enabled: true,
          },
          zai: {
            name: 'Z.AI',
            apiKey: 'regular-key',
            capabilities: ['vision'],
            enabled: true,
          },
        },
        routingRules: {
          thinking: 'minimax',
          regular: 'zai',
        },
        firstRunCompleted: true,
      };

      const success = await configManager.updateConfig(hybridConfig);
      expect(success).toBe(true);

      expect(configManager.getSelectedModel()).toBe('zai:gpt-4-vision');
      expect(configManager.getSavedThinkingModel()).toBe('minimax:claude-3-thinking');
    });
  });

  describe('Fallback Behavior', () => {
    it('should handle scenarios with misconfigured providers', async () => {
      const configWithIssues = {
        apiKey: 'fallback-key',
        baseUrl: 'https://api.fallback.com',
        providers: {
          // Provider with missing API key
          minimax: {
            name: 'Minimax',
            apiKey: '',
            enabled: true,
          },
          // Disabled provider
          zai: {
            name: 'Z.AI',
            apiKey: 'key2',
            enabled: false,
          },
        },
        firstRunCompleted: true,
      };

      const success = await configManager.updateConfig(configWithIssues);
      expect(success).toBe(true);

      // Should fallback to default API key when providers are unavailable
      expect(configManager.getApiKey()).toBe('fallback-key');
      expect(configManager.config.providers?.minimax.apiKey).toBe('');
      expect(configManager.config.providers?.zai.enabled).toBe(false);
    });
  });

  describe('Configuration Migration', () => {
    it('should handle legacy configuration migration', async () => {
      // Legacy single-provider config
      const legacyConfig = {
        apiKey: 'legacy-api-key',
        baseUrl: 'https://api.legacy.com',
        selectedModel: 'legacy:gpt-4',
        firstRunCompleted: true,
      };

      // First, set legacy config
      await configManager.updateConfig(legacyConfig);

      // Then migrate to multi-provider
      const migratedConfig = {
        ...legacyConfig,
        providers: {
          synthetic: {
            name: 'Synthetic.New',
            apiKey: legacyConfig.apiKey,
            baseUrl: legacyConfig.baseUrl,
            enabled: true,
          },
        },
        routingRules: {
          default: 'synthetic',
        },
      };

      const success = await configManager.updateConfig(migratedConfig);
      expect(success).toBe(true);

      // Verify migration preserves important settings
      expect(configManager.getSelectedModel()).toBe('legacy:gpt-4');
      expect(configManager.config.providers?.synthetic.apiKey).toBe('legacy-api-key');
      expect(configManager.isFirstRun()).toBe(false);
    });
  });
});