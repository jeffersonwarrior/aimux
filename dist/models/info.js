"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.ModelInfoImpl = void 0;
exports.createModelInfo = createModelInfo;
class ModelInfoImpl {
    id;
    object;
    created;
    owned_by;
    provider;
    always_on;
    hugging_face_id;
    name;
    input_modalities;
    output_modalities;
    context_length;
    max_output_length;
    pricing;
    quantization;
    supported_sampling_parameters;
    supported_features;
    openrouter;
    datacenters;
    constructor(data) {
        const result = require('./types').ModelInfoSchema.safeParse(data);
        if (!result.success) {
            throw new Error(`Invalid model data: ${result.error.message}`);
        }
        const modelData = result.data;
        this.id = modelData.id;
        this.object = modelData.object;
        this.created = modelData.created;
        this.owned_by = modelData.owned_by;
        this.provider = modelData.provider;
        this.always_on = modelData.always_on;
        this.hugging_face_id = modelData.hugging_face_id;
        this.name = modelData.name;
        this.input_modalities = modelData.input_modalities;
        this.output_modalities = modelData.output_modalities;
        this.context_length = modelData.context_length;
        this.max_output_length = modelData.max_output_length;
        this.pricing = modelData.pricing;
        this.quantization = modelData.quantization;
        this.supported_sampling_parameters = modelData.supported_sampling_parameters;
        this.supported_features = modelData.supported_features;
        this.openrouter = modelData.openrouter;
        this.datacenters = modelData.datacenters;
    }
    getDisplayName() {
        return this.name || this.id;
    }
    getProvider() {
        return (this.provider ||
            (() => {
                if (this.id.includes(':')) {
                    return this.id.split(':', 1)[0] || 'unknown';
                }
                return 'unknown';
            })());
    }
    getModelName() {
        return (this.name ||
            (() => {
                if (this.id.includes(':')) {
                    return this.id.split(':', 2)[1] || this.id;
                }
                return this.id;
            })());
    }
    toJSON() {
        return {
            id: this.id,
            object: this.object,
            created: this.created,
            owned_by: this.owned_by,
            provider: this.provider,
            always_on: this.always_on,
            hugging_face_id: this.hugging_face_id,
            name: this.name,
            input_modalities: this.input_modalities,
            output_modalities: this.output_modalities,
            context_length: this.context_length,
            max_output_length: this.max_output_length,
            pricing: this.pricing,
            quantization: this.quantization,
            supported_sampling_parameters: this.supported_sampling_parameters,
            supported_features: this.supported_features,
            openrouter: this.openrouter,
            datacenters: this.datacenters,
        };
    }
}
exports.ModelInfoImpl = ModelInfoImpl;
function createModelInfo(data) {
    return new ModelInfoImpl(data);
}
//# sourceMappingURL=info.js.map