import { ModelInfo } from './types';
export declare class ModelInfoImpl implements ModelInfo {
    id: string;
    object: string;
    created?: number;
    owned_by?: string;
    provider?: string;
    always_on?: boolean;
    hugging_face_id?: string;
    name?: string;
    input_modalities?: string[];
    output_modalities?: string[];
    context_length?: number;
    max_output_length?: number;
    pricing?: {
        prompt?: string;
        completion?: string;
        image?: string;
        request?: string;
        input_cache_reads?: string;
        input_cache_writes?: string;
    };
    quantization?: string;
    supported_sampling_parameters?: string[];
    supported_features?: string[];
    openrouter?: {
        slug?: string;
    };
    datacenters?: Array<{
        country_code?: string;
    }>;
    constructor(data: ModelInfo);
    getDisplayName(): string;
    getProvider(): string;
    getModelName(): string;
    toJSON(): ModelInfo;
}
export declare function createModelInfo(data: ModelInfo): ModelInfoImpl;
//# sourceMappingURL=info.d.ts.map