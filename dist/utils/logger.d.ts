export type LogLevel = 'debug' | 'info' | 'warn' | 'error';
interface LoggerOptions {
    level: LogLevel;
    verbose?: boolean;
    quiet?: boolean;
}
export declare class Logger {
    private level;
    private verbose;
    private quiet;
    constructor(options: LoggerOptions);
    private shouldLog;
    private formatMessage;
    debug(message: string, ...args: any[]): void;
    info(message: string, ...args: any[]): void;
    warn(message: string, ...args: any[]): void;
    error(message: string, ...args: any[]): void;
    setLevel(level: LogLevel): void;
    setVerbose(verbose: boolean): void;
    setQuiet(quiet: boolean): void;
}
export declare function setupLogging(verbose?: boolean, quiet?: boolean): void;
export declare function getLogger(): Logger;
export declare const logger: Logger;
export declare const log: {
    debug: (message: string, ...args: any[]) => void;
    info: (message: string, ...args: any[]) => void;
    warn: (message: string, ...args: any[]) => void;
    error: (message: string, ...args: any[]) => void;
};
export {};
//# sourceMappingURL=logger.d.ts.map