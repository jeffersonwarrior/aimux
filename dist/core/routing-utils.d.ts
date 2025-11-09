import { AIRequest } from '../providers/types';
import { ProviderCapabilities } from '../providers/types';
/**
 * Request requirements analysis results
 */
export interface RequestRequirements {
    /** Primary request type */
    type: 'thinking' | 'regular' | 'vision' | 'tools' | 'hybrid';
    /** Required capabilities for this request */
    capabilities: (keyof ProviderCapabilities)[];
    /** Flags for specific requirements */
    requiresThinking: boolean;
    requiresVision: boolean;
    requiresTools: boolean;
    requiresStreaming: boolean;
    /** Request complexity estimate */
    complexity: 'low' | 'medium' | 'high';
    /** Estimated token usage */
    estimatedTokens: number;
    /** Priority level based on content */
    priority: 'low' | 'medium' | 'high';
}
/**
 * Analyze AI request to determine requirements and capabilities needed
 */
export declare function analyzeRequestRequirements(request: AIRequest): RequestRequirements;
/**
 * Determine if request appears to be a thinking/reasoning request
 */
export declare function hasThinkingRequest(request: AIRequest): boolean;
/**
 * Determine if request includes image/vision content
 */
export declare function hasVisionRequest(request: AIRequest): boolean;
/**
 * Determine if request requires tools/function calling
 */
export declare function hasToolsRequest(request: AIRequest): boolean;
/**
 * Determine primary request type based on capability requirements
 */
export declare function determineRequestType(hasThinking: boolean, hasVision: boolean, hasTools: boolean): 'thinking' | 'regular' | 'vision' | 'tools' | 'hybrid';
/**
 * Estimate request complexity based on content and metadata
 */
export declare function estimateRequestComplexity(request: AIRequest): 'low' | 'medium' | 'high';
/**
 * Estimate token usage for a request
 */
export declare function estimateTokenUsage(request: AIRequest): number;
/**
 * Determine request priority based on type, complexity, and content
 */
export declare function determineRequestPriority(request: AIRequest, requestType: string, complexity: 'low' | 'medium' | 'high'): 'low' | 'medium' | 'high';
/**
 * Extract all text content from messages
 */
export declare function extractTextContent(request: AIRequest): string;
/**
 * Normalize capability requirements for provider matching
 */
export declare function normalizeCapabilityRequirements(requirements: RequestRequirements): RequestRequirements;
/**
 * Check if provider can satisfy all required capabilities
 */
export declare function canProviderSatisfyRequirements(provider: any, // BaseProvider instance
requirements: RequestRequirements): boolean;
/**
 * Calculate compatibility score between provider and requirements
 */
export declare function calculateProviderCompatibility(provider: any, // BaseProvider instance
requirements: RequestRequirements): number;
/**
 * Determine optimal load balancing strategy based on request patterns
 */
export declare function determineLoadBalancingStrategy(recentRequests: RequestRequirements[]): 'round-robin' | 'weighted' | 'least-connections' | 'performance-based';
//# sourceMappingURL=routing-utils.d.ts.map