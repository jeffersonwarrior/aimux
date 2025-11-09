import React from 'react';
export interface ProviderInfo {
    id: string;
    name: string;
    apiKey: string;
    enabled: boolean;
    capabilities: string[];
    priority: number;
    timeout: number;
    maxRetries: number;
    costLevel: 'low' | 'medium' | 'high';
    speedLevel: 'fast' | 'medium' | 'slow';
}
export interface ProviderRoutingConfig {
    defaultProvider: string;
    thinkingProvider?: string;
    visionProvider?: string;
    toolsProvider?: string;
    strategy: 'capability' | 'cost' | 'performance' | 'round-robin';
    enableFailover: boolean;
    loadBalancing: boolean;
}
interface ProviderConfigProps {
    providers: ProviderInfo[];
    routing: ProviderRoutingConfig;
    onSave: (providers: ProviderInfo[], routing: ProviderRoutingConfig) => void;
    onCancel: () => void;
    onTestProvider: (providerId: string) => Promise<boolean>;
}
export declare const ProviderConfig: React.FC<ProviderConfigProps>;
export default ProviderConfig;
//# sourceMappingURL=ProviderConfig.d.ts.map