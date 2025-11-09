import { BaseProvider } from './base-provider';
import { ProviderRegistry } from './provider-registry';
import { MinimaxM2Provider } from './minimax-provider';
import { ZAIProvider } from './zai-provider';
import { SyntheticProvider } from './synthetic-provider';
import type { ProviderCapabilities, EnhancedModelInfo, AIRequest, AIResponse, ProviderHealth, ProviderConfig, AuthenticationResult } from './types';
import { ProviderError } from './types';
export type { ModelInfo, ApiModelsResponse } from '../models/types';
export { BaseProvider, ProviderRegistry };
export { MinimaxM2Provider, ZAIProvider, SyntheticProvider };
export type { ProviderCapabilities, EnhancedModelInfo, AIRequest, AIResponse, ProviderHealth, ProviderConfig, AuthenticationResult, };
export { ProviderError };
/**
 * Create and configure a new provider registry
 * @returns A configured ProviderRegistry instance
 */
export declare function createProviderRegistry(): ProviderRegistry;
/**
 * Validate provider configuration against schema
 * @param config Configuration to validate
 * @returns True if valid, throws error if invalid
 */
export declare function validateProviderConfig(config: any): config is ProviderConfig;
/**
 * Create a standard AI request object
 * @param params Request parameters
 * @returns A standardized AI request
 */
export declare function createAIRequest(params: {
    model: string;
    messages: AIRequest['messages'];
    max_tokens?: number;
    temperature?: number;
    top_p?: number;
    stream?: boolean;
    tools?: AIRequest['tools'];
    tool_choice?: AIRequest['tool_choice'];
    metadata?: AIRequest['metadata'];
}): AIRequest;
/**
 * Create a standard message object
 * @param role Message role
 * @param content Message content
 * @param name Optional message name
 * @returns A standardized message object
 */
export declare function createMessage(role: 'system' | 'user' | 'assistant' | 'tool', content: string | AIRequest['messages'][0]['content'], name?: string): AIRequest['messages'][0];
/**
 * Create a text message with optional image URL
 * @param content Text content
 * @param imageUrl Optional image URL
 * @returns Content array for message
 */
export declare function createContentWithImage(content: string, imageUrl?: string, imageDetail?: 'low' | 'high' | 'auto'): AIRequest['messages'][0]['content'];
/**
 * Create a tool definition
 * @param name Tool name
 * @param description Tool description
 * @param parameters Tool parameters schema
 * @returns A tool definition object
 */
export declare function createTool(name: string, description: string, parameters?: Record<string, any>): NonNullable<AIRequest['tools']>[0];
/**
 * Determine request type based on message content and other factors
 * @param request The AI request to analyze
 * @returns The determined request type
 */
export declare function determineRequestType(request: AIRequest): 'thinking' | 'regular' | 'vision' | 'tools';
/**
 * Provider system version
 */
export declare const PROVIDER_SYSTEM_VERSION = "1.0.0";
/**
 * Default timeout for provider requests (in milliseconds)
 */
export declare const DEFAULT_REQUEST_TIMEOUT = 30000;
/**
 * Default retry configuration
 */
export declare const DEFAULT_RETRY_CONFIG: {
    maxRetries: number;
    initialDelayMs: number;
    maxDelayMs: number;
    backoffMultiplier: number;
};
/**
 * Utility to delay execution
 * @param ms Milliseconds to delay
 * @returns Promise that resolves after delay
 */
export declare function delay(ms: number): Promise<void>;
/**
 * Retry a function with exponential backoff
 * @param fn Function to retry
 * @param config Retry configuration
 * @returns Promise resolving to function result
 */
export declare function retryWithBackoff<T>(fn: () => Promise<T>, config?: Partial<typeof DEFAULT_RETRY_CONFIG>): Promise<T>;
//# sourceMappingURL=index.d.ts.map