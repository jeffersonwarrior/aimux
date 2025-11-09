import React from 'react';
import { ModelInfoImpl } from '../../models';
interface ModelSelectorProps {
    models: ModelInfoImpl[];
    onSelect: (regularModel: ModelInfoImpl | null, thinkingModel: ModelInfoImpl | null) => void;
    onCancel: () => void;
    searchPlaceholder?: string;
    initialRegularModel?: ModelInfoImpl | null;
    initialThinkingModel?: ModelInfoImpl | null;
    showProviderBadges?: boolean;
    providerCapabilities?: Record<string, string[]>;
    enableMultiProvider?: boolean;
}
export declare const ModelSelector: React.FC<ModelSelectorProps>;
export {};
//# sourceMappingURL=ModelSelector.d.ts.map