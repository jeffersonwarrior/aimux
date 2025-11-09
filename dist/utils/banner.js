"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.normalizeDangerousFlags = normalizeDangerousFlags;
exports.createBanner = createBanner;
const fs_1 = require("fs");
const path_1 = require("path");
const config_1 = require("../config");
const chalk_1 = __importDefault(require("chalk"));
function normalizeDangerousFlags(args) {
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
function createBanner(options = {}) {
    // Read version from package.json
    const packageJsonPath = (0, path_1.join)(__dirname, '../../package.json');
    const version = JSON.parse((0, fs_1.readFileSync)(packageJsonPath, 'utf8')).version;
    // Get current config for models
    const configManager = new config_1.ConfigManager();
    const config = configManager.config;
    // Determine options
    const activeOptions = [];
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
        chalk_1.default.cyan.bold('SynClaude') + chalk_1.default.gray(` v${version}`),
        chalk_1.default.gray('━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━'),
        `${chalk_1.default.blue('Model:')}     ${chalk_1.default.cyan(defaultModel)}`,
        `${chalk_1.default.magenta('Thinking:')}  ${chalk_1.default.magenta(thinkingModel)}`,
        `${chalk_1.default.green('Network:')}    ${chalk_1.default.green(process.env.AIMUX_PROVIDER === 'minimax-m2' ? 'Minimax M2' : process.env.AIMUX_PROVIDER === 'z-ai' ? 'Z.AI' : 'Synthetic.New')}`,
        `${chalk_1.default.yellow('Options:')}    ${chalk_1.default.yellow(optionsStr)}`,
        '',
    ].join('\n');
}
//# sourceMappingURL=banner.js.map