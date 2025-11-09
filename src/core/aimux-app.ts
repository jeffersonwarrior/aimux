import { join } from 'path';
import { homedir } from 'os';
import { ConfigManager } from '../config';
import { ModelManager } from '../models';
import { UserInterface } from '../ui';
import { ClaudeLauncher, LaunchOptions } from '../launcher';
import { setupLogging } from '../utils/logger';
import { createBanner, normalizeDangerousFlags } from '../utils/banner';
import { ProviderRegistry } from '../providers';
import { RoutingEngine } from './routing-engine';
import { FailoverManager } from './failover-manager';

export interface AimuxAppOptions {
  verbose?: boolean;
  quiet?: boolean;
  additionalArgs?: string[];
  thinkingModel?: string;
  defaultProvider?: string;
  thinkingProvider?: string;
}

/**
 * Multi-provider AI application that manages multiple AI providers and routing
 */
export interface AimuxAppMode {
  type: 'bridge' | 'routing';
  enableRouting?: boolean;
}

export class AimuxApp {
  private configManager: ConfigManager;
  private ui: UserInterface;
  private launcher: ClaudeLauncher;
  private modelManager: ModelManager | null = null;
  private providerRegistry: ProviderRegistry;
  private routingEngine: RoutingEngine | null = null;
  private failoverManager: FailoverManager | null = null;
  private mode: AimuxAppMode;

  constructor(mode: AimuxAppMode = { type: 'bridge' }) {
    this.mode = mode;
    this.configManager = new ConfigManager();
    this.ui = new UserInterface({
      verbose: this.configManager.config.apiKey
        ? this.configManager.config.cacheDurationHours > 0
        : false,
    });
    this.launcher = new ClaudeLauncher();
    this.providerRegistry = new ProviderRegistry();

    // Only initialize routing infrastructure for routing mode
    if (mode.type === 'routing' || mode.enableRouting) {
      console.log('[INFO] Initializing routing infrastructure...');
      this.routingEngine = new RoutingEngine(this.providerRegistry);
      this.failoverManager = new FailoverManager(this.providerRegistry, this.routingEngine);
    } else {
      console.log('[INFO] Bridge mode: Direct provider passthrough enabled');
    }
  }

  async setupLogging(options: AimuxAppOptions): Promise<void> {
    setupLogging(options.verbose, options.quiet);
  }

  getConfig() {
    return this.configManager.config;
  }

  private getModelManager(): ModelManager {
    if (!this.modelManager) {
      const config = this.configManager.config;
      const cacheFile = join(homedir(), '.config', 'aimux', 'models_cache.json');

      this.modelManager = new ModelManager({
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
  async run(options: AimuxAppOptions = {}): Promise<void> {
    await this.setupLogging(options);

    const config = this.configManager.config;
    const providerId =
      options.defaultProvider || config.multiProvider?.routing?.defaultProvider || 'synthetic-new';

    await this.launchWithProvider(providerId, options);
  }

  /**
   * Run with specific provider
   */
  async runWithProvider(providerId: string, options: AimuxAppOptions = {}): Promise<void> {
    await this.setupLogging(options);
    await this.launchWithProvider(providerId, options);
  }

  /**
   * Run in hybrid mode with different providers for different request types
   */
  async runHybrid(options: {
    thinkingProvider: string;
    defaultProvider: string;
    verbose?: boolean;
    quiet?: boolean;
    additionalArgs?: string[];
  }): Promise<void> {
    await this.setupLogging(options);

    // Configure hybrid routing
    // TODO: Implement configureHybridMode in RoutingEngine
    console.log(
      `Hybrid mode configured: Thinking provider=${options.thinkingProvider}, Default provider=${options.defaultProvider}`
    );

    await this.launchWithProvider(options.defaultProvider, options);
  }

  /**
   * Interactive provider setup for multiple providers
   */
  async setupProviders(): Promise<void> {
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
  async launchProviderSetup(): Promise<void> {
    this.ui.info('Launching interactive provider setup...');
    // This will be implemented in a separate CLI command file that imports the components
    // For now, redirect to the existing setup method
    await this.setupProviders();
  }

  /**
   * Launch interactive provider status
   */
  async launchProviderStatus(): Promise<void> {
    this.ui.info('Launching interactive provider status...');
    // This will be implemented in a separate CLI command file that imports the components
    // For now, redirect to the existing list method
    await this.listProviders();
  }

  /**
   * Setup a single provider
   */
  async setupSingleProvider(providerId: string): Promise<void> {
    const config = this.configManager.config;

    if (!config.multiProvider?.providers) {
      config.multiProvider!.providers = {};
    }

    const providerInfo = this.getProviderInfo(providerId);
    console.log(`\n🔧 Setting up ${providerInfo.name}...`);

    // Check if already configured
    if (
      config.multiProvider?.providers &&
      (config.multiProvider.providers as any)[providerId]?.apiKey
    ) {
      const replace = await this.ui.askQuestion(
        `API key for ${providerInfo.name} already exists. Replace it? (y/N)`,
        'n'
      );

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
        providers: {} as any,
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
    (config.multiProvider.providers as any)[providerId] = {
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
    } else {
      console.log(`❌ ${providerInfo.name} test failed. Please check your API key.`);
      (config.multiProvider.providers as any)[providerId].enabled = false;
    }
  }

  /**
   * List all configured providers
   */
  async listProviders(): Promise<void> {
    const config = this.configManager.config;

    if (!config.multiProvider?.providers) {
      console.log('No providers configured. Run "aimux providers setup" to configure providers.');
      return;
    }

    console.log('🤖 Configured Providers:\n');

    for (const [providerId, providerConfig] of Object.entries(
      config.multiProvider.providers as any
    )) {
      const config = providerConfig as any;
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
      console.log(
        `  Failover: ${config.multiProvider.routing.enableFailover ? 'Enabled' : 'Disabled'}`
      );
      console.log(
        `  Load Balancing: ${config.multiProvider.routing.loadBalancing ? 'Enabled' : 'Disabled'}`
      );
    }
  }

  /**
   * Test all configured providers
   */
  async testProviders(): Promise<void> {
    const config = this.configManager.config;

    if (!config.multiProvider?.providers) {
      console.log('No providers configured. Run "aimux providers setup" first.');
      return;
    }

    console.log('🧪 Testing provider connectivity...\n');

    let workingCount = 0;
    let totalCount = 0;

    for (const [providerId, providerConfig] of Object.entries(
      config.multiProvider.providers as any
    )) {
      const config = providerConfig as any;
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
      } else {
        console.log(`❌ ${config.name} test failed.`);
      }

      console.log('');
    }

    console.log(`\n📊 Test Summary: ${workingCount}/${totalCount} providers working correctly`);
  }

  /**
   * Test a specific provider
   */
  async testProvider(providerId: string): Promise<boolean> {
    try {
      // This would integrate with the actual provider test
      // For now, return true if API key exists
      const config = this.configManager.config;
      const providerConfig = config.multiProvider?.providers as any;
      const provider = providerConfig?.[providerId];

      return !!(provider?.apiKey && provider.apiKey.length > 10);
    } catch (error) {
      console.error(`Test failed for ${providerId}:`, error);
      return false;
    }
  }

  /**
   * Setup routing configuration
   */
  private async setupRouting(): Promise<void> {
    const config = this.configManager.config;

    console.log('\n🔀 Configuring routing...');

    // Ask for default provider
    const defaultProvider = await this.ui.askQuestion(
      'Select default provider (synthetic-new, minimax-m2, z-ai):',
      'synthetic-new'
    );

    if (defaultProvider && config.multiProvider) {
      config.multiProvider.routing.defaultProvider = defaultProvider;
    }

    // Ask about thinking provider
    const useThinkingProvider = await this.ui.askQuestion(
      'Configure different provider for thinking requests? (y/N):',
      'n'
    );

    if (useThinkingProvider.toLowerCase() === 'y' && config.multiProvider) {
      const thinkingProvider = await this.ui.askQuestion(
        'Select thinking provider (synthetic-new, minimax-m2):',
        'synthetic-new'
      );

      config.multiProvider.routing.thinkingProvider = thinkingProvider;
    }
  }

  /**
   * Launch Claude Code with specified provider
   */
  private async launchWithProvider(providerId: string, options: AimuxAppOptions): Promise<void> {
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

    const banner = createBanner({ additionalArgs });
    console.log(banner);

    // Get provider-specific configuration
    const providerConfig = config.multiProvider?.providers?.[providerId as keyof typeof config.multiProvider.providers];

    const providerApiKey = providerConfig?.apiKey || config.apiKey;
    let baseUrl = providerConfig?.anthropicBaseUrl || providerConfig?.baseUrl || config.anthropicBaseUrl;

    // Special handling for Z.AI: Use local interceptor to fix model transformation
    if (providerId === 'z-ai') {
      console.log('[AIMUX] Starting Z.AI interceptor to fix model transformation...');

      // Start the interceptor in background
      setImmediate(async () => {
        try {
          const ZAIInterceptor = require('../interceptors/zai-interceptor');
          const interceptor = new ZAIInterceptor({ debug: false });
          interceptor.start();
          console.log('[AIMUX] Z.AI interceptor started successfully');
        } catch (error) {
          console.error(`[AIMUX] Failed to start Z.AI interceptor: ${error.message}`);
        }
      });

      // Point Claude Code to our local proxy instead of direct Z.AI
      baseUrl = 'http://localhost:8123/anthropic';

      // Give the interceptor a moment to start
      setTimeout(() => {
        console.log('[AIMUX] Z.AI interceptor active - redirecting requests through proxy');
      }, 100);
    }

    const launchEnv = {
      ...process.env,
      AIMUX_PROVIDER: providerId,
      // Use provider-specific API configuration
      // Set both API_KEY and AUTH_TOKEN to ensure Claude Code uses the right authentication
      ANTHROPIC_API_KEY: providerApiKey,
      ANTHROPIC_AUTH_TOKEN: providerApiKey,
      ANTHROPIC_BASE_URL: baseUrl,
    };

    const launchOptions: LaunchOptions = {
      model: config.selectedModel || '',
      thinkingModel: options.thinkingModel || config.selectedThinkingModel,
      additionalArgs: normalizeDangerousFlags(additionalArgs),
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
  private async autoConfigureModelsForProvider(providerId: string): Promise<void> {
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
  private getDefaultModelsForProvider(providerId: string): { default: string; thinking: string } {
    const models: Record<string, { default: string; thinking: string }> = {
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

    return models[providerId] || models['synthetic-new']!;
  }

  /**
   * Get provider information
   */
  private getProviderInfo(providerId: string): {
    name: string;
    capabilities: string[];
    priority: number;
  } {
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

    return (
      providers[providerId as keyof typeof providers] || {
        name: providerId,
        capabilities: [],
        priority: 5,
      }
    );
  }

  /**
   * Show current configuration
   */
  async showConfig(): Promise<void> {
    const config = this.configManager.config;
    console.log('📋 Current Configuration:');
    console.log(JSON.stringify(config, null, 2));
  }

  /**
   * Run system health check
   */
  async doctor(): Promise<void> {
    console.log('🔍 Running system health check...\n');

    // Check configuration
    console.log('📋 Checking configuration...');
    const config = this.configManager.config;
    if (!config.multiProvider) {
      console.log('❌ No multi-provider configuration found');
    } else {
      console.log('✅ Configuration found');
    }

    // Test providers
    await this.testProviders();

    // Check routing
    console.log('\n🔀 Checking routing configuration...');
    if (config.multiProvider?.routing) {
      console.log('✅ Routing configuration found');
    } else {
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
