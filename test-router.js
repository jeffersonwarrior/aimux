#!/usr/bin/env node

import { createRouter, createRouterManager } from './dist/router/index.js';
import { createProviderRegistry, MinimaxM2Provider } from './dist/providers/index.js';

async function testRouter() {
  console.log('🌐 Testing HTTP Proxy Router...');

  try {
    // Create provider registry and add Minimax M2
    const registry = createProviderRegistry();
    const provider = new MinimaxM2Provider();

    // Load configuration for the provider
    const ConfigManager = (await import('./dist/config/manager.js')).ConfigManager;
    const manager = new ConfigManager();
    const config = manager.getMultiProviderConfig();

    const providerConfig = config.providers['minimax-m2'];
    provider.config = providerConfig;

    await registry.register(provider);
    console.log('✅ Provider registered:', provider.name);

    // Create routing engine
    const { RoutingEngine } = await import('./dist/core/routing-engine.js');
    const routingEngine = new RoutingEngine(registry);

    // Create router
    const router = createRouter(registry, routingEngine, {
      port: 8888,
      enableLogging: true,
      timeout: 30000
    });

    console.log('🚀 Starting router...');

    // Start test API call
    const testRequest = async () => {
      try {
        console.log('📡 Making test request to router...');

        const response = await fetch('http://localhost:8888/v1/chat/completions', {
          method: 'POST',
          headers: {
            'Content-Type': 'application/json',
          },
          body: JSON.stringify({
            model: 'abab6.5-chat',
            messages: [
              { role: 'user', content: 'Hello! Please respond with just "Router test successful"' }
            ],
            temperature: 0.1,
            max_tokens: 100
          })
        });

        if (response.ok) {
          const result = await response.json();
          console.log('✅ Router request successful!');
          console.log('Response:', {
            provider: result.provider,
            model: result.model,
            content: result.choices?.[0]?.message?.content?.substring(0, 100) + '...',
            responseTime: result.metadata?.responseTime
          });
        } else {
          console.error('❌ Router request failed:', response.status, response.statusText);
        }
      } catch (error) {
        console.error('❌ Router test error:', error.message);
      }
    };

    // Start router and test
    router.start().then(async () => {
      console.log('✅ Router started on port 8080');

      // Wait a moment for router to fully start
      setTimeout(testRequest, 2000);

      // Test for 10 seconds
      setTimeout(() => {
        router.stop().then(() => {
          console.log('🔌 Router stopped');
          process.exit(0);
        });
      }, 10000);

    }).catch(error => {
      console.error('❌ Failed to start router:', error);
      process.exit(1);
    });

  } catch (error) {
    console.error('❌ Router test failed:', error.message);
    console.error(error);
    process.exit(1);
  }
}

testRouter();