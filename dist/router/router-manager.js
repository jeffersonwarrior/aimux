"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.RouterManager = void 0;
const events_1 = require("events");
const logger_1 = require("../utils/logger");
const simple_router_1 = require("./simple-router");
class RouterManager extends events_1.EventEmitter {
    router;
    providerRegistry;
    routingEngine;
    config;
    isManaged = false;
    startTime;
    metrics;
    restartAttempts = 0;
    restartTimer;
    metricsTimer;
    errors = [];
    constructor(providerRegistry, routingEngine, config = {}) {
        super();
        this.providerRegistry = providerRegistry;
        this.routingEngine = routingEngine;
        this.config = {
            autoRestart: true,
            restartDelay: 5000, // 5 seconds
            maxRestartAttempts: 10,
            enableMetrics: true,
            metricsInterval: 30000, // 30 seconds
            ...config,
        };
        this.metrics = {
            uptime: 0,
            requestsHandled: 0,
            requestsSuccessful: 0,
            requestsFailed: 0,
            averageResponseTime: 0,
            providerUsage: {},
        };
    }
    /**
     * Initialize the router manager
     */
    async initialize() {
        try {
            logger_1.logger.info('Initializing router manager...');
            // Create the router instance
            this.router = new simple_router_1.SimpleRouter(this.providerRegistry, this.routingEngine, this.config.router);
            // Set up error handling
            this.setupErrorHandling();
            // Set up metrics collection if enabled
            if (this.config.enableMetrics) {
                this.startMetricsCollection();
            }
            logger_1.logger.info('Router manager initialized successfully');
        }
        catch (error) {
            logger_1.logger.error('Failed to initialize router manager:', error);
            throw error;
        }
    }
    /**
     * Start the router
     */
    async start() {
        if (!this.router) {
            throw new Error('Router manager not initialized. Call initialize() first.');
        }
        if (this.isRunning()) {
            logger_1.logger.warn('Router is already running');
            return;
        }
        try {
            logger_1.logger.info('Starting router...');
            await this.router.start();
            this.startTime = new Date();
            this.isManaged = true;
            this.restartAttempts = 0;
            this.emit('router:started', {
                startTime: this.startTime,
                config: this.router?.getStatus().config,
            });
            logger_1.logger.info('Router started successfully');
        }
        catch (error) {
            logger_1.logger.error('Failed to start router:', error);
            this.recordError('Failed to start router', error instanceof Error ? error.message : 'Unknown error');
            if (this.config.autoRestart) {
                this.scheduleRestart();
            }
            throw error;
        }
    }
    /**
     * Stop the router
     */
    async stop() {
        if (!this.isRunning()) {
            logger_1.logger.warn('Router is not running');
            return;
        }
        try {
            logger_1.logger.info('Stopping router...');
            // Cancel any pending restart
            if (this.restartTimer) {
                clearTimeout(this.restartTimer);
                this.restartTimer = undefined;
            }
            // Stop metrics collection
            if (this.metricsTimer) {
                clearInterval(this.metricsTimer);
                this.metricsTimer = undefined;
            }
            // Stop the router
            if (this.router) {
                await this.router.stop();
            }
            this.isManaged = false;
            this.startTime = undefined;
            this.emit('router:stopped', {
                stopTime: new Date(),
                finalMetrics: this.metrics,
            });
            logger_1.logger.info('Router stopped successfully');
        }
        catch (error) {
            logger_1.logger.error('Failed to stop router:', error);
            throw error;
        }
    }
    /**
     * Restart the router
     */
    async restart() {
        logger_1.logger.info('Restarting router...');
        if (this.isRunning()) {
            await this.stop();
        }
        await new Promise(resolve => setTimeout(resolve, 1000)); // Brief pause
        await this.start();
        this.emit('router:restarted', {
            restartTime: new Date(),
            attempts: this.restartAttempts,
        });
    }
    /**
     * Add or update a provider configuration
     */
    async updateProvider(providerConfig) {
        try {
            logger_1.logger.info(`Updating provider configuration: ${providerConfig.name}`);
            // Update provider in registry
            const provider = this.providerRegistry.getProvider(providerConfig.name);
            if (provider) {
                // Update existing provider
                provider.config = providerConfig;
                await provider.authenticate();
            }
            // If router is running, notify it of the update
            if (this.isRunning()) {
                this.emit('router:provider:updated', { provider: providerConfig.name });
            }
            logger_1.logger.info(`Provider ${providerConfig.name} updated successfully`);
        }
        catch (error) {
            logger_1.logger.error(`Failed to update provider ${providerConfig.name}:`, error);
            this.recordError(`Failed to update provider ${providerConfig.name}`, error instanceof Error ? error.message : 'Unknown error', providerConfig.name);
            throw error;
        }
    }
    /**
     * Remove a provider
     */
    async removeProvider(providerName) {
        try {
            logger_1.logger.info(`Removing provider: ${providerName}`);
            // Remove from registry
            this.providerRegistry.unregister(providerName);
            // Clean up metrics for this provider
            delete this.metrics.providerUsage[providerName];
            // If router is running, notify it of the removal
            if (this.isRunning()) {
                this.emit('router:provider:removed', { provider: providerName });
            }
            logger_1.logger.info(`Provider ${providerName} removed successfully`);
        }
        catch (error) {
            logger_1.logger.error(`Failed to remove provider ${providerName}:`, error);
            throw error;
        }
    }
    /**
     * Get router status
     */
    getStatus() {
        const routerStatus = this.router?.getStatus();
        return {
            isRunning: this.isRunning(),
            startTime: this.startTime,
            uptime: this.startTime ? Date.now() - this.startTime.getTime() : 0,
            config: routerStatus?.config || {},
            metrics: this.metrics,
            providerCount: routerStatus?.providerCount || 0,
            lastRestart: this.restartTimer ? new Date() : undefined,
            restartAttempts: this.restartAttempts,
            errors: [...this.errors],
        };
    }
    /**
     * Get detailed metrics
     */
    getMetrics() {
        const uptime = this.startTime ? Date.now() - this.startTime.getTime() : 0;
        return {
            ...this.metrics,
            uptime,
            averageResponseTime: this.metrics.requestsHandled > 0
                ? this.metrics.averageResponseTime / this.metrics.requestsHandled
                : 0,
        };
    }
    /**
     * Check if router is running
     */
    isRunning() {
        return (this.isManaged && this.router?.getStatus().isRunning) || false;
    }
    /**
     * Perform health check
     */
    async healthCheck() {
        const results = {
            router: this.isRunning(),
            providers: {},
            overall: false,
        };
        // Check all providers
        const providers = this.providerRegistry.getAllProviders();
        for (const provider of providers) {
            try {
                const health = await provider.healthCheck();
                results.providers[provider.name] = health;
            }
            catch (error) {
                results.providers[provider.name] = {
                    status: 'unhealthy',
                    error: error instanceof Error ? error.message : 'Unknown error',
                };
            }
        }
        // Overall health is true if router is running and at least one provider is healthy
        results.overall =
            results.router && Object.values(results.providers).some((p) => p.status === 'healthy');
        return results;
    }
    /**
     * Set up error handling
     */
    setupErrorHandling() {
        // Listen to router events
        this.on('router:error', error => {
            this.recordError('Router error', error.message || 'Unknown error');
            if (this.config.autoRestart && this.isRunning()) {
                this.scheduleRestart();
            }
        });
        // Listen to provider events (if registry supports events)
        if (typeof this.providerRegistry.on === 'function') {
            this.providerRegistry.on('provider:error', (data) => {
                this.recordError('Provider error', data.error, data.provider);
            });
        }
        // Listen to routing engine events (if engine supports events)
        if (typeof this.routingEngine.on === 'function') {
            this.routingEngine.on('routing:error', (data) => {
                this.recordError('Routing error', data.error, data.provider);
            });
        }
    }
    /**
     * Schedule automatic restart
     */
    scheduleRestart() {
        if (this.restartAttempts >= (this.config.maxRestartAttempts || 10)) {
            logger_1.logger.error('Max restart attempts reached. Stopping automatic restarts.');
            this.emit('router:max-restart-attempts', { attempts: this.restartAttempts });
            return;
        }
        this.restartAttempts++;
        const delay = this.config.restartDelay || 5000;
        logger_1.logger.info(`Scheduling router restart attempt ${this.restartAttempts}/${this.config.maxRestartAttempts} in ${delay}ms`);
        this.restartTimer = setTimeout(async () => {
            try {
                await this.restart();
            }
            catch (error) {
                logger_1.logger.error('Automatic restart failed:', error);
                // Schedule another attempt
                if (this.config.autoRestart) {
                    this.scheduleRestart();
                }
            }
        }, delay);
    }
    /**
     * Start metrics collection
     */
    startMetricsCollection() {
        this.metricsTimer = setInterval(() => {
            this.updateMetrics();
        }, this.config.metricsInterval || 30000);
        // Initial metrics update
        this.updateMetrics();
    }
    /**
     * Update metrics
     */
    updateMetrics() {
        if (!this.isRunning()) {
            return;
        }
        // Update uptime
        if (this.startTime) {
            this.metrics.uptime = Date.now() - this.startTime.getTime();
        }
        // Update provider usage metrics
        const providers = this.providerRegistry.getAllProviders();
        for (const provider of providers) {
            if (!this.metrics.providerUsage[provider.name]) {
                this.metrics.providerUsage[provider.name] = {
                    requests: 0,
                    responseTime: 0,
                    errorRate: 0,
                };
            }
        }
        this.emit('router:metrics-updated', this.metrics);
    }
    /**
     * Record an error
     */
    recordError(message, details, provider) {
        const error = {
            timestamp: new Date(),
            message: `${message}: ${details}`,
            provider,
        };
        this.errors.push(error);
        // Keep only last 100 errors
        if (this.errors.length > 100) {
            this.errors = this.errors.slice(-100);
        }
        this.metrics.requestsFailed++;
        this.emit('router:error', error);
        logger_1.logger.error('Router error recorded:', error);
    }
    /**
     * Update request metrics (called by SimpleRouter)
     */
    updateRequestMetrics(providerName, success, responseTime) {
        this.metrics.requestsHandled++;
        if (success) {
            this.metrics.requestsSuccessful++;
        }
        else {
            this.metrics.requestsFailed++;
        }
        // Update provider-specific metrics
        if (!this.metrics.providerUsage[providerName]) {
            this.metrics.providerUsage[providerName] = {
                requests: 0,
                responseTime: 0,
                errorRate: 0,
            };
        }
        const providerMetrics = this.metrics.providerUsage[providerName];
        providerMetrics.requests++;
        // Update average response time
        const totalResponseTime = providerMetrics.responseTime * (providerMetrics.requests - 1) + responseTime;
        providerMetrics.responseTime = totalResponseTime / providerMetrics.requests;
        // Update error rate
        const totalErrors = this.metrics.requestsFailed;
        const totalRequests = this.metrics.requestsHandled;
        providerMetrics.errorRate = totalRequests > 0 ? (totalErrors / totalRequests) * 100 : 0;
        // Update global average response time
        const globalTotalResponseTime = this.metrics.averageResponseTime * (this.metrics.requestsHandled - 1) + responseTime;
        this.metrics.averageResponseTime = globalTotalResponseTime / this.metrics.requestsHandled;
    }
    /**
     * Cleanup resources
     */
    async cleanup() {
        try {
            await this.stop();
            if (this.restartTimer) {
                clearTimeout(this.restartTimer);
            }
            if (this.metricsTimer) {
                clearInterval(this.metricsTimer);
            }
            this.removeAllListeners();
            logger_1.logger.info('Router manager cleaned up successfully');
        }
        catch (error) {
            logger_1.logger.error('Failed to cleanup router manager:', error);
            throw error;
        }
    }
}
exports.RouterManager = RouterManager;
//# sourceMappingURL=router-manager.js.map