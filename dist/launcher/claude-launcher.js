"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.ClaudeLauncher = void 0;
const child_process_1 = require("child_process");
class ClaudeLauncher {
    claudePath;
    constructor(claudePath) {
        this.claudePath = claudePath || 'claude';
    }
    async launchClaudeCode(options) {
        try {
            // Set up environment variables for Claude Code
            const env = {
                ...process.env,
                ...this.createClaudeEnvironment(options),
                ...options.env,
            };
            // Debug logging
            if (options.env?.ANTHROPIC_BASE_URL?.includes('api.z.ai')) {
                console.log('[LAUNCHER DEBUG] Z.AI Configuration:');
                console.log(`[LAUNCHER DEBUG] ANTHROPIC_BASE_URL: ${env.ANTHROPIC_BASE_URL}`);
                console.log(`[LAUNCHER DEBUG] ANTHROPIC_API_KEY: ${env.ANTHROPIC_API_KEY ? env.ANTHROPIC_API_KEY.substring(0, 10) + '...' : 'MISSING'}`);
                console.log(`[LAUNCHER DEBUG] ANTHROPIC_DEFAULT_MODEL: ${env.ANTHROPIC_DEFAULT_MODEL}`);
                console.log(`[LAUNCHER DEBUG] ANTHROPIC_DEFAULT_SONNET_MODEL: ${env.ANTHROPIC_DEFAULT_SONNET_MODEL}`);
                console.log(`[LAUNCHER DEBUG] CLAUDE_CODE_SUBAGENT_MODEL: ${env.CLAUDE_CODE_SUBAGENT_MODEL}`);
            }
            // Prepare command arguments
            const args = [...(options.additionalArgs || [])];
            return new Promise(resolve => {
                const child = (0, child_process_1.spawn)(this.claudePath, args, {
                    stdio: 'inherit',
                    env,
                    // Remove detached mode to maintain proper terminal interactivity
                });
                child.on('spawn', () => {
                    resolve({
                        success: true,
                        pid: child.pid || undefined,
                    });
                });
                child.on('error', error => {
                    console.error(`Failed to launch Claude Code: ${error.message}`);
                    resolve({
                        success: false,
                        error: error.message,
                    });
                });
                // Don't unref the process - let it maintain control of the terminal
            });
        }
        catch (error) {
            const errorMessage = error instanceof Error ? error.message : 'Unknown error';
            console.error(`Error launching Claude Code: ${errorMessage}`);
            return {
                success: false,
                error: errorMessage,
            };
        }
    }
    createClaudeEnvironment(options) {
        const env = {};
        const model = options.model;
        // Use provider-specific URL if available, otherwise set based on provider detection
        const hasProviderUrl = options.env?.ANTHROPIC_BASE_URL || process.env.ANTHROPIC_BASE_URL;
        if (hasProviderUrl) {
            // If we already have a provider URL (from aimux app), use it
            env.ANTHROPIC_BASE_URL = hasProviderUrl;
        }
        else {
            // Fallback to synthetic.new only if no provider URL is configured
            env.ANTHROPIC_BASE_URL = 'https://api.synthetic.new/anthropic';
        }
        // Set all the model environment variables to the correct model identifier
        // This ensures Claude Code uses the correct model regardless of which tier it requests
        env.ANTHROPIC_DEFAULT_OPUS_MODEL = model;
        env.ANTHROPIC_DEFAULT_SONNET_MODEL = model;
        env.ANTHROPIC_DEFAULT_HAIKU_MODEL = model;
        env.ANTHROPIC_DEFAULT_HF_MODEL = model;
        env.ANTHROPIC_DEFAULT_MODEL = model;
        // Special handling for Z.AI: Use model format that works with Z.AI API
        if (options.env?.ANTHROPIC_BASE_URL?.includes('api.z.ai')) {
            // Set to GLM-4.6 format - Claude Code will transform to hf:zai-org/GLM-4.6
            // But we'll intercept and fix this transformation
            env.ANTHROPIC_DEFAULT_OPUS_MODEL = 'GLM-4.6';
            env.ANTHROPIC_DEFAULT_SONNET_MODEL = 'GLM-4.6';
            env.ANTHROPIC_DEFAULT_HAIKU_MODEL = 'GLM-4.6';
            env.ANTHROPIC_DEFAULT_HF_MODEL = 'GLM-4.6';
            env.ANTHROPIC_DEFAULT_MODEL = 'GLM-4.6';
        }
        // Set Claude Code subagent model
        // For Z.AI, use their native model to avoid confusion
        if (options.env?.ANTHROPIC_BASE_URL?.includes('api.z.ai')) {
            env.CLAUDE_CODE_SUBAGENT_MODEL = 'GLM-4.6';
        }
        else {
            env.CLAUDE_CODE_SUBAGENT_MODEL = model;
        }
        // Set thinking model if provided
        if (options.thinkingModel) {
            env.ANTHROPIC_THINKING_MODEL = options.thinkingModel;
        }
        // Disable non-essential traffic
        env.CLAUDE_CODE_DISABLE_NONESSENTIAL_TRAFFIC = '1';
        return env;
    }
    async checkClaudeInstallation() {
        return new Promise(resolve => {
            const child = (0, child_process_1.spawn)(this.claudePath, ['--version'], {
                stdio: 'pipe',
            });
            child.on('spawn', () => {
                resolve(true);
            });
            child.on('error', () => {
                resolve(false);
            });
            // Force resolution after timeout
            setTimeout(() => resolve(false), 5000);
        });
    }
    async getClaudeVersion() {
        return new Promise(resolve => {
            const child = (0, child_process_1.spawn)(this.claudePath, ['--version'], {
                stdio: 'pipe',
            });
            let output = '';
            child.stdout?.on('data', data => {
                output += data.toString();
            });
            child.on('close', code => {
                if (code === 0) {
                    resolve(output.trim());
                }
                else {
                    resolve(null);
                }
            });
            child.on('error', () => {
                resolve(null);
            });
            // Force resolution after timeout
            setTimeout(() => resolve(null), 5000);
        });
    }
    setClaudePath(path) {
        this.claudePath = path;
    }
    getClaudePath() {
        return this.claudePath;
    }
}
exports.ClaudeLauncher = ClaudeLauncher;
//# sourceMappingURL=claude-launcher.js.map