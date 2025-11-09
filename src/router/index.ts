/**
 * Router module exports and interfaces
 *
 * This module provides the simplified HTTP proxy router implementation for MVP.
 * Future versions will replace this with the native C++ router implementation.
 */

import { createServer } from 'net';
import axios from 'axios';
import * as os from 'os';
import process from 'process';

// Import main classes with explicit types
import { SimpleRouter, type SimpleRouterConfig } from './simple-router';
import {
  RouterManager,
  type RouterManagerConfig,
  type RouterMetrics,
  type RouterStatus,
} from './router-manager';

// Main router classes
export { SimpleRouter, RouterManager };

// Core interfaces and types
export type { SimpleRouterConfig, RouterManagerConfig, RouterMetrics, RouterStatus };

// Factory function for creating router instances
export function createRouter(
  providerRegistry: any,
  routingEngine: any,
  config?: Partial<SimpleRouterConfig>
): SimpleRouter {
  return new SimpleRouter(providerRegistry, routingEngine, config);
}

// Factory function for creating router manager
export function createRouterManager(
  providerRegistry: any,
  routingEngine: any,
  config?: RouterManagerConfig
): RouterManager {
  return new RouterManager(providerRegistry, routingEngine, config);
}

// Default router configuration
export const DEFAULT_ROUTER_CONFIG: SimpleRouterConfig = {
  port: 8080,
  host: 'localhost',
  enableLogging: true,
  maxRequestSize: 10 * 1024 * 1024, // 10MB
  timeout: 30000, // 30 seconds
  enableHealthCheck: true,
  healthCheckInterval: 60000, // 1 minute
};

// Default router manager configuration
export const DEFAULT_MANAGER_CONFIG: RouterManagerConfig = {
  router: DEFAULT_ROUTER_CONFIG,
  autoRestart: true,
  restartDelay: 5000, // 5 seconds
  maxRestartAttempts: 10,
  enableMetrics: true,
  metricsInterval: 30000, // 30 seconds
};

// Router utility functions
export class RouterUtils {
  /**
   * Validate router port is available
   */
  static async validatePort(port: number, host = 'localhost'): Promise<boolean> {
    try {
      return new Promise(resolve => {
        const server = createServer();

        server.listen(port, host, () => {
          server.close(() => resolve(true));
        });

        server.on('error', () => resolve(false));
      });
    } catch (error) {
      return false;
    }
  }

  /**
   * Find an available port starting from the given port
   */
  static async findAvailablePort(startPort: number, host = 'localhost'): Promise<number> {
    let port = startPort;
    const maxPort = startPort + 100; // Try up to 100 ports

    while (port <= maxPort) {
      if (await this.validatePort(port, host)) {
        return port;
      }
      port++;
    }

    throw new Error(`No available ports found between ${startPort} and ${maxPort}`);
  }

  /**
   * Test router connectivity
   */
  static async testConnectivity(port: number, host = 'localhost'): Promise<boolean> {
    try {
      const response = await axios.get(`http://${host}:${port}/health`, {
        timeout: 5000,
        validateStatus: () => true, // Accept any status code
      });
      return response.status < 500; // Consider 2xx, 3xx, 4xx as successful connectivity
    } catch (error) {
      return false;
    }
  }

  /**
   * Get router system information
   */
  static getSystemInfo(): {
    nodeVersion: string;
    platform: string;
    arch: string;
    memory: {
      total: number;
      free: number;
      used: number;
    };
  } {
    const totalMemory = os.totalmem();
    const freeMemory = os.freemem();

    return {
      nodeVersion: process.version,
      platform: os.platform(),
      arch: os.arch(),
      memory: {
        total: totalMemory,
        free: freeMemory,
        used: totalMemory - freeMemory,
      },
    };
  }

  /**
   * Validate router configuration
   */
  static validateConfig(config: Partial<SimpleRouterConfig>): {
    isValid: boolean;
    errors: string[];
  } {
    const errors: string[] = [];

    if (config.port !== undefined) {
      if (!Number.isInteger(config.port) || config.port < 1 || config.port > 65535) {
        errors.push('Port must be an integer between 1 and 65535');
      }
    }

    if (config.host !== undefined && typeof config.host !== 'string') {
      errors.push('Host must be a string');
    }

    if (config.maxRequestSize !== undefined) {
      if (!Number.isInteger(config.maxRequestSize) || config.maxRequestSize < 0) {
        errors.push('Max request size must be a positive integer');
      }
    }

    if (config.timeout !== undefined) {
      if (!Number.isInteger(config.timeout) || config.timeout < 0) {
        errors.push('Timeout must be a positive integer');
      }
    }

    if (config.healthCheckInterval !== undefined) {
      if (!Number.isInteger(config.healthCheckInterval) || config.healthCheckInterval < 1000) {
        errors.push('Health check interval must be at least 1000ms');
      }
    }

    return {
      isValid: errors.length === 0,
      errors,
    };
  }
}

// Router constants
export const ROUTER_CONSTANTS = {
  DEFAULT_PORT: 8080,
  DEFAULT_HOST: 'localhost',
  DEFAULT_TIMEOUT: 30000,
  DEFAULT_MAX_REQUEST_SIZE: 10 * 1024 * 1024, // 10MB
  DEFAULT_HEALTH_CHECK_INTERVAL: 60000, // 1 minute
  MAX_RESTART_ATTEMPTS: 10,
  RESTART_DELAY: 5000, // 5 seconds
  METRICS_INTERVAL: 30000, // 30 seconds
  MAX_ERRORS_STORED: 100,
};

// Router events enum
export enum RouterEvents {
  ROUTER_STARTED = 'router:started',
  ROUTER_STOPPED = 'router:stopped',
  ROUTER_RESTARTED = 'router:restarted',
  ROUTER_ERROR = 'router:error',
  ROUTER_METRICS_UPDATED = 'router:metrics-updated',
  ROUTER_MAX_RESTART_ATTEMPTS = 'router:max-restart-attempts',
  ROUTER_PROVIDER_UPDATED = 'router:provider:updated',
  ROUTER_PROVIDER_REMOVED = 'router:provider:removed',
}

// Router error types
export enum RouterErrorTypes {
  STARTUP_FAILED = 'startup_failed',
  CONFIGURATION_ERROR = 'configuration_error',
  NETWORK_ERROR = 'network_error',
  PROVIDER_ERROR = 'provider_error',
  TIMEOUT_ERROR = 'timeout_error',
  VALIDATION_ERROR = 'validation_error',
}

// Custom error class for router-specific errors
export class RouterError extends Error {
  constructor(
    message: string,
    public type: RouterErrorTypes = RouterErrorTypes.NETWORK_ERROR,
    public provider?: string,
    public originalError?: Error
  ) {
    super(message);
    this.name = 'RouterError';

    // Ensure the stack trace shows the original error location
    if (originalError && originalError.stack) {
      this.stack = `RouterError: ${message}\nCaused by: ${originalError.stack}`;
    }
  }
}

// Utility for creating typed router errors
export function createRouterError(
  message: string,
  type: RouterErrorTypes = RouterErrorTypes.NETWORK_ERROR,
  provider?: string,
  originalError?: Error
): RouterError {
  return new RouterError(message, type, provider, originalError);
}

// Re-export for backward compatibility and easier access
export type { ProviderRegistry } from '../providers/provider-registry';
export type { RoutingEngine } from '../core/routing-engine';
export type { BaseProvider } from '../providers/base-provider';
export type {
  AIRequest,
  AIResponse,
  ProviderConfig,
  ProviderCapabilities,
  ProviderHealth,
  ProviderError,
} from '../providers/types';

/**
 * Router module version information
 */
export const VERSION = '1.0.0-mvp';

/**
 * Module metadata
 */
export const MODULE_INFO = {
  name: 'aimux-router',
  version: VERSION,
  description: 'Simplified HTTP proxy router for Aimux MVP',
  author: 'Aimux Contributors',
  license: 'MIT',
  repository: 'https://github.com/parnexcodes/aimux',
  type: 'typescript-proxy',
  capabilities: [
    'http-proxy',
    'provider-routing',
    'request-analysis',
    'response-normalization',
    'health-checking',
    'metrics-collection',
    'auto-restart',
    'error-handling',
  ],
  future: [
    'native-c++-implementation',
    'advanced-load-balancing',
    'connection-pooling',
    'request-caching',
    'rate-limiting',
    'advanced-analytics',
  ],
};

// Module exports summary
export const ROUTER_EXPORTS = {
  // Main classes (imported to avoid namespace issues)
  SimpleRouter,
  RouterManager,

  // Factory functions
  createRouter,
  createRouterManager,

  // Utilities
  RouterUtils,
  RouterError,
  createRouterError,

  // Constants and enums
  DEFAULT_ROUTER_CONFIG,
  DEFAULT_MANAGER_CONFIG,
  ROUTER_CONSTANTS,
  RouterEvents,
  RouterErrorTypes,
  VERSION,
  MODULE_INFO,
};
