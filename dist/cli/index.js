#!/usr/bin/env node
"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.main = main;
const commands_1 = require("./commands");
async function main() {
    try {
        const program = (0, commands_1.createProgram)();
        // Parse command line arguments
        program.parse(process.argv);
    }
    catch (error) {
        console.error('Fatal error:', error);
        process.exit(1);
    }
}
// Handle unhandled promise rejections
process.on('unhandledRejection', (reason, promise) => {
    console.error('Unhandled Rejection at:', promise, 'reason:', reason);
    process.exit(1);
});
// Handle uncaught exceptions
process.on('uncaughtException', error => {
    console.error('Uncaught Exception:', error);
    process.exit(1);
});
// Run main function
if (require.main === module) {
    main();
}
//# sourceMappingURL=index.js.map