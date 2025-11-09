import { z } from 'zod';
export declare const ProviderConfigSchema: z.ZodObject<{
    name: z.ZodString;
    apiKey: z.ZodDefault<z.ZodString>;
    baseUrl: z.ZodOptional<z.ZodString>;
    anthropicBaseUrl: z.ZodOptional<z.ZodString>;
    modelsApiUrl: z.ZodOptional<z.ZodString>;
    models: z.ZodOptional<z.ZodArray<z.ZodString, "many">>;
    capabilities: z.ZodOptional<z.ZodArray<z.ZodEnum<["thinking", "vision", "tools"]>, "many">>;
    enabled: z.ZodDefault<z.ZodBoolean>;
    priority: z.ZodDefault<z.ZodNumber>;
    timeout: z.ZodDefault<z.ZodNumber>;
    maxRetries: z.ZodDefault<z.ZodNumber>;
}, "strip", z.ZodTypeAny, {
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
}, {
    name: string;
    apiKey?: string | undefined;
    baseUrl?: string | undefined;
    anthropicBaseUrl?: string | undefined;
    modelsApiUrl?: string | undefined;
    models?: string[] | undefined;
    capabilities?: ("thinking" | "vision" | "tools")[] | undefined;
    enabled?: boolean | undefined;
    priority?: number | undefined;
    timeout?: number | undefined;
    maxRetries?: number | undefined;
}>;
export declare const RoutingConfigSchema: z.ZodObject<{
    defaultProvider: z.ZodDefault<z.ZodString>;
    thinkingProvider: z.ZodOptional<z.ZodString>;
    visionProvider: z.ZodOptional<z.ZodString>;
    toolsProvider: z.ZodOptional<z.ZodString>;
    strategy: z.ZodDefault<z.ZodEnum<["capability", "cost", "speed", "reliability"]>>;
    enableFailover: z.ZodDefault<z.ZodBoolean>;
    loadBalancing: z.ZodDefault<z.ZodBoolean>;
}, "strip", z.ZodTypeAny, {
    defaultProvider: string;
    strategy: "capability" | "cost" | "speed" | "reliability";
    enableFailover: boolean;
    loadBalancing: boolean;
    thinkingProvider?: string | undefined;
    visionProvider?: string | undefined;
    toolsProvider?: string | undefined;
}, {
    defaultProvider?: string | undefined;
    thinkingProvider?: string | undefined;
    visionProvider?: string | undefined;
    toolsProvider?: string | undefined;
    strategy?: "capability" | "cost" | "speed" | "reliability" | undefined;
    enableFailover?: boolean | undefined;
    loadBalancing?: boolean | undefined;
}>;
export declare const FallbackConfigSchema: z.ZodObject<{
    enabled: z.ZodDefault<z.ZodBoolean>;
    retryDelay: z.ZodDefault<z.ZodNumber>;
    maxFailures: z.ZodDefault<z.ZodNumber>;
    healthCheck: z.ZodDefault<z.ZodBoolean>;
    healthCheckInterval: z.ZodDefault<z.ZodNumber>;
}, "strip", z.ZodTypeAny, {
    enabled: boolean;
    retryDelay: number;
    maxFailures: number;
    healthCheck: boolean;
    healthCheckInterval: number;
}, {
    enabled?: boolean | undefined;
    retryDelay?: number | undefined;
    maxFailures?: number | undefined;
    healthCheck?: boolean | undefined;
    healthCheckInterval?: number | undefined;
}>;
export declare const MultiProviderConfigSchema: z.ZodObject<{
    providers: z.ZodObject<{
        'minimax-m2': z.ZodOptional<z.ZodObject<{
            name: z.ZodString;
            apiKey: z.ZodDefault<z.ZodString>;
            baseUrl: z.ZodOptional<z.ZodString>;
            anthropicBaseUrl: z.ZodOptional<z.ZodString>;
            modelsApiUrl: z.ZodOptional<z.ZodString>;
            models: z.ZodOptional<z.ZodArray<z.ZodString, "many">>;
            capabilities: z.ZodOptional<z.ZodArray<z.ZodEnum<["thinking", "vision", "tools"]>, "many">>;
            enabled: z.ZodDefault<z.ZodBoolean>;
            priority: z.ZodDefault<z.ZodNumber>;
            timeout: z.ZodDefault<z.ZodNumber>;
            maxRetries: z.ZodDefault<z.ZodNumber>;
        }, "strip", z.ZodTypeAny, {
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
        }, {
            name: string;
            apiKey?: string | undefined;
            baseUrl?: string | undefined;
            anthropicBaseUrl?: string | undefined;
            modelsApiUrl?: string | undefined;
            models?: string[] | undefined;
            capabilities?: ("thinking" | "vision" | "tools")[] | undefined;
            enabled?: boolean | undefined;
            priority?: number | undefined;
            timeout?: number | undefined;
            maxRetries?: number | undefined;
        }>>;
        'z-ai': z.ZodOptional<z.ZodObject<{
            name: z.ZodString;
            apiKey: z.ZodDefault<z.ZodString>;
            baseUrl: z.ZodOptional<z.ZodString>;
            anthropicBaseUrl: z.ZodOptional<z.ZodString>;
            modelsApiUrl: z.ZodOptional<z.ZodString>;
            models: z.ZodOptional<z.ZodArray<z.ZodString, "many">>;
            capabilities: z.ZodOptional<z.ZodArray<z.ZodEnum<["thinking", "vision", "tools"]>, "many">>;
            enabled: z.ZodDefault<z.ZodBoolean>;
            priority: z.ZodDefault<z.ZodNumber>;
            timeout: z.ZodDefault<z.ZodNumber>;
            maxRetries: z.ZodDefault<z.ZodNumber>;
        }, "strip", z.ZodTypeAny, {
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
        }, {
            name: string;
            apiKey?: string | undefined;
            baseUrl?: string | undefined;
            anthropicBaseUrl?: string | undefined;
            modelsApiUrl?: string | undefined;
            models?: string[] | undefined;
            capabilities?: ("thinking" | "vision" | "tools")[] | undefined;
            enabled?: boolean | undefined;
            priority?: number | undefined;
            timeout?: number | undefined;
            maxRetries?: number | undefined;
        }>>;
        'synthetic-new': z.ZodOptional<z.ZodObject<{
            name: z.ZodString;
            apiKey: z.ZodDefault<z.ZodString>;
            baseUrl: z.ZodOptional<z.ZodString>;
            anthropicBaseUrl: z.ZodOptional<z.ZodString>;
            modelsApiUrl: z.ZodOptional<z.ZodString>;
            models: z.ZodOptional<z.ZodArray<z.ZodString, "many">>;
            capabilities: z.ZodOptional<z.ZodArray<z.ZodEnum<["thinking", "vision", "tools"]>, "many">>;
            enabled: z.ZodDefault<z.ZodBoolean>;
            priority: z.ZodDefault<z.ZodNumber>;
            timeout: z.ZodDefault<z.ZodNumber>;
            maxRetries: z.ZodDefault<z.ZodNumber>;
        }, "strip", z.ZodTypeAny, {
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
        }, {
            name: string;
            apiKey?: string | undefined;
            baseUrl?: string | undefined;
            anthropicBaseUrl?: string | undefined;
            modelsApiUrl?: string | undefined;
            models?: string[] | undefined;
            capabilities?: ("thinking" | "vision" | "tools")[] | undefined;
            enabled?: boolean | undefined;
            priority?: number | undefined;
            timeout?: number | undefined;
            maxRetries?: number | undefined;
        }>>;
    }, "strip", z.ZodTypeAny, {
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
    }, {
        'synthetic-new'?: {
            name: string;
            apiKey?: string | undefined;
            baseUrl?: string | undefined;
            anthropicBaseUrl?: string | undefined;
            modelsApiUrl?: string | undefined;
            models?: string[] | undefined;
            capabilities?: ("thinking" | "vision" | "tools")[] | undefined;
            enabled?: boolean | undefined;
            priority?: number | undefined;
            timeout?: number | undefined;
            maxRetries?: number | undefined;
        } | undefined;
        'minimax-m2'?: {
            name: string;
            apiKey?: string | undefined;
            baseUrl?: string | undefined;
            anthropicBaseUrl?: string | undefined;
            modelsApiUrl?: string | undefined;
            models?: string[] | undefined;
            capabilities?: ("thinking" | "vision" | "tools")[] | undefined;
            enabled?: boolean | undefined;
            priority?: number | undefined;
            timeout?: number | undefined;
            maxRetries?: number | undefined;
        } | undefined;
        'z-ai'?: {
            name: string;
            apiKey?: string | undefined;
            baseUrl?: string | undefined;
            anthropicBaseUrl?: string | undefined;
            modelsApiUrl?: string | undefined;
            models?: string[] | undefined;
            capabilities?: ("thinking" | "vision" | "tools")[] | undefined;
            enabled?: boolean | undefined;
            priority?: number | undefined;
            timeout?: number | undefined;
            maxRetries?: number | undefined;
        } | undefined;
    }>;
    routing: z.ZodObject<{
        defaultProvider: z.ZodDefault<z.ZodString>;
        thinkingProvider: z.ZodOptional<z.ZodString>;
        visionProvider: z.ZodOptional<z.ZodString>;
        toolsProvider: z.ZodOptional<z.ZodString>;
        strategy: z.ZodDefault<z.ZodEnum<["capability", "cost", "speed", "reliability"]>>;
        enableFailover: z.ZodDefault<z.ZodBoolean>;
        loadBalancing: z.ZodDefault<z.ZodBoolean>;
    }, "strip", z.ZodTypeAny, {
        defaultProvider: string;
        strategy: "capability" | "cost" | "speed" | "reliability";
        enableFailover: boolean;
        loadBalancing: boolean;
        thinkingProvider?: string | undefined;
        visionProvider?: string | undefined;
        toolsProvider?: string | undefined;
    }, {
        defaultProvider?: string | undefined;
        thinkingProvider?: string | undefined;
        visionProvider?: string | undefined;
        toolsProvider?: string | undefined;
        strategy?: "capability" | "cost" | "speed" | "reliability" | undefined;
        enableFailover?: boolean | undefined;
        loadBalancing?: boolean | undefined;
    }>;
    fallback: z.ZodObject<{
        enabled: z.ZodDefault<z.ZodBoolean>;
        retryDelay: z.ZodDefault<z.ZodNumber>;
        maxFailures: z.ZodDefault<z.ZodNumber>;
        healthCheck: z.ZodDefault<z.ZodBoolean>;
        healthCheckInterval: z.ZodDefault<z.ZodNumber>;
    }, "strip", z.ZodTypeAny, {
        enabled: boolean;
        retryDelay: number;
        maxFailures: number;
        healthCheck: boolean;
        healthCheckInterval: number;
    }, {
        enabled?: boolean | undefined;
        retryDelay?: number | undefined;
        maxFailures?: number | undefined;
        healthCheck?: boolean | undefined;
        healthCheckInterval?: number | undefined;
    }>;
}, "strip", z.ZodTypeAny, {
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
}, {
    providers: {
        'synthetic-new'?: {
            name: string;
            apiKey?: string | undefined;
            baseUrl?: string | undefined;
            anthropicBaseUrl?: string | undefined;
            modelsApiUrl?: string | undefined;
            models?: string[] | undefined;
            capabilities?: ("thinking" | "vision" | "tools")[] | undefined;
            enabled?: boolean | undefined;
            priority?: number | undefined;
            timeout?: number | undefined;
            maxRetries?: number | undefined;
        } | undefined;
        'minimax-m2'?: {
            name: string;
            apiKey?: string | undefined;
            baseUrl?: string | undefined;
            anthropicBaseUrl?: string | undefined;
            modelsApiUrl?: string | undefined;
            models?: string[] | undefined;
            capabilities?: ("thinking" | "vision" | "tools")[] | undefined;
            enabled?: boolean | undefined;
            priority?: number | undefined;
            timeout?: number | undefined;
            maxRetries?: number | undefined;
        } | undefined;
        'z-ai'?: {
            name: string;
            apiKey?: string | undefined;
            baseUrl?: string | undefined;
            anthropicBaseUrl?: string | undefined;
            modelsApiUrl?: string | undefined;
            models?: string[] | undefined;
            capabilities?: ("thinking" | "vision" | "tools")[] | undefined;
            enabled?: boolean | undefined;
            priority?: number | undefined;
            timeout?: number | undefined;
            maxRetries?: number | undefined;
        } | undefined;
    };
    routing: {
        defaultProvider?: string | undefined;
        thinkingProvider?: string | undefined;
        visionProvider?: string | undefined;
        toolsProvider?: string | undefined;
        strategy?: "capability" | "cost" | "speed" | "reliability" | undefined;
        enableFailover?: boolean | undefined;
        loadBalancing?: boolean | undefined;
    };
    fallback: {
        enabled?: boolean | undefined;
        retryDelay?: number | undefined;
        maxFailures?: number | undefined;
        healthCheck?: boolean | undefined;
        healthCheckInterval?: number | undefined;
    };
}>;
export declare const AppConfigSchema: z.ZodObject<{
    apiKey: z.ZodDefault<z.ZodString>;
    baseUrl: z.ZodDefault<z.ZodString>;
    anthropicBaseUrl: z.ZodDefault<z.ZodString>;
    modelsApiUrl: z.ZodDefault<z.ZodString>;
    cacheDurationHours: z.ZodDefault<z.ZodNumber>;
    selectedModel: z.ZodDefault<z.ZodString>;
    selectedThinkingModel: z.ZodDefault<z.ZodString>;
    firstRunCompleted: z.ZodDefault<z.ZodBoolean>;
    multiProvider: z.ZodOptional<z.ZodObject<{
        providers: z.ZodObject<{
            'minimax-m2': z.ZodOptional<z.ZodObject<{
                name: z.ZodString;
                apiKey: z.ZodDefault<z.ZodString>;
                baseUrl: z.ZodOptional<z.ZodString>;
                anthropicBaseUrl: z.ZodOptional<z.ZodString>;
                modelsApiUrl: z.ZodOptional<z.ZodString>;
                models: z.ZodOptional<z.ZodArray<z.ZodString, "many">>;
                capabilities: z.ZodOptional<z.ZodArray<z.ZodEnum<["thinking", "vision", "tools"]>, "many">>;
                enabled: z.ZodDefault<z.ZodBoolean>;
                priority: z.ZodDefault<z.ZodNumber>;
                timeout: z.ZodDefault<z.ZodNumber>;
                maxRetries: z.ZodDefault<z.ZodNumber>;
            }, "strip", z.ZodTypeAny, {
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
            }, {
                name: string;
                apiKey?: string | undefined;
                baseUrl?: string | undefined;
                anthropicBaseUrl?: string | undefined;
                modelsApiUrl?: string | undefined;
                models?: string[] | undefined;
                capabilities?: ("thinking" | "vision" | "tools")[] | undefined;
                enabled?: boolean | undefined;
                priority?: number | undefined;
                timeout?: number | undefined;
                maxRetries?: number | undefined;
            }>>;
            'z-ai': z.ZodOptional<z.ZodObject<{
                name: z.ZodString;
                apiKey: z.ZodDefault<z.ZodString>;
                baseUrl: z.ZodOptional<z.ZodString>;
                anthropicBaseUrl: z.ZodOptional<z.ZodString>;
                modelsApiUrl: z.ZodOptional<z.ZodString>;
                models: z.ZodOptional<z.ZodArray<z.ZodString, "many">>;
                capabilities: z.ZodOptional<z.ZodArray<z.ZodEnum<["thinking", "vision", "tools"]>, "many">>;
                enabled: z.ZodDefault<z.ZodBoolean>;
                priority: z.ZodDefault<z.ZodNumber>;
                timeout: z.ZodDefault<z.ZodNumber>;
                maxRetries: z.ZodDefault<z.ZodNumber>;
            }, "strip", z.ZodTypeAny, {
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
            }, {
                name: string;
                apiKey?: string | undefined;
                baseUrl?: string | undefined;
                anthropicBaseUrl?: string | undefined;
                modelsApiUrl?: string | undefined;
                models?: string[] | undefined;
                capabilities?: ("thinking" | "vision" | "tools")[] | undefined;
                enabled?: boolean | undefined;
                priority?: number | undefined;
                timeout?: number | undefined;
                maxRetries?: number | undefined;
            }>>;
            'synthetic-new': z.ZodOptional<z.ZodObject<{
                name: z.ZodString;
                apiKey: z.ZodDefault<z.ZodString>;
                baseUrl: z.ZodOptional<z.ZodString>;
                anthropicBaseUrl: z.ZodOptional<z.ZodString>;
                modelsApiUrl: z.ZodOptional<z.ZodString>;
                models: z.ZodOptional<z.ZodArray<z.ZodString, "many">>;
                capabilities: z.ZodOptional<z.ZodArray<z.ZodEnum<["thinking", "vision", "tools"]>, "many">>;
                enabled: z.ZodDefault<z.ZodBoolean>;
                priority: z.ZodDefault<z.ZodNumber>;
                timeout: z.ZodDefault<z.ZodNumber>;
                maxRetries: z.ZodDefault<z.ZodNumber>;
            }, "strip", z.ZodTypeAny, {
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
            }, {
                name: string;
                apiKey?: string | undefined;
                baseUrl?: string | undefined;
                anthropicBaseUrl?: string | undefined;
                modelsApiUrl?: string | undefined;
                models?: string[] | undefined;
                capabilities?: ("thinking" | "vision" | "tools")[] | undefined;
                enabled?: boolean | undefined;
                priority?: number | undefined;
                timeout?: number | undefined;
                maxRetries?: number | undefined;
            }>>;
        }, "strip", z.ZodTypeAny, {
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
        }, {
            'synthetic-new'?: {
                name: string;
                apiKey?: string | undefined;
                baseUrl?: string | undefined;
                anthropicBaseUrl?: string | undefined;
                modelsApiUrl?: string | undefined;
                models?: string[] | undefined;
                capabilities?: ("thinking" | "vision" | "tools")[] | undefined;
                enabled?: boolean | undefined;
                priority?: number | undefined;
                timeout?: number | undefined;
                maxRetries?: number | undefined;
            } | undefined;
            'minimax-m2'?: {
                name: string;
                apiKey?: string | undefined;
                baseUrl?: string | undefined;
                anthropicBaseUrl?: string | undefined;
                modelsApiUrl?: string | undefined;
                models?: string[] | undefined;
                capabilities?: ("thinking" | "vision" | "tools")[] | undefined;
                enabled?: boolean | undefined;
                priority?: number | undefined;
                timeout?: number | undefined;
                maxRetries?: number | undefined;
            } | undefined;
            'z-ai'?: {
                name: string;
                apiKey?: string | undefined;
                baseUrl?: string | undefined;
                anthropicBaseUrl?: string | undefined;
                modelsApiUrl?: string | undefined;
                models?: string[] | undefined;
                capabilities?: ("thinking" | "vision" | "tools")[] | undefined;
                enabled?: boolean | undefined;
                priority?: number | undefined;
                timeout?: number | undefined;
                maxRetries?: number | undefined;
            } | undefined;
        }>;
        routing: z.ZodObject<{
            defaultProvider: z.ZodDefault<z.ZodString>;
            thinkingProvider: z.ZodOptional<z.ZodString>;
            visionProvider: z.ZodOptional<z.ZodString>;
            toolsProvider: z.ZodOptional<z.ZodString>;
            strategy: z.ZodDefault<z.ZodEnum<["capability", "cost", "speed", "reliability"]>>;
            enableFailover: z.ZodDefault<z.ZodBoolean>;
            loadBalancing: z.ZodDefault<z.ZodBoolean>;
        }, "strip", z.ZodTypeAny, {
            defaultProvider: string;
            strategy: "capability" | "cost" | "speed" | "reliability";
            enableFailover: boolean;
            loadBalancing: boolean;
            thinkingProvider?: string | undefined;
            visionProvider?: string | undefined;
            toolsProvider?: string | undefined;
        }, {
            defaultProvider?: string | undefined;
            thinkingProvider?: string | undefined;
            visionProvider?: string | undefined;
            toolsProvider?: string | undefined;
            strategy?: "capability" | "cost" | "speed" | "reliability" | undefined;
            enableFailover?: boolean | undefined;
            loadBalancing?: boolean | undefined;
        }>;
        fallback: z.ZodObject<{
            enabled: z.ZodDefault<z.ZodBoolean>;
            retryDelay: z.ZodDefault<z.ZodNumber>;
            maxFailures: z.ZodDefault<z.ZodNumber>;
            healthCheck: z.ZodDefault<z.ZodBoolean>;
            healthCheckInterval: z.ZodDefault<z.ZodNumber>;
        }, "strip", z.ZodTypeAny, {
            enabled: boolean;
            retryDelay: number;
            maxFailures: number;
            healthCheck: boolean;
            healthCheckInterval: number;
        }, {
            enabled?: boolean | undefined;
            retryDelay?: number | undefined;
            maxFailures?: number | undefined;
            healthCheck?: boolean | undefined;
            healthCheckInterval?: number | undefined;
        }>;
    }, "strip", z.ZodTypeAny, {
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
    }, {
        providers: {
            'synthetic-new'?: {
                name: string;
                apiKey?: string | undefined;
                baseUrl?: string | undefined;
                anthropicBaseUrl?: string | undefined;
                modelsApiUrl?: string | undefined;
                models?: string[] | undefined;
                capabilities?: ("thinking" | "vision" | "tools")[] | undefined;
                enabled?: boolean | undefined;
                priority?: number | undefined;
                timeout?: number | undefined;
                maxRetries?: number | undefined;
            } | undefined;
            'minimax-m2'?: {
                name: string;
                apiKey?: string | undefined;
                baseUrl?: string | undefined;
                anthropicBaseUrl?: string | undefined;
                modelsApiUrl?: string | undefined;
                models?: string[] | undefined;
                capabilities?: ("thinking" | "vision" | "tools")[] | undefined;
                enabled?: boolean | undefined;
                priority?: number | undefined;
                timeout?: number | undefined;
                maxRetries?: number | undefined;
            } | undefined;
            'z-ai'?: {
                name: string;
                apiKey?: string | undefined;
                baseUrl?: string | undefined;
                anthropicBaseUrl?: string | undefined;
                modelsApiUrl?: string | undefined;
                models?: string[] | undefined;
                capabilities?: ("thinking" | "vision" | "tools")[] | undefined;
                enabled?: boolean | undefined;
                priority?: number | undefined;
                timeout?: number | undefined;
                maxRetries?: number | undefined;
            } | undefined;
        };
        routing: {
            defaultProvider?: string | undefined;
            thinkingProvider?: string | undefined;
            visionProvider?: string | undefined;
            toolsProvider?: string | undefined;
            strategy?: "capability" | "cost" | "speed" | "reliability" | undefined;
            enableFailover?: boolean | undefined;
            loadBalancing?: boolean | undefined;
        };
        fallback: {
            enabled?: boolean | undefined;
            retryDelay?: number | undefined;
            maxFailures?: number | undefined;
            healthCheck?: boolean | undefined;
            healthCheckInterval?: number | undefined;
        };
    }>>;
    providers: z.ZodOptional<z.ZodRecord<z.ZodString, z.ZodObject<{
        name: z.ZodString;
        apiKey: z.ZodDefault<z.ZodString>;
        baseUrl: z.ZodOptional<z.ZodString>;
        anthropicBaseUrl: z.ZodOptional<z.ZodString>;
        modelsApiUrl: z.ZodOptional<z.ZodString>;
        models: z.ZodOptional<z.ZodArray<z.ZodString, "many">>;
        capabilities: z.ZodOptional<z.ZodArray<z.ZodEnum<["thinking", "vision", "tools"]>, "many">>;
        enabled: z.ZodDefault<z.ZodBoolean>;
        priority: z.ZodDefault<z.ZodNumber>;
        timeout: z.ZodDefault<z.ZodNumber>;
        maxRetries: z.ZodDefault<z.ZodNumber>;
    }, "strip", z.ZodTypeAny, {
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
    }, {
        name: string;
        apiKey?: string | undefined;
        baseUrl?: string | undefined;
        anthropicBaseUrl?: string | undefined;
        modelsApiUrl?: string | undefined;
        models?: string[] | undefined;
        capabilities?: ("thinking" | "vision" | "tools")[] | undefined;
        enabled?: boolean | undefined;
        priority?: number | undefined;
        timeout?: number | undefined;
        maxRetries?: number | undefined;
    }>>>;
    routingRules: z.ZodOptional<z.ZodRecord<z.ZodString, z.ZodString>>;
}, "strip", z.ZodTypeAny, {
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
}, {
    apiKey?: string | undefined;
    baseUrl?: string | undefined;
    anthropicBaseUrl?: string | undefined;
    modelsApiUrl?: string | undefined;
    providers?: Record<string, {
        name: string;
        apiKey?: string | undefined;
        baseUrl?: string | undefined;
        anthropicBaseUrl?: string | undefined;
        modelsApiUrl?: string | undefined;
        models?: string[] | undefined;
        capabilities?: ("thinking" | "vision" | "tools")[] | undefined;
        enabled?: boolean | undefined;
        priority?: number | undefined;
        timeout?: number | undefined;
        maxRetries?: number | undefined;
    }> | undefined;
    cacheDurationHours?: number | undefined;
    selectedModel?: string | undefined;
    selectedThinkingModel?: string | undefined;
    firstRunCompleted?: boolean | undefined;
    multiProvider?: {
        providers: {
            'synthetic-new'?: {
                name: string;
                apiKey?: string | undefined;
                baseUrl?: string | undefined;
                anthropicBaseUrl?: string | undefined;
                modelsApiUrl?: string | undefined;
                models?: string[] | undefined;
                capabilities?: ("thinking" | "vision" | "tools")[] | undefined;
                enabled?: boolean | undefined;
                priority?: number | undefined;
                timeout?: number | undefined;
                maxRetries?: number | undefined;
            } | undefined;
            'minimax-m2'?: {
                name: string;
                apiKey?: string | undefined;
                baseUrl?: string | undefined;
                anthropicBaseUrl?: string | undefined;
                modelsApiUrl?: string | undefined;
                models?: string[] | undefined;
                capabilities?: ("thinking" | "vision" | "tools")[] | undefined;
                enabled?: boolean | undefined;
                priority?: number | undefined;
                timeout?: number | undefined;
                maxRetries?: number | undefined;
            } | undefined;
            'z-ai'?: {
                name: string;
                apiKey?: string | undefined;
                baseUrl?: string | undefined;
                anthropicBaseUrl?: string | undefined;
                modelsApiUrl?: string | undefined;
                models?: string[] | undefined;
                capabilities?: ("thinking" | "vision" | "tools")[] | undefined;
                enabled?: boolean | undefined;
                priority?: number | undefined;
                timeout?: number | undefined;
                maxRetries?: number | undefined;
            } | undefined;
        };
        routing: {
            defaultProvider?: string | undefined;
            thinkingProvider?: string | undefined;
            visionProvider?: string | undefined;
            toolsProvider?: string | undefined;
            strategy?: "capability" | "cost" | "speed" | "reliability" | undefined;
            enableFailover?: boolean | undefined;
            loadBalancing?: boolean | undefined;
        };
        fallback: {
            enabled?: boolean | undefined;
            retryDelay?: number | undefined;
            maxFailures?: number | undefined;
            healthCheck?: boolean | undefined;
            healthCheckInterval?: number | undefined;
        };
    } | undefined;
    routingRules?: Record<string, string> | undefined;
}>;
export type AppConfig = z.infer<typeof AppConfigSchema>;
export type ProviderConfig = z.infer<typeof ProviderConfigSchema>;
export type ConfigRoutingConfig = z.infer<typeof RoutingConfigSchema>;
export type FallbackConfig = z.infer<typeof FallbackConfigSchema>;
export type MultiProviderConfig = z.infer<typeof MultiProviderConfigSchema>;
export type RoutingConfig = ConfigRoutingConfig;
export declare class ConfigValidationError extends Error {
    cause?: unknown | undefined;
    constructor(message: string, cause?: unknown | undefined);
}
export declare class ConfigLoadError extends Error {
    cause?: unknown | undefined;
    constructor(message: string, cause?: unknown | undefined);
}
export declare class ConfigSaveError extends Error {
    cause?: unknown | undefined;
    constructor(message: string, cause?: unknown | undefined);
}
//# sourceMappingURL=types.d.ts.map