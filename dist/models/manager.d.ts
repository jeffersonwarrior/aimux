import { ModelInfoImpl } from './info';
export interface ModelManagerOptions {
    apiKey: string;
    modelsApiUrl: string;
    cacheFile: string;
    cacheDurationHours?: number;
}
export declare class ModelManager {
    private apiKey;
    private modelsApiUrl;
    private cache;
    constructor(options: ModelManagerOptions);
    fetchModels(forceRefresh?: boolean): Promise<ModelInfoImpl[]>;
    private fetchFromApi;
    getModels(models?: ModelInfoImpl[]): ModelInfoImpl[];
    searchModels(query: string, models?: ModelInfoImpl[]): Promise<ModelInfoImpl[]>;
    getModelById(modelId: string, models?: ModelInfoImpl[]): Promise<ModelInfoImpl | null>;
    clearCache(): Promise<boolean>;
    getCacheInfo(): Promise<Record<string, any>>;
}
//# sourceMappingURL=manager.d.ts.map