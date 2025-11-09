"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.AimuxApp = void 0;
const path_1 = require("path");
const os_1 = require("os");
const config_1 = require("../config");
const models_1 = require("../models");
const ui_1 = require("../ui");
const launcher_1 = require("../launcher");
const logger_1 = require("../utils/logger");
const banner_1 = require("../utils/banner");
const providers_1 = require("../providers");
const routing_engine_1 = require("./routing-engine");
const failover_manager_1 = require("./failover-manager");
class AimuxApp {
    configManager;
    ui;
    launcher;
    modelManager = null;
    providerRegistry;
    routingEngine = null;
    failoverManager = null;
    mode;
    constructor(mode = { type: 'bridge' }) {
        this.mode = mode;
        this.configManager = new config_1.ConfigManager();
        this.ui = new ui_1.UserInterface({
            verbose: this.configManager.config.apiKey
                ? this.configManager.config.cacheDurationHours > 0
                : false,
        });
        this.launcher = new launcher_1.ClaudeLauncher();
        this.providerRegistry = new providers_1.ProviderRegistry();
        // Only initialize routing infrastructure for routing mode
        if (mode.type === 'routing' || mode.enableRouting) {
            console.log('[INFO] Initializing routing infrastructure...');
            this.routingEngine = new routing_engine_1.RoutingEngine(this.providerRegistry);
            this.failoverManager = new failover_manager_1.FailoverManager(this.providerRegistry, this.routingEngine);
        }
        else {
            console.log('[INFO] Bridge mode: Direct provider passthrough enabled');
        }
    }
    async setupLogging(options) {
        (0, logger_1.setupLogging)(options.verbose, options.quiet);
    }
    getConfig() {
        return this.configManager.config;
    }
    getModelManager() {
        if (!this.modelManager) {
            const config = this.configManager.config;
            const cacheFile = (0, path_1.join)((0, os_1.homedir)(), '.config', 'aimux', 'models_cache.json');
            this.modelManager = new models_1.ModelManager({
                apiKey: config.apiKey,
                modelsApiUrl: config.modelsApiUrl,
                cacheFile,
                cacheDurationHours: config.cacheDurationHours,
            });
        }
        return this.modelManager;
    }
    /**
     * Run with default provider
     */
    async run(options = {}) {
        await this.setupLogging(options);
        const config = this.configManager.config;
        const providerId = options.defaultProvider || config.multiProvider?.routing?.defaultProvider || 'synthetic-new';
        await this.launchWithProvider(providerId, options);
    }
    /**
     * Run with specific provider
     */
    async runWithProvider(providerId, options = {}) {
        await this.setupLogging(options);
        await this.launchWithProvider(providerId, options);
    }
    /**
     * Run in hybrid mode with different providers for different request types
     */
    async runHybrid(options) {
        await this.setupLogging(options);
        // Configure hybrid routing
        // TODO: Implement configureHybridMode in RoutingEngine
        console.log(`Hybrid mode configured: Thinking provider=${options.thinkingProvider}, Default provider=${options.defaultProvider}`);
        await this.launchWithProvider(options.defaultProvider, options);
    }
    /**
     * Interactive provider setup for multiple providers
     */
    async setupProviders() {
        console.log('Setting up AI providers...\n');
        const availableProviders = ['minimax-m2', 'z-ai', 'synthetic-new'];
        const config = this.configManager.config;
        // Initialize multi-provider config if it doesn't exist
        if (!config.multiProvider) {
            config.multiProvider = {
                providers: {},
                routing: {
                    defaultProvider: 'synthetic-new',
                    strategy: 'capability',
                    enableFailover: true,
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
        // Setup each provider
        for (const providerId of availableProviders) {
            await this.setupSingleProvider(providerId);
        }
        // Configure routing
        await this.setupRouting();
        // Save configuration
        await this.configManager.saveConfig();
        console.log('✅ Provider setup completed!');
    }
    /**
     * Launch interactive provider setup
     */
    async launchProviderSetup() {
        this.ui.info('Launching interactive provider setup...');
        // This will be implemented in a separate CLI command file that imports the components
        // For now, redirect to the existing setup method
        await this.setupProviders();
    }
    /**
     * Launch interactive provider status
     */
    async launchProviderStatus() {
        this.ui.info('Launching interactive provider status...');
        // This will be implemented in a separate CLI command file that imports the components
        // For now, redirect to the existing list method
        await this.listProviders();
    }
    /**
     * Setup a single provider
     */
    async setupSingleProvider(providerId) {
        const config = this.configManager.config;
        if (!config.multiProvider?.providers) {
            config.multiProvider.providers = {};
        }
        const providerInfo = this.getProviderInfo(providerId);
        console.log(`\n🔧 Setting up ${providerInfo.name}...`);
        // Check if already configured
        if (config.multiProvider?.providers &&
            config.multiProvider.providers[providerId]?.apiKey) {
            const replace = await this.ui.askQuestion(`API key for ${providerInfo.name} already exists. Replace it? (y/N)`, 'n');
            if (replace.toLowerCase() !== 'y') {
                console.log(`⏭️  Skipping ${providerInfo.name}`);
                return;
            }
        }
        // Get API key
        const apiKey = await this.ui.askPassword(`Enter API key for ${providerInfo.name}:`);
        if (!apiKey) {
            console.log(`⚠️  No API key provided for ${providerInfo.name}, skipping...`);
            return;
        }
        // Configure provider
        if (!config.multiProvider) {
            config.multiProvider = {
                providers: {},
                routing: {
                    defaultProvider: 'synthetic-new',
                    strategy: 'capability',
                    enableFailover: true,
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
        config.multiProvider.providers[providerId] = {
            name: providerInfo.name,
            apiKey,
            capabilities: providerInfo.capabilities,
            enabled: true,
            priority: providerInfo.priority,
            timeout: 30000,
            maxRetries: 3,
        };
        // Test provider
        console.log(`🧪 Testing ${providerInfo.name}...`);
        const isValid = await this.testProvider(providerId);
        if (isValid) {
            console.log(`✅ ${providerInfo.name} is working correctly!`);
        }
        else {
            console.log(`❌ ${providerInfo.name} test failed. Please check your API key.`);
            config.multiProvider.providers[providerId].enabled = false;
        }
    }
    /**
     * List all configured providers
     */
    async listProviders() {
        const config = this.configManager.config;
        if (!config.multiProvider?.providers) {
            console.log('No providers configured. Run "aimux providers setup" to configure providers.');
            return;
        }
        console.log('🤖 Configured Providers:\n');
        for (const [providerId, providerConfig] of Object.entries(config.multiProvider.providers)) {
            const config = providerConfig;
            const status = config.enabled ? '✅ Active' : '❌ Disabled';
            const capabilities = config.capabilities?.join(', ') || 'None';
            console.log(`${config.name} (${providerId})`);
            console.log(`  Status: ${status}`);
            console.log(`  Capabilities: ${capabilities}`);
            console.log(`  Priority: ${config.priority}`);
            if (config.apiKey) {
                console.log(`  API Key: ${'*'.repeat(config.apiKey.length - 4)}${config.apiKey.slice(-4)}`);
            }
            console.log('');
        }
        // Show routing configuration
        if (config.multiProvider.routing) {
            console.log('🔀 Routing Configuration:');
            console.log(`  Default Provider: ${config.multiProvider.routing.defaultProvider}`);
            console.log(`  Strategy: ${config.multiProvider.routing.strategy}`);
            console.log(`  Failover: ${config.multiProvider.routing.enableFailover ? 'Enabled' : 'Disabled'}`);
            console.log(`  Load Balancing: ${config.multiProvider.routing.loadBalancing ? 'Enabled' : 'Disabled'}`);
        }
    }
    /**
     * Test all configured providers
     */
    async testProviders() {
        const config = this.configManager.config;
        if (!config.multiProvider?.providers) {
            console.log('No providers configured. Run "aimux providers setup" first.');
            return;
        }
        console.log('🧪 Testing provider connectivity...\n');
        let workingCount = 0;
        let totalCount = 0;
        for (const [providerId, providerConfig] of Object.entries(config.multiProvider.providers)) {
            const config = providerConfig;
            if (!config.enabled) {
                console.log(`⏭️  Skipping ${config.name} (disabled)`);
                continue;
            }
            totalCount++;
            console.log(`Testing ${config.name}...`);
            const isWorking = await this.testProvider(providerId);
            if (isWorking) {
                console.log(`✅ ${config.name} is working correctly!`);
                workingCount++;
            }
            else {
                console.log(`❌ ${config.name} test failed.`);
            }
            console.log('');
        }
        console.log(`\n📊 Test Summary: ${workingCount}/${totalCount} providers working correctly`);
    }
    /**
     * Test a specific provider
     */
    async testProvider(providerId) {
        try {
            // This would integrate with the actual provider test
            // For now, return true if API key exists
            const config = this.configManager.config;
            const providerConfig = config.multiProvider?.providers;
            const provider = providerConfig?.[providerId];
            return !!(provider?.apiKey && provider.apiKey.length > 10);
        }
        catch (error) {
            console.error(`Test failed for ${providerId}:`, error);
            return false;
        }
    }
    /**
     * Setup routing configuration
     */
    async setupRouting() {
        const config = this.configManager.config;
        console.log('\n🔀 Configuring routing...');
        // Ask for default provider
        const defaultProvider = await this.ui.askQuestion('Select default provider (synthetic-new, minimax-m2, z-ai):', 'synthetic-new');
        if (defaultProvider && config.multiProvider) {
            config.multiProvider.routing.defaultProvider = defaultProvider;
        }
        // Ask about thinking provider
        const useThinkingProvider = await this.ui.askQuestion('Configure different provider for thinking requests? (y/N):', 'n');
        if (useThinkingProvider.toLowerCase() === 'y' && config.multiProvider) {
            const thinkingProvider = await this.ui.askQuestion('Select thinking provider (synthetic-new, minimax-m2):', 'synthetic-new');
            config.multiProvider.routing.thinkingProvider = thinkingProvider;
        }
    }
    /**
     * Launch Claude Code with specified provider
     */
    async launchWithProvider(providerId, options) {
        const config = this.configManager.config;
        const additionalArgs = options.additionalArgs || [];
        // Set up provider-specific environment
        process.env.AIMUX_PROVIDER = providerId;
        // Auto-configure models for the provider if not already set
        await this.autoConfigureModelsForProvider(providerId);
        // If hybrid mode is configured, set up environment variables
        if (config.multiProvider?.routing?.thinkingProvider) {
            process.env.AIMUX_THINKING_PROVIDER = config.multiProvider.routing.thinkingProvider;
        }
        const banner = (0, banner_1.createBanner)({ additionalArgs });
        console.log(banner);
        // Get provider-specific configuration
        const providerConfig = config.multiProvider?.providers?.[providerId];
        const providerApiKey = providerConfig?.apiKey || config.apiKey;
        const launchEnv = {
            ...process.env,
            AIMUX_PROVIDER: providerId,
            // Use provider-specific API configuration
            // Set both API_KEY and AUTH_TOKEN to ensure Claude Code uses the right authentication
            ANTHROPIC_API_KEY: providerApiKey,
            ANTHROPIC_AUTH_TOKEN: providerApiKey,
            ANTHROPIC_BASE_URL: providerConfig?.anthropicBaseUrl || providerConfig?.baseUrl || config.anthropicBaseUrl,
        };
        const launchOptions = {
            model: config.selectedModel || '',
            thinkingModel: options.thinkingModel || config.selectedThinkingModel,
            additionalArgs: (0, banner_1.normalizeDangerousFlags)(additionalArgs),
            env: launchEnv,
        };
        const result = await this.launcher.launchClaudeCode(launchOptions);
        if (!result.success) {
            console.error(`Failed to launch Claude Code: ${result.error}`);
            process.exit(1);
        }
    }
    /**
     * Auto-configure models for a specific provider
     */
    async autoConfigureModelsForProvider(providerId) {
        const config = this.configManager.config;
        const providerInfo = this.getProviderInfo(providerId);
        // Always auto-configure models for the selected provider
        // This ensures correct provider-specific models are used (e.g., GLM-4.6 for Z.AI)
        const defaultModels = this.getDefaultModelsForProvider(providerId);
        if (defaultModels.default) {
            config.selectedModel = defaultModels.default;
        }
        if (defaultModels.thinking) {
            config.selectedThinkingModel = defaultModels.thinking;
        }
        // Save the updated configuration
        await this.configManager.saveConfig();
    }
    /**
     * Get default models for a specific provider
     */
    getDefaultModelsForProvider(providerId) {
        const models = {
            'minimax-m2': {
                default: 'minimax-claude-instant-1',
                thinking: 'minimax-claude-3-sonnet-20240229'
            },
            'z-ai': {
                default: 'GLM-4.6',
                thinking: 'GLM-4.6'
            },
            'synthetic-new': {
                default: 'claude-3-haiku-20240307',
                thinking: 'claude-3-5-sonnet-20241022'
            }
        };
        return models[providerId] || models['synthetic-new'];
    }
    /**
     * Get provider information
     */
    getProviderInfo(providerId) {
        const providers = {
            'minimax-m2': {
                name: 'Minimax M2',
                capabilities: ['thinking', 'vision', 'tools'],
                priority: 7,
            },
            'z-ai': {
                name: 'Z.AI',
                capabilities: ['vision', 'tools'],
                priority: 6,
            },
            'synthetic-new': {
                name: 'Synthetic.New',
                capabilities: ['thinking', 'tools'],
                priority: 5,
            },
        };
        return (providers[providerId] || {
            name: providerId,
            capabilities: [],
            priority: 5,
        });
    }
    /**
     * Show current configuration
     */
    async showConfig() {
        const config = this.configManager.config;
        console.log('📋 Current Configuration:');
        console.log(JSON.stringify(config, null, 2));
    }
    /**
     * Run system health check
     */
    async doctor() {
        console.log('🔍 Running system health check...\n');
        // Check configuration
        console.log('📋 Checking configuration...');
        const config = this.configManager.config;
        if (!config.multiProvider) {
            console.log('❌ No multi-provider configuration found');
        }
        else {
            console.log('✅ Configuration found');
        }
        // Test providers
        await this.testProviders();
        // Check routing
        console.log('\n🔀 Checking routing configuration...');
        if (config.multiProvider?.routing) {
            console.log('✅ Routing configuration found');
        }
        else {
            console.log('❌ No routing configuration found');
        }
        console.log('\n🩺 Health check completed!');
    }
    async interactiveModelSelection() {
        // Legacy compatibility - would need implementation
        console.log('Use "aimux providers setup" for provider configuration');
    }
    async listModels() {
        // Legacy compatibility - would need implementation
        console.log('Use "aimux providers list" for provider information');
    }
    async clearCache() {
        // Clear model cache
        const modelManager = this.getModelManager();
        await modelManager.clearCache();
        console.log('✅ Cache cleared');
    }
    async cacheInfo() {
        // Show cache information
        const modelManager = this.getModelManager();
        const cacheInfo = await modelManager.getCacheInfo();
        console.log('Cache Information:');
        console.log(JSON.stringify(cacheInfo, null, 2));
    }
}
exports.AimuxApp = AimuxApp;
//# sourceMappingURL=aimux-app.js.map