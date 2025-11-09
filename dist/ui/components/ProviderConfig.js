"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.ProviderConfig = void 0;
const jsx_runtime_1 = require("react/jsx-runtime");
const react_1 = require("react");
const ink_1 = require("ink");
const StatusMessage_1 = require("./StatusMessage");
const ProviderConfig = ({ providers, routing, onSave, onCancel, onTestProvider }) => {
    const { exit } = (0, ink_1.useApp)();
    // State
    const [currentStep, setCurrentStep] = (0, react_1.useState)('overview');
    const [selectedIndex, setSelectedIndex] = (0, react_1.useState)(0);
    const [editedProviders, setEditedProviders] = (0, react_1.useState)(providers);
    const [editedRouting, setEditedRouting] = (0, react_1.useState)(routing);
    const [editingField, setEditingField] = (0, react_1.useState)(null);
    const [testResults, setTestResults] = (0, react_1.useState)({});
    const [isTesting, setIsTesting] = (0, react_1.useState)(false);
    // Reset state when changing steps
    const resetStepState = (0, react_1.useCallback)((step) => {
        setSelectedIndex(0);
        setEditingField(null);
        if (step === 'overview') {
            setEditedProviders(providers);
            setEditedRouting(routing);
        }
    }, [providers, routing]);
    // Navigation helpers
    const goToStep = (0, react_1.useCallback)((step) => {
        resetStepState(step);
        setCurrentStep(step);
    }, [resetStepState]);
    // Provider editing helpers
    const toggleProviderEnabled = (0, react_1.useCallback)((providerId) => {
        setEditedProviders(prev => prev.map(p => p.id === providerId ? { ...p, enabled: !p.enabled } : p));
    }, []);
    const adjustProviderPriority = (0, react_1.useCallback)((providerId, delta) => {
        setEditedProviders(prev => prev.map(p => p.id === providerId ? { ...p, priority: Math.max(1, Math.min(10, p.priority + delta)) } : p));
    }, []);
    const adjustProviderTimeout = (0, react_1.useCallback)((providerId, delta) => {
        setEditedProviders(prev => prev.map(p => p.id === providerId ? { ...p, timeout: Math.max(5000, p.timeout + delta * 1000) } : p));
    }, []);
    const adjustProviderRetries = (0, react_1.useCallback)((providerId, delta) => {
        setEditedProviders(prev => prev.map(p => p.id === providerId ? { ...p, maxRetries: Math.max(0, Math.min(10, p.maxRetries + delta)) } : p));
    }, []);
    // Test all providers
    const testAllProviders = (0, react_1.useCallback)(async () => {
        setIsTesting(true);
        setTestResults({});
        const results = {};
        for (const provider of editedProviders) {
            if (provider.apiKey && provider.apiKey.length > 0) {
                try {
                    const result = await onTestProvider(provider.id);
                    results[provider.id] = result;
                }
                catch (error) {
                    results[provider.id] = false;
                }
            }
        }
        setTestResults(results);
        setIsTesting(false);
    }, [editedProviders, onTestProvider]);
    // Save configuration
    const saveConfiguration = (0, react_1.useCallback)(() => {
        onSave(editedProviders, editedRouting);
        exit();
    }, [editedProviders, editedRouting, onSave, exit]);
    // Get status color for provider
    const getProviderStatusColor = (provider) => {
        if (!provider.enabled)
            return 'gray';
        if (!provider.apiKey || provider.apiKey.length === 0)
            return 'red';
        if (testResults[provider.id] === false)
            return 'red';
        if (testResults[provider.id] === true)
            return 'green';
        return 'yellow';
    };
    // Handle keyboard input
    (0, ink_1.useInput)((input, key) => {
        if (key.escape) {
            onCancel();
            exit();
            return;
        }
        switch (currentStep) {
            case 'overview':
                if (key.upArrow) {
                    setSelectedIndex(prev => Math.max(0, prev - 1));
                }
                else if (key.downArrow) {
                    setSelectedIndex(prev => Math.min(editedProviders.length - 1, prev + 1));
                }
                else if (key.return) {
                    goToStep('edit-provider');
                }
                else if (input === 'r') {
                    goToStep('edit-routing');
                }
                else if (input === 't') {
                    goToStep('test-connection');
                }
                else if (input === 's') {
                    goToStep('confirm-save');
                }
                else if (input === 'q') {
                    onCancel();
                    exit();
                }
                break;
            case 'edit-provider':
                if (key.escape || input === 'b') {
                    goToStep('overview');
                }
                else if (key.upArrow) {
                    setSelectedIndex(prev => Math.max(0, prev - 1));
                }
                else if (key.downArrow) {
                    setSelectedIndex(prev => Math.min(editedProviders.length - 1, prev + 1));
                }
                else if (input === ' ') {
                    const provider = editedProviders[selectedIndex];
                    if (provider) {
                        toggleProviderEnabled(provider.id);
                    }
                }
                else if (key.leftArrow && editingField === 'priority') {
                    const provider = editedProviders[selectedIndex];
                    if (provider) {
                        adjustProviderPriority(provider.id, -1);
                    }
                }
                else if (key.rightArrow && editingField === 'priority') {
                    const provider = editedProviders[selectedIndex];
                    if (provider) {
                        adjustProviderPriority(provider.id, 1);
                    }
                }
                else if (input === '1') {
                    setEditingField('enabled');
                }
                else if (input === '2') {
                    setEditingField('priority');
                }
                else if (input === '3') {
                    setEditingField('timeout');
                }
                else if (input === '4') {
                    setEditingField('maxRetries');
                }
                break;
            case 'edit-routing':
                if (key.escape || input === 'b') {
                    goToStep('overview');
                }
                else if (key.upArrow) {
                    setSelectedIndex(prev => Math.max(0, prev - 1));
                }
                else if (key.downArrow) {
                    setSelectedIndex(prev => Math.min(3, prev + 1)); // 4 routing options
                }
                else if (input === ' ') {
                    // Toggle boolean routing options
                    if (selectedIndex === 2) {
                        setEditedRouting(prev => ({ ...prev, enableFailover: !prev.enableFailover }));
                    }
                    else if (selectedIndex === 3) {
                        setEditedRouting(prev => ({ ...prev, loadBalancing: !prev.loadBalancing }));
                    }
                }
                else if (key.leftArrow || key.rightArrow) {
                    // Change numeric selections
                    if (selectedIndex === 0) {
                        // Change default provider
                        const enabledProviders = editedProviders.filter(p => p.enabled).map(p => p.id);
                        const currentIndex = enabledProviders.indexOf(editedRouting.defaultProvider);
                        const direction = key.leftArrow ? -1 : 1;
                        const newIndex = (currentIndex + direction + enabledProviders.length) % enabledProviders.length;
                        setEditedRouting(prev => ({ ...prev, defaultProvider: enabledProviders[newIndex] || prev.defaultProvider }));
                    }
                }
                break;
            case 'test-connection':
                if (key.escape || input === 'b') {
                    goToStep('overview');
                }
                else if (key.return || input === 't') {
                    testAllProviders();
                }
                break;
            case 'confirm-save':
                if (key.return || input === 'y') {
                    saveConfiguration();
                }
                else if (input === 'n' || key.escape) {
                    goToStep('overview');
                }
                break;
        }
    });
    // Render overview
    const renderOverview = () => ((0, jsx_runtime_1.jsxs)(ink_1.Box, { flexDirection: "column", children: [(0, jsx_runtime_1.jsx)(ink_1.Box, { marginBottom: 1, children: (0, jsx_runtime_1.jsx)(ink_1.Text, { color: "cyan", bold: true, children: "\uD83D\uDD27 Provider Configuration" }) }), (0, jsx_runtime_1.jsx)(ink_1.Box, { marginBottom: 1, children: (0, jsx_runtime_1.jsx)(ink_1.Text, { color: "gray", children: "Configure providers, routing, and test connections." }) }), (0, jsx_runtime_1.jsx)(ink_1.Box, { marginBottom: 1, children: (0, jsx_runtime_1.jsx)(ink_1.Text, { color: "white", bold: true, children: "Configured Providers:" }) }), editedProviders.map((provider, index) => {
                const statusColor = getProviderStatusColor(provider);
                const status = provider.enabled ? '✓' : '✗';
                const testStatus = testResults[provider.id] === true ? '✅' :
                    testResults[provider.id] === false ? '❌' : '❓';
                return ((0, jsx_runtime_1.jsxs)(ink_1.Box, { marginBottom: 1, children: [(0, jsx_runtime_1.jsxs)(ink_1.Text, { color: index === selectedIndex ? 'green' : statusColor, bold: index === selectedIndex, children: [index === selectedIndex ? '▸ ' : '  ', status, " ", provider.name, " (", provider.id, ")"] }), (0, jsx_runtime_1.jsxs)(ink_1.Text, { color: "gray", children: [" | Priority: ", provider.priority] }), (0, jsx_runtime_1.jsxs)(ink_1.Text, { color: "gray", children: [" | ", testStatus] })] }, provider.id));
            }), (0, jsx_runtime_1.jsx)(ink_1.Box, { marginTop: 1, marginBottom: 1, children: (0, jsx_runtime_1.jsx)(ink_1.Text, { color: "white", bold: true, children: "Routing Configuration:" }) }), (0, jsx_runtime_1.jsxs)(ink_1.Box, { marginLeft: 2, flexDirection: "column", children: [(0, jsx_runtime_1.jsxs)(ink_1.Text, { color: "gray", children: ["Default: ", editedRouting.defaultProvider, " | Strategy: ", editedRouting.strategy] }), (0, jsx_runtime_1.jsxs)(ink_1.Text, { color: "gray", children: ["Failover: ", editedRouting.enableFailover ? 'Enabled' : 'Disabled', " | Load Balancing: ", editedRouting.loadBalancing ? 'Enabled' : 'Disabled'] })] }), (0, jsx_runtime_1.jsx)(ink_1.Box, { marginTop: 1, children: (0, jsx_runtime_1.jsx)(ink_1.Text, { color: "gray", children: "\u2191\u2193 Navigate | Enter: Edit Provider | r: Routing | t: Test | s: Save | q: Quit" }) })] }));
    // Render provider edit
    const renderEditProvider = () => {
        if (editedProviders.length === 0) {
            return ((0, jsx_runtime_1.jsxs)(ink_1.Box, { flexDirection: "column", children: [(0, jsx_runtime_1.jsx)(StatusMessage_1.StatusMessage, { type: "error", message: "No providers configured" }), (0, jsx_runtime_1.jsx)(ink_1.Text, { color: "gray", children: "Press 'b' to go back" })] }));
        }
        const provider = editedProviders[selectedIndex];
        if (!provider)
            return null;
        return ((0, jsx_runtime_1.jsxs)(ink_1.Box, { flexDirection: "column", children: [(0, jsx_runtime_1.jsx)(ink_1.Box, { marginBottom: 1, children: (0, jsx_runtime_1.jsxs)(ink_1.Text, { color: "cyan", bold: true, children: ["Edit Provider: ", provider.name] }) }), (0, jsx_runtime_1.jsxs)(ink_1.Box, { flexDirection: "column", marginBottom: 1, children: [(0, jsx_runtime_1.jsx)(ink_1.Box, { marginBottom: 1, children: (0, jsx_runtime_1.jsxs)(ink_1.Text, { color: editingField === 'enabled' ? 'green' : 'white', children: ["1. Enabled: ", provider.enabled ? 'Yes' : 'No', " ", editingField === 'enabled' && '(Space to toggle)'] }) }), (0, jsx_runtime_1.jsx)(ink_1.Box, { marginBottom: 1, children: (0, jsx_runtime_1.jsxs)(ink_1.Text, { color: editingField === 'priority' ? 'green' : 'white', children: ["2. Priority: ", provider.priority, " ", editingField === 'priority' && '(← → to adjust)'] }) }), (0, jsx_runtime_1.jsx)(ink_1.Box, { marginBottom: 1, children: (0, jsx_runtime_1.jsxs)(ink_1.Text, { color: editingField === 'timeout' ? 'green' : 'white', children: ["3. Timeout: ", provider.timeout, "ms ", editingField === 'timeout' && '(← → to adjust)'] }) }), (0, jsx_runtime_1.jsx)(ink_1.Box, { marginBottom: 1, children: (0, jsx_runtime_1.jsxs)(ink_1.Text, { color: editingField === 'maxRetries' ? 'green' : 'white', children: ["4. Max Retries: ", provider.maxRetries, " ", editingField === 'maxRetries' && '(← → to adjust)'] }) })] }), (0, jsx_runtime_1.jsxs)(ink_1.Box, { flexDirection: "column", marginBottom: 1, children: [(0, jsx_runtime_1.jsx)(ink_1.Text, { color: "white", bold: true, children: "Provider Information:" }), (0, jsx_runtime_1.jsxs)(ink_1.Text, { color: "gray", children: ["Capabilities: ", provider.capabilities.join(', ')] }), (0, jsx_runtime_1.jsxs)(ink_1.Text, { color: "gray", children: ["Cost Level: ", provider.costLevel] }), (0, jsx_runtime_1.jsxs)(ink_1.Text, { color: "gray", children: ["Speed Level: ", provider.speedLevel] }), (0, jsx_runtime_1.jsxs)(ink_1.Text, { color: "gray", children: ["API Key: ", provider.apiKey ? `${'*'.repeat(provider.apiKey.length - 4)}${provider.apiKey.slice(-4)}` : 'Not set'] })] }), (0, jsx_runtime_1.jsx)(ink_1.Box, { marginTop: 1, children: (0, jsx_runtime_1.jsx)(ink_1.Text, { color: "gray", children: "1-4: Select field | Space: Toggle enabled | \u2190 \u2192: Adjust value | \u2191\u2193: Change provider | b: Back" }) })] }));
    };
    // Render routing edit
    const renderEditRouting = () => {
        const routingOptions = [
            `Default Provider: ${editedRouting.defaultProvider}`,
            `Strategy: ${editedRouting.strategy}`,
            `Enable Failover: ${editedRouting.enableFailover ? 'Yes' : 'No'}`,
            `Enable Load Balancing: ${editedRouting.loadBalancing ? 'Yes' : 'No'}`
        ];
        return ((0, jsx_runtime_1.jsxs)(ink_1.Box, { flexDirection: "column", children: [(0, jsx_runtime_1.jsx)(ink_1.Box, { marginBottom: 1, children: (0, jsx_runtime_1.jsx)(ink_1.Text, { color: "cyan", bold: true, children: "Edit Routing Configuration" }) }), routingOptions.map((option, index) => ((0, jsx_runtime_1.jsxs)(ink_1.Box, { marginBottom: 1, children: [(0, jsx_runtime_1.jsxs)(ink_1.Text, { color: index === selectedIndex ? 'green' : 'white', bold: index === selectedIndex, children: [index === selectedIndex ? '▸ ' : '  ', index + 1, ". ", option] }), index === 0 && ((0, jsx_runtime_1.jsx)(ink_1.Text, { color: "gray", children: " (\u2190 \u2192 to change provider)" })), (index === 2 || index === 3) && ((0, jsx_runtime_1.jsx)(ink_1.Text, { color: "gray", children: " (Space to toggle)" }))] }, index))), (0, jsx_runtime_1.jsx)(ink_1.Box, { marginTop: 1, children: (0, jsx_runtime_1.jsx)(ink_1.Text, { color: "gray", children: "\u2191\u2193: Navigate | Space: Toggle option | \u2190 \u2192: Change provider | b: Back" }) })] }));
    };
    // Render test connection
    const renderTestConnection = () => ((0, jsx_runtime_1.jsxs)(ink_1.Box, { flexDirection: "column", children: [(0, jsx_runtime_1.jsx)(ink_1.Box, { marginBottom: 1, children: (0, jsx_runtime_1.jsx)(ink_1.Text, { color: "cyan", bold: true, children: "Test Provider Connections" }) }), isTesting ? ((0, jsx_runtime_1.jsx)(ink_1.Box, { marginBottom: 1, children: (0, jsx_runtime_1.jsx)(ink_1.Text, { color: "yellow", children: "Testing connections to all providers..." }) })) : ((0, jsx_runtime_1.jsxs)(jsx_runtime_1.Fragment, { children: [(0, jsx_runtime_1.jsx)(ink_1.Box, { marginBottom: 1, children: (0, jsx_runtime_1.jsx)(ink_1.Text, { color: "gray", children: "Connection test results:" }) }), editedProviders.map(provider => {
                        const result = testResults[provider.id];
                        const status = result === true ? '✅ Connected' :
                            result === false ? '❌ Failed' : '❓ Not Tested';
                        return ((0, jsx_runtime_1.jsx)(ink_1.Box, { marginBottom: 1, children: (0, jsx_runtime_1.jsxs)(ink_1.Text, { children: [provider.name, ": ", status] }) }, provider.id));
                    }), Object.keys(testResults).length > 0 && ((0, jsx_runtime_1.jsx)(ink_1.Box, { marginTop: 1, children: (0, jsx_runtime_1.jsx)(StatusMessage_1.StatusMessage, { type: Object.values(testResults).every(Boolean) ? 'success' : 'warning', message: `${Object.values(testResults).filter(Boolean).length}/${editedProviders.length} providers connected` }) }))] })), (0, jsx_runtime_1.jsx)(ink_1.Box, { marginTop: 1, children: (0, jsx_runtime_1.jsx)(ink_1.Text, { color: "gray", children: "t: Test Connections | b: Back" }) })] }));
    // Render confirm save
    const renderConfirmSave = () => ((0, jsx_runtime_1.jsxs)(ink_1.Box, { flexDirection: "column", children: [(0, jsx_runtime_1.jsx)(ink_1.Box, { marginBottom: 1, children: (0, jsx_runtime_1.jsx)(ink_1.Text, { color: "cyan", bold: true, children: "Confirm Save Configuration" }) }), (0, jsx_runtime_1.jsx)(ink_1.Box, { marginBottom: 1, children: (0, jsx_runtime_1.jsx)(ink_1.Text, { color: "white", children: "Are you sure you want to save these configuration changes?" }) }), (0, jsx_runtime_1.jsx)(ink_1.Box, { marginTop: 1, children: (0, jsx_runtime_1.jsx)(ink_1.Text, { color: "gray", children: "y: Yes, save | n: No, go back | Escape: Cancel" }) })] }));
    // Main render
    switch (currentStep) {
        case 'overview':
            return renderOverview();
        case 'edit-provider':
            return renderEditProvider();
        case 'edit-routing':
            return renderEditRouting();
        case 'test-connection':
            return renderTestConnection();
        case 'confirm-save':
            return renderConfirmSave();
        default:
            return (0, jsx_runtime_1.jsx)(StatusMessage_1.StatusMessage, { type: "error", message: "Invalid configuration step" });
    }
};
exports.ProviderConfig = ProviderConfig;
exports.default = exports.ProviderConfig;
//# sourceMappingURL=ProviderConfig.js.map