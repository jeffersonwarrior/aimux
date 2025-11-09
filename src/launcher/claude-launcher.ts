import { spawn, ChildProcess } from 'child_process';
import { AppConfig } from '../config';

export interface LaunchOptions {
  model: string;
  claudePath?: string;
  additionalArgs?: string[];
  env?: Record<string, string>;
  thinkingModel?: string | null;
}

export interface LaunchResult {
  success: boolean;
  pid?: number;
  error?: string;
}

export class ClaudeLauncher {
  private claudePath: string;

  constructor(claudePath?: string) {
    this.claudePath = claudePath || 'claude';
  }

  async launchClaudeCode(options: LaunchOptions): Promise<LaunchResult> {
    try {
      // Set up environment variables for Claude Code
      const env = {
        ...process.env,
        ...this.createClaudeEnvironment(options),
        ...options.env,
      };


      // Prepare command arguments
      const args = [...(options.additionalArgs || [])];

      return new Promise(resolve => {
        const child = spawn(this.claudePath, args, {
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
    } catch (error) {
      const errorMessage = error instanceof Error ? error.message : 'Unknown error';
      console.error(`Error launching Claude Code: ${errorMessage}`);
      return {
        success: false,
        error: errorMessage,
      };
    }
  }

  private createClaudeEnvironment(options: LaunchOptions): Record<string, string> {
    const env: Record<string, string> = {};
    const model = options.model;

    // Use provider-specific URL if available, otherwise set based on provider detection
    const hasProviderUrl = options.env?.ANTHROPIC_BASE_URL || process.env.ANTHROPIC_BASE_URL;
    if (hasProviderUrl) {
      // If we already have a provider URL (from aimux app), use it
      env.ANTHROPIC_BASE_URL = hasProviderUrl;
    } else {
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

    // Special handling for Z.AI: Use Z.AI-specific model identifiers
    if (options.env?.ANTHROPIC_BASE_URL?.includes('api.z.ai')) {
      // Z.AI models: zai-glm-46 for main tasks, zai-glm-45-air for lighter tasks
      env.ANTHROPIC_DEFAULT_OPUS_MODEL = 'zai-glm-46';
      env.ANTHROPIC_DEFAULT_SONNET_MODEL = 'zai-glm-46';
      env.ANTHROPIC_DEFAULT_HAIKU_MODEL = 'zai-glm-45-air';
      env.ANTHROPIC_DEFAULT_HF_MODEL = 'zai-glm-46';
      env.ANTHROPIC_DEFAULT_MODEL = 'zai-glm-46';
    }

    // Set Claude Code subagent model
    // For Z.AI, use their native model to avoid confusion
    if (options.env?.ANTHROPIC_BASE_URL?.includes('api.z.ai')) {
      env.CLAUDE_CODE_SUBAGENT_MODEL = 'zai-glm-46';
    } else {
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

  async checkClaudeInstallation(): Promise<boolean> {
    return new Promise(resolve => {
      const child = spawn(this.claudePath, ['--version'], {
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

  async getClaudeVersion(): Promise<string | null> {
    return new Promise(resolve => {
      const child = spawn(this.claudePath, ['--version'], {
        stdio: 'pipe',
      });

      let output = '';

      child.stdout?.on('data', data => {
        output += data.toString();
      });

      child.on('close', code => {
        if (code === 0) {
          resolve(output.trim());
        } else {
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

  setClaudePath(path: string): void {
    this.claudePath = path;
  }

  getClaudePath(): string {
    return this.claudePath;
  }
}
