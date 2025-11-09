#!/usr/bin/env node

import { default as configManager } from './dist/config/manager.js';
import { readFile } from 'fs/promises';

async function testSimple() {
  console.log('🔧 Testing simple configuration...');

  try {
    const manager = new configManager();

    // Check if we can read the config file
    const configPath = '/home/agent-syn/.config/aimux/config.json';
    const rawConfig = await readFile(configPath, 'utf8');
    const parsedConfig = JSON.parse(rawConfig);

    console.log('✅ Raw config loaded:');
    console.log('  - minimax-m2 apiKey exists:', !!parsedConfig.providers?.['minimax-m2']?.apiKey);
    console.log('  - apiKey length:', parsedConfig.providers?.['minimax-m2']?.apiKey?.length);

    // Now test with the provider directly
    console.log('\n🧪 Testing Minimax provider with loaded config...');
    const { MinimaxM2Provider } = await import('./dist/providers/minimax-provider.js');

    const provider = new MinimaxM2Provider();

    // Manually set the config since we need to test this
    provider.config = parsedConfig.providers['minimax-m2'];

    console.log('✅ Config set to provider:');
    console.log('  - name:', provider.config.name);
    console.log('  - hasApiKey:', !!provider.config.apiKey);

    // Test authentication
    const authResult = await provider.authenticate();
    console.log('✅ Auth result:', authResult);

  } catch (error) {
    console.error('❌ Test failed:', error.message);
    console.error(error);
  }
}

testSimple();