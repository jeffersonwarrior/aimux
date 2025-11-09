#!/usr/bin/env node

import { SyntheticProvider } from './dist/providers/synthetic-provider.js';

async function testSyntheticProvider() {
  console.log('🧪 Testing Synthetic.New Provider Directly...');

  try {
    const provider = new SyntheticProvider();

    // Load config from file
    const ConfigManager = (await import('./dist/config/manager.js')).ConfigManager;
    const manager = new ConfigManager();
    const config = manager.getMultiProviderConfig();

    // Set provider configuration
    const providerConfig = config.providers['synthetic-new'];
    provider.config = providerConfig;

    console.log('🔧 Provider config loaded:', {
      name: providerConfig.name,
      hasApiKey: !!providerConfig.apiKey,
      baseUrl: providerConfig.baseUrl,
      hasAnthropicUrl: !!providerConfig.anthropicBaseUrl,
      capabilities: providerConfig.capabilities
    });

    // Test authentication
    console.log('\n🔐 Testing authentication...');
    const authResult = await provider.authenticate();
    console.log('✅ Auth result:', {
      success: authResult.success,
      plan: authResult.plan,
      capabilities: authResult.verifiedCapabilities
    });

    // Test model fetching
    console.log('\n📦 Testing model fetching...');
    try {
      const models = await provider.fetchModels();
      console.log(`✅ Found ${models.length} models:`);
      models.slice(0, 5).forEach(model => {
        console.log(`  - ${model.id} (${model.provider})`);
      });
    } catch (error) {
      console.log('⚠️ Model fetch failed (expected):', error.message);
    }

    // Test direct API call
    console.log('\n📡 Testing direct API call...');
    const response = await provider.makeRequest({
      model: 'claude-3-haiku-20240307',
      messages: [
        { role: 'user', content: 'Hello! Please respond with just "Synthetic.New test successful"' }
      ],
      temperature: 0.1,
      max_tokens: 100
    });

    console.log('✅ Direct API call successful!');
    console.log('Response:', {
      provider: response.provider,
      model: response.model,
      content: response.choices?.[0]?.message?.content?.substring(0, 100),
      usage: response.usage,
      responseTime: response.metadata?.responseTime
    });

    // Test health check
    console.log('\n🏥 Testing health check...');
    const health = await provider.healthCheck();
    console.log('✅ Health status:', {
      status: health.status,
      responseTime: health.responseTime,
      capabilities: health.capabilities
    });

    console.log('\n🎉 Synthetic.New provider test completed successfully!');

  } catch (error) {
    console.error('❌ Synthetic.New provider test failed:', error.message);
    console.error(error);
  }
}

testSyntheticProvider();