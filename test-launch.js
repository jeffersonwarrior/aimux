#!/usr/bin/env node

// Simple test to check how aimux sets environment variables for Z.AI
const path = require('path');
const fs = require('fs');

console.log('=== Aimux Environment Variable Test ===');

// Load the aimux configuration
const configPath = path.join(process.env.HOME || '/root', '.config/aimux/config.json');
console.log('Config path:', configPath);

if (fs.existsSync(configPath)) {
  const config = JSON.parse(fs.readFileSync(configPath, 'utf8'));
  console.log('\n=== Configuration Analysis ===');

  const providerConfig = config.multiProvider?.providers?.['z-ai'];
  if (providerConfig) {
    console.log('✅ Z.AI Provider Config Found:');
    console.log('  - API Key:', providerConfig.apiKey ? `${providerConfig.apiKey.substring(0, 10)}...` : 'MISSING');
    console.log('  - Base URL:', providerConfig.baseUrl || 'MISSING');

    // Simulate the environment variable logic from launchWithProvider
    const anthropicApiKey = providerConfig?.apiKey || config.apiKey;
    const anthropicBaseUrl = providerConfig?.anthropicBaseUrl || providerConfig?.baseUrl || config.anthropicBaseUrl;

    console.log('\n=== Environment Variables That Would Be Set ===');
    console.log('ANTHROPIC_API_KEY:', anthropicApiKey ? `${anthropicApiKey.substring(0, 10)}...` : 'EMPTY/MISSING');
    console.log('ANTHROPIC_BASE_URL:', anthropicBaseUrl || 'MISSING');

    // Test if these work with a curl call
    const { spawn } = require('child_process');
    console.log('\n=== Testing Environment Variables with curl ===');

    const testRequest = {
      model: "glm-4.6",
      max_tokens: 5,
      messages: [{ role: "user", content: "test" }]
    };

    const curl = spawn('curl', [
      '-s', '-w', '\\nHTTP_CODE:%{http_code}',
      '-X', `${anthropicBaseUrl}/messages`,
      '-H', 'Content-Type: application/json',
      '-H', `Authorization: Bearer ${anthropicApiKey}`,
      '-d', JSON.stringify(testRequest)
    ], {
      env: {
        ...process.env,
        ANTHROPIC_API_KEY: anthropicApiKey,
        ANTHROPIC_BASE_URL: anthropicBaseUrl
      }
    });

    let output = '';
    curl.stdout.on('data', (data) => {
      output += data.toString();
    });

    curl.on('close', (code) => {
      console.log('Curl output:', output);
      const httpCode = output.match(/HTTP_CODE:(\d+)/);
      if (httpCode && httpCode[1] === '200') {
        console.log('✅ SUCCESS: Environment variables work correctly!');
      } else {
        console.log('❌ FAILED: HTTP', httpCode ? httpCode[1] : 'UNKNOWN');
      }
    });

  } else {
    console.log('❌ Z.AI Provider Config Missing');
  }
} else {
  console.log('❌ Configuration file not found');
}