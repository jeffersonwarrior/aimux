"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.ConfigManager = void 0;
const promises_1 = require("fs/promises");
const fs_1 = require("fs");
const path_1 = require("path");
const os_1 = require("os");
const types_1 = require("./types");
class ConfigManager {
    configDir;
    configPath;
    _config = null;
    constructor(configDir) {
        this.configDir = configDir || (0, path_1.join)((0, os_1.homedir)(), '.config', 'aimux');
        this.configPath = (0, path_1.join)(this.configDir, 'config.json');
    }
    get config() {
        if (this._config === null) {
            this._config = this.loadConfig();
        }
        return this._config;
    }
    async ensureConfigDir() {
        try {
            await (0, promises_1.mkdir)(this.configDir, { recursive: true });
        }
        catch (error) {
            throw new types_1.ConfigSaveError(`Failed to create config directory: ${this.configDir}`, error);
        }
    }
    loadConfig() {
        try {
            if (!(0, fs_1.existsSync)(this.configPath)) {
                // Config file doesn't exist, return defaults
                return types_1.AppConfigSchema.parse({});
            }
            const configData = JSON.parse((0, fs_1.readFileSync)(this.configPath, 'utf-8'));
            // Check if migration is needed
            const migratedConfig = this.migrateConfig(configData);
            const result = types_1.AppConfigSchema.safeParse(migratedConfig);
            if (!result.success) {
                // Try to preserve firstRunCompleted flag even if other config is invalid
                const preservedConfig = {
                    firstRunCompleted: migratedConfig.firstRunCompleted || false,
                };
                const fallbackResult = types_1.AppConfigSchema.safeParse(preservedConfig);
                if (fallbackResult.success) {
                    return fallbackResult.data;
                }
                return types_1.AppConfigSchema.parse({});
            }
            // Save migrated config if changes were made
            if (JSON.stringify(migratedConfig) !== JSON.stringify(configData)) {
                this.saveConfigAsync(result.data);
            }
            return result.data;
        }
        catch (error) {
            // Try to recover firstRunCompleted from partial config data
            if ((0, fs_1.existsSync)(this.configPath)) {
                try {
                    const partialConfig = JSON.parse((0, fs_1.readFileSync)(this.configPath, 'utf-8'));
                    if (partialConfig.firstRunCompleted === true) {
                        return types_1.AppConfigSchema.parse({ firstRunCompleted: true });
                    }
                }
                catch {
                    // Recovery failed, use defaults
                }
            }
            return types_1.AppConfigSchema.parse({});
        }
    }
    /**
     * Migrate legacy configuration to multi-provider format
     */
    migrateConfig(configData) {
        const migrated = { ...configData };
        // Check if we need to migrate from legacy single-provider to multi-provider
        if (!configData.multiProvider &&
            (configData.apiKey || configData.providers || configData.routingRules)) {
            const multiProviderConfig = {
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
                Object.entries(configData.providers).forEach(([key, provider]) => {
                    if (typeof provider === 'object') {
                        multiProviderConfig.providers[key] = {
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
    async saveConfigAsync(config) {
        try {
            await this.ensureConfigDir();
            const configJson = JSON.stringify(config, null, 2);
            await (0, promises_1.writeFile)(this.configPath, configJson, 'utf-8');
            await (0, promises_1.chmod)(this.configPath, 0o600);
        }
        catch (error) {
            // Silently fail for background saves
            console.warn('Failed to save migrated config:', error);
        }
    }
    async saveConfig(config) {
        const configToSave = config || this._config;
        if (!configToSave) {
            throw new types_1.ConfigSaveError('No configuration to save');
        }
        try {
            await this.ensureConfigDir();
            // Create backup of existing config
            try {
                if ((0, fs_1.existsSync)(this.configPath)) {
                    const backupPath = `${this.configPath}.backup`;
                    const existingData = await (0, promises_1.readFile)(this.configPath, 'utf-8');
                    await (0, promises_1.writeFile)(backupPath, existingData, 'utf-8');
                }
            }
            catch (backupError) {
                // Backup failed, but continue with saving
                console.warn('Failed to create config backup:', backupError);
            }
            // Write new configuration
            const configJson = JSON.stringify(configToSave, null, 2);
            await (0, promises_1.writeFile)(this.configPath, configJson, 'utf-8');
            // Set secure permissions
            try {
                await (0, promises_1.chmod)(this.configPath, 0o600);
            }
            catch (chmodError) {
                console.warn('Failed to set secure permissions on config file:', chmodError);
            }
            this._config = configToSave;
            return true;
        }
        catch (error) {
            throw new types_1.ConfigSaveError(`Failed to save configuration to ${this.configPath}`, error);
        }
    }
    async updateConfig(updates) {
        try {
            const currentData = this.config;
            const updatedData = { ...currentData, ...updates };
            const result = types_1.AppConfigSchema.safeParse(updatedData);
            if (!result.success) {
                throw new types_1.ConfigValidationError(`Invalid configuration update: ${result.error.message}`);
            }
            return await this.saveConfig(result.data);
        }
        catch (error) {
            if (error instanceof types_1.ConfigValidationError || error instanceof types_1.ConfigSaveError) {
                throw error;
            }
            throw new types_1.ConfigSaveError('Failed to update configuration', error);
        }
    }
    hasApiKey() {
        return Boolean(this.config.apiKey);
    }
    getApiKey() {
        return this.config.apiKey;
    }
    async setApiKey(apiKey) {
        return this.updateConfig({ apiKey });
    }
    getSelectedModel() {
        return this.config.selectedModel;
    }
    async setSelectedModel(model) {
        return this.updateConfig({ selectedModel: model });
    }
    getCacheDuration() {
        return this.config.cacheDurationHours;
    }
    async setCacheDuration(hours) {
        try {
            return await this.updateConfig({ cacheDurationHours: hours });
        }
        catch (error) {
            if (error instanceof types_1.ConfigValidationError) {
                return false;
            }
            throw error;
        }
    }
    async isCacheValid(cacheFile) {
        try {
            const stats = await (0, promises_1.stat)(cacheFile);
            const cacheAge = Date.now() - stats.mtime.getTime();
            const maxAge = this.config.cacheDurationHours * 60 * 60 * 1000;
            return cacheAge < maxAge;
        }
        catch (error) {
            return false;
        }
    }
    isFirstRun() {
        return !this.config.firstRunCompleted;
    }
    async markFirstRunCompleted() {
        return this.updateConfig({ firstRunCompleted: true });
    }
    hasSavedModel() {
        return Boolean(this.config.selectedModel && this.config.firstRunCompleted);
    }
    getSavedModel() {
        if (this.hasSavedModel()) {
            return this.config.selectedModel;
        }
        return '';
    }
    async setSavedModel(model) {
        return this.updateConfig({ selectedModel: model, firstRunCompleted: true });
    }
    hasSavedThinkingModel() {
        return Boolean(this.config.selectedThinkingModel && this.config.firstRunCompleted);
    }
    getSavedThinkingModel() {
        if (this.hasSavedThinkingModel()) {
            return this.config.selectedThinkingModel;
        }
        return '';
    }
    async setSavedThinkingModel(model) {
        return this.updateConfig({ selectedThinkingModel: model, firstRunCompleted: true });
    }
    // Multi-provider configuration methods
    /**
     * Get multi-provider configuration
     */
    getMultiProviderConfig() {
        return this.config.multiProvider || null;
    }
    /**
     * Check if multi-provider mode is enabled
     */
    isMultiProviderEnabled() {
        return Boolean(this.config.multiProvider);
    }
    /**
     * Get configuration for a specific provider
     */
    getProviderConfig(providerId) {
        const multiProvider = this.getMultiProviderConfig();
        if (!multiProvider)
            return null;
        return multiProvider.providers[providerId] || null;
    }
    /**
     * Set or update provider configuration
     */
    async setProviderConfig(providerId, config) {
        try {
            const currentConfig = this.config;
            const multiProvider = currentConfig.multiProvider || {
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
            // Update or add provider configuration with proper merging
            const existingProvider = multiProvider.providers[providerId];
            const baseProvider = {
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
        }
        catch (error) {
            if (error instanceof types_1.ConfigValidationError) {
                throw error;
            }
            throw new types_1.ConfigSaveError('Failed to update provider configuration', error);
        }
    }
    /**
     * Get routing configuration
     */
    getRoutingConfig() {
        const multiProvider = this.getMultiProviderConfig();
        if (!multiProvider)
            return null;
        return multiProvider.routing;
    }
    /**
     * Update routing configuration
     */
    async updateRoutingConfig(routingConfig) {
        try {
            const currentConfig = this.config;
            const multiProvider = currentConfig.multiProvider || {
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
            multiProvider.routing = {
                ...multiProvider.routing,
                ...routingConfig,
            };
            return await this.updateConfig({ multiProvider });
        }
        catch (error) {
            if (error instanceof types_1.ConfigValidationError) {
                throw error;
            }
            throw new types_1.ConfigSaveError('Failed to update routing configuration', error);
        }
    }
    /**
     * Get fallback configuration
     */
    getFallbackConfig() {
        const multiProvider = this.getMultiProviderConfig();
        if (!multiProvider)
            return null;
        return multiProvider.fallback;
    }
    /**
     * Update fallback configuration
     */
    async updateFallbackConfig(fallbackConfig) {
        try {
            const currentConfig = this.config;
            const multiProvider = currentConfig.multiProvider || {
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
            multiProvider.fallback = {
                ...multiProvider.fallback,
                ...fallbackConfig,
            };
            return await this.updateConfig({ multiProvider });
        }
        catch (error) {
            if (error instanceof types_1.ConfigValidationError) {
                throw error;
            }
            throw new types_1.ConfigSaveError('Failed to update fallback configuration', error);
        }
    }
    /**
     * Get list of configured providers
     */
    getConfiguredProviders() {
        const multiProvider = this.getMultiProviderConfig();
        if (!multiProvider)
            return [];
        return Object.entries(multiProvider.providers)
            .filter(([_, config]) => config && config.enabled)
            .map(([id, config]) => ({ id, config: config }));
    }
    /**
     * Get provider for specific request type based on routing rules
     */
    getProviderForRequestType(requestType) {
        const routing = this.getRoutingConfig();
        if (!routing)
            return null;
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
    async setProviderEnabled(providerId, enabled) {
        const providerConfig = this.getProviderConfig(providerId);
        if (!providerConfig) {
            throw new types_1.ConfigValidationError(`Provider ${providerId} not found`);
        }
        return await this.setProviderConfig(providerId, { enabled });
    }
    /**
     * Validate provider configuration
     */
    validateProviderConfig(providerConfig) {
        const errors = [];
        if (!providerConfig.name || typeof providerConfig.name !== 'string') {
            errors.push('Provider name is required and must be a string');
        }
        if (typeof providerConfig.enabled !== 'boolean') {
            errors.push('Provider enabled status must be a boolean');
        }
        if (providerConfig.priority !== undefined) {
            if (typeof providerConfig.priority !== 'number' ||
                providerConfig.priority < 1 ||
                providerConfig.priority > 10) {
                errors.push('Provider priority must be a number between 1 and 10');
            }
        }
        if (providerConfig.timeout !== undefined) {
            if (typeof providerConfig.timeout !== 'number' ||
                providerConfig.timeout < 1000 ||
                providerConfig.timeout > 300000) {
                errors.push('Provider timeout must be a number between 1000 and 300000 (ms)');
            }
        }
        if (providerConfig.maxRetries !== undefined) {
            if (typeof providerConfig.maxRetries !== 'number' ||
                providerConfig.maxRetries < 0 ||
                providerConfig.maxRetries > 5) {
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
    async importFromSynclaude() {
        try {
            const synclaudeConfigPath = (0, path_1.join)((0, os_1.homedir)(), '.config', 'synclaude', 'config.json');
            if (!(0, fs_1.existsSync)(synclaudeConfigPath)) {
                throw new types_1.ConfigLoadError('Synclaude configuration file not found');
            }
            const synclaudeConfig = JSON.parse((0, fs_1.readFileSync)(synclaudeConfigPath, 'utf-8'));
            // Create synthetic-new provider from synclaude config
            const providerConfig = {
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
        }
        catch (error) {
            throw new types_1.ConfigLoadError('Failed to import Synclaude configuration', error);
        }
    }
}
exports.ConfigManager = ConfigManager;
//# sourceMappingURL=manager.js.map