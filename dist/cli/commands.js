"use strict";
var __createBinding = (this && this.__createBinding) || (Object.create ? (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    var desc = Object.getOwnPropertyDescriptor(m, k);
    if (!desc || ("get" in desc ? !m.__esModule : desc.writable || desc.configurable)) {
      desc = { enumerable: true, get: function() { return m[k]; } };
    }
    Object.defineProperty(o, k2, desc);
}) : (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    o[k2] = m[k];
}));
var __setModuleDefault = (this && this.__setModuleDefault) || (Object.create ? (function(o, v) {
    Object.defineProperty(o, "default", { enumerable: true, value: v });
}) : function(o, v) {
    o["default"] = v;
});
var __importStar = (this && this.__importStar) || (function () {
    var ownKeys = function(o) {
        ownKeys = Object.getOwnPropertyNames || function (o) {
            var ar = [];
            for (var k in o) if (Object.prototype.hasOwnProperty.call(o, k)) ar[ar.length] = k;
            return ar;
        };
        return ownKeys(o);
    };
    return function (mod) {
        if (mod && mod.__esModule) return mod;
        var result = {};
        if (mod != null) for (var k = ownKeys(mod), i = 0; i < k.length; i++) if (k[i] !== "default") __createBinding(result, mod, k[i]);
        __setModuleDefault(result, mod);
        return result;
    };
})();
Object.defineProperty(exports, "__esModule", { value: true });
exports.createProgram = createProgram;
const commander_1 = require("commander");
const app_1 = require("../core/app");
const aimux_app_1 = require("../core/aimux-app");
const fs_1 = require("fs");
const path_1 = require("path");
const os_1 = require("os");
const banner_1 = require("../utils/banner");
function createProgram() {
    const program = new commander_1.Command();
    // Read version from package.json
    const packageJsonPath = (0, path_1.join)(__dirname, '../../package.json');
    const packageVersion = JSON.parse((0, fs_1.readFileSync)(packageJsonPath, 'utf8')).version;
    program
        .name('aimux')
        .description('AI Multiplexer (Aimux) - Multi-provider CLI tool that intelligently routes Claude Code requests across multiple AI providers\n\nAdditional Claude Code flags (e.g., --dangerously-skip-permissions) are passed through to Claude Code.')
        .version(packageVersion)
        .enablePositionalOptions(true)
        .passThroughOptions(true);
    // Explicit provider commands
    const launchWithProvider = async (providerId, options) => {
        const app = new aimux_app_1.AimuxApp({ type: 'bridge' }); // Bridge mode for direct provider access
        const additionalArgs = [];
        // Get remaining args for Claude Code
        const rawArgs = process.argv.slice(2);
        const providerIndex = rawArgs.indexOf(providerId);
        if (providerIndex !== -1) {
            const additionalStart = providerIndex + 1;
            for (let i = additionalStart; i < rawArgs.length; i++) {
                if (rawArgs[i] &&
                    !rawArgs[i].startsWith('-v') &&
                    !rawArgs[i].startsWith('--verbose') &&
                    !rawArgs[i].startsWith('-q') &&
                    !rawArgs[i].startsWith('--quiet')) {
                    additionalArgs.push(rawArgs[i]);
                }
            }
        }
        const normalizedArgs = (0, banner_1.normalizeDangerousFlags)(additionalArgs);
        await app.runWithProvider(providerId, { ...options, additionalArgs: normalizedArgs });
    };
    // Minimax M2 provider
    program
        .command('minimax-m2')
        .description('Launch with Minimax M2 provider')
        .option('-v, --verbose', 'Enable verbose logging')
        .option('-q, --quiet', 'Suppress non-error output')
        .allowUnknownOption(true)
        .action(async (options) => launchWithProvider('minimax-m2', options));
    // Z.AI provider
    program
        .command('z-ai')
        .description('Launch with Z.AI provider')
        .option('-v, --verbose', 'Enable verbose logging')
        .option('-q, --quiet', 'Suppress non-error output')
        .allowUnknownOption(true)
        .action(async (options) => launchWithProvider('z-ai', options));
    // Synthetic.New provider
    program
        .command('synthetic-new')
        .description('Launch with Synthetic.New provider')
        .option('-v, --verbose', 'Enable verbose logging')
        .option('-q, --quiet', 'Suppress non-error output')
        .allowUnknownOption(true)
        .action(async (options) => launchWithProvider('synthetic-new', options));
    program
        .option('-m, --model <model>', 'Use specific model (skip selection)')
        .option('-t, --thinking-model <model>', 'Use specific thinking model (for Claude thinking mode)')
        .option('-v, --verbose', 'Enable verbose logging')
        .option('-q, --quiet', 'Suppress non-error output')
        .allowUnknownOption(true);
    // Provider management commands
    const providersCmd = program.command('providers').description('Manage AI providers');
    providersCmd
        .command('setup')
        .description('Interactive setup for multiple providers')
        .option('--provider <provider>', 'Setup specific provider only')
        .action(async (options) => {
        const app = new aimux_app_1.AimuxApp();
        if (options.provider) {
            await app.setupSingleProvider(options.provider);
        }
        else {
            await app.setupProviders();
        }
    });
    providersCmd
        .command('list')
        .description('Show configured providers and their status')
        .action(async () => {
        const app = new aimux_app_1.AimuxApp();
        await app.listProviders();
    });
    providersCmd
        .command('test')
        .description('Test provider connectivity and authentication')
        .option('--provider <provider>', 'Test specific provider only')
        .action(async (options) => {
        const app = new aimux_app_1.AimuxApp();
        if (options.provider) {
            const isWorking = await app.testProvider(options.provider);
            console.log(`${options.provider}: ${isWorking ? '✅ Working' : '❌ Failed'}`);
        }
        else {
            await app.testProviders();
        }
    });
    // Individual provider Setup (alternative syntax)
    program
        .command('<provider> setup')
        .description('Setup specific provider')
        .action(async (provider) => {
        const app = new aimux_app_1.AimuxApp();
        await app.setupSingleProvider(provider);
    });
    // Hybrid mode command
    program
        .command('hybrid')
        .description('Launch in hybrid mode with different providers for different request types')
        .requiredOption('--thinking-provider <provider>', 'Provider for thinking-capable requests')
        .requiredOption('--default-provider <provider>', 'Provider for regular requests')
        .option('-v, --verbose', 'Enable verbose logging')
        .option('-q, --quiet', 'Suppress non-error output')
        .allowUnknownOption(true)
        .action(async (options) => {
        const app = new aimux_app_1.AimuxApp({ type: 'bridge' }); // Bridge mode for direct provider access
        const additionalArgs = [];
        // Get remaining args for Claude Code
        const rawArgs = process.argv.slice(2);
        const hybridIndex = rawArgs.findIndex(arg => arg === 'hybrid');
        if (hybridIndex !== -1) {
            for (let i = hybridIndex; i < rawArgs.length; i++) {
                if (rawArgs[i] &&
                    !rawArgs[i].startsWith('--thinking-provider') &&
                    !rawArgs[i].startsWith('--default-provider') &&
                    !rawArgs[i].startsWith('-v') &&
                    !rawArgs[i].startsWith('--verbose') &&
                    !rawArgs[i].startsWith('-q') &&
                    !rawArgs[i].startsWith('--quiet')) {
                    additionalArgs.push(rawArgs[i]);
                }
            }
        }
        const normalizedArgs = (0, banner_1.normalizeDangerousFlags)(additionalArgs);
        await app.runHybrid({
            thinkingProvider: options.thinkingProvider,
            defaultProvider: options.defaultProvider,
            verbose: options.verbose,
            quiet: options.quiet,
            additionalArgs: normalizedArgs,
        });
    });
    // Router management commands
    const routerCmd = program.command('router').description('Manage the request router');
    routerCmd
        .command('start')
        .description('Start the router service')
        .option('-p, --port <port>', 'Port number (default: 8080)', '8080')
        .action(async (options) => {
        console.log(`Starting router on port ${options.port}...`);
        // TODO: Implement router start functionality
        console.log('Router start functionality not yet implemented');
    });
    routerCmd
        .command('stop')
        .description('Stop the router service')
        .action(async () => {
        console.log('Stopping router...');
        // TODO: Implement router stop functionality
        console.log('Router stop functionality not yet implemented');
    });
    routerCmd
        .command('status')
        .description('Show router status')
        .action(async () => {
        console.log('Router status:');
        // TODO: Implement router status functionality
        console.log('Router status functionality not yet implemented');
        console.log('Status: Not running');
    });
    routerCmd
        .command('test')
        .description('Test router functionality')
        .action(async () => {
        console.log('Testing router...');
        // TODO: Implement router test functionality
        console.log('Router test functionality not yet implemented');
    });
    // Enhanced configuration commands
    const configCmd = program.command('config').description('Manage multi-provider configuration');
    configCmd
        .command('show')
        .description('Show current configuration')
        .action(async () => {
        const app = new aimux_app_1.AimuxApp();
        await app.showConfig();
    });
    configCmd
        .command('edit')
        .description('Open configuration in editor')
        .action(async () => {
        const editor = process.env.EDITOR || 'nano';
        const configPath = (0, path_1.join)((0, os_1.homedir)(), '.config', 'aimux', 'config.json');
        const { spawn } = await Promise.resolve().then(() => __importStar(require('child_process')));
        spawn(editor, [configPath], { stdio: 'inherit' });
    });
    // Legacy config commands for backward compatibility
    configCmd
        .command('set <key> <value>')
        .description('Set configuration value (legacy support)')
        .action(async (key, value) => {
        const app = new app_1.SyntheticClaudeApp(); // Use legacy app for backward compatibility
        await app.setConfig(key, value);
    });
    configCmd
        .command('reset')
        .description('Reset configuration to defaults')
        .action(async () => {
        const app = new app_1.SyntheticClaudeApp(); // Use legacy app for backward compatibility
        await app.resetConfig();
    });
    // Setup command (multi-provider)
    program
        .command('setup')
        .description('Run initial multi-provider setup')
        .action(async () => {
        const app = new aimux_app_1.AimuxApp();
        await app.setupProviders();
    });
    // Doctor command - comprehensive system health check
    program
        .command('doctor')
        .description('Comprehensive system health check and troubleshooting')
        .action(async () => {
        const app = new aimux_app_1.AimuxApp();
        await app.doctor();
    });
    // Legacy dangerous command
    program
        .command('dangerous')
        .description('Launch with --dangerously-skip-permissions')
        .option('-v, --verbose', 'Enable verbose logging')
        .option('-q, --quiet', 'Suppress non-error output')
        .action(async (options) => {
        const app = new app_1.SyntheticClaudeApp();
        await app.interactiveModelSelection();
        // After successful model selection, launch Claude Code with --dangerously-skip-permissions
        const config = app.getConfig();
        if (config.selectedModel || config.selectedThinkingModel) {
            await app.run({
                verbose: options.verbose,
                quiet: options.quiet,
                model: '', // Will use saved models from config
                additionalArgs: ['--dangerously-skip-permissions'],
            });
        }
    });
    // Legacy commands for backward compatibility
    program
        .command('model')
        .description('Interactive model selection and launch Claude Code (legacy)')
        .option('-v, --verbose', 'Enable verbose logging')
        .option('-q, --quiet', 'Suppress non-error output')
        .action(async (options) => {
        const app = new app_1.SyntheticClaudeApp();
        await app.interactiveModelSelection();
        const config = app.getConfig();
        if (config.selectedModel || config.selectedThinkingModel) {
            await app.run({
                verbose: options.verbose,
                quiet: options.quiet,
                model: '',
            });
        }
    });
    program
        .command('thinking-model')
        .description('Interactive thinking model selection (legacy)')
        .option('-v, --verbose', 'Enable verbose logging')
        .action(async (_options) => {
        const app = new app_1.SyntheticClaudeApp();
        await app.interactiveThinkingModelSelection();
    });
    program
        .command('models')
        .description('List available models (legacy)')
        .option('--refresh', 'Force refresh model cache')
        .action(async (options) => {
        const app = new app_1.SyntheticClaudeApp();
        await app.listModels(options);
    });
    program
        .command('search <query>')
        .description('Search models by name or provider (legacy)')
        .option('--refresh', 'Force refresh model cache')
        .action(async (query, options) => {
        const app = new app_1.SyntheticClaudeApp();
        await app.searchModels(query, options);
    });
    // Cache management
    const cacheCmd = program.command('cache').description('Manage model cache');
    cacheCmd
        .command('clear')
        .description('Clear model cache')
        .action(async () => {
        const app = new aimux_app_1.AimuxApp();
        await app.clearCache();
    });
    cacheCmd
        .command('info')
        .description('Show cache information')
        .action(async () => {
        const app = new aimux_app_1.AimuxApp();
        await app.cacheInfo();
    });
    return program;
}
//# sourceMappingURL=commands.js.map