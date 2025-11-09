"use strict";
/**
 * Router module exports and interfaces
 *
 * This module provides the simplified HTTP proxy router implementation for MVP.
 * Future versions will replace this with the native C++ router implementation.
 */
var __createBinding = (this && this.__createBinding) || (Object.create ? (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    var desc = Object.getOwnPropertyDescriptor(m, k);
    if (!desc || ("get" in desc ? !m.__esModule : desc.writable || desc.configurable)) {
      desc = { enumerable: true, get: function() { return m[k]; } };
    }
    Object.defineProperty(o, k2, desc);
}) : (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    o[k2] = m[k];
}));
var __setModuleDefault = (this && this.__setModuleDefault) || (Object.create ? (function(o, v) {
    Object.defineProperty(o, "default", { enumerable: true, value: v });
}) : function(o, v) {
    o["default"] = v;
});
var __importStar = (this && this.__importStar) || (function () {
    var ownKeys = function(o) {
        ownKeys = Object.getOwnPropertyNames || function (o) {
            var ar = [];
            for (var k in o) if (Object.prototype.hasOwnProperty.call(o, k)) ar[ar.length] = k;
            return ar;
        };
        return ownKeys(o);
    };
    return function (mod) {
        if (mod && mod.__esModule) return mod;
        var result = {};
        if (mod != null) for (var k = ownKeys(mod), i = 0; i < k.length; i++) if (k[i] !== "default") __createBinding(result, mod, k[i]);
        __setModuleDefault(result, mod);
        return result;
    };
})();
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.ROUTER_EXPORTS = exports.MODULE_INFO = exports.VERSION = exports.RouterError = exports.RouterErrorTypes = exports.RouterEvents = exports.ROUTER_CONSTANTS = exports.RouterUtils = exports.DEFAULT_MANAGER_CONFIG = exports.DEFAULT_ROUTER_CONFIG = exports.RouterManager = exports.SimpleRouter = void 0;
exports.createRouter = createRouter;
exports.createRouterManager = createRouterManager;
exports.createRouterError = createRouterError;
const net_1 = require("net");
const axios_1 = __importDefault(require("axios"));
const os = __importStar(require("os"));
const process_1 = __importDefault(require("process"));
// Import main classes with explicit types
const simple_router_1 = require("./simple-router");
Object.defineProperty(exports, "SimpleRouter", { enumerable: true, get: function () { return simple_router_1.SimpleRouter; } });
const router_manager_1 = require("./router-manager");
Object.defineProperty(exports, "RouterManager", { enumerable: true, get: function () { return router_manager_1.RouterManager; } });
// Factory function for creating router instances
function createRouter(providerRegistry, routingEngine, config) {
    return new simple_router_1.SimpleRouter(providerRegistry, routingEngine, config);
}
// Factory function for creating router manager
function createRouterManager(providerRegistry, routingEngine, config) {
    return new router_manager_1.RouterManager(providerRegistry, routingEngine, config);
}
// Default router configuration
exports.DEFAULT_ROUTER_CONFIG = {
    port: 8080,
    host: 'localhost',
    enableLogging: true,
    maxRequestSize: 10 * 1024 * 1024, // 10MB
    timeout: 30000, // 30 seconds
    enableHealthCheck: true,
    healthCheckInterval: 60000, // 1 minute
};
// Default router manager configuration
exports.DEFAULT_MANAGER_CONFIG = {
    router: exports.DEFAULT_ROUTER_CONFIG,
    autoRestart: true,
    restartDelay: 5000, // 5 seconds
    maxRestartAttempts: 10,
    enableMetrics: true,
    metricsInterval: 30000, // 30 seconds
};
// Router utility functions
class RouterUtils {
    /**
     * Validate router port is available
     */
    static async validatePort(port, host = 'localhost') {
        try {
            return new Promise(resolve => {
                const server = (0, net_1.createServer)();
                server.listen(port, host, () => {
                    server.close(() => resolve(true));
                });
                server.on('error', () => resolve(false));
            });
        }
        catch (error) {
            return false;
        }
    }
    /**
     * Find an available port starting from the given port
     */
    static async findAvailablePort(startPort, host = 'localhost') {
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
    static async testConnectivity(port, host = 'localhost') {
        try {
            const response = await axios_1.default.get(`http://${host}:${port}/health`, {
                timeout: 5000,
                validateStatus: () => true, // Accept any status code
            });
            return response.status < 500; // Consider 2xx, 3xx, 4xx as successful connectivity
        }
        catch (error) {
            return false;
        }
    }
    /**
     * Get router system information
     */
    static getSystemInfo() {
        const totalMemory = os.totalmem();
        const freeMemory = os.freemem();
        return {
            nodeVersion: process_1.default.version,
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
    static validateConfig(config) {
        const errors = [];
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
exports.RouterUtils = RouterUtils;
// Router constants
exports.ROUTER_CONSTANTS = {
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
var RouterEvents;
(function (RouterEvents) {
    RouterEvents["ROUTER_STARTED"] = "router:started";
    RouterEvents["ROUTER_STOPPED"] = "router:stopped";
    RouterEvents["ROUTER_RESTARTED"] = "router:restarted";
    RouterEvents["ROUTER_ERROR"] = "router:error";
    RouterEvents["ROUTER_METRICS_UPDATED"] = "router:metrics-updated";
    RouterEvents["ROUTER_MAX_RESTART_ATTEMPTS"] = "router:max-restart-attempts";
    RouterEvents["ROUTER_PROVIDER_UPDATED"] = "router:provider:updated";
    RouterEvents["ROUTER_PROVIDER_REMOVED"] = "router:provider:removed";
})(RouterEvents || (exports.RouterEvents = RouterEvents = {}));
// Router error types
var RouterErrorTypes;
(function (RouterErrorTypes) {
    RouterErrorTypes["STARTUP_FAILED"] = "startup_failed";
    RouterErrorTypes["CONFIGURATION_ERROR"] = "configuration_error";
    RouterErrorTypes["NETWORK_ERROR"] = "network_error";
    RouterErrorTypes["PROVIDER_ERROR"] = "provider_error";
    RouterErrorTypes["TIMEOUT_ERROR"] = "timeout_error";
    RouterErrorTypes["VALIDATION_ERROR"] = "validation_error";
})(RouterErrorTypes || (exports.RouterErrorTypes = RouterErrorTypes = {}));
// Custom error class for router-specific errors
class RouterError extends Error {
    type;
    provider;
    originalError;
    constructor(message, type = RouterErrorTypes.NETWORK_ERROR, provider, originalError) {
        super(message);
        this.type = type;
        this.provider = provider;
        this.originalError = originalError;
        this.name = 'RouterError';
        // Ensure the stack trace shows the original error location
        if (originalError && originalError.stack) {
            this.stack = `RouterError: ${message}\nCaused by: ${originalError.stack}`;
        }
    }
}
exports.RouterError = RouterError;
// Utility for creating typed router errors
function createRouterError(message, type = RouterErrorTypes.NETWORK_ERROR, provider, originalError) {
    return new RouterError(message, type, provider, originalError);
}
/**
 * Router module version information
 */
exports.VERSION = '1.0.0-mvp';
/**
 * Module metadata
 */
exports.MODULE_INFO = {
    name: 'aimux-router',
    version: exports.VERSION,
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
exports.ROUTER_EXPORTS = {
    // Main classes (imported to avoid namespace issues)
    SimpleRouter: simple_router_1.SimpleRouter,
    RouterManager: router_manager_1.RouterManager,
    // Factory functions
    createRouter,
    createRouterManager,
    // Utilities
    RouterUtils,
    RouterError,
    createRouterError,
    // Constants and enums
    DEFAULT_ROUTER_CONFIG: exports.DEFAULT_ROUTER_CONFIG,
    DEFAULT_MANAGER_CONFIG: exports.DEFAULT_MANAGER_CONFIG,
    ROUTER_CONSTANTS: exports.ROUTER_CONSTANTS,
    RouterEvents,
    RouterErrorTypes,
    VERSION: exports.VERSION,
    MODULE_INFO: exports.MODULE_INFO,
};
//# sourceMappingURL=index.js.map