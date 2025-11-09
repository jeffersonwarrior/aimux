import { readFileSync } from 'fs';
import { join } from 'path';
import { ConfigManager } from '../config';
import chalk from 'chalk';

interface BannerOptions {
  verbose?: boolean;
  additionalArgs?: string[];
}

export function normalizeDangerousFlags(args: string[]): string[] {
  const dangerousPatterns = [
    /^--dangerously-skip-permissions$/, // correct
    /^--dangerously-skip-permission$/, // missing s
    /^--dangerous-skip-permissions$/, // missing ly
    /^--dangerous-skip-permission$/, // missing ly + s
    /^--dangerously skip-permissions$/, // space instead of dash
    /^--dangerously_skip_permissions$/, // underscores
    /^--dangerous(ly)?$/, // shortened
    /^--skip-permissions$/, // incomplete
    /^--skip-permission$/, // incomplete + missing s
    /^dangerous$/, // plain dangerous (no dashes)
  ];

  const processedArgs = args.map(arg => {
    if (typeof arg === 'string') {
      const lowerArg = arg.toLowerCase().replace(/[_\s]+/g, '-');
      if (dangerousPatterns.some(pattern => pattern.test(lowerArg))) {
        return '--dangerously-skip-permissions';
      }
    }
    return arg;
  });

  return processedArgs;
}

export function createBanner(options: BannerOptions = {}): string {
  // Read version from package.json
  const packageJsonPath = join(__dirname, '../../package.json');
  const version = JSON.parse(readFileSync(packageJsonPath, 'utf8')).version;

  // Get current config for models
  const configManager = new ConfigManager();
  const config = configManager.config;

  // Determine options
  const activeOptions: string[] = [];

  if (options.additionalArgs) {
    const normalizedArgs = normalizeDangerousFlags(options.additionalArgs);
    if (normalizedArgs.includes('--dangerously-skip-permissions')) {
      activeOptions.push('Dangerous');
    }
  }

  if (options.verbose) {
    activeOptions.push('Verbose');
  }

  const defaultModel = config.selectedModel || 'None';
  const thinkingModel = config.selectedThinkingModel || 'None';
  const optionsStr = activeOptions.length > 0 ? activeOptions.join(', ') : 'None';

  return [
    chalk.cyan.bold('SynClaude') + chalk.gray(` v${version}`),
    chalk.gray(
      '━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━'
    ),
    `${chalk.blue('Model:')}     ${chalk.cyan(defaultModel)}`,
    `${chalk.magenta('Thinking:')}  ${chalk.magenta(thinkingModel)}`,
    `${chalk.green('Network:')}    ${chalk.green(process.env.AIMUX_PROVIDER === 'minimax-m2' ? 'Minimax M2' : process.env.AIMUX_PROVIDER === 'z-ai' ? 'Z.AI' : 'Synthetic.New')}`,
    `${chalk.yellow('Options:')}    ${chalk.yellow(optionsStr)}`,
    '',
  ].join('\n');
}
