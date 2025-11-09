#!/usr/bin/env node

import { readFile } from 'fs/promises';

async function debugConfig() {
  console.log('🔍 Debugging aimux configuration...');

  try {
    // Read config file directly
    const configPath = '/home/agent-syn/.config/aimux/config.json';
    console.log('1. Reading config from:', configPath);

    const rawData = await readFile(configPath, 'utf8');
    const config = JSON.parse(rawData);

    console.log('2. Raw config structure:', {
      hasMultiProvider: !!config.multiProvider,
      hasProviders: !!config.providers,
      multiProviderKeys: config.multiProvider ? Object.keys(config.multiProvider) : [],
      providerKeys: config.providers ? Object.keys(config.providers) : [],
      multiProviderProviderKeys: config.multiProvider?.providers ? Object.keys(config.multiProvider.providers) : []
    });

    // Test using ConfigManager
    console.log('\n3. Testing ConfigManager...');
    const ConfigManager = (await import('./dist/config/manager.js')).ConfigManager;
    const manager = new ConfigManager();

    const baseConfig = manager.config;
    console.log('Base config:', {
      hasMultiProvider: !!baseConfig.multiProvider,
      multiProviderKeys: baseConfig.multiProvider ? Object.keys(baseConfig.multiProvider) : []
    });

    const multiConfig = manager.getMultiProviderConfig();
    console.log('Multi-provider config:', {
      hasProviders: !!multiConfig.providers,
      providerCount: multiConfig.providers ? Object.keys(multiConfig.providers).length : 0,
      providerKeys: multiConfig.providers ? Object.keys(multiConfig.providers) : []
    });

  } catch (error) {
    console.error('❌ Debug failed:', error.message);
    console.error(error);
  }
}

debugConfig();