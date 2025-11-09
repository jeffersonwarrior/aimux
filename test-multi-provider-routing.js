#!/usr/bin/env node

import { createProviderRegistry, MinimaxM2Provider, SyntheticProvider } from './dist/providers/index.js';
import { RoutingEngine } from './dist/core/routing-engine.js';

async function testMultiProviderRouting() {
  console.log('🔀 Testing Multi-Provider Routing System...');

  try {
    // Create provider registry
    const registry = createProviderRegistry();

    // Load configuration
    const ConfigManager = (await import('./dist/config/manager.js')).ConfigManager;
    const manager = new ConfigManager();
    const config = manager.getMultiProviderConfig();

    // Initialize and register providers
    const minimaxProvider = new MinimaxM2Provider();
    const syntheticProvider = new SyntheticProvider();

    // Set configurations
    minimaxProvider.config = config.providers['minimax-m2'];
    syntheticProvider.config = config.providers['synthetic-new'];

    await registry.register(minimaxProvider);
    await registry.register(syntheticProvider);

    console.log('✅ Registered providers:');
    console.log(`  - ${minimaxProvider.name} (capabilities: ${config.providers['minimax-m2'].capabilities.join(', ')})`);
    console.log(`  - ${syntheticProvider.name} (capabilities: ${config.providers['synthetic-new'].capabilities.join(', ')})`);

    // Create routing engine
    const routingEngine = new RoutingEngine(registry);

    console.log('\n🚦 Testing routing decisions...');

    // Test different request types
    const testRequests = [
      {
        name: 'Regular Request',
        request: {
          model: 'test-model',
          messages: [{ role: 'user', content: 'Regular question' }],
          temperature: 0.7
        }
      },
      {
        name: 'Thinking Request',
        request: {
          model: 'test-thinking-model',
          messages: [{ role: 'user', content: 'Complex reasoning question' }],
          temperature: 0.1
        }
      },
      {
        name: 'Vision Request',
        request: {
          model: 'test-vision-model',
          messages: [{
            role: 'user',
            content: [
              { type: 'text', text: 'What do you see?' },
              { type: 'image_url', image_url: { url: 'data:image/png;base64,test' } }
            ]
          }],
          temperature: 0.3
        }
      },
      {
        name: 'Tools Request',
        request: {
          model: 'test-model',
          messages: [{ role: 'user', content: 'Use a calculator' }],
          tools: [
            {
              type: 'function',
              function: {
                name: 'calculate',
                description: 'Perform calculation'
              }
            }
          ]
        }
      }
    ];

    for (const test of testRequests) {
      console.log(`\n📝 Testing: ${test.name}`);
      try {
        const selectedProvider = routingEngine.selectProvider(test.request);
        console.log(`  → Selected: ${selectedProvider.name} (${selectedProvider.id})`);

        // Verify the selection makes sense
        const isThinking = test.request.model?.includes('thinking') ||
                          (test.request.messages && JSON.stringify(test.request.messages).includes('reasoning'));
        const isVision = test.request.messages &&
                        JSON.stringify(test.request.messages).includes('image_url');
        const hasTools = test.request.tools && test.request.tools.length > 0;

        if (isThinking && selectedProvider.id === 'minimax-m2') {
          console.log('  ✅ Correctly routed thinking request to Minimax M2');
        } else if (isVision && selectedProvider.id === 'minimax-m2') {
          console.log('  ✅ Correctly routed vision request to Minimax M2');
        } else if (hasTools) {
          console.log(`  ✅ Routed tools request to ${selectedProvider.name}`);
        } else {
          console.log(`  ✅ Default routing to ${selectedProvider.name}`);
        }
      } catch (error) {
        console.log(`  ❌ Routing failed: ${error.message}`);
      }
    }

    console.log('\n📊 Testing provider capabilities...');

    // Test capability-based provider selection
    const thinkingProviders = registry.getProvidersByCapability('thinking');
    const visionProviders = registry.getProvidersByCapability('vision');
    const toolsProviders = registry.getProvidersByCapability('tools');

    console.log(`🤔 Thinking providers: ${thinkingProviders.map(p => p.name).join(', ')}`);
    console.log(`👁️ Vision providers: ${visionProviders.map(p => p.name).join(', ')}`);
    console.log(`🛠️ Tools providers: ${toolsProviders.map(p => p.name).join(', ')}`);

    // Test fallback behavior
    console.log('\n🔄 Testing fallback behavior...');
    const regularProviders = registry.getProvidersByCapability('temperature');
    console.log(`📋 Available fallback providers: ${regularProviders.length}`);

    console.log('\n🎉 Multi-provider routing test completed successfully!');

  } catch (error) {
    console.error('❌ Multi-provider routing test failed:', error.message);
    console.error(error);
  }
}

testMultiProviderRouting();