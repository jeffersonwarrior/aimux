import React from 'react';
type StatusType = 'info' | 'success' | 'warning' | 'error';
interface StatusMessageProps {
    type: StatusType;
    message: string;
    icon?: string;
}
export declare const StatusMessage: React.FC<StatusMessageProps>;
export {};
//# sourceMappingURL=StatusMessage.d.ts.map