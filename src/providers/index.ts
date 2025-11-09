// Core provider classes
import { BaseProvider } from './base-provider';
import { ProviderRegistry } from './provider-registry';

// Provider implementations
import { MinimaxM2Provider } from './minimax-provider';
import { ZAIProvider } from './zai-provider';
import { SyntheticProvider } from './synthetic-provider';

// Type definitions and interfaces
import type {
  ProviderCapabilities,
  EnhancedModelInfo,
  AIRequest,
  AIResponse,
  ProviderHealth,
  ProviderConfig,
  AuthenticationResult,
} from './types';

// Error classes
import { ProviderError } from './types';

// Re-export existing types that are still useful
export type { ModelInfo, ApiModelsResponse } from '../models/types';

// Export core classes
export { BaseProvider, ProviderRegistry };

// Export provider implementations
export { MinimaxM2Provider, ZAIProvider, SyntheticProvider };

// Export types
export type {
  ProviderCapabilities,
  EnhancedModelInfo,
  AIRequest,
  AIResponse,
  ProviderHealth,
  ProviderConfig,
  AuthenticationResult,
};

// Export error class
export { ProviderError };

/**
 * Create and configure a new provider registry
 * @returns A configured ProviderRegistry instance
 */
export function createProviderRegistry(): ProviderRegistry {
  const registry = new ProviderRegistry();

  // Start health monitoring by default
  registry.startHealthMonitoring(30000); // 30 seconds

  return registry;
}

/**
 * Validate provider configuration against schema
 * @param config Configuration to validate
 * @returns True if valid, throws error if invalid
 */
export function validateProviderConfig(config: any): config is ProviderConfig {
  if (!config || typeof config !== 'object') {
    throw new Error('Provider config must be an object');
  }

  if (typeof config.name !== 'string' || !config.name.trim()) {
    throw new Error('Provider name is required and must be a non-empty string');
  }

  if (typeof config.apiKey !== 'string' || !config.apiKey.trim()) {
    throw new Error('Provider API key is required and must be a non-empty string');
  }

  if (typeof config.enabled !== 'boolean') {
    throw new Error('Provider enabled status must be a boolean');
  }

  if (!config.capabilities || typeof config.capabilities !== 'object') {
    throw new Error('Provider capabilities are required');
  }

  return true;
}

/**
 * Create a standard AI request object
 * @param params Request parameters
 * @returns A standardized AI request
 */
export function createAIRequest(params: {
  model: string;
  messages: AIRequest['messages'];
  max_tokens?: number;
  temperature?: number;
  top_p?: number;
  stream?: boolean;
  tools?: AIRequest['tools'];
  tool_choice?: AIRequest['tool_choice'];
  metadata?: AIRequest['metadata'];
}): AIRequest {
  return {
    model: params.model,
    messages: params.messages,
    max_tokens: params.max_tokens,
    temperature: params.temperature,
    top_p: params.top_p,
    stream: params.stream || false,
    tools: params.tools,
    tool_choice: params.tool_choice,
    metadata: params.metadata || {},
  };
}

/**
 * Create a standard message object
 * @param role Message role
 * @param content Message content
 * @param name Optional message name
 * @returns A standardized message object
 */
export function createMessage(
  role: 'system' | 'user' | 'assistant' | 'tool',
  content: string | AIRequest['messages'][0]['content'],
  name?: string
): AIRequest['messages'][0] {
  const message: AIRequest['messages'][0] = {
    role,
    content,
  };

  if (name) {
    message.name = name;
  }

  return message;
}

/**
 * Create a text message with optional image URL
 * @param content Text content
 * @param imageUrl Optional image URL
 * @returns Content array for message
 */
export function createContentWithImage(
  content: string,
  imageUrl?: string,
  imageDetail?: 'low' | 'high' | 'auto'
): AIRequest['messages'][0]['content'] {
  if (!imageUrl) {
    return content;
  }

  return [
    {
      type: 'text',
      text: content,
    },
    {
      type: 'image_url',
      image_url: {
        url: imageUrl,
        detail: imageDetail || 'auto',
      },
    },
  ];
}

/**
 * Create a tool definition
 * @param name Tool name
 * @param description Tool description
 * @param parameters Tool parameters schema
 * @returns A tool definition object
 */
export function createTool(
  name: string,
  description: string,
  parameters?: Record<string, any>
): NonNullable<AIRequest['tools']>[0] {
  return {
    type: 'function',
    function: {
      name,
      description,
      parameters,
    },
  };
}

/**
 * Determine request type based on message content and other factors
 * @param request The AI request to analyze
 * @returns The determined request type
 */
export function determineRequestType(
  request: AIRequest
): 'thinking' | 'regular' | 'vision' | 'tools' {
  // Check for tools
  if (request.tools && request.tools.length > 0) {
    return 'tools';
  }

  // Check for vision content
  for (const message of request.messages) {
    if (Array.isArray(message.content)) {
      for (const part of message.content) {
        if (part.type === 'image_url') {
          return 'vision';
        }
      }
    }
  }

  // Check for thinking indicators (simple heuristic)
  const textContent = request.messages
    .map((msg: AIRequest['messages'][0]) =>
      Array.isArray(msg.content) ? msg.content.map((p: any) => p.text || '').join('') : msg.content
    )
    .join(' ')
    .toLowerCase();

  const thinkingKeywords = [
    'think step by step',
    'let me think',
    'reasoning',
    'analyze',
    'consider',
    'evaluate',
    'step by step',
    'thought process',
  ];

  if (thinkingKeywords.some(keyword => textContent.includes(keyword))) {
    return 'thinking';
  }

  return 'regular';
}

/**
 * Provider system version
 */
export const PROVIDER_SYSTEM_VERSION = '1.0.0';

/**
 * Default timeout for provider requests (in milliseconds)
 */
export const DEFAULT_REQUEST_TIMEOUT = 30000;

/**
 * Default retry configuration
 */
export const DEFAULT_RETRY_CONFIG = {
  maxRetries: 3,
  initialDelayMs: 1000,
  maxDelayMs: 10000,
  backoffMultiplier: 2,
};

/**
 * Utility to delay execution
 * @param ms Milliseconds to delay
 * @returns Promise that resolves after delay
 */
export function delay(ms: number): Promise<void> {
  return new Promise(resolve => setTimeout(resolve, ms));
}

/**
 * Retry a function with exponential backoff
 * @param fn Function to retry
 * @param config Retry configuration
 * @returns Promise resolving to function result
 */
export async function retryWithBackoff<T>(
  fn: () => Promise<T>,
  config: Partial<typeof DEFAULT_RETRY_CONFIG> = {}
): Promise<T> {
  const finalConfig = { ...DEFAULT_RETRY_CONFIG, ...config };
  let lastError: any;

  for (let attempt = 0; attempt <= finalConfig.maxRetries; attempt++) {
    try {
      return await fn();
    } catch (error) {
      lastError = error;

      if (attempt === finalConfig.maxRetries) {
        break;
      }

      const delayMs = Math.min(
        finalConfig.initialDelayMs * Math.pow(finalConfig.backoffMultiplier, attempt),
        finalConfig.maxDelayMs
      );

      console.warn(`Attempt ${attempt + 1} failed, retrying in ${delayMs}ms:`, error);
      await delay(delayMs);
    }
  }

  throw lastError;
}
