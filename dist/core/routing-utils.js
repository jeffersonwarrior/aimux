"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.analyzeRequestRequirements = analyzeRequestRequirements;
exports.hasThinkingRequest = hasThinkingRequest;
exports.hasVisionRequest = hasVisionRequest;
exports.hasToolsRequest = hasToolsRequest;
exports.determineRequestType = determineRequestType;
exports.estimateRequestComplexity = estimateRequestComplexity;
exports.estimateTokenUsage = estimateTokenUsage;
exports.determineRequestPriority = determineRequestPriority;
exports.extractTextContent = extractTextContent;
exports.normalizeCapabilityRequirements = normalizeCapabilityRequirements;
exports.canProviderSatisfyRequirements = canProviderSatisfyRequirements;
exports.calculateProviderCompatibility = calculateProviderCompatibility;
exports.determineLoadBalancingStrategy = determineLoadBalancingStrategy;
/**
 * Analyze AI request to determine requirements and capabilities needed
 */
function analyzeRequestRequirements(request) {
    const hasThinking = hasThinkingRequest(request);
    const hasVision = hasVisionRequest(request);
    const hasTools = hasToolsRequest(request);
    const needsStreaming = request.stream === true;
    // Determine request type based on capabilities
    const type = determineRequestType(hasThinking, hasVision, hasTools);
    // Build required capabilities list
    const capabilities = [];
    if (hasThinking)
        capabilities.push('thinking');
    if (hasVision)
        capabilities.push('vision');
    if (hasTools)
        capabilities.push('tools');
    if (needsStreaming)
        capabilities.push('streaming');
    if (request.messages.some(msg => msg.role === 'system'))
        capabilities.push('systemMessages');
    if (request.temperature !== undefined)
        capabilities.push('temperature');
    if (request.top_p !== undefined)
        capabilities.push('topP');
    // Estimate complexity and tokens
    const complexity = estimateRequestComplexity(request);
    const estimatedTokens = estimateTokenUsage(request);
    // Determine priority
    const priority = determineRequestPriority(request, type, complexity);
    return {
        type,
        capabilities,
        requiresThinking: hasThinking,
        requiresVision: hasVision,
        requiresTools: hasTools,
        requiresStreaming: needsStreaming,
        complexity,
        estimatedTokens,
        priority,
    };
}
/**
 * Determine if request appears to be a thinking/reasoning request
 */
function hasThinkingRequest(request) {
    // Check explicit metadata first
    if (request.metadata?.requestType === 'thinking') {
        return true;
    }
    // Check for thinking indicators in user messages
    const textContent = extractTextContent(request);
    const thinkingIndicators = [
        'think step by step',
        'reason through',
        'analyze this',
        'break down',
        'consider',
        'evaluate',
        'explain your reasoning',
        'walk through',
        'step by step',
        'let me think',
        'approach this',
        'methodical',
        'systematic',
        'logical',
    ];
    const lowerText = textContent.toLowerCase();
    return (thinkingIndicators.some(indicator => lowerText.includes(indicator)) ||
        hasComplexProblemIndicators(lowerText));
}
/**
 * Determine if request includes image/vision content
 */
function hasVisionRequest(request) {
    // Check explicit metadata first
    if (request.metadata?.requestType === 'vision') {
        return true;
    }
    // Check for image content in messages
    return request.messages.some(msg => Array.isArray(msg.content) && msg.content.some(part => part.type === 'image_url'));
}
/**
 * Determine if request requires tools/function calling
 */
function hasToolsRequest(request) {
    // Check explicit metadata first
    if (request.metadata?.requestType === 'tools') {
        return true;
    }
    // Check for tools in request
    if (request.tools && request.tools.length > 0) {
        return true;
    }
    // Check for tool_choice configuration
    if (request.tool_choice && request.tool_choice !== 'none') {
        return true;
    }
    // Check for tool calls in message history
    return request.messages.some(msg => msg.role === 'tool' || (msg.tool_calls && msg.tool_calls.length > 0));
}
/**
 * Determine primary request type based on capability requirements
 */
function determineRequestType(hasThinking, hasVision, hasTools) {
    // Count active capabilities
    const activeCapabilities = [hasThinking, hasVision, hasTools].filter(Boolean).length;
    // If multiple capabilities are required, it's a hybrid request
    if (activeCapabilities > 1) {
        return 'hybrid';
    }
    // Single capability requests
    if (hasThinking)
        return 'thinking';
    if (hasVision)
        return 'vision';
    if (hasTools)
        return 'tools';
    // Default to regular request
    return 'regular';
}
/**
 * Estimate request complexity based on content and metadata
 */
function estimateRequestComplexity(request) {
    const textContent = extractTextContent(request);
    const messageCount = request.messages.length;
    // Token-based complexity estimation
    const estimatedTokens = estimateTokenUsage(request);
    // Factor in multiple complexity indicators
    let complexityScore = 0;
    // Message count factor
    if (messageCount > 10)
        complexityScore += 2;
    else if (messageCount > 5)
        complexityScore += 1;
    // Token count factor
    if (estimatedTokens > 8000)
        complexityScore += 3;
    else if (estimatedTokens > 4000)
        complexityScore += 2;
    else if (estimatedTokens > 2000)
        complexityScore += 1;
    // Content complexity factor
    const lowerText = textContent.toLowerCase();
    if (hasCodeContent(lowerText))
        complexityScore += 1;
    if (hasMathematicalContent(lowerText))
        complexityScore += 1;
    if (hasComplexProblemIndicators(lowerText))
        complexityScore += 2;
    // Tools/complex parameters factor
    if (request.tools && request.tools.length > 3)
        complexityScore += 2;
    else if (request.tools && request.tools.length > 0)
        complexityScore += 1;
    // Determine complexity based on score
    if (complexityScore >= 5)
        return 'high';
    if (complexityScore >= 2)
        return 'medium';
    return 'low';
}
/**
 * Estimate token usage for a request
 */
function estimateTokenUsage(request) {
    let tokens = 0;
    // Tokens from message content
    for (const message of request.messages) {
        if (typeof message.content === 'string') {
            tokens += Math.ceil(message.content.length / 4); // Rough estimate: 1 token ≈ 4 chars
        }
        else if (Array.isArray(message.content)) {
            for (const part of message.content) {
                if (part.type === 'text' && part.text) {
                    tokens += Math.ceil(part.text.length / 4);
                }
                else if (part.type === 'image_url') {
                    // Image tokens vary by detail level
                    const detail = part.image_url?.detail || 'auto';
                    if (detail === 'high')
                        tokens += 85;
                    else if (detail === 'low')
                        tokens += 65;
                    else
                        tokens += 85; // Default to high detail
                }
            }
        }
        // Add overhead for message structure
        tokens += 10;
    }
    // Tokens from tools/function definitions
    if (request.tools) {
        for (const tool of request.tools) {
            // Rough estimate for tool schema
            const schemaJson = JSON.stringify(tool.function.parameters || {});
            tokens += Math.ceil(schemaJson.length / 4) + 50; // Base overhead + schema
        }
    }
    // Add requested max_tokens if specified
    if (request.max_tokens) {
        tokens += request.max_tokens;
    }
    // Add buffer for system messages and formatting (20% overhead)
    return Math.ceil(tokens * 1.2);
}
/**
 * Determine request priority based on type, complexity, and content
 */
function determineRequestPriority(request, requestType, complexity) {
    // Check explicit priority in metadata
    if (request.metadata?.priority) {
        return request.metadata.priority;
    }
    let priorityScore = 0;
    // Request type priority
    switch (requestType) {
        case 'thinking':
            priorityScore += 2; // Thinking requests often more complex
            break;
        case 'tools':
            priorityScore += 1; // Tool requests can be time-sensitive
            break;
        case 'vision':
            priorityScore += 1; // Vision requests can be resource-intensive
            break;
        case 'hybrid':
            priorityScore += 2; // Hybrid requests are most complex
            break;
    }
    // Complexity priority
    switch (complexity) {
        case 'high':
            priorityScore += 2;
            break;
        case 'medium':
            priorityScore += 1;
            break;
    }
    // Time-sensitive content indicators
    const textContent = extractTextContent(request).toLowerCase();
    const urgentIndicators = [
        'urgent',
        'asap',
        'immediately',
        'deadline',
        'time sensitive',
        'emergency',
        'critical',
        'now',
    ];
    if (urgentIndicators.some(indicator => textContent.includes(indicator))) {
        priorityScore += 2;
    }
    // Determine priority based on score
    if (priorityScore >= 4)
        return 'high';
    if (priorityScore >= 2)
        return 'medium';
    return 'low';
}
/**
 * Extract all text content from messages
 */
function extractTextContent(request) {
    let textContent = '';
    for (const message of request.messages) {
        if (typeof message.content === 'string') {
            textContent += message.content + ' ';
        }
        else if (Array.isArray(message.content)) {
            for (const part of message.content) {
                if (part.type === 'text' && part.text) {
                    textContent += part.text + ' ';
                }
            }
        }
    }
    return textContent.trim();
}
/**
 * Check if text has complex problem indicators
 */
function hasComplexProblemIndicators(text) {
    const complexIndicators = [
        'algorithm',
        'optimization',
        'architecture',
        'design pattern',
        'complex',
        'sophisticated',
        'advanced',
        'challenge problem',
        'difficult',
        'intricate',
        'multi-step',
        'nested',
        'recursive',
    ];
    return complexIndicators.some(indicator => text.includes(indicator));
}
/**
 * Check if text contains code
 */
function hasCodeContent(text) {
    const codeIndicators = [
        '```',
        'function',
        'class ',
        'import ',
        'def ',
        'public ',
        'private ',
        'const ',
        'let ',
        'var ',
        'async ',
        'await ',
        'return ',
        '=>',
        '(){',
        '.then(',
        '.catch(',
    ];
    return codeIndicators.some(indicator => text.includes(indicator));
}
/**
 * Check if text contains mathematical/analytical content
 */
function hasMathematicalContent(text) {
    const mathIndicators = [
        'calculate',
        'formula',
        'equation',
        'algorithm',
        'optimize',
        'probability',
        'statistics',
        'matrix',
        'vector',
        'derivative',
        'integral',
        '%',
        '^',
        'sqrt',
        'log',
    ];
    return mathIndicators.some(indicator => text.includes(indicator));
}
/**
 * Normalize capability requirements for provider matching
 */
function normalizeCapabilityRequirements(requirements) {
    // Create a copy to avoid mutation
    const normalized = { ...requirements };
    // Remove redundant capabilities
    const capabilitySet = new Set(normalized.capabilities);
    normalized.capabilities = Array.from(capabilitySet);
    // Sort capabilities by priority (most specific first)
    const capabilityPriority = [
        'thinking',
        'vision',
        'tools',
        'streaming',
        'systemMessages',
        'temperature',
        'topP',
    ];
    normalized.capabilities.sort((a, b) => {
        const aIndex = capabilityPriority.indexOf(a);
        const bIndex = capabilityPriority.indexOf(b);
        return (aIndex === -1 ? 999 : aIndex) - (bIndex === -1 ? 999 : bIndex);
    });
    return normalized;
}
/**
 * Check if provider can satisfy all required capabilities
 */
function canProviderSatisfyRequirements(provider, // BaseProvider instance
requirements) {
    // Check basic required capabilities
    for (const capability of requirements.capabilities) {
        if (!provider.hasCapability(capability)) {
            return false;
        }
    }
    // Check token limits
    const maxTokens = provider.capabilities?.maxTokens || 100000;
    if (requirements.estimatedTokens > maxTokens) {
        return false;
    }
    // Check if provider can handle request type
    if (requirements.type === 'thinking' && !provider.hasCapability('thinking')) {
        return false;
    }
    if (requirements.type === 'vision' && !provider.hasCapability('vision')) {
        return false;
    }
    if (requirements.type === 'tools' && !provider.hasCapability('tools')) {
        return false;
    }
    return true;
}
/**
 * Calculate compatibility score between provider and requirements
 */
function calculateProviderCompatibility(provider, // BaseProvider instance
requirements) {
    let score = 0;
    const maxScore = 10;
    // Capability matching (up to 4 points)
    const matchedCapabilities = requirements.capabilities.filter(cap => provider.hasCapability(cap)).length;
    score += (matchedCapabilities / Math.max(requirements.capabilities.length, 1)) * 4;
    // Token compatibility (up to 2 points)
    const maxTokens = provider.capabilities?.maxTokens || 100000;
    const tokenRatio = Math.min(requirements.estimatedTokens / maxTokens, 1);
    score += (1 - tokenRatio) * 2; // Better score if more headroom
    // Provider health (up to 2 points)
    const health = provider.getLastHealthCheck?.();
    if (health) {
        if (health.status === 'healthy')
            score += 2;
        else if (health.status === 'degraded')
            score += 1;
    }
    else {
        score += 1; // Unknown health gets partial credit
    }
    // Performance metrics (up to 2 points)
    const metrics = provider.getPerformanceMetrics?.();
    if (metrics) {
        // Success rate
        score += (metrics.successRate / 100) * 1.5;
        // Response time (inverse relationship)
        const avgResponseTime = metrics.averageResponseTime || 1000;
        const timeScore = Math.max(0, 1 - (avgResponseTime - 500) / 2000); // Normalize around 500ms baseline
        score += Math.min(timeScore, 0.5);
    }
    return Math.min(score, maxScore);
}
/**
 * Determine optimal load balancing strategy based on request patterns
 */
function determineLoadBalancingStrategy(recentRequests) {
    if (recentRequests.length < 10) {
        return 'round-robin'; // Default for low volume
    }
    // Analyze request patterns
    const complexityDistribution = {
        low: recentRequests.filter(r => r.complexity === 'low').length,
        medium: recentRequests.filter(r => r.complexity === 'medium').length,
        high: recentRequests.filter(r => r.complexity === 'high').length,
    };
    const totalRequests = recentRequests.length;
    const highComplexityRatio = complexityDistribution.high / totalRequests;
    // Strategy selection based on patterns
    if (highComplexityRatio > 0.6) {
        return 'performance-based'; // Complex requests benefit from performance optimization
    }
    const typeVariety = new Set(recentRequests.map(r => r.type)).size;
    if (typeVariety > 3) {
        return 'weighted'; // Mixed request types benefit from weighted routing
    }
    // Check for provider utilization patterns
    const avgTokens = recentRequests.reduce((sum, r) => sum + r.estimatedTokens, 0) / totalRequests;
    if (avgTokens > 5000) {
        return 'least-connections'; // Heavy requests benefit from load distribution
    }
    return 'round-robin'; // Default strategy
}
//# sourceMappingURL=routing-utils.js.map