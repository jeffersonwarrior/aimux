"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.ApiError = exports.ModelValidationError = exports.ModelInfoSchema = void 0;
const zod_1 = require("zod");
exports.ModelInfoSchema = zod_1.z.object({
    id: zod_1.z.string().describe('Model identifier'),
    object: zod_1.z.string().default('model').describe('Object type'),
    created: zod_1.z.number().optional().describe('Creation timestamp'),
    owned_by: zod_1.z.string().optional().describe('Model owner'),
    provider: zod_1.z.string().optional().describe('Model provider'),
    always_on: zod_1.z.boolean().optional().describe('Always available'),
    hugging_face_id: zod_1.z.string().optional().describe('Hugging Face model ID'),
    name: zod_1.z.string().optional().describe('Model display name'),
    input_modalities: zod_1.z.array(zod_1.z.string()).optional().describe('Supported input modalities'),
    output_modalities: zod_1.z.array(zod_1.z.string()).optional().describe('Supported output modalities'),
    context_length: zod_1.z.number().optional().describe('Context window size'),
    max_output_length: zod_1.z.number().optional().describe('Maximum output tokens'),
    pricing: zod_1.z
        .object({
        prompt: zod_1.z.string().optional(),
        completion: zod_1.z.string().optional(),
        image: zod_1.z.string().optional(),
        request: zod_1.z.string().optional(),
        input_cache_reads: zod_1.z.string().optional(),
        input_cache_writes: zod_1.z.string().optional(),
    })
        .optional()
        .describe('Pricing information'),
    quantization: zod_1.z.string().optional().describe('Model quantization'),
    supported_sampling_parameters: zod_1.z
        .array(zod_1.z.string())
        .optional()
        .describe('Supported sampling parameters'),
    supported_features: zod_1.z.array(zod_1.z.string()).optional().describe('Supported features'),
    openrouter: zod_1.z
        .object({
        slug: zod_1.z.string().optional(),
    })
        .optional()
        .describe('OpenRouter metadata'),
    datacenters: zod_1.z
        .array(zod_1.z.object({
        country_code: zod_1.z.string().optional(),
    }))
        .optional()
        .describe('Available datacenters'),
});
class ModelValidationError extends Error {
    cause;
    constructor(message, cause) {
        super(message);
        this.cause = cause;
        this.name = 'ModelValidationError';
    }
}
exports.ModelValidationError = ModelValidationError;
class ApiError extends Error {
    status;
    response;
    constructor(message, status, response) {
        super(message);
        this.status = status;
        this.response = response;
        this.name = 'ApiError';
    }
}
exports.ApiError = ApiError;
//# sourceMappingURL=types.js.map