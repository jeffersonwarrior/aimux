import React from 'react';
export interface ProviderInfo {
    id: string;
    name: string;
    description: string;
    capabilities: string[];
    priority: number;
    costLevel: 'low' | 'medium' | 'high';
    speedLevel: 'fast' | 'medium' | 'slow';
}
export interface ProviderConfig {
    id: string;
    name: string;
    apiKey: string;
    capabilities: string[];
    enabled: boolean;
    priority: number;
    timeout: number;
    maxRetries: number;
}
export interface RoutingConfig {
    defaultProvider: string;
    thinkingProvider?: string;
    visionProvider?: string;
    toolsProvider?: string;
    strategy: 'capability' | 'cost' | 'performance' | 'round-robin';
    enableFailover: boolean;
    loadBalancing: boolean;
}
export interface SetupResult {
    success: boolean;
    providers: ProviderConfig[];
    routing: RoutingConfig;
    errors?: string[];
}
interface ProviderSetupProps {
    onSetup: (result: SetupResult) => void;
    onCancel: () => void;
    initialProviders?: ProviderConfig[];
    initialRouting?: RoutingConfig;
    availableProviders?: ProviderInfo[];
}
export declare const ProviderSetup: React.FC<ProviderSetupProps>;
export {};
//# sourceMappingURL=provider-setup.d.ts.map