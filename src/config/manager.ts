import { readFile, writeFile, mkdir, chmod, stat } from 'fs/promises';
import { existsSync, readFileSync } from 'fs';
import { join } from 'path';
import { homedir } from 'os';
import {
  AppConfigSchema,
  AppConfig,
  ConfigValidationError,
  ConfigLoadError,
  ConfigSaveError,
  ProviderConfig,
  ConfigRoutingConfig,
  FallbackConfig,
  MultiProviderConfig,
} from './types';

export class ConfigManager {
  private configDir: string;
  private configPath: string;
  private _config: AppConfig | null = null;

  constructor(configDir?: string) {
    this.configDir = configDir || join(homedir(), '.config', 'aimux');
    this.configPath = join(this.configDir, 'config.json');
  }

  get config(): AppConfig {
    if (this._config === null) {
      this._config = this.loadConfig();
    }
    return this._config;
  }

  private async ensureConfigDir(): Promise<void> {
    try {
      await mkdir(this.configDir, { recursive: true });
    } catch (error) {
      throw new ConfigSaveError(`Failed to create config directory: ${this.configDir}`, error);
    }
  }

  private loadConfig(): AppConfig {
    try {
      if (!existsSync(this.configPath)) {
        // Config file doesn't exist, return defaults
        return AppConfigSchema.parse({});
      }

      const configData = JSON.parse(readFileSync(this.configPath, 'utf-8'));

      // Check if migration is needed
      const migratedConfig = this.migrateConfig(configData);

      const result = AppConfigSchema.safeParse(migratedConfig);

      if (!result.success) {
        // Try to preserve firstRunCompleted flag even if other config is invalid
        const preservedConfig = {
          firstRunCompleted: migratedConfig.firstRunCompleted || false,
        };

        const fallbackResult = AppConfigSchema.safeParse(preservedConfig);

        if (fallbackResult.success) {
          return fallbackResult.data;
        }

        return AppConfigSchema.parse({});
      }

      // Save migrated config if changes were made
      if (JSON.stringify(migratedConfig) !== JSON.stringify(configData)) {
        this.saveConfigAsync(result.data);
      }

      return result.data;
    } catch (error) {
      // Try to recover firstRunCompleted from partial config data
      if (existsSync(this.configPath)) {
        try {
          const partialConfig = JSON.parse(readFileSync(this.configPath, 'utf-8'));
          if (partialConfig.firstRunCompleted === true) {
            return AppConfigSchema.parse({ firstRunCompleted: true });
          }
        } catch {
          // Recovery failed, use defaults
        }
      }

      return AppConfigSchema.parse({});
    }
  }

  /**
   * Migrate legacy configuration to multi-provider format
   */
  private migrateConfig(configData: any): any {
    const migrated = { ...configData };

    // Check if we need to migrate from legacy single-provider to multi-provider
    if (
      !configData.multiProvider &&
      (configData.apiKey || configData.providers || configData.routingRules)
    ) {
      const multiProviderConfig: MultiProviderConfig = {
        providers: {},
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

      // Migrate synthetic-new provider from legacy config
      if (configData.apiKey || configData.baseUrl) {
        multiProviderConfig.providers['synthetic-new'] = {
          name: 'Synthetic.New',
          apiKey: configData.apiKey || '',
          baseUrl: configData.baseUrl,
          anthropicBaseUrl: configData.anthropicBaseUrl,
          modelsApiUrl: configData.modelsApiUrl,
          enabled: true,
          priority: 5,
          timeout: 30000,
          maxRetries: 3,
        };
      }

      // Migrate legacy providers object
      if (configData.providers) {
        Object.entries(configData.providers).forEach(([key, provider]: [string, any]) => {
          if (typeof provider === 'object') {
            (multiProviderConfig.providers as any)[key] = {
              ...provider,
              priority: provider.priority || 5,
              timeout: provider.timeout || 30000,
              maxRetries: provider.maxRetries || 3,
            };
          }
        });
      }

      // Migrate routing rules
      if (configData.routingRules) {
        if (configData.routingRules.thinking) {
          multiProviderConfig.routing.thinkingProvider = configData.routingRules.thinking;
        }
        if (configData.routingRules.vision) {
          multiProviderConfig.routing.visionProvider = configData.routingRules.vision;
        }
        if (configData.routingRules.tools) {
          multiProviderConfig.routing.toolsProvider = configData.routingRules.tools;
        }
      }

      migrated.multiProvider = multiProviderConfig;
    }

    return migrated;
  }

  /**
   * Async version of saveConfig for internal use
   */
  private async saveConfigAsync(config: AppConfig): Promise<void> {
    try {
      await this.ensureConfigDir();
      const configJson = JSON.stringify(config, null, 2);
      await writeFile(this.configPath, configJson, 'utf-8');
      await chmod(this.configPath, 0o600);
    } catch (error) {
      // Silently fail for background saves
      console.warn('Failed to save migrated config:', error);
    }
  }

  async saveConfig(config?: AppConfig): Promise<boolean> {
    const configToSave = config || this._config;
    if (!configToSave) {
      throw new ConfigSaveError('No configuration to save');
    }

    try {
      await this.ensureConfigDir();

      // Create backup of existing config
      try {
        if (existsSync(this.configPath)) {
          const backupPath = `${this.configPath}.backup`;
          const existingData = await readFile(this.configPath, 'utf-8');
          await writeFile(backupPath, existingData, 'utf-8');
        }
      } catch (backupError) {
        // Backup failed, but continue with saving
        console.warn('Failed to create config backup:', backupError);
      }

      // Write new configuration
      const configJson = JSON.stringify(configToSave, null, 2);
      await writeFile(this.configPath, configJson, 'utf-8');

      // Set secure permissions
      try {
        await chmod(this.configPath, 0o600);
      } catch (chmodError) {
        console.warn('Failed to set secure permissions on config file:', chmodError);
      }

      this._config = configToSave;
      return true;
    } catch (error) {
      throw new ConfigSaveError(`Failed to save configuration to ${this.configPath}`, error);
    }
  }

  async updateConfig(updates: Partial<AppConfig>): Promise<boolean> {
    try {
      const currentData = this.config;
      const updatedData = { ...currentData, ...updates };

      const result = AppConfigSchema.safeParse(updatedData);
      if (!result.success) {
        throw new ConfigValidationError(`Invalid configuration update: ${result.error.message}`);
      }

      return await this.saveConfig(result.data);
    } catch (error) {
      if (error instanceof ConfigValidationError || error instanceof ConfigSaveError) {
        throw error;
      }
      throw new ConfigSaveError('Failed to update configuration', error);
    }
  }

  hasApiKey(): boolean {
    return Boolean(this.config.apiKey);
  }

  getApiKey(): string {
    return this.config.apiKey;
  }

  async setApiKey(apiKey: string): Promise<boolean> {
    return this.updateConfig({ apiKey });
  }

  getSelectedModel(): string {
    return this.config.selectedModel;
  }

  async setSelectedModel(model: string): Promise<boolean> {
    return this.updateConfig({ selectedModel: model });
  }

  getCacheDuration(): number {
    return this.config.cacheDurationHours;
  }

  async setCacheDuration(hours: number): Promise<boolean> {
    try {
      return await this.updateConfig({ cacheDurationHours: hours });
    } catch (error) {
      if (error instanceof ConfigValidationError) {
        return false;
      }
      throw error;
    }
  }

  async isCacheValid(cacheFile: string): Promise<boolean> {
    try {
      const stats = await stat(cacheFile);
      const cacheAge = Date.now() - stats.mtime.getTime();
      const maxAge = this.config.cacheDurationHours * 60 * 60 * 1000;
      return cacheAge < maxAge;
    } catch (error) {
      return false;
    }
  }

  isFirstRun(): boolean {
    return !this.config.firstRunCompleted;
  }

  async markFirstRunCompleted(): Promise<boolean> {
    return this.updateConfig({ firstRunCompleted: true });
  }

  hasSavedModel(): boolean {
    return Boolean(this.config.selectedModel && this.config.firstRunCompleted);
  }

  getSavedModel(): string {
    if (this.hasSavedModel()) {
      return this.config.selectedModel;
    }
    return '';
  }

  async setSavedModel(model: string): Promise<boolean> {
    return this.updateConfig({ selectedModel: model, firstRunCompleted: true });
  }

  hasSavedThinkingModel(): boolean {
    return Boolean(this.config.selectedThinkingModel && this.config.firstRunCompleted);
  }

  getSavedThinkingModel(): string {
    if (this.hasSavedThinkingModel()) {
      return this.config.selectedThinkingModel;
    }
    return '';
  }

  async setSavedThinkingModel(model: string): Promise<boolean> {
    return this.updateConfig({ selectedThinkingModel: model, firstRunCompleted: true });
  }

  // Multi-provider configuration methods

  /**
   * Get multi-provider configuration
   */
  getMultiProviderConfig(): MultiProviderConfig | null {
    return this.config.multiProvider || null;
  }

  /**
   * Check if multi-provider mode is enabled
   */
  isMultiProviderEnabled(): boolean {
    return Boolean(this.config.multiProvider);
  }

  /**
   * Get configuration for a specific provider
   */
  getProviderConfig(providerId: string): ProviderConfig | null {
    const multiProvider = this.getMultiProviderConfig();
    if (!multiProvider) return null;

    return (multiProvider.providers as any)[providerId] || null;
  }

  /**
   * Set or update provider configuration
   */
  async setProviderConfig(providerId: string, config: Partial<ProviderConfig>): Promise<boolean> {
    try {
      const currentConfig = this.config;
      const multiProvider = currentConfig.multiProvider || {
        providers: {} as any,
        routing: {
          defaultProvider: 'synthetic-new',
          enableFailover: true,
          strategy: 'capability' as const,
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

      // Update or add provider configuration with proper merging
      const existingProvider = multiProvider.providers[providerId];
      const baseProvider: ProviderConfig = {
        name: providerId,
        apiKey: '',
        enabled: true,
        priority: 5,
        timeout: 30000,
        maxRetries: 3,
        ...existingProvider,
        ...config,
      };

      multiProvider.providers[providerId] = baseProvider;

      return await this.updateConfig({ multiProvider });
    } catch (error) {
      if (error instanceof ConfigValidationError) {
        throw error;
      }
      throw new ConfigSaveError('Failed to update provider configuration', error);
    }
  }

  /**
   * Get routing configuration
   */
  getRoutingConfig(): ConfigRoutingConfig | null {
    const multiProvider = this.getMultiProviderConfig();
    if (!multiProvider) return null;

    return multiProvider.routing;
  }

  /**
   * Update routing configuration
   */
  async updateRoutingConfig(routingConfig: Partial<ConfigRoutingConfig>): Promise<boolean> {
    try {
      const currentConfig = this.config;
      const multiProvider = currentConfig.multiProvider || {
        providers: {} as any,
        routing: {
          defaultProvider: 'synthetic-new',
          enableFailover: true,
          strategy: 'capability' as const,
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

      multiProvider.routing = {
        ...multiProvider.routing,
        ...routingConfig,
      };

      return await this.updateConfig({ multiProvider });
    } catch (error) {
      if (error instanceof ConfigValidationError) {
        throw error;
      }
      throw new ConfigSaveError('Failed to update routing configuration', error);
    }
  }

  /**
   * Get fallback configuration
   */
  getFallbackConfig(): FallbackConfig | null {
    const multiProvider = this.getMultiProviderConfig();
    if (!multiProvider) return null;

    return multiProvider.fallback;
  }

  /**
   * Update fallback configuration
   */
  async updateFallbackConfig(fallbackConfig: Partial<FallbackConfig>): Promise<boolean> {
    try {
      const currentConfig = this.config;
      const multiProvider = currentConfig.multiProvider || {
        providers: {} as any,
        routing: {
          defaultProvider: 'synthetic-new',
          enableFailover: true,
          strategy: 'capability' as const,
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

      multiProvider.fallback = {
        ...multiProvider.fallback,
        ...fallbackConfig,
      };

      return await this.updateConfig({ multiProvider });
    } catch (error) {
      if (error instanceof ConfigValidationError) {
        throw error;
      }
      throw new ConfigSaveError('Failed to update fallback configuration', error);
    }
  }

  /**
   * Get list of configured providers
   */
  getConfiguredProviders(): { id: string; config: ProviderConfig }[] {
    const multiProvider = this.getMultiProviderConfig();
    if (!multiProvider) return [];

    return Object.entries(multiProvider.providers)
      .filter(([_, config]) => config && config.enabled)
      .map(([id, config]) => ({ id, config: config }));
  }

  /**
   * Get provider for specific request type based on routing rules
   */
  getProviderForRequestType(
    requestType: 'default' | 'thinking' | 'vision' | 'tools'
  ): string | null {
    const routing = this.getRoutingConfig();
    if (!routing) return null;

    switch (requestType) {
      case 'thinking':
        return routing.thinkingProvider || routing.defaultProvider;
      case 'vision':
        return routing.visionProvider || routing.defaultProvider;
      case 'tools':
        return routing.toolsProvider || routing.defaultProvider;
      default:
        return routing.defaultProvider;
    }
  }

  /**
   * Enable or disable a provider
   */
  async setProviderEnabled(providerId: string, enabled: boolean): Promise<boolean> {
    const providerConfig = this.getProviderConfig(providerId);
    if (!providerConfig) {
      throw new ConfigValidationError(`Provider ${providerId} not found`);
    }

    return await this.setProviderConfig(providerId, { enabled });
  }

  /**
   * Validate provider configuration
   */
  validateProviderConfig(providerConfig: any): { valid: boolean; errors: string[] } {
    const errors: string[] = [];

    if (!providerConfig.name || typeof providerConfig.name !== 'string') {
      errors.push('Provider name is required and must be a string');
    }

    if (typeof providerConfig.enabled !== 'boolean') {
      errors.push('Provider enabled status must be a boolean');
    }

    if (providerConfig.priority !== undefined) {
      if (
        typeof providerConfig.priority !== 'number' ||
        providerConfig.priority < 1 ||
        providerConfig.priority > 10
      ) {
        errors.push('Provider priority must be a number between 1 and 10');
      }
    }

    if (providerConfig.timeout !== undefined) {
      if (
        typeof providerConfig.timeout !== 'number' ||
        providerConfig.timeout < 1000 ||
        providerConfig.timeout > 300000
      ) {
        errors.push('Provider timeout must be a number between 1000 and 300000 (ms)');
      }
    }

    if (providerConfig.maxRetries !== undefined) {
      if (
        typeof providerConfig.maxRetries !== 'number' ||
        providerConfig.maxRetries < 0 ||
        providerConfig.maxRetries > 5
      ) {
        errors.push('Provider maxRetries must be a number between 0 and 5');
      }
    }

    return {
      valid: errors.length === 0,
      errors,
    };
  }

  /**
   * Import configuration from synclaude (migration utility)
   */
  async importFromSynclaude(): Promise<boolean> {
    try {
      const synclaudeConfigPath = join(homedir(), '.config', 'synclaude', 'config.json');

      if (!existsSync(synclaudeConfigPath)) {
        throw new ConfigLoadError('Synclaude configuration file not found');
      }

      const synclaudeConfig = JSON.parse(readFileSync(synclaudeConfigPath, 'utf-8'));

      // Create synthetic-new provider from synclaude config
      const providerConfig: ProviderConfig = {
        name: 'Synthetic.New (migrated from SynClaude)',
        apiKey: synclaudeConfig.apiKey || '',
        baseUrl: synclaudeConfig.baseUrl,
        anthropicBaseUrl: synclaudeConfig.anthropicBaseUrl,
        modelsApiUrl: synclaudeConfig.modelsApiUrl,
        enabled: true,
        priority: 5,
        timeout: 30000,
        maxRetries: 3,
      };

      return await this.setProviderConfig('synthetic-new', providerConfig);
    } catch (error) {
      throw new ConfigLoadError('Failed to import Synclaude configuration', error);
    }
  }
}
