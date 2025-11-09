#!/usr/bin/env node

/**
 * Z.AI HTTP Request Interceptor
 *
 * This proxy intercepts Claude Code requests to Z.AI and fixes model transformation:
 * - Claude Code sends: hf:zai-org/GLM-4.6
 * - Z.AI expects: GLM-4.6
 */

const http = require('http');
const https = require('https');
const url = require('url');

import { IncomingMessage, ServerResponse } from 'http';

interface ZAIInterceptorOptions {
  port?: number;
  debug?: boolean;
}

interface RequestData {
  model?: string;
  [key: string]: any;
}

class ZAIInterceptor {
  private port: number;
  private targetHost: string;
  private targetPort: number;
  private debug: boolean;

  constructor(options: ZAIInterceptorOptions = {}) {
    this.port = options.port || 8123;
    this.targetHost = 'api.z.ai';
    this.targetPort = 443;
    this.debug = options.debug || false;

    console.log(`[Z.AI INTERCEPTOR] Starting proxy on localhost:${this.port}`);
    console.log(`[Z.AI INTERCEPTOR] Intercepting requests to ${this.targetHost}`);
  }

  private log(message: string): void {
    if (this.debug) {
      console.log(`[Z.AI INTERCEPTOR] ${message}`);
    }
  }

  private transformRequest(requestBody: string): string {
    try {
      const data = JSON.parse(requestBody);

      // Transform Claude Code's model format back to Z.AI format
      if (data.model && data.model.startsWith('hf:zai-org/')) {
        const originalModel = data.model;
        data.model = originalModel.replace('hf:zai-org/', '');
        this.log(`Transformed model: ${originalModel} -> ${data.model}`);
      }

      return JSON.stringify(data);
    } catch (error) {
      this.log(`Error parsing request body: ${(error as Error).message}`);
      return requestBody;
    }
  }

  private createProxyRequest(req: IncomingMessage, res: ServerResponse, requestBody: string): void {
    // Map /anthropic/* to /api/anthropic/* for Z.AI
    let targetPath = req.url || '';
    if (targetPath.startsWith('/anthropic')) {
      targetPath = targetPath.replace('/anthropic', '/api/anthropic');
    }

    const targetUrl = `https://${this.targetHost}${targetPath}`;
    const parsedUrl = new url.URL(targetUrl);

    // Transform the request body to fix model name
    const transformedBody = this.transformRequest(requestBody);

    const options = {
      hostname: this.targetHost,
      port: this.targetPort,
      path: parsedUrl.pathname + parsedUrl.search,
      method: req.method,
      headers: {
        ...req.headers,
        host: this.targetHost,
        'content-length': Buffer.byteLength(transformedBody)
      }
    };

    this.log(`Proxying ${req.method} ${targetUrl}`);
    this.log(`Transformed body: ${transformedBody.substring(0, 200)}...`);

    const proxyReq = https.request(options, (proxyRes: any) => {
      this.log(`Response status: ${proxyRes.statusCode}`);

      // Copy response headers
      Object.keys(proxyRes.headers).forEach(key => {
        res.setHeader(key, proxyRes.headers[key]);
      });

      res.writeHead(proxyRes.statusCode || 200);

      // Pipe response body
      proxyRes.on('data', (chunk: any) => {
        res.write(chunk);
      });

      proxyRes.on('end', () => {
        res.end();
        this.log('Request completed successfully');
      });
    });

    proxyReq.on('error', (error: any) => {
      this.log(`Proxy request error: ${(error as Error).message}`);
      res.writeHead(502);
      res.end(JSON.stringify({ error: 'Bad Gateway', message: (error as Error).message }));
    });

    // Send the transformed request
    proxyReq.write(transformedBody);
    proxyReq.end();
  }

  start(): void {
    const server = http.createServer((req: any, res: any) => {
      let requestBody = '';

      req.on('data', (chunk: any) => {
        requestBody += chunk.toString();
      });

      req.on('end', () => {
        // Only intercept requests to Z.AI API
        const requestUrl = req.url || '';
        this.log(`Received ${req.method} ${requestUrl}`);
        this.log(`Request body: ${requestBody.substring(0, 200)}...`);
        console.log(`[Z.AI INTERCEPTOR] Request URL: ${requestUrl}`);
        if (requestUrl.includes('/messages') || requestUrl.includes('/anthropic')) {
          this.createProxyRequest(req, res, requestBody);
        } else {
          // Return 404 for non-API requests
          console.log(`[Z.AI INTERCEPTOR] Rejecting non-API request: ${requestUrl}`);
          res.writeHead(404);
          res.end('Not Found');
        }
      });
    });

    server.listen(this.port, () => {
      console.log(`[Z.AI INTERCEPTOR] 🚀 Proxy server running on http://localhost:${this.port}`);
      console.log(`[Z.AI INTERCEPTOR] 📡 Ready to intercept Z.AI requests`);
      console.log(`[Z.AI INTERCEPTOR] 🎯 Configure aimux to use: http://localhost:${this.port}/anthropic`);
    });

    server.on('error', (error: any) => {
      console.error(`[Z.AI INTERCEPTOR] Server error: ${(error as Error).message}`);
    });
  }
}

// Start the interceptor if this file is run directly
if (require.main === module) {
  const debug = process.argv.includes('--debug');
  const interceptor = new ZAIInterceptor({ debug });
  interceptor.start();

  // Handle graceful shutdown
  process.on('SIGINT', () => {
    console.log('\n[Z.AI INTERCEPTOR] Shutting down gracefully...');
    process.exit(0);
  });
}

module.exports = ZAIInterceptor;