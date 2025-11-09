"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.ApiClient = void 0;
const axios_1 = __importDefault(require("axios"));
const types_1 = require("../models/types");
class ApiClient {
    axios;
    defaultHeaders;
    constructor(options = {}) {
        this.defaultHeaders = {
            'Content-Type': 'application/json',
            'User-Agent': 'synclaude/1.0.0',
            ...options.headers,
        };
        this.axios = axios_1.default.create({
            baseURL: options.baseURL || 'https://api.synthetic.new',
            timeout: options.timeout || 30000,
            headers: this.defaultHeaders,
        });
        this.setupInterceptors();
    }
    setupInterceptors() {
        // Request interceptor
        this.axios.interceptors.request.use(config => {
            console.debug(`API Request: ${config.method?.toUpperCase()} ${config.url}`);
            return config;
        }, error => {
            console.error('API Request Error:', error);
            return Promise.reject(error);
        });
        // Response interceptor
        this.axios.interceptors.response.use(response => {
            console.debug(`API Response: ${response.status} ${response.statusText}`);
            return response;
        }, error => {
            if (axios_1.default.isAxiosError(error)) {
                if (error.response) {
                    console.error(`API Error Response: ${error.response.status} ${error.response.statusText}`);
                }
                else if (error.request) {
                    console.error('API Network Error: No response received');
                }
                else {
                    console.error('API Request Setup Error:', error.message);
                }
            }
            return Promise.reject(error);
        });
    }
    setApiKey(apiKey) {
        this.axios.defaults.headers.common['Authorization'] = `Bearer ${apiKey}`;
    }
    setBaseURL(baseURL) {
        this.axios.defaults.baseURL = baseURL;
    }
    async get(url, config) {
        try {
            return await this.axios.get(url, config);
        }
        catch (error) {
            throw this.handleError(error);
        }
    }
    async post(url, data, config) {
        try {
            return await this.axios.post(url, data, config);
        }
        catch (error) {
            throw this.handleError(error);
        }
    }
    async put(url, data, config) {
        try {
            return await this.axios.put(url, data, config);
        }
        catch (error) {
            throw this.handleError(error);
        }
    }
    async delete(url, config) {
        try {
            return await this.axios.delete(url, config);
        }
        catch (error) {
            throw this.handleError(error);
        }
    }
    async fetchModels(apiKey, modelsUrl) {
        this.setApiKey(apiKey);
        try {
            const response = await this.get(modelsUrl);
            return response.data;
        }
        catch (error) {
            if (error instanceof types_1.ApiError) {
                throw error;
            }
            throw new types_1.ApiError(`Failed to fetch models: ${error.message}`);
        }
    }
    handleError(error) {
        if (axios_1.default.isAxiosError(error)) {
            if (error.response) {
                const status = error.response.status;
                const data = error.response.data;
                const message = data?.message || data?.error || error.response.statusText;
                return new types_1.ApiError(`API error ${status}: ${message}`, status, data);
            }
            else if (error.request) {
                return new types_1.ApiError('Network error: No response received from API');
            }
            else {
                return new types_1.ApiError(`Request error: ${error.message}`);
            }
        }
        return new types_1.ApiError(`Unknown error: ${error.message}`);
    }
    getAxiosInstance() {
        return this.axios;
    }
}
exports.ApiClient = ApiClient;
//# sourceMappingURL=client.js.map