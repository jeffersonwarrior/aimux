import { AxiosInstance, AxiosResponse } from 'axios';
import { ApiModelsResponse } from '../models/types';
export interface ApiClientOptions {
    baseURL?: string;
    timeout?: number;
    headers?: Record<string, string>;
}
export declare class ApiClient {
    private axios;
    private defaultHeaders;
    constructor(options?: ApiClientOptions);
    private setupInterceptors;
    setApiKey(apiKey: string): void;
    setBaseURL(baseURL: string): void;
    get<T = any>(url: string, config?: any): Promise<AxiosResponse<T>>;
    post<T = any>(url: string, data?: any, config?: any): Promise<AxiosResponse<T>>;
    put<T = any>(url: string, data?: any, config?: any): Promise<AxiosResponse<T>>;
    delete<T = any>(url: string, config?: any): Promise<AxiosResponse<T>>;
    fetchModels(apiKey: string, modelsUrl: string): Promise<ApiModelsResponse>;
    private handleError;
    getAxiosInstance(): AxiosInstance;
}
//# sourceMappingURL=client.d.ts.map