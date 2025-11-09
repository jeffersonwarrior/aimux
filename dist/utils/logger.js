"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.log = exports.logger = exports.Logger = void 0;
exports.setupLogging = setupLogging;
exports.getLogger = getLogger;
class Logger {
    level;
    verbose;
    quiet;
    constructor(options) {
        this.level = options.level;
        this.verbose = options.verbose || false;
        this.quiet = options.quiet || false;
    }
    shouldLog(level) {
        if (this.quiet && level !== 'error') {
            return false;
        }
        if (this.verbose) {
            return true;
        }
        const levels = {
            debug: 0,
            info: 1,
            warn: 2,
            error: 3,
        };
        return levels[level] >= levels[this.level];
    }
    formatMessage(level, message, ...args) {
        const timestamp = new Date().toISOString();
        const prefix = `[${timestamp}] [${level.toUpperCase()}]`;
        if (args.length > 0) {
            return [`${prefix} ${message}`, ...args];
        }
        return [`${prefix} ${message}`];
    }
    debug(message, ...args) {
        if (this.shouldLog('debug')) {
            const [formattedMessage, ...formattedArgs] = this.formatMessage('debug', message, ...args);
            console.debug(formattedMessage, ...formattedArgs);
        }
    }
    info(message, ...args) {
        if (this.shouldLog('info')) {
            const [formattedMessage, ...formattedArgs] = this.formatMessage('info', message, ...args);
            console.info(formattedMessage, ...formattedArgs);
        }
    }
    warn(message, ...args) {
        if (this.shouldLog('warn')) {
            const [formattedMessage, ...formattedArgs] = this.formatMessage('warn', message, ...args);
            console.warn(formattedMessage, ...formattedArgs);
        }
    }
    error(message, ...args) {
        if (this.shouldLog('error')) {
            const [formattedMessage, ...formattedArgs] = this.formatMessage('error', message, ...args);
            console.error(formattedMessage, ...formattedArgs);
        }
    }
    setLevel(level) {
        this.level = level;
    }
    setVerbose(verbose) {
        this.verbose = verbose;
    }
    setQuiet(quiet) {
        this.quiet = quiet;
    }
}
exports.Logger = Logger;
let globalLogger = null;
function setupLogging(verbose = false, quiet = false) {
    let level = 'info';
    if (quiet) {
        level = 'error';
    }
    else if (verbose) {
        level = 'debug';
    }
    globalLogger = new Logger({ level, verbose, quiet });
}
function getLogger() {
    if (!globalLogger) {
        globalLogger = new Logger({ level: 'info' });
    }
    return globalLogger;
}
// Export convenience functions
exports.logger = getLogger();
exports.log = {
    debug: (message, ...args) => getLogger().debug(message, ...args),
    info: (message, ...args) => getLogger().info(message, ...args),
    warn: (message, ...args) => getLogger().warn(message, ...args),
    error: (message, ...args) => getLogger().error(message, ...args),
};
//# sourceMappingURL=logger.js.map