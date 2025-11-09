import { ModelInfoImpl } from '../models';
export interface UIOptions {
    verbose?: boolean;
    quiet?: boolean;
}
export declare class UserInterface {
    private verbose;
    private quiet;
    constructor(options?: UIOptions);
    info(message: string, ...args: any[]): void;
    success(message: string, ...args: any[]): void;
    coloredSuccess(message: string, ...args: any[]): void;
    coloredInfo(message: string, ...args: any[]): void;
    highlightInfo(message: string, highlights?: string[]): void;
    warning(message: string, ...args: any[]): void;
    error(message: string, ...args: any[]): void;
    debug(message: string, ...args: any[]): void;
    showModelList(models: ModelInfoImpl[], selectedIndex?: number): void;
    selectModel(models: ModelInfoImpl[]): Promise<ModelInfoImpl | null>;
    selectDualModels(models: ModelInfoImpl[]): Promise<{
        regular: ModelInfoImpl | null;
        thinking: ModelInfoImpl | null;
    }>;
    showProgress(current: number, total: number, label?: string): void;
    askQuestion(question: string, defaultValue?: string): Promise<string>;
    askPassword(question: string): Promise<string>;
    confirm(message: string, defaultValue?: boolean): Promise<boolean>;
    selectModelsWithProviders(models: ModelInfoImpl[], providerCapabilities?: Record<string, string[]>): Promise<{
        regular: ModelInfoImpl | null;
        thinking: ModelInfoImpl | null;
    }>;
    showStatus(type: 'info' | 'success' | 'warning' | 'error', message: string): void;
    clear(): void;
}
//# sourceMappingURL=user-interface.d.ts.map