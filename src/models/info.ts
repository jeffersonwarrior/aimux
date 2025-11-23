import { ModelInfo, ModelInfoSchema } from './types';

export class ModelInfoImpl implements ModelInfo {
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

  constructor(data: ModelInfo) {
    const result = ModelInfoSchema.safeParse(data);
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

  getDisplayName(): string {
    return this.name || this.id;
  }

  getProvider(): string {
    return (
      this.provider ||
      (() => {
        if (this.id.includes(':')) {
          return this.id.split(':', 1)[0] || 'unknown';
        }
        return 'unknown';
      })()
    );
  }

  getModelName(): string {
    return (
      this.name ||
      (() => {
        if (this.id.includes(':')) {
          return this.id.split(':', 2)[1] || this.id;
        }
        return this.id;
      })()
    );
  }

  toJSON(): ModelInfo {
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

export function createModelInfo(data: ModelInfo): ModelInfoImpl {
  return new ModelInfoImpl(data);
}
