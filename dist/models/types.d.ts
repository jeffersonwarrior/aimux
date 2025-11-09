import { z } from 'zod';
export declare const ModelInfoSchema: z.ZodObject<{
    id: z.ZodString;
    object: z.ZodDefault<z.ZodString>;
    created: z.ZodOptional<z.ZodNumber>;
    owned_by: z.ZodOptional<z.ZodString>;
    provider: z.ZodOptional<z.ZodString>;
    always_on: z.ZodOptional<z.ZodBoolean>;
    hugging_face_id: z.ZodOptional<z.ZodString>;
    name: z.ZodOptional<z.ZodString>;
    input_modalities: z.ZodOptional<z.ZodArray<z.ZodString, "many">>;
    output_modalities: z.ZodOptional<z.ZodArray<z.ZodString, "many">>;
    context_length: z.ZodOptional<z.ZodNumber>;
    max_output_length: z.ZodOptional<z.ZodNumber>;
    pricing: z.ZodOptional<z.ZodObject<{
        prompt: z.ZodOptional<z.ZodString>;
        completion: z.ZodOptional<z.ZodString>;
        image: z.ZodOptional<z.ZodString>;
        request: z.ZodOptional<z.ZodString>;
        input_cache_reads: z.ZodOptional<z.ZodString>;
        input_cache_writes: z.ZodOptional<z.ZodString>;
    }, "strip", z.ZodTypeAny, {
        prompt?: string | undefined;
        completion?: string | undefined;
        image?: string | undefined;
        request?: string | undefined;
        input_cache_reads?: string | undefined;
        input_cache_writes?: string | undefined;
    }, {
        prompt?: string | undefined;
        completion?: string | undefined;
        image?: string | undefined;
        request?: string | undefined;
        input_cache_reads?: string | undefined;
        input_cache_writes?: string | undefined;
    }>>;
    quantization: z.ZodOptional<z.ZodString>;
    supported_sampling_parameters: z.ZodOptional<z.ZodArray<z.ZodString, "many">>;
    supported_features: z.ZodOptional<z.ZodArray<z.ZodString, "many">>;
    openrouter: z.ZodOptional<z.ZodObject<{
        slug: z.ZodOptional<z.ZodString>;
    }, "strip", z.ZodTypeAny, {
        slug?: string | undefined;
    }, {
        slug?: string | undefined;
    }>>;
    datacenters: z.ZodOptional<z.ZodArray<z.ZodObject<{
        country_code: z.ZodOptional<z.ZodString>;
    }, "strip", z.ZodTypeAny, {
        country_code?: string | undefined;
    }, {
        country_code?: string | undefined;
    }>, "many">>;
}, "strip", z.ZodTypeAny, {
    object: string;
    id: string;
    name?: string | undefined;
    provider?: string | undefined;
    created?: number | undefined;
    owned_by?: string | undefined;
    always_on?: boolean | undefined;
    hugging_face_id?: string | undefined;
    input_modalities?: string[] | undefined;
    output_modalities?: string[] | undefined;
    context_length?: number | undefined;
    max_output_length?: number | undefined;
    pricing?: {
        prompt?: string | undefined;
        completion?: string | undefined;
        image?: string | undefined;
        request?: string | undefined;
        input_cache_reads?: string | undefined;
        input_cache_writes?: string | undefined;
    } | undefined;
    quantization?: string | undefined;
    supported_sampling_parameters?: string[] | undefined;
    supported_features?: string[] | undefined;
    openrouter?: {
        slug?: string | undefined;
    } | undefined;
    datacenters?: {
        country_code?: string | undefined;
    }[] | undefined;
}, {
    id: string;
    object?: string | undefined;
    name?: string | undefined;
    provider?: string | undefined;
    created?: number | undefined;
    owned_by?: string | undefined;
    always_on?: boolean | undefined;
    hugging_face_id?: string | undefined;
    input_modalities?: string[] | undefined;
    output_modalities?: string[] | undefined;
    context_length?: number | undefined;
    max_output_length?: number | undefined;
    pricing?: {
        prompt?: string | undefined;
        completion?: string | undefined;
        image?: string | undefined;
        request?: string | undefined;
        input_cache_reads?: string | undefined;
        input_cache_writes?: string | undefined;
    } | undefined;
    quantization?: string | undefined;
    supported_sampling_parameters?: string[] | undefined;
    supported_features?: string[] | undefined;
    openrouter?: {
        slug?: string | undefined;
    } | undefined;
    datacenters?: {
        country_code?: string | undefined;
    }[] | undefined;
}>;
export type ModelInfo = z.infer<typeof ModelInfoSchema>;
export interface CacheInfo {
    exists: boolean;
    filePath?: string;
    modifiedTime?: string;
    sizeBytes?: number;
    modelCount?: number;
    isValid?: boolean;
    error?: string;
}
export interface ApiModelsResponse {
    data: ModelInfo[];
    object?: string;
}
export declare class ModelValidationError extends Error {
    cause?: unknown | undefined;
    constructor(message: string, cause?: unknown | undefined);
}
export declare class ApiError extends Error {
    status?: number | undefined;
    response?: any | undefined;
    constructor(message: string, status?: number | undefined, response?: any | undefined);
}
//# sourceMappingURL=types.d.ts.map