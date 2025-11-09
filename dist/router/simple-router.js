"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.SimpleRouter = void 0;
const http_1 = require("http");
const axios_1 = require("axios");
const logger_1 = require("../utils/logger");
class SimpleRouter {
    server = null;
    isRunning = false;
    providerRegistry;
    routingEngine;
    config;
    healthCheckTimer;
    constructor(providerRegistry, routingEngine, config = {}) {
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
    async start() {
        if (this.isRunning) {
            logger_1.logger.warn('Router is already running');
            return;
        }
        try {
            await new Promise((resolve, reject) => {
                this.server = (0, http_1.createServer)((req, res) => {
                    this.handleRequest(req, res).catch(error => {
                        logger_1.logger.error('Request handling error:', error);
                        if (!res.headersSent) {
                            res.writeHead(500, { 'Content-Type': 'application/json' });
                            res.end(JSON.stringify({
                                error: 'Internal server error',
                                message: 'Failed to process request',
                            }));
                        }
                    });
                });
                this.server.on('error', reject);
                this.server.listen(this.config.port, this.config.host, () => {
                    logger_1.logger.info(`Router started on ${this.config.host}:${this.config.port}`);
                    this.isRunning = true;
                    resolve();
                });
            });
            // Start health check if enabled
            if (this.config.enableHealthCheck) {
                this.startHealthCheck();
            }
        }
        catch (error) {
            logger_1.logger.error('Failed to start router:', error);
            throw error;
        }
    }
    /**
     * Stop the HTTP proxy server
     */
    async stop() {
        if (!this.isRunning) {
            logger_1.logger.warn('Router is not running');
            return;
        }
        try {
            // Stop health check
            if (this.healthCheckTimer) {
                clearInterval(this.healthCheckTimer);
                this.healthCheckTimer = undefined;
            }
            // Stop server
            await new Promise((resolve, reject) => {
                if (!this.server) {
                    resolve();
                    return;
                }
                this.server.close((error) => {
                    if (error) {
                        reject(error);
                    }
                    else {
                        logger_1.logger.info('Router stopped');
                        this.isRunning = false;
                        resolve();
                    }
                });
            });
        }
        catch (error) {
            logger_1.logger.error('Failed to stop router:', error);
            throw error;
        }
    }
    /**
     * Handle incoming HTTP requests
     */
    async handleRequest(req, res) {
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
                res.end(JSON.stringify({
                    error: 'Service unavailable',
                    message: 'No suitable provider available for this request',
                }));
                return;
            }
            // Log routing decision if enabled
            if (this.config.enableLogging) {
                logger_1.logger.info(`Routing request to ${selectedProvider.name}:`, {
                    requestType: requestAnalysis.metadata?.requestType,
                    model: requestAnalysis.model,
                    hasVision: requestAnalysis.messages.some(m => Array.isArray(m.content) && m.content.some(c => c.type === 'image_url')),
                    hasTools: !!requestAnalysis.tools,
                });
            }
            // Forward request to selected provider
            const response = await this.forwardRequest(selectedProvider, requestAnalysis, req);
            // Normalize response
            const normalizedResponse = this.normalizeResponse(response, selectedProvider.name, Date.now() - startTime);
            // Send response back to client
            res.writeHead(normalizedResponse.statusCode || 200, normalizedResponse.headers);
            res.end(normalizedResponse.data);
        }
        catch (error) {
            const duration = Date.now() - startTime;
            logger_1.logger.error(`Request failed after ${duration}ms:`, error);
            if (!res.headersSent) {
                const statusCode = error instanceof axios_1.AxiosError ? error.response?.status || 500 : 500;
                res.writeHead(statusCode, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({
                    error: 'Request failed',
                    message: error instanceof Error ? error.message : 'Unknown error',
                    duration,
                }));
            }
        }
    }
    /**
     * Parse request body with size limit
     */
    async parseRequestBody(req) {
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
                }
                catch (error) {
                    reject(new Error('Invalid JSON in request body'));
                }
            });
            req.on('error', reject);
        });
    }
    /**
     * Analyze request to determine routing requirements
     */
    analyzeRequest(body) {
        const request = {
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
            request.metadata.requestType = 'thinking';
        }
        // Check for vision content
        const hasImage = request.messages.some(message => Array.isArray(message.content) &&
            message.content.some(content => content.type === 'image_url'));
        if (hasImage) {
            request.metadata.requestType = 'vision';
        }
        // Check for tools
        if (request.tools && request.tools.length > 0) {
            request.metadata.requestType = 'tools';
        }
        return request;
    }
    /**
     * Forward request to selected provider
     */
    async forwardRequest(provider, request, originalReq) {
        try {
            // Get provider's HTTP client
            const httpClient = provider.getHttpClient?.() || provider.httpClient;
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
                ...(provider.getDefaultHeaders?.() || {}),
            };
            // Make the request
            const config = {
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
        }
        catch (error) {
            logger_1.logger.error(`Failed to forward request to ${provider.name}:`, error);
            throw error;
        }
    }
    /**
     * Build target URL for provider
     */
    buildTargetUrl(provider, _request) {
        const baseUrl = provider.config?.baseUrl || provider.baseUrl;
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
    normalizeResponse(response, providerName, duration) {
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
    startHealthCheck() {
        this.healthCheckTimer = setInterval(async () => {
            try {
                await this.performHealthCheck();
            }
            catch (error) {
                logger_1.logger.error('Health check failed:', error);
            }
        }, this.config.healthCheckInterval);
    }
    /**
     * Perform health check on all providers
     */
    async performHealthCheck() {
        const providers = this.providerRegistry.getAllProviders();
        for (const provider of providers) {
            try {
                const health = await provider.healthCheck();
                logger_1.logger.debug(`Health check for ${provider.name}:`, health);
            }
            catch (error) {
                logger_1.logger.warn(`Health check failed for ${provider.name}:`, error);
            }
        }
    }
    /**
     * Get router status
     */
    getStatus() {
        return {
            isRunning: this.isRunning,
            config: this.config,
            uptime: this.server ? Date.now() - this.server._startTime : undefined,
            providerCount: this.providerRegistry.getAllProviders().length,
        };
    }
}
exports.SimpleRouter = SimpleRouter;
//# sourceMappingURL=simple-router.js.map