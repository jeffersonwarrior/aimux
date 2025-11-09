import { CacheInfo } from './types';
import { ModelInfoImpl } from './info';
export interface ModelCacheOptions {
    cacheFile: string;
    cacheDurationHours: number;
}
export declare class ModelCache {
    private cacheFile;
    private cacheDurationMs;
    constructor(options: ModelCacheOptions);
    isValid(): Promise<boolean>;
    load(): Promise<ModelInfoImpl[]>;
    save(models: ModelInfoImpl[]): Promise<boolean>;
    clear(): Promise<boolean>;
    getInfo(): Promise<CacheInfo>;
}
//# sourceMappingURL=cache.d.ts.map