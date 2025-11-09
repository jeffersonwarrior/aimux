#!/usr/bin/env node

import { AimuxApp } from './dist/core/aimux-app.js';

async function testEnvironmentVariable() {
  console.log('🔧 Testing environment variable setting...');

  const app = new AimuxApp();

  console.log('Before launching:');
  console.log('  process.env.AIMUX_PROVIDER:', process.env.AIMUX_PROVIDER);

  try {
    // Simulate the launchWithProvider call that sets the environment variable
    process.env.AIMUX_PROVIDER = 'minimax-m2';
    console.log('After setting AIMUX_PROVIDER:');
    console.log('  process.env.AIMUX_PROVIDER:', process.env.AIMUX_PROVIDER);

    // Test the banner creation with the environment set
    const { createBanner } = await import('./dist/utils/banner.js');
    const banner = createBanner({ providerId: 'minimax-m2' });
    console.log('\nBanner with providerId option:');
    console.log(banner);

    // Also test with just the environment variable
    const banner2 = createBanner();
    console.log('\nBanner with environment variable only:');
    console.log(banner2);

  } catch (error) {
    console.error('Error:', error.message);
  }
}

testEnvironmentVariable();