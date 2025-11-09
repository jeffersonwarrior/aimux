"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.ModelManager = void 0;
const axios_1 = __importDefault(require("axios"));
const types_1 = require("./types");
const info_1 = require("./info");
const cache_1 = require("./cache");
class ModelManager {
    apiKey;
    modelsApiUrl;
    cache;
    constructor(options) {
        this.apiKey = options.apiKey;
        this.modelsApiUrl = options.modelsApiUrl;
        this.cache = new cache_1.ModelCache({
            cacheFile: options.cacheFile,
            cacheDurationHours: options.cacheDurationHours || 24,
        });
    }
    async fetchModels(forceRefresh = false) {
        if (!forceRefresh && (await this.cache.isValid())) {
            console.info('Loading models from cache');
            return this.cache.load();
        }
        if (!this.apiKey) {
            console.warn('No API key configured');
            return [];
        }
        console.info('Fetching models from API');
        const models = await this.fetchFromApi();
        if (models.length > 0) {
            await this.cache.save(models);
            console.info(`Fetched ${models.length} models`);
        }
        else {
            console.warn('No models received from API');
        }
        return models;
    }
    async fetchFromApi() {
        try {
            const headers = {
                Authorization: `Bearer ${this.apiKey}`,
                'Content-Type': 'application/json',
            };
            const response = await axios_1.default.get(this.modelsApiUrl, {
                headers,
                timeout: 30000,
            });
            if (response.status === 200) {
                const modelsData = response.data.data || [];
                // Convert to ModelInfoImpl objects
                const models = [];
                for (const modelData of modelsData) {
                    try {
                        const model = new info_1.ModelInfoImpl(modelData);
                        models.push(model);
                    }
                    catch (error) {
                        console.warn(`Invalid model data: ${modelData.id || 'unknown'}:`, error);
                    }
                }
                return models;
            }
            else {
                throw new types_1.ApiError(`API error: ${response.status} - ${response.statusText}`, response.status, response.data);
            }
        }
        catch (error) {
            if (axios_1.default.isAxiosError(error)) {
                if (error.response) {
                    throw new types_1.ApiError(`API error: ${error.response.status} - ${error.response.statusText}`, error.response.status, error.response.data);
                }
                else if (error.request) {
                    throw new types_1.ApiError('Network error: No response received from API');
                }
                else {
                    throw new types_1.ApiError(`Network error: ${error.message}`);
                }
            }
            throw new types_1.ApiError(`Error fetching models: ${error.message}`);
        }
    }
    getModels(models) {
        if (!models) {
            throw new Error('Models must be provided or fetched first');
        }
        // Sort models by ID for consistent display
        return [...models].sort((a, b) => a.id.localeCompare(b.id));
    }
    async searchModels(query, models) {
        if (!models) {
            models = await this.fetchModels();
        }
        if (!query) {
            return this.getModels(models);
        }
        const queryLower = query.toLowerCase();
        const matchingModels = [];
        for (const model of models) {
            // Search in model ID and components
            const searchText = [
                model.id.toLowerCase(),
                model.getProvider().toLowerCase(),
                model.getModelName().toLowerCase(),
            ].join(' ');
            if (searchText.includes(queryLower)) {
                matchingModels.push(model);
            }
        }
        // Sort results by ID
        return matchingModels.sort((a, b) => a.id.localeCompare(b.id));
    }
    async getModelById(modelId, models) {
        if (!models) {
            models = await this.fetchModels();
        }
        return models.find(model => model.id === modelId) || null;
    }
    async clearCache() {
        return this.cache.clear();
    }
    async getCacheInfo() {
        return this.cache.getInfo();
    }
}
exports.ModelManager = ModelManager;
//# sourceMappingURL=manager.js.map