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
export declare class ClaudeLauncher {
    private claudePath;
    constructor(claudePath?: string);
    launchClaudeCode(options: LaunchOptions): Promise<LaunchResult>;
    private createClaudeEnvironment;
    checkClaudeInstallation(): Promise<boolean>;
    getClaudeVersion(): Promise<string | null>;
    setClaudePath(path: string): void;
    getClaudePath(): string;
}
//# sourceMappingURL=claude-launcher.d.ts.map