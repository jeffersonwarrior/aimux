"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.ProgressBar = void 0;
const jsx_runtime_1 = require("react/jsx-runtime");
const ink_1 = require("ink");
const ProgressBar = ({ current, total, label, width = 40, character = '█', backgroundColor = 'gray', fillColor = 'green' }) => {
    const percentage = Math.min(100, Math.max(0, (current / total) * 100));
    const filledChars = Math.round((percentage / 100) * width);
    const emptyChars = width - filledChars;
    return ((0, jsx_runtime_1.jsxs)(ink_1.Box, { flexDirection: "column", children: [label && ((0, jsx_runtime_1.jsx)(ink_1.Box, { marginBottom: 1, children: (0, jsx_runtime_1.jsx)(ink_1.Text, { children: label }) })), (0, jsx_runtime_1.jsxs)(ink_1.Box, { children: [(0, jsx_runtime_1.jsx)(ink_1.Text, { color: fillColor, children: character.repeat(filledChars) }), (0, jsx_runtime_1.jsx)(ink_1.Text, { color: backgroundColor, dimColor: true, children: character.repeat(emptyChars) }), (0, jsx_runtime_1.jsxs)(ink_1.Text, { children: [" ", percentage.toFixed(1), "%"] })] }), total > 0 && ((0, jsx_runtime_1.jsxs)(ink_1.Text, { color: "gray", dimColor: true, children: [current, " / ", total] }))] }));
};
exports.ProgressBar = ProgressBar;
//# sourceMappingURL=ProgressBar.js.map