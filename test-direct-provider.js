#!/usr/bin/env node

import { MinimaxM2Provider } from './dist/providers/minimax-provider.js';

async function testDirectProvider() {
  console.log('🧪 Testing direct provider API call...');

  try {
    const provider = new MinimaxM2Provider();

    // Load config from file
    const ConfigManager = (await import('./dist/config/manager.js')).ConfigManager;
    const manager = new ConfigManager();
    const config = manager.getMultiProviderConfig();

    // Set provider configuration
    const providerConfig = config.providers['minimax-m2'];
    provider.config = providerConfig;

    console.log('🔧 Provider config loaded:', {
      name: providerConfig.name,
      hasApiKey: !!providerConfig.apiKey,
      baseUrl: providerConfig.baseUrl,
      capabilities: providerConfig.capabilities
    });

    // Test a direct API call
    console.log('\n📡 Making direct API call...');
    const response = await provider.makeRequest({
      model: 'abab6.5-chat',
      messages: [
        { role: 'user', content: 'Hello! Please respond with just "Direct API test successful"' }
      ],
      temperature: 0.1,
      max_tokens: 100
    });

    console.log('✅ Direct API call successful!');
    console.log('Response:', {
      provider: response.provider,
      model: response.model,
      content: response.choices?.[0]?.message?.content,
      usage: response.usage,
      responseTime: response.metadata?.responseTime
    });

  } catch (error) {
    console.error('❌ Direct provider test failed:', error.message);
    console.error(error);
  }
}

testDirectProvider();