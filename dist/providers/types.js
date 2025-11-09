"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.ProviderError = exports.ProviderCapabilitiesSchema = void 0;
const zod_1 = require("zod");
/**
 * Provider capabilities schema
 */
exports.ProviderCapabilitiesSchema = zod_1.z.object({
    thinking: zod_1.z.boolean().default(false).describe('Supports thinking/reasoning capabilities'),
    vision: zod_1.z.boolean().default(false).describe('Supports image/vision input'),
    tools: zod_1.z.boolean().default(false).describe('Supports function calling/tools'),
    maxTokens: zod_1.z.number().positive().default(100000).describe('Maximum token limit'),
    streaming: zod_1.z.boolean().default(true).describe('Supports streaming responses'),
    systemMessages: zod_1.z.boolean().default(true).describe('Supports system messages'),
    temperature: zod_1.z.boolean().default(true).describe('Supports temperature parameter'),
    topP: zod_1.z.boolean().default(true).describe('Supports top_p parameter'),
    maxOutputTokens: zod_1.z.number().positive().optional().describe('Maximum output tokens'),
});
/**
 * Provider-specific error class
 */
class ProviderError extends Error {
    providerName;
    statusCode;
    originalError;
    constructor(message, providerName, statusCode, originalError) {
        super(message);
        this.providerName = providerName;
        this.statusCode = statusCode;
        this.originalError = originalError;
        this.name = 'ProviderError';
    }
}
exports.ProviderError = ProviderError;
//# sourceMappingURL=types.js.map