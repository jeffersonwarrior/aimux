"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.StatusMessage = void 0;
const jsx_runtime_1 = require("react/jsx-runtime");
const ink_1 = require("ink");
const statusConfig = {
    info: { color: 'blue', icon: 'ℹ' },
    success: { color: 'green', icon: '✓' },
    warning: { color: 'yellow', icon: '⚠' },
    error: { color: 'red', icon: '✗' },
};
const StatusMessage = ({ type, message, icon }) => {
    const config = statusConfig[type];
    const displayIcon = icon || config.icon;
    return ((0, jsx_runtime_1.jsx)(ink_1.Box, { children: (0, jsx_runtime_1.jsxs)(ink_1.Text, { color: config.color, bold: true, children: [displayIcon, " ", message] }) }));
};
exports.StatusMessage = StatusMessage;
//# sourceMappingURL=StatusMessage.js.map