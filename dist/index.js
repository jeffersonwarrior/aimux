"use strict";
var __createBinding = (this && this.__createBinding) || (Object.create ? (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    var desc = Object.getOwnPropertyDescriptor(m, k);
    if (!desc || ("get" in desc ? !m.__esModule : desc.writable || desc.configurable)) {
      desc = { enumerable: true, get: function() { return m[k]; } };
    }
    Object.defineProperty(o, k2, desc);
}) : (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    o[k2] = m[k];
}));
var __exportStar = (this && this.__exportStar) || function(m, exports) {
    for (var p in m) if (p !== "default" && !Object.prototype.hasOwnProperty.call(exports, p)) __createBinding(exports, m, p);
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.SyntheticClaudeApp = void 0;
// Main entry point for Synclaude
__exportStar(require("./cli"), exports);
__exportStar(require("./core"), exports);
__exportStar(require("./config"), exports);
__exportStar(require("./models"), exports);
__exportStar(require("./ui"), exports);
__exportStar(require("./launcher"), exports);
__exportStar(require("./utils"), exports);
__exportStar(require("./api"), exports);
// Default export for convenience
var app_1 = require("./core/app");
Object.defineProperty(exports, "SyntheticClaudeApp", { enumerable: true, get: function () { return app_1.SyntheticClaudeApp; } });
//# sourceMappingURL=index.js.map