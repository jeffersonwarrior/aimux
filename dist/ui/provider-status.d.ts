import React from 'react';
export interface ProviderStatus {
    id: string;
    name: string;
    enabled: boolean;
    connectionStatus: 'connected' | 'disconnected' | 'testing' | 'error';
    apiKeyValid: boolean;
    lastCheck: Date;
    responseTime?: number;
    successRate?: number;
    uptimePercentage?: number;
    errorCount?: number;
    totalRequests?: number;
    capabilities: string[];
    priority: number;
}
export interface ProviderMetrics {
    ResponseTime: number;
    SuccessRate: number;
    TotalRequests: number;
    ErrorCount: number;
    Uptime: number;
}
export interface StatusSnapshot {
    timestamp: Date;
    providers: ProviderStatus[];
    globalStats: {
        totalProviders: number;
        activeProviders: number;
        totalRequests: number;
        globalSuccessRate: number;
    };
}
interface ProviderStatusProps {
    providers: ProviderStatus[];
    onRefresh: () => Promise<void>;
    onTestProvider: (providerId: string) => Promise<boolean>;
    onToggleProvider: (providerId: string) => void;
    onExit: () => void;
    autoRefresh?: boolean;
    refreshInterval?: number;
}
export declare const ProviderStatus: React.FC<ProviderStatusProps>;
export declare const createSampleProviderStatus: () => ProviderStatus[];
export default ProviderStatus;
//# sourceMappingURL=provider-status.d.ts.map