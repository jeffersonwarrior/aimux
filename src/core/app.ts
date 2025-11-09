import { join } from 'path';
import { homedir } from 'os';
import { ConfigManager, AppConfigSchema } from '../config';
import { ModelManager } from '../models';
import { UserInterface } from '../ui';
import { ClaudeLauncher, LaunchOptions } from '../launcher';
import { setupLogging, log } from '../utils/logger';
import { createBanner, normalizeDangerousFlags } from '../utils/banner';

export interface AppOptions {
  verbose?: boolean;
  quiet?: boolean;
  additionalArgs?: string[];
  thinkingModel?: string;
}

export class SyntheticClaudeApp {
  private configManager: ConfigManager;
  private ui: UserInterface;
  private launcher: ClaudeLauncher;
  private modelManager: ModelManager | null = null;

  constructor() {
    this.configManager = new ConfigManager();
    this.ui = new UserInterface({
      verbose: this.configManager.config.apiKey
        ? this.configManager.config.cacheDurationHours > 0
        : false,
    });
    this.launcher = new ClaudeLauncher();
  }

  async setupLogging(options: AppOptions): Promise<void> {
    setupLogging(options.verbose, options.quiet);
    // Removed verbose startup log
  }

  getConfig() {
    return this.configManager.config;
  }

  private getModelManager(): ModelManager {
    if (!this.modelManager) {
      const config = this.configManager.config;
      const cacheFile = join(homedir(), '.config', 'synclaude', 'models_cache.json');

      this.modelManager = new ModelManager({
        apiKey: config.apiKey,
        modelsApiUrl: config.modelsApiUrl,
        cacheFile,
        cacheDurationHours: config.cacheDurationHours,
      });
    }

    return this.modelManager;
  }

  async run(options: AppOptions & LaunchOptions): Promise<void> {
    // Normalize dangerous flags first
    if (options.additionalArgs) {
      options.additionalArgs = normalizeDangerousFlags(options.additionalArgs);
    }

    await this.setupLogging(options);

    // Display banner unless quiet mode
    if (!options.quiet) {
      console.log(createBanner(options));
    }

    // Note: Updates are now handled manually by users via `npm update -g synclaude`
    // This eliminates complex update checking and related bugs

    // Handle first-time setup
    if (this.configManager.isFirstRun()) {
      await this.setup();
      return;
    }

    // Get model to use
    const model = await this.selectModel(options.model);
    if (!model) {
      this.ui.error('No model selected');
      return;
    }

    // Get thinking model to use (if specified)
    const thinkingModel = await this.selectThinkingModel(options.thinkingModel);

    // Launch Claude Code
    await this.launchClaudeCode(model, options, thinkingModel);
  }

  async interactiveModelSelection(): Promise<boolean> {
    if (!this.configManager.hasApiKey()) {
      this.ui.error('No API key configured. Please run "synclaude setup" first.');
      return false;
    }

    try {
      const modelManager = this.getModelManager();
      this.ui.coloredInfo('Fetching available models...');
      const models = await modelManager.fetchModels();

      if (models.length === 0) {
        this.ui.error('No models available. Please check your API key and connection.');
        return false;
      }

      // Sort models for consistent display
      const sortedModels = modelManager.getModels(models);
      const { regular: selectedRegularModel, thinking: selectedThinkingModel } =
        await this.ui.selectDualModels(sortedModels);

      if (!selectedRegularModel && !selectedThinkingModel) {
        this.ui.info('Model selection cancelled');
        return false;
      }

      // Save models to config
      if (selectedRegularModel) {
        await this.configManager.setSavedModel(selectedRegularModel.id);
        this.ui.coloredSuccess(`Regular model saved: ${selectedRegularModel.getDisplayName()}`);
      }

      if (selectedThinkingModel) {
        await this.configManager.setSavedThinkingModel(selectedThinkingModel.id);
        this.ui.coloredSuccess(`Thinking model saved: ${selectedThinkingModel.getDisplayName()}`);
      }

      this.ui.highlightInfo(
        'Now run "synclaude" to start Claude Code with your selected model(s).',
        ['synclaude']
      );
      return true;
    } catch (error) {
      this.ui.error(`Error during model selection: ${error}`);
      return false;
    }
  }

  async interactiveThinkingModelSelection(): Promise<boolean> {
    if (!this.configManager.hasApiKey()) {
      this.ui.error('No API key configured. Please run "synclaude setup" first.');
      return false;
    }

    try {
      const modelManager = this.getModelManager();
      this.ui.coloredInfo('Fetching available models...');
      const models = await modelManager.fetchModels();

      if (models.length === 0) {
        this.ui.error('No models available. Please check your API key and connection.');
        return false;
      }

      // Sort models for consistent display
      const sortedModels = modelManager.getModels(models);
      const selectedThinkingModel = await this.ui.selectModel(sortedModels);
      if (!selectedThinkingModel) {
        this.ui.info('Thinking model selection cancelled');
        return false;
      }

      await this.configManager.updateConfig({ selectedThinkingModel: selectedThinkingModel.id });
      this.ui.coloredSuccess(`Thinking model saved: ${selectedThinkingModel.getDisplayName()}`);
      this.ui.highlightInfo(
        'Now run "synclaude --thinking-model" to start Claude Code with this thinking model.',
        ['synclaude', '--thinking-model']
      );
      return true;
    } catch (error) {
      this.ui.error(`Error during thinking model selection: ${error}`);
      return false;
    }
  }

  async listModels(options: { refresh?: boolean }): Promise<void> {
    log.info('Listing models', { options });

    if (!this.configManager.hasApiKey()) {
      this.ui.error('No API key configured. Please run "synclaude setup" first.');
      return;
    }

    try {
      const modelManager = this.getModelManager();
      this.ui.coloredInfo('Fetching available models...');
      const models = await modelManager.fetchModels(options.refresh);

      // Sort and display all models
      const sortedModels = modelManager.getModels(models);
      this.ui.showModelList(sortedModels);
    } catch (error) {
      this.ui.error(`Error fetching models: ${error}`);
    }
  }

  async searchModels(query: string, options: { refresh?: boolean }): Promise<void> {
    log.info('Searching models', { query, options });

    if (!this.configManager.hasApiKey()) {
      this.ui.error('No API key configured. Please run "synclaude setup" first.');
      return;
    }

    try {
      const modelManager = this.getModelManager();
      this.ui.coloredInfo(`Searching for models matching "${query}"...`);
      const models = await modelManager.searchModels(query, undefined);

      if (models.length === 0) {
        this.ui.info(`No models found matching "${query}"`);
        return;
      }

      this.ui.coloredInfo(
        `Found ${models.length} model${models.length === 1 ? '' : 's'} matching "${query}":`
      );
      this.ui.showModelList(models);
    } catch (error) {
      this.ui.error(`Error searching models: ${error}`);
    }
  }

  async showConfig(): Promise<void> {
    const config = this.configManager.config;

    this.ui.info('Current Configuration:');
    this.ui.info('=====================');
    this.ui.info(`API Key: ${config.apiKey ? '••••••••' + config.apiKey.slice(-4) : 'Not set'}`);
    this.ui.info(`Base URL: ${config.baseUrl}`);
    this.ui.info(`Models API: ${config.modelsApiUrl}`);
    this.ui.info(`Cache Duration: ${config.cacheDurationHours} hours`);
    this.ui.info(`Selected Model: ${config.selectedModel || 'None'}`);
    this.ui.info(`Selected Thinking Model: ${config.selectedThinkingModel || 'None'}`);
    this.ui.info(`First Run Completed: ${config.firstRunCompleted}`);
  }

  async setConfig(key: string, value: string): Promise<void> {
    // Simple key-value config setting
    const updates: Record<string, any> = {};

    switch (key) {
      case 'apiKey':
        updates.apiKey = value;
        break;
      case 'baseUrl':
        updates.baseUrl = value;
        break;
      case 'modelsApiUrl':
        updates.modelsApiUrl = value;
        break;
      case 'cacheDurationHours':
        updates.cacheDurationHours = parseInt(value, 10);
        break;
      case 'selectedModel':
        updates.selectedModel = value;
        break;
      case 'selectedThinkingModel':
        updates.selectedThinkingModel = value;
        break;
      default:
        this.ui.error(`Unknown configuration key: ${key}`);
        return;
    }

    const success = await this.configManager.updateConfig(updates);
    if (success) {
      this.ui.success(`Configuration updated: ${key} = ${value}`);
    } else {
      this.ui.error(`Failed to update configuration: ${key}`);
    }
  }

  async resetConfig(): Promise<void> {
    const confirmed = await this.ui.confirm(
      'Are you sure you want to reset all configuration to defaults?'
    );
    if (!confirmed) {
      this.ui.info('Configuration reset cancelled');
      return;
    }

    // Clear config
    await this.configManager.saveConfig(AppConfigSchema.parse({}));
    this.ui.success('Configuration reset to defaults');
  }

  async setup(): Promise<void> {
    this.ui.coloredInfo("Welcome to Synclaude! Let's set up your configuration.");
    this.ui.info('==============================================');

    const config = this.configManager.config;

    // Get API key if not set
    let apiKey = config.apiKey;
    if (!apiKey) {
      apiKey = await this.ui.askPassword('Enter your Synthetic API key');
      if (!apiKey) {
        this.ui.error('API key is required');
        return;
      }
    }

    // Update config with API key
    const success = await this.configManager.setApiKey(apiKey);
    if (!success) {
      this.ui.error('Failed to save API key');
      return;
    }

    this.ui.coloredSuccess('API key saved');

    // Optional: Test API connection
    const testConnection = await this.ui.confirm('Test API connection?', true);
    if (testConnection) {
      try {
        const modelManager = this.getModelManager();
        this.ui.coloredInfo('Testing connection...');
        const models = await modelManager.fetchModels(true);
        this.ui.coloredSuccess(`Connection successful! Found ${models.length} models`);
      } catch (error) {
        this.ui.error(`Connection failed: ${error}`);
        return;
      }
    }

    // Interactive model selection
    const selectModel = await this.ui.confirm('Select a model now?', true);
    if (selectModel) {
      await this.interactiveModelSelection();
    }

    // Mark first run as completed
    await this.configManager.markFirstRunCompleted();

    this.ui.coloredSuccess('Setup completed successfully!');
    this.ui.highlightInfo('You can now run "synclaude" to launch Claude Code', ['synclaude']);
  }

  async doctor(): Promise<void> {
    this.ui.info('System Health Check');
    this.ui.info('===================');

    // Check Claude Code installation
    const claudeInstalled = await this.launcher.checkClaudeInstallation();
    this.ui.showStatus(
      claudeInstalled ? 'success' : 'error',
      `Claude Code: ${claudeInstalled ? 'Installed' : 'Not found'}`
    );

    if (claudeInstalled) {
      const version = await this.launcher.getClaudeVersion();
      if (version) {
        this.ui.info(`Claude Code version: ${version}`);
      }
    }

    // Check configuration
    this.ui.showStatus(
      this.configManager.hasApiKey() ? 'success' : 'error',
      'Configuration: API key ' + (this.configManager.hasApiKey() ? 'configured' : 'missing')
    );

    // Check API connection
    if (this.configManager.hasApiKey()) {
      try {
        const modelManager = this.getModelManager();
        const models = await modelManager.fetchModels(true);
        this.ui.showStatus('success', `API connection: OK (${models.length} models)`);
      } catch (error) {
        this.ui.showStatus('error', `API connection: Failed (${error})`);
      }
    }

    // Note: Manual updates via `npm update -g synclaude`
    this.ui.info('To check for updates, run: npm update -g synclaude');
  }

  async clearCache(): Promise<void> {
    const modelManager = this.getModelManager();
    const success = await modelManager.clearCache();

    if (success) {
      this.ui.success('Model cache cleared');
    } else {
      this.ui.error('Failed to clear cache');
    }
  }

  async cacheInfo(): Promise<void> {
    const modelManager = this.getModelManager();
    const cacheInfo = await modelManager.getCacheInfo();

    this.ui.info('Cache Information:');
    this.ui.info('==================');

    if (cacheInfo.exists) {
      this.ui.info(`Status: ${cacheInfo.isValid ? 'Valid' : 'Expired'}`);
      this.ui.info(`File: ${cacheInfo.filePath}`);
      this.ui.info(`Size: ${cacheInfo.sizeBytes} bytes`);
      this.ui.info(`Models: ${cacheInfo.modelCount}`);
      this.ui.info(`Modified: ${cacheInfo.modifiedTime}`);
    } else {
      this.ui.info('Status: No cache file');
    }
  }

  private async selectModel(preselectedModel?: string): Promise<string | null> {
    if (preselectedModel) {
      return preselectedModel;
    }

    // Use saved model if available, otherwise show error
    if (this.configManager.hasSavedModel()) {
      return this.configManager.getSavedModel();
    }

    this.ui.error('No model selected. Run "synclaude model" to select a model.');
    return null;
  }

  private async selectThinkingModel(preselectedThinkingModel?: string): Promise<string | null> {
    if (preselectedThinkingModel) {
      return preselectedThinkingModel;
    }

    // Use saved thinking model if available
    if (this.configManager.hasSavedThinkingModel()) {
      return this.configManager.getSavedThinkingModel();
    }

    return null; // Thinking model is optional
  }

  private async launchClaudeCode(
    model: string,
    options: LaunchOptions,
    thinkingModel?: string | null
  ): Promise<void> {
    const launchInfo = thinkingModel
      ? `Launching with ${model} (thinking: ${thinkingModel}). Use "synclaude model" to change model.`
      : `Launching with ${model}. Use "synclaude model" to change model.`;

    this.ui.highlightInfo(launchInfo, [model, 'synclaude model']);

    const result = await this.launcher.launchClaudeCode({
      model,
      thinkingModel,
      additionalArgs: options.additionalArgs,
      env: {
        ANTHROPIC_AUTH_TOKEN: this.configManager.getApiKey(),
      },
    });

    if (!result.success) {
      this.ui.error(`Failed to launch Claude Code: ${result.error}`);
    }
  }
}
