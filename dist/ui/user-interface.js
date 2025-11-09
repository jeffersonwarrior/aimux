"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.UserInterface = void 0;
const jsx_runtime_1 = require("react/jsx-runtime");
const ink_1 = require("ink");
const chalk_1 = __importDefault(require("chalk"));
const ModelSelector_1 = require("./components/ModelSelector");
const StatusMessage_1 = require("./components/StatusMessage");
class UserInterface {
    verbose;
    quiet;
    constructor(options = {}) {
        this.verbose = options.verbose || false;
        this.quiet = options.quiet || false;
    }
    // Simple console output methods
    info(message, ...args) {
        if (!this.quiet) {
            console.log(`ℹ ${message}`, ...args);
        }
    }
    success(message, ...args) {
        if (!this.quiet) {
            console.log(`✓ ${message}`, ...args);
        }
    }
    // Colored success message for important notifications
    coloredSuccess(message, ...args) {
        if (!this.quiet) {
            console.log(chalk_1.default.green(`✓ ${message}`), ...args);
        }
    }
    // Colored info message for important notifications
    coloredInfo(message, ...args) {
        if (!this.quiet) {
            console.log(chalk_1.default.blue(`ℹ ${message}`), ...args);
        }
    }
    // Highlighted message with colored elements within
    highlightInfo(message, highlights = []) {
        if (!this.quiet) {
            let output = chalk_1.default.blue('ℹ ');
            let processedMessage = message;
            // Color each highlighted occurrence
            highlights.forEach(highlight => {
                const regex = new RegExp(`(${highlight})`, 'g');
                processedMessage = processedMessage.replace(regex, chalk_1.default.cyan('$1'));
            });
            output += processedMessage;
            console.log(output);
        }
    }
    warning(message, ...args) {
        if (!this.quiet) {
            console.warn(`⚠ ${message}`, ...args);
        }
    }
    error(message, ...args) {
        console.error(`✗ ${message}`, ...args);
    }
    debug(message, ...args) {
        if (this.verbose) {
            console.debug(`🐛 ${message}`, ...args);
        }
    }
    // Show a simple list of models
    showModelList(models, selectedIndex) {
        if (models.length === 0) {
            this.info('No models available');
            return;
        }
        console.log('\nAvailable Models:');
        console.log('================');
        models.forEach((model, index) => {
            const marker = selectedIndex === index ? '➤' : ' ';
            console.log(`${marker} ${index + 1}. ${model.getDisplayName()}`);
            console.log(`    Provider: ${model.getProvider()}`);
            if (model.owned_by) {
                console.log(`    Owner: ${model.owned_by}`);
            }
            console.log('');
        });
    }
    // Interactive model selection using Ink (single model - for backward compatibility)
    async selectModel(models) {
        if (models.length === 0) {
            this.error('No models available for selection');
            return null;
        }
        return new Promise(resolve => {
            const { waitUntilExit } = (0, ink_1.render)((0, jsx_runtime_1.jsx)(ModelSelector_1.ModelSelector, { models: models, onSelect: (regularModel, thinkingModel) => {
                    const selected = regularModel || thinkingModel;
                    if (selected) {
                        this.success(`Selected model: ${selected.getDisplayName()}`);
                        resolve(selected);
                    }
                    else {
                        this.info('No model selected');
                        resolve(null);
                    }
                }, onCancel: () => {
                    this.info('Model selection cancelled');
                    resolve(null);
                } }));
            waitUntilExit().catch(() => {
                resolve(null);
            });
        });
    }
    // Interactive dual model selection using Ink
    async selectDualModels(models) {
        if (models.length === 0) {
            this.error('No models available for selection');
            return { regular: null, thinking: null };
        }
        return new Promise(resolve => {
            const { waitUntilExit } = (0, ink_1.render)((0, jsx_runtime_1.jsx)(ModelSelector_1.ModelSelector, { models: models, onSelect: (regularModel, thinkingModel) => {
                    if (regularModel || thinkingModel) {
                        if (regularModel)
                            this.success(`Regular model: ${regularModel.getDisplayName()}`);
                        if (thinkingModel)
                            this.success(`Thinking model: ${thinkingModel.getDisplayName()}`);
                    }
                    else {
                        this.info('No models selected');
                    }
                    resolve({ regular: regularModel, thinking: thinkingModel });
                }, onCancel: () => {
                    this.info('Model selection cancelled');
                    resolve({ regular: null, thinking: null });
                } }));
            waitUntilExit().catch(() => {
                resolve({ regular: null, thinking: null });
            });
        });
    }
    // Show progress (simple console version)
    showProgress(current, total, label) {
        if (this.quiet)
            return;
        const percentage = Math.round((current / total) * 100);
        const barLength = 20;
        const filledLength = Math.round((percentage / 100) * barLength);
        const bar = '█'.repeat(filledLength) + '░'.repeat(barLength - filledLength);
        const labelStr = label ? `${label} ` : '';
        process.stdout.write(`\r${labelStr}[${bar}] ${percentage}% (${current}/${total})`);
        if (current >= total) {
            console.log(''); // New line when complete
        }
    }
    // Ask for user input (simple)
    async askQuestion(question, defaultValue) {
        return new Promise(resolve => {
            const readline = require('readline');
            const rl = readline.createInterface({
                input: process.stdin,
                output: process.stdout,
            });
            const prompt = defaultValue ? `${question} (${defaultValue}): ` : `${question}: `;
            rl.question(prompt, (answer) => {
                rl.close();
                resolve(answer || defaultValue || '');
            });
        });
    }
    // Ask for password input (masked with asterisks)
    async askPassword(question) {
        return new Promise(resolve => {
            const readline = require('readline');
            // Store original settings
            const stdin = process.stdin;
            const stdout = process.stdout;
            const wasRaw = stdin.isRaw;
            let password = '';
            stdout.write(`${question}: `);
            // Enable raw mode to capture individual keystrokes
            stdin.setRawMode(true);
            stdin.resume();
            stdin.setEncoding('utf8');
            const onData = (key) => {
                switch (key) {
                    case '\n':
                    case '\r':
                    case '\u0004': // Ctrl+D
                        // Restore original stdin settings
                        stdin.setRawMode(wasRaw);
                        stdin.pause();
                        stdin.removeListener('data', onData);
                        stdout.write('\n');
                        resolve(password);
                        break;
                    case '\u0003': // Ctrl+C
                        // Restore original stdin settings
                        stdin.setRawMode(wasRaw);
                        stdin.pause();
                        stdin.removeListener('data', onData);
                        stdout.write('\n');
                        resolve('');
                        break;
                    case '\u007F': // Backspace (DEL)
                    case '\u0008': // Backspace (BS)
                        if (password.length > 0) {
                            password = password.slice(0, -1);
                            stdout.write('\b \b');
                        }
                        break;
                    default:
                        // Handle multi-character input (like paste) and individual keypresses
                        for (let i = 0; i < key.length; i++) {
                            const char = key[i];
                            // Only accept printable ASCII characters
                            if (char && char >= ' ' && char <= '~') {
                                password += char;
                                stdout.write('*');
                            }
                        }
                }
            };
            stdin.on('data', onData);
        });
    }
    // Confirm action
    async confirm(message, defaultValue = false) {
        const defaultStr = defaultValue ? 'Y/n' : 'y/N';
        const answer = await this.askQuestion(`${message} (${defaultStr})`, defaultValue ? 'y' : 'n');
        return answer.toLowerCase().startsWith('y');
    }
    // Enhanced model selection with provider capabilities
    async selectModelsWithProviders(models, providerCapabilities) {
        // Use existing model selector with enhanced features
        return await this.selectDualModels(models);
    }
    // Show status message using Ink component
    showStatus(type, message) {
        const { waitUntilExit } = (0, ink_1.render)((0, jsx_runtime_1.jsx)(StatusMessage_1.StatusMessage, { type: type, message: message }));
        waitUntilExit();
    }
    // Clear terminal
    clear() {
        console.clear();
    }
}
exports.UserInterface = UserInterface;
//# sourceMappingURL=user-interface.js.map