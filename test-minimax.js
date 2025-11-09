#!/usr/bin/env node

import { MinimaxM2Provider } from './dist/providers/minimax-provider.js';
import { createProviderRegistry } from './dist/providers/index.js';

async function testMinimaxProvider() {
  console.log('🧪 Testing Minimax M2 Provider...');

  try {
    // Create provider instance
    const provider = new MinimaxM2Provider();

    // Test authentication
    console.log('1. Testing authentication...');
    const authResult = await provider.authenticate();
    console.log('✅ Auth Result:', {
      success: authResult.success,
      plan: authResult.plan,
      capabilities: authResult.verifiedCapabilities
    });

    // Test model fetching
    console.log('\n2. Testing model fetching...');
    const models = await provider.fetchModels();
    console.log(`✅ Found ${models.length} models:`);
    models.forEach(model => {
      console.log(`  - ${model.id} (maxTokens: ${model.maxTokens}, capabilities: ${model.capabilities.thinking ? 'thinking' : ''}${model.capabilities.vision ? ', vision' : ''}${model.capabilities.tools ? ', tools' : ''})`);
    });

    // Test basic request
    console.log('\n3. Testing basic request...');
    const testRequest = {
      model: models[0]?.id || 'abab6.5-chat',
      messages: [
        { role: 'user', content: 'Hello! Can you respond with just "API test successful"?' }
      ],
      temperature: 0.1,
      maxTokens: 100
    };

    const response = await provider.makeRequest(testRequest);
    console.log('✅ Request Response:', {
      provider: response.provider,
      model: response.model,
      content: response.choices[0]?.message?.content?.substring(0, 100) + '...',
      usage: response.usage,
      responseTime: response.metadata?.responseTime
    });

    // Test health check
    console.log('\n4. Testing health check...');
    const health = await provider.healthCheck();
    console.log('✅ Health Status:', {
      status: health.status,
      responseTime: health.responseTime,
      capabilities: health.capabilities
    });

    console.log('\n🎉 All Minimax M2 tests passed!');

  } catch (error) {
    console.error('❌ Test failed:', error.message);
    console.error(error);
    process.exit(1);
  }
}

testMinimaxProvider();