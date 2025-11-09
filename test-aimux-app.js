#!/usr/bin/env node

import { AimuxApp } from './dist/core/aimux-app.js';

async function testAimuxApp() {
  console.log('🧪 Testing AimuxApp...');

  try {
    const app = new AimuxApp();

    // Test loading existing configuration
    console.log('1. Listing providers...');
    const providers = await app.listProviders();
    console.log(`✅ Found ${providers.length} providers:`);

    if (providers.length > 0) {
      // Test first provider
      const firstProviderId = providers[0];
      console.log(`\n2. Testing provider: ${firstProviderId}`);

      try {
        const testResult = await app.testProvider(firstProviderId);
        console.log('✅ Provider test result:', testResult);
      } catch (error) {
        console.log('❌ Provider test failed:', error.message);
      }
    } else {
      console.log('❌ No providers found in configuration');
    }

    console.log('\n🎉 AimuxApp test completed!');

  } catch (error) {
    console.error('❌ Test failed:', error.message);
    console.error(error);
  }
}

testAimuxApp();