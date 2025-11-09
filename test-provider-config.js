#!/usr/bin/env node

import { readFile } from 'fs/promises';
import { loadConfig } from './dist/config/manager.js';

async function testProviderConfig() {
  console.log('🔧 Testing provider configuration loading...');

  try {
    // Load the configuration
    const configManager = loadConfig();
    const config = configManager.getConfig();

    console.log('✅ Configuration loaded:', {
      providers: config.providers ? Object.keys(config.providers) : 'none',
      routing: config.routing
    });

    // Test multi-provider config
    const multiConfig = configManager.getMultiProviderConfig();
    console.log('✅ Multi-provider config:', {
      providers: Object.keys(multiConfig.providers || {}),
      defaultProvider: multiConfig.routing?.defaultProvider,
      routing: multiConfig.routing
    });

    // Get specific provider config
    const minimaxConfig = configManager.getProviderConfig('minimax-m2');
    console.log('✅ Minimax M2 config:', {
      name: minimaxConfig?.name,
      hasApiKey: !!minimaxConfig?.apiKey,
      apiKeyStart: minimaxConfig?.apiKey?.substring(0, 20) + '...',
      enabled: minimaxConfig?.enabled,
      priority: minimaxConfig?.priority
    });

    // Test if configuration file exists and is readable
    const configPath = '/home/agent-syn/.config/aimux/config.json';
    const rawConfig = await readFile(configPath, 'utf8');
    const parsedConfig = JSON.parse(rawConfig);

    console.log('✅ Raw configuration file:', {
      exists: true,
      size: rawConfig.length,
      hasMinimax: !!parsedConfig.providers?.['minimax-m2'],
      minimaxHasKey: !!parsedConfig.providers?.['minimax-m2']?.apiKey,
      minimaxKeyLength: parsedConfig.providers?.['minimax-m2']?.apiKey?.length
    });

  } catch (error) {
    console.error('❌ Configuration test failed:', error.message);
    console.error(error);
    process.exit(1);
  }
}

testProviderConfig();