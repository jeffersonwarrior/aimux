"use strict";
/**
 * Demo file showing how to use the routing engine and failover manager
 * This is for documentation purposes and not part of the main implementation
 */
Object.defineProperty(exports, "__esModule", { value: true });
exports.AIRequestProcessor = exports.exampleRequests = void 0;
exports.createRoutingSystem = createRoutingSystem;
exports.processAIRequest = processAIRequest;
exports.demonstrateMonitoring = demonstrateMonitoring;
const routing_engine_1 = require("./routing-engine");
const failover_manager_1 = require("./failover-manager");
/**
 * Example of setting up the routing system with configuration
 */
function createRoutingSystem(registry) {
    // Routing engine configuration
    const routingConfig = {
        enablePerformanceRouting: true,
        enableHealthRouting: true,
        enableFallback: true,
        maxProviderAttempts: 3,
        capabilityPreferences: {
            thinking: ['minimax-m2', 'synthetic-new'], // Prefer thinking-capable providers
            vision: ['minimax-m2', 'z-ai'], // Prefer vision-capable providers
            tools: ['minimax-m2', 'synthetic-new'], // Prefer providers with tool support
            streaming: ['minimax-m2'], // Prefer streaming-capable provider
        },
        customRules: [
            {
                id: 'high-priority-rule',
                name: 'High Priority Requests to Fastest Provider',
                condition: (request) => {
                    return request.metadata?.priority === 'high';
                },
                providerSelector: (providers) => {
                    // Return the provider with the lowest average response time
                    // This would use performance metrics in a real implementation
                    return providers[0] || null;
                },
                priority: 1,
                enabled: true,
            },
            {
                id: 'thinking-rule',
                name: 'Thinking Requests to Specialized Provider',
                condition: (request) => {
                    const text = JSON.stringify(request.messages).toLowerCase();
                    return text.includes('think step by step') || text.includes('analyze this');
                },
                providerSelector: (providers) => {
                    return providers.find(p => p.hasCapability('thinking')) || null;
                },
                priority: 2,
                enabled: true,
            },
        ],
    };
    // Failover manager configuration
    const failoverConfig = {
        maxRetriesPerProvider: 2,
        maxTotalRetries: 6,
        initialRetryDelay: 1000,
        maxRetryDelay: 8000,
        backoffMultiplier: 2,
        enableJitter: true,
        enableCircuitBreaker: true,
        circuitBreakerThreshold: 3,
        circuitBreakerTimeout: 30000, // 30 seconds
        enableIntelligentFailover: true,
    };
    // Create routing engine and failover manager
    const routingEngine = new routing_engine_1.RoutingEngine(registry, routingConfig);
    const failoverManager = new failover_manager_1.FailoverManager(registry, routingEngine, failoverConfig);
    return { routingEngine, failoverManager };
}
/**
 * Example of processing an AI request using the routing system
 */
async function processAIRequest(request, routingEngine, failoverManager) {
    try {
        // Step 1: Select best provider using routing engine
        console.log('🔍 Selecting provider for request...');
        const selectedProvider = routingEngine.selectProvider(request);
        if (!selectedProvider) {
            throw new Error('No suitable provider found for this request');
        }
        console.log(`✅ Selected provider: ${selectedProvider.name} (${selectedProvider.id})`);
        // Step 2: Make the request
        console.log(`🚀 Making request to ${selectedProvider.name}...`);
        const startTime = Date.now();
        try {
            const response = await selectedProvider.makeRequest(request);
            const responseTime = Date.now() - startTime;
            // Step 3: Update performance metrics on success
            routingEngine.updateProviderPerformance(selectedProvider.id, responseTime, true);
            console.log(`✅ Request successful in ${responseTime}ms`);
            // Mark response with routing metadata
            if (response.metadata) {
                response.metadata.routing_decision = `primary:${selectedProvider.id}`;
            }
            return response;
        }
        catch (error) {
            const responseTime = Date.now() - startTime;
            // Step 4: Update performance metrics on failure
            routingEngine.updateProviderPerformance(selectedProvider.id, responseTime, false, error instanceof Error ? error.message : 'Unknown error');
            console.log(`❌ Request to ${selectedProvider.name} failed:`, error);
            // Step 5: Handle failover
            console.log('🔄 Initiating failover...');
            const failedProviders = [selectedProvider.id];
            const fallbackResponse = await failoverManager.handleFailover(request, failedProviders, error instanceof Error ? error : new Error(String(error)));
            console.log('✅ Fallback successful');
            return fallbackResponse;
        }
    }
    catch (error) {
        console.error('💥 Complete request processing failed:', error);
        throw error;
    }
}
/**
 * Example request scenarios for demonstration
 */
exports.exampleRequests = {
    thinkingRequest: {
        model: 'claude-3-sonnet-20241022',
        messages: [
            {
                role: 'user',
                content: 'Please think step by step and analyze this complex business problem: We need to optimize our supply chain while reducing costs and maintaining quality.',
            },
        ],
        max_tokens: 2000,
        temperature: 0.7,
    },
    visionRequest: {
        model: 'claude-3-sonnet-20241022',
        messages: [
            {
                role: 'user',
                content: [
                    {
                        type: 'text',
                        text: 'What do you see in this image?',
                    },
                    {
                        type: 'image_url',
                        image_url: {
                            url: 'data:image/jpeg;base64,/9j/4AAQSkZJRgABA...',
                            detail: 'high',
                        },
                    },
                ],
            },
        ],
        max_tokens: 500,
    },
    toolsRequest: {
        model: 'claude-3-sonnet-20241022',
        messages: [
            {
                role: 'user',
                content: 'What is the weather like in New York?',
            },
        ],
        tools: [
            {
                type: 'function',
                function: {
                    name: 'get_weather',
                    description: 'Get current weather information for a location',
                    parameters: {
                        type: 'object',
                        properties: {
                            location: {
                                type: 'string',
                                description: 'The city and state, e.g. San Francisco, CA',
                            },
                        },
                        required: ['location'],
                    },
                },
            },
        ],
        tool_choice: 'auto',
    },
    hybridRequest: {
        model: 'claude-3-sonnet-20241022',
        messages: [
            {
                role: 'user',
                content: [
                    {
                        type: 'text',
                        text: 'Think step by step to analyze this image and then use the calculator tool to compute the total cost.',
                    },
                    {
                        type: 'image_url',
                        image_url: {
                            url: 'data:image/jpeg;base64,/9j/4AAQSkZJRgABA...',
                        },
                    },
                ],
            },
        ],
        tools: [
            {
                type: 'function',
                function: {
                    name: 'calculate_total',
                    description: 'Calculate total cost from items',
                    parameters: {
                        type: 'object',
                        properties: {
                            items: {
                                type: 'array',
                                items: {
                                    type: 'object',
                                    properties: {
                                        name: { type: 'string' },
                                        price: { type: 'number' },
                                        quantity: { type: 'integer' },
                                    },
                                },
                            },
                        },
                        required: ['items'],
                    },
                },
            },
        ],
        max_tokens: 1000,
        temperature: 0.5,
    },
};
/**
 * Demonstration of routing statistics and monitoring
 */
function demonstrateMonitoring(routingEngine, failoverManager) {
    console.log('\n📊 Routing Statistics:');
    const routingStats = routingEngine.getRoutingStatistics();
    console.log({
        totalRoutings: routingStats.totalRoutings,
        successfulRoutings: routingStats.successfulRoutings,
        averageRoutingTime: `${routingStats.averageRoutingTime.toFixed(2)}ms`,
        providerUsageCounts: routingStats.providerUsageCounts,
        capabilityUsageCounts: routingStats.capabilityUsageCounts,
    });
    console.log('\n🔧 Failover Statistics:');
    const failoverStats = failoverManager.getFailoverStatistics();
    console.log({
        activeCircuitBreakers: failoverStats.activeCircuitBreakers,
        providerFailureCounts: failoverStats.providerFailureCounts,
    });
    console.log('\n⚡ Performance Metrics:');
    const perfMetrics = routingEngine.getProviderPerformanceMetrics();
    Object.entries(perfMetrics).forEach(([providerId, metrics]) => {
        console.log(`${providerId}:`, {
            successRate: `${metrics.successRate.toFixed(1)}%`,
            averageResponseTime: `${metrics.averageResponseTime.toFixed(0)}ms`,
            totalRequests: metrics.totalRequests,
        });
    });
}
/**
 * Example of how to integrate with the main application
 */
class AIRequestProcessor {
    routingEngine;
    failoverManager;
    constructor(registry) {
        const { routingEngine, failoverManager } = createRoutingSystem(registry);
        this.routingEngine = routingEngine;
        this.failoverManager = failoverManager;
    }
    async processRequest(request) {
        return processAIRequest(request, this.routingEngine, this.failoverManager);
    }
    getStatistics() {
        return {
            routing: this.routingEngine.getRoutingStatistics(),
            failover: this.failoverManager.getFailoverStatistics(),
            performance: this.routingEngine.getProviderPerformanceMetrics(),
        };
    }
    updateRoutingConfig(config) {
        this.routingEngine.updateConfig(config);
    }
    updateFailoverConfig(config) {
        this.failoverManager.updateConfig(config);
    }
}
exports.AIRequestProcessor = AIRequestProcessor;
//# sourceMappingURL=routing-demo.js.map