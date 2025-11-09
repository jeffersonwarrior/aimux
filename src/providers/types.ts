import { z } from 'zod';

/**
 * Provider capabilities schema
 */
export const ProviderCapabilitiesSchema = z.object({
  thinking: z.boolean().default(false).describe('Supports thinking/reasoning capabilities'),
  vision: z.boolean().default(false).describe('Supports image/vision input'),
  tools: z.boolean().default(false).describe('Supports function calling/tools'),
  maxTokens: z.number().positive().default(100000).describe('Maximum token limit'),
  streaming: z.boolean().default(true).describe('Supports streaming responses'),
  systemMessages: z.boolean().default(true).describe('Supports system messages'),
  temperature: z.boolean().default(true).describe('Supports temperature parameter'),
  topP: z.boolean().default(true).describe('Supports top_p parameter'),
  maxOutputTokens: z.number().positive().optional().describe('Maximum output tokens'),
});

export type ProviderCapabilities = z.infer<typeof ProviderCapabilitiesSchema>;

/**
 * Enhanced ModelInfo interface extending the base one
 */
export interface EnhancedModelInfo {
  id: string;
  name?: string;
  object?: string;
  created?: number;
  owned_by?: string;
  provider?: string;
  capabilities: ProviderCapabilities;
  context_length?: number;
  max_output_length?: number;
  pricing?: {
    prompt?: string;
    completion?: string;
    image?: string;
    request?: string;
  };
  specialized?: {
    thinking?: boolean;
    coding?: boolean;
    reasoning?: boolean;
    creative?: boolean;
  };
}

/**
 * AI Request interface for standardizing requests across providers
 */
export interface AIRequest {
  model: string;
  messages: Array<{
    role: 'system' | 'user' | 'assistant' | 'tool';
    content:
      | string
      | Array<{
          type: 'text' | 'image_url';
          text?: string;
          image_url?: {
            url: string;
            detail?: 'low' | 'high' | 'auto';
          };
        }>;
    name?: string;
    tool_calls?: Array<{
      id: string;
      type: string;
      function: {
        name: string;
        arguments: string;
      };
    }>;
    tool_call_id?: string;
  }>;
  max_tokens?: number;
  temperature?: number;
  top_p?: number;
  stream?: boolean;
  stop?: string | string[];
  tools?: Array<{
    type: string;
    function: {
      name: string;
      description?: string;
      parameters?: Record<string, any>;
    };
  }>;
  tool_choice?:
    | 'none'
    | 'auto'
    | 'required'
    | {
        type: 'function';
        function: {
          name: string;
        };
      };
  metadata?: {
    requestType?: 'thinking' | 'regular' | 'vision' | 'tools';
    priority?: 'low' | 'medium' | 'high';
    timeout?: number;
    retryCount?: number;
  };
}

/**
 * AI Response interface for standardizing responses across providers
 */
export interface AIResponse {
  id: string;
  object: 'chat.completion' | 'text_completion';
  created: number;
  model: string;
  choices: Array<{
    index: number;
    message?: {
      role: 'assistant';
      content?: string | null;
      tool_calls?: Array<{
        id: string;
        type: string;
        function: {
          name: string;
          arguments: string;
        };
      }>;
    };
    delta?: {
      content?: string;
      tool_calls?: Array<{
        id: string;
        type: string;
        function?: {
          name?: string;
          arguments?: string;
        };
      }>;
    };
    finish_reason?: 'stop' | 'length' | 'tool_calls' | 'content_filter' | 'function_call';
  }>;
  usage?: {
    prompt_tokens: number;
    completion_tokens: number;
    total_tokens: number;
  };
  provider: string;
  response_time?: number;
  metadata?: {
    cached?: boolean;
    routing_decision?: string;
    fallback_used?: boolean;
    failover_attempts?: number;
  };
}

/**
 * Provider health status interface
 */
export interface ProviderHealth {
  status: 'healthy' | 'degraded' | 'unhealthy' | 'unknown';
  response_time?: number;
  last_check: Date;
  error_rate?: number;
  error_message?: string;
  uptime_percentage?: number;
  capabilities_status?: {
    [key: string]: 'available' | 'limited' | 'unavailable';
  };
}

/**
 * Provider configuration interface
 */
export interface ProviderConfig {
  name: string;
  apiKey: string;
  baseUrl?: string;
  modelsUrl?: string;
  timeout?: number;
  maxRetries?: number;
  retryDelay?: number;
  capabilities: ProviderCapabilities;
  enabled: boolean;
  priority?: number;
  customization?: Record<string, any>;
}

/**
 * Provider-specific error class
 */
export class ProviderError extends Error {
  constructor(
    message: string,
    public providerName: string,
    public statusCode?: number,
    public originalError?: any
  ) {
    super(message);
    this.name = 'ProviderError';
  }
}

/**
 * Authentication result interface
 */
export interface AuthenticationResult {
  success: boolean;
  error?: string;
  details?: {
    plan?: string;
    limits?: {
      requests_per_minute?: number;
      tokens_per_minute?: number;
      daily_requests?: number;
    };
    verified_capabilities?: string[];
  };
}
