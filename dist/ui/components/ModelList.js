"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.ModelList = void 0;
const jsx_runtime_1 = require("react/jsx-runtime");
const ink_1 = require("ink");
const ModelList = ({ models, selectedIndex, showCategories = false }) => {
    if (models.length === 0) {
        return ((0, jsx_runtime_1.jsxs)(ink_1.Box, { flexDirection: "column", children: [(0, jsx_runtime_1.jsx)(ink_1.Text, { color: "gray", children: "No models available." }), (0, jsx_runtime_1.jsx)(ink_1.Newline, {}), (0, jsx_runtime_1.jsx)(ink_1.Text, { color: "gray", children: "Try running 'synclaude models --refresh' to update the model list." })] }));
    }
    return ((0, jsx_runtime_1.jsx)(ink_1.Box, { flexDirection: "column", children: models.map((model, index) => ((0, jsx_runtime_1.jsxs)(ink_1.Box, { marginBottom: 1, children: [(0, jsx_runtime_1.jsxs)(ink_1.Text, { color: selectedIndex === index ? 'blue' : 'white', children: [selectedIndex === index ? '➤ ' : '  ', index + 1, ". ", model.getDisplayName()] }), (0, jsx_runtime_1.jsx)(ink_1.Newline, {}), (0, jsx_runtime_1.jsxs)(ink_1.Text, { color: "gray", dimColor: true, children: ['    ', "Provider: ", model.getProvider()] }), model.context_length && ((0, jsx_runtime_1.jsxs)(jsx_runtime_1.Fragment, { children: [(0, jsx_runtime_1.jsx)(ink_1.Newline, {}), (0, jsx_runtime_1.jsxs)(ink_1.Text, { color: "gray", dimColor: true, children: ['    ', "Context: ", Math.round(model.context_length / 1024), "K tokens"] })] })), model.quantization && ((0, jsx_runtime_1.jsxs)(jsx_runtime_1.Fragment, { children: [(0, jsx_runtime_1.jsx)(ink_1.Newline, {}), (0, jsx_runtime_1.jsxs)(ink_1.Text, { color: "gray", dimColor: true, children: ['    ', "Quantization: ", model.quantization] })] })), model.owned_by && ((0, jsx_runtime_1.jsxs)(jsx_runtime_1.Fragment, { children: [(0, jsx_runtime_1.jsx)(ink_1.Newline, {}), (0, jsx_runtime_1.jsxs)(ink_1.Text, { color: "gray", dimColor: true, children: ['    ', "Owner: ", model.owned_by] })] }))] }, model.id))) }));
};
exports.ModelList = ModelList;
//# sourceMappingURL=ModelList.js.map