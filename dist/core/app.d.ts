import { LaunchOptions } from '../launcher';
export interface AppOptions {
    verbose?: boolean;
    quiet?: boolean;
    additionalArgs?: string[];
    thinkingModel?: string;
}
export declare class SyntheticClaudeApp {
    private configManager;
    private ui;
    private launcher;
    private modelManager;
    constructor();
    setupLogging(options: AppOptions): Promise<void>;
    getConfig(): {
        apiKey: string;
        baseUrl: string;
        anthropicBaseUrl: string;
        modelsApiUrl: string;
        cacheDurationHours: number;
        selectedModel: string;
        selectedThinkingModel: string;
        firstRunCompleted: boolean;
        providers?: Record<string, {
            name: string;
            apiKey: string;
            enabled: boolean;
            priority: number;
            timeout: number;
            maxRetries: number;
            baseUrl?: string | undefined;
            anthropicBaseUrl?: string | undefined;
            modelsApiUrl?: string | undefined;
            models?: string[] | undefined;
            capabilities?: ("thinking" | "vision" | "tools")[] | undefined;
        }> | undefined;
        multiProvider?: {
            providers: {
                'synthetic-new'?: {
                    name: string;
                    apiKey: string;
                    enabled: boolean;
                    priority: number;
                    timeout: number;
                    maxRetries: number;
                    baseUrl?: string | undefined;
                    anthropicBaseUrl?: string | undefined;
                    modelsApiUrl?: string | undefined;
                    models?: string[] | undefined;
                    capabilities?: ("thinking" | "vision" | "tools")[] | undefined;
                } | undefined;
                'minimax-m2'?: {
                    name: string;
                    apiKey: string;
                    enabled: boolean;
                    priority: number;
                    timeout: number;
                    maxRetries: number;
                    baseUrl?: string | undefined;
                    anthropicBaseUrl?: string | undefined;
                    modelsApiUrl?: string | undefined;
                    models?: string[] | undefined;
                    capabilities?: ("thinking" | "vision" | "tools")[] | undefined;
                } | undefined;
                'z-ai'?: {
                    name: string;
                    apiKey: string;
                    enabled: boolean;
                    priority: number;
                    timeout: number;
                    maxRetries: number;
                    baseUrl?: string | undefined;
                    anthropicBaseUrl?: string | undefined;
                    modelsApiUrl?: string | undefined;
                    models?: string[] | undefined;
                    capabilities?: ("thinking" | "vision" | "tools")[] | undefined;
                } | undefined;
            };
            routing: {
                defaultProvider: string;
                strategy: "capability" | "cost" | "speed" | "reliability";
                enableFailover: boolean;
                loadBalancing: boolean;
                thinkingProvider?: string | undefined;
                visionProvider?: string | undefined;
                toolsProvider?: string | undefined;
            };
            fallback: {
                enabled: boolean;
                retryDelay: number;
                maxFailures: number;
                healthCheck: boolean;
                healthCheckInterval: number;
            };
        } | undefined;
        routingRules?: Record<string, string> | undefined;
    };
    private getModelManager;
    run(options: AppOptions & LaunchOptions): Promise<void>;
    interactiveModelSelection(): Promise<boolean>;
    interactiveThinkingModelSelection(): Promise<boolean>;
    listModels(options: {
        refresh?: boolean;
    }): Promise<void>;
    searchModels(query: string, options: {
        refresh?: boolean;
    }): Promise<void>;
    showConfig(): Promise<void>;
    setConfig(key: string, value: string): Promise<void>;
    resetConfig(): Promise<void>;
    setup(): Promise<void>;
    doctor(): Promise<void>;
    clearCache(): Promise<void>;
    cacheInfo(): Promise<void>;
    private selectModel;
    private selectThinkingModel;
    private launchClaudeCode;
}
//# sourceMappingURL=app.d.ts.map