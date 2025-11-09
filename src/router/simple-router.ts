import { createServer, IncomingMessage, ServerResponse } from 'http';
import { AxiosError, AxiosRequestConfig, AxiosResponse } from 'axios';
import { logger } from '../utils/logger';
import { BaseProvider } from '../providers/base-provider';
import { ProviderRegistry } from '../providers/provider-registry';
import { AIRequest } from '../providers/types';
import { RoutingEngine } from '../core/routing-engine';

export interface SimpleRouterConfig {
  port: number;
  host: string;
  enableLogging: boolean;
  maxRequestSize: number;
  timeout: number;
  enableHealthCheck: boolean;
  healthCheckInterval: number;
}

export class SimpleRouter {
  private server: any = null;
  private isRunning = false;
  private providerRegistry: ProviderRegistry;
  private routingEngine: RoutingEngine;
  private config: SimpleRouterConfig;
  private healthCheckTimer?: NodeJS.Timeout;

  constructor(
    providerRegistry: ProviderRegistry,
    routingEngine: RoutingEngine,
    config: Partial<SimpleRouterConfig> = {}
  ) {
    this.providerRegistry = providerRegistry;
    this.routingEngine = routingEngine;
    this.config = {
      port: 8080,
      host: 'localhost',
      enableLogging: true,
      maxRequestSize: 10 * 1024 * 1024, // 10MB
      timeout: 30000, // 30 seconds
      enableHealthCheck: true,
      healthCheckInterval: 60000, // 1 minute
      ...config,
    };
  }

  /**
   * Start the HTTP proxy server
   */
  async start(): Promise<void> {
    if (this.isRunning) {
      logger.warn('Router is already running');
      return;
    }

    try {
      await new Promise<void>((resolve, reject) => {
        this.server = createServer((req: IncomingMessage, res: ServerResponse) => {
          this.handleRequest(req, res).catch(error => {
            logger.error('Request handling error:', error);
            if (!res.headersSent) {
              res.writeHead(500, { 'Content-Type': 'application/json' });
              res.end(
                JSON.stringify({
                  error: 'Internal server error',
                  message: 'Failed to process request',
                })
              );
            }
          });
        });

        this.server.on('error', reject);

        this.server.listen(this.config.port, this.config.host, () => {
          logger.info(`Router started on ${this.config.host}:${this.config.port}`);
          this.isRunning = true;
          resolve();
        });
      });

      // Start health check if enabled
      if (this.config.enableHealthCheck) {
        this.startHealthCheck();
      }
    } catch (error) {
      logger.error('Failed to start router:', error);
      throw error;
    }
  }

  /**
   * Stop the HTTP proxy server
   */
  async stop(): Promise<void> {
    if (!this.isRunning) {
      logger.warn('Router is not running');
      return;
    }

    try {
      // Stop health check
      if (this.healthCheckTimer) {
        clearInterval(this.healthCheckTimer);
        this.healthCheckTimer = undefined;
      }

      // Stop server
      await new Promise<void>((resolve, reject) => {
        if (!this.server) {
          resolve();
          return;
        }

        this.server.close((error: any) => {
          if (error) {
            reject(error);
          } else {
            logger.info('Router stopped');
            this.isRunning = false;
            resolve();
          }
        });
      });
    } catch (error) {
      logger.error('Failed to stop router:', error);
      throw error;
    }
  }

  /**
   * Handle incoming HTTP requests
   */
  private async handleRequest(req: IncomingMessage, res: ServerResponse): Promise<void> {
    const startTime = Date.now();

    try {
      // Enable CORS
      res.setHeader('Access-Control-Allow-Origin', '*');
      res.setHeader('Access-Control-Allow-Methods', 'GET, POST, PUT, DELETE, OPTIONS');
      res.setHeader('Access-Control-Allow-Headers', 'Content-Type, Authorization, x-api-key');

      // Handle preflight requests
      if (req.method === 'OPTIONS') {
        res.writeHead(200);
        res.end();
        return;
      }

      // Only handle POST requests for now (OpenAI API)
      if (req.method !== 'POST') {
        res.writeHead(405, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({ error: 'Method not allowed' }));
        return;
      }

      // Parse request body
      const body = await this.parseRequestBody(req);

      // Analyze request to determine routing
      const requestAnalysis = this.analyzeRequest(body);

      // Select appropriate provider
      const selectedProvider = this.routingEngine.selectProvider(requestAnalysis);

      if (!selectedProvider) {
        res.writeHead(503, { 'Content-Type': 'application/json' });
        res.end(
          JSON.stringify({
            error: 'Service unavailable',
            message: 'No suitable provider available for this request',
          })
        );
        return;
      }

      // Log routing decision if enabled
      if (this.config.enableLogging) {
        logger.info(`Routing request to ${selectedProvider.name}:`, {
          requestType: requestAnalysis.metadata?.requestType,
          model: requestAnalysis.model,
          hasVision: requestAnalysis.messages.some(
            m => Array.isArray(m.content) && m.content.some(c => c.type === 'image_url')
          ),
          hasTools: !!requestAnalysis.tools,
        });
      }

      // Forward request to selected provider
      const response = await this.forwardRequest(selectedProvider, requestAnalysis, req);

      // Normalize response
      const normalizedResponse = this.normalizeResponse(
        response,
        selectedProvider.name,
        Date.now() - startTime
      );

      // Send response back to client
      res.writeHead(normalizedResponse.statusCode || 200, normalizedResponse.headers);
      res.end(normalizedResponse.data);
    } catch (error) {
      const duration = Date.now() - startTime;
      logger.error(`Request failed after ${duration}ms:`, error);

      if (!res.headersSent) {
        const statusCode = error instanceof AxiosError ? error.response?.status || 500 : 500;
        res.writeHead(statusCode, { 'Content-Type': 'application/json' });
        res.end(
          JSON.stringify({
            error: 'Request failed',
            message: error instanceof Error ? error.message : 'Unknown error',
            duration,
          })
        );
      }
    }
  }

  /**
   * Parse request body with size limit
   */
  private async parseRequestBody(req: IncomingMessage): Promise<any> {
    return new Promise((resolve, reject) => {
      let body = '';
      let size = 0;

      req.on('data', chunk => {
        size += chunk.length;
        if (size > this.config.maxRequestSize) {
          reject(new Error('Request too large'));
          return;
        }
        body += chunk.toString();
      });

      req.on('end', () => {
        try {
          const parsed = JSON.parse(body);
          resolve(parsed);
        } catch (error) {
          reject(new Error('Invalid JSON in request body'));
        }
      });

      req.on('error', reject);
    });
  }

  /**
   * Analyze request to determine routing requirements
   */
  private analyzeRequest(body: any): AIRequest {
    const request: AIRequest = {
      model: body.model || 'unknown',
      messages: body.messages || [],
      max_tokens: body.max_tokens,
      temperature: body.temperature,
      top_p: body.top_p,
      stream: body.stream,
      stop: body.stop,
      tools: body.tools,
      tool_choice: body.tool_choice,
      metadata: {
        requestType: 'regular',
        priority: 'medium',
      },
    };

    // Determine request type
    if (body.model && body.model.toLowerCase().includes('thinking')) {
      request.metadata!.requestType = 'thinking';
    }

    // Check for vision content
    const hasImage = request.messages.some(
      message =>
        Array.isArray(message.content) &&
        message.content.some(content => content.type === 'image_url')
    );
    if (hasImage) {
      request.metadata!.requestType = 'vision';
    }

    // Check for tools
    if (request.tools && request.tools.length > 0) {
      request.metadata!.requestType = 'tools';
    }

    return request;
  }

  /**
   * Forward request to selected provider
   */
  private async forwardRequest(
    provider: BaseProvider,
    request: AIRequest,
    originalReq: IncomingMessage
  ): Promise<AxiosResponse> {
    try {
      // Get provider's HTTP client
      const httpClient = (provider as any).getHttpClient?.() || (provider as any).httpClient;

      if (!httpClient) {
        throw new Error(`Provider ${provider.name} does not have an HTTP client`);
      }

      // Build the target URL
      const targetUrl = this.buildTargetUrl(provider, request);

      // Prepare headers
      const headers = {
        'Content-Type': 'application/json',
        Authorization: originalReq.headers.authorization,
        'x-api-key': originalReq.headers['x-api-key'],
        'User-Agent': 'aimux-proxy/1.0',
        ...((provider as any).getDefaultHeaders?.() || {}),
      };

      // Make the request
      const config: AxiosRequestConfig = {
        method: 'POST',
        url: targetUrl,
        data: {
          model: request.model,
          messages: request.messages,
          max_tokens: request.max_tokens,
          temperature: request.temperature,
          top_p: request.top_p,
          stream: request.stream,
          stop: request.stop,
          tools: request.tools,
          tool_choice: request.tool_choice,
        },
        headers,
        timeout: this.config.timeout,
        responseType: request.stream ? 'stream' : 'json',
      };

      const response = await httpClient(config);
      return response;
    } catch (error) {
      logger.error(`Failed to forward request to ${provider.name}:`, error);
      throw error;
    }
  }

  /**
   * Build target URL for provider
   */
  private buildTargetUrl(provider: BaseProvider, _request: AIRequest): string {
    const baseUrl = (provider as any).config?.baseUrl || (provider as any).baseUrl;
    if (!baseUrl) {
      throw new Error(`Provider ${provider.name} has no base URL configured`);
    }

    // Most providers use /v1/chat/completions endpoint
    const endpoint = '/v1/chat/completions';
    return baseUrl.replace(/\/$/, '') + endpoint;
  }

  /**
   * Normalize response from provider
   */
  private normalizeResponse(
    response: AxiosResponse,
    providerName: string,
    duration: number
  ): { statusCode?: number; headers: any; data: any } {
    let normalizedData = response.data;

    // Add routing metadata if response is JSON
    if (typeof normalizedData === 'object' && normalizedData !== null) {
      normalizedData = {
        ...normalizedData,
        provider: providerName,
        response_time: duration,
        metadata: {
          routing_decision: `provider:${providerName}`,
          fallback_used: false,
          failover_attempts: 0,
        },
      };
    }

    return {
      statusCode: response.status,
      headers: {
        'Content-Type': response.headers['content-type'] || 'application/json',
        'Access-Control-Allow-Origin': '*',
        ...response.headers,
      },
      data: typeof normalizedData === 'string' ? normalizedData : JSON.stringify(normalizedData),
    };
  }

  /**
   * Start health check timer
   */
  private startHealthCheck(): void {
    this.healthCheckTimer = setInterval(async () => {
      try {
        await this.performHealthCheck();
      } catch (error) {
        logger.error('Health check failed:', error);
      }
    }, this.config.healthCheckInterval);
  }

  /**
   * Perform health check on all providers
   */
  private async performHealthCheck(): Promise<void> {
    const providers = this.providerRegistry.getAllProviders();

    for (const provider of providers) {
      try {
        const health = await provider.healthCheck();
        logger.debug(`Health check for ${provider.name}:`, health);
      } catch (error) {
        logger.warn(`Health check failed for ${provider.name}:`, error);
      }
    }
  }

  /**
   * Get router status
   */
  getStatus(): {
    isRunning: boolean;
    config: SimpleRouterConfig;
    uptime?: number;
    providerCount: number;
  } {
    return {
      isRunning: this.isRunning,
      config: this.config,
      uptime: this.server ? Date.now() - this.server._startTime : undefined,
      providerCount: this.providerRegistry.getAllProviders().length,
    };
  }
}
