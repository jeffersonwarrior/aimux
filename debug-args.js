#!/usr/bin/env node

// Debug script to test argument parsing
const rawArgs = process.argv.slice(2);
console.log('Raw args:', rawArgs);

const provider = 'minimax-m2';
const providerIndex = rawArgs.indexOf(provider);
console.log('Provider index:', providerIndex);

const additionalArgs = [];
if (providerIndex !== -1) {
  const additionalStart = providerIndex + 1;
  for (let i = additionalStart; i < rawArgs.length; i++) {
    if (rawArgs[i] && !rawArgs[i].startsWith('-v') && !rawArgs[i].startsWith('--verbose') &&
        !rawArgs[i].startsWith('-q') && !rawArgs[i].startsWith('--quiet')) {
      additionalArgs.push(rawArgs[i]);
    }
  }
}

console.log('Additional args before normalize:', additionalArgs);

// Test the normalize function
const dangerousPatterns = [
  /^--dangerously-skip-permissions$/,
  /^dangerous$/
];

const normalizedArgs = additionalArgs.map(arg => {
  if (typeof arg === 'string') {
    const lowerArg = arg.toLowerCase().replace(/[_\s]+/g, '-');
    if (dangerousPatterns.some(pattern => pattern.test(lowerArg))) {
      return '--dangerously-skip-permissions';
    }
  }
  return arg;
});

console.log('Additional args after normalize:', normalizedArgs);
console.log('Dangerous flag detected:', normalizedArgs.includes('--dangerously-skip-permissions'));