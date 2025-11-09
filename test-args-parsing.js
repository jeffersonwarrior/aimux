#!/usr/bin/env node

import { normalizeDangerousFlags, createBanner } from './dist/utils/banner.js';

async function testArgsParsing() {
  console.log('🔧 Testing argument parsing...');

  // Test different dangerous flag variations
  const testFlags = [
    '--dangerously-skip-permissions',
    '--dangerously-skip-permission',
    '--dangerous-skip-permissions',
    '--dangerous',
    '--skip-permissions'
  ];

  console.log('\n1. Testing normalizeDangerous:');
  testFlags.forEach(flag => {
    const normalized = normalizeDangerousFlags([flag]);
    console.log(`  ${flag} -> ${normalized[0]}`);
  });

  console.log('\n2. Testing banner with dangerous flag:');
  const bannerWithDangerous = createBanner({
    additionalArgs: ['--dangerously-skip-permissions']
  });
  console.log(bannerWithDangerous);

  console.log('\n3. Testing command line args simulation:');
  // Simulate the raw args processing from CLI
  const rawArgs = ['minimax-m2', 'dangerous'];
  console.log(`  Raw args: ${JSON.stringify(rawArgs)}`);

  // Find the dangerous flag
  const dangerousIndex = rawArgs.indexOf('dangerous');
  const additionalArgs = dangerousIndex !== -1 ? rawArgs.slice(dangerousIndex) : [];
  console.log(`  Additional args: ${JSON.stringify(additionalArgs)}`);

  const normalizedArgs = normalizeDangerousFlags(additionalArgs);
  console.log(`  Normalized args: ${JSON.stringify(normalizedArgs)}`);

  const_banner = createBanner({ additionalArgs: normalizedArgs });
  console.log(_banner);
}

testArgsParsing();