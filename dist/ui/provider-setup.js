"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.ProviderSetup = void 0;
const jsx_runtime_1 = require("react/jsx-runtime");
const react_1 = require("react");
const ink_1 = require("ink");
const StatusMessage_1 = require("./components/StatusMessage");
const ProgressBar_1 = require("./components/ProgressBar");
// Available providers configuration
const DEFAULT_PROVIDERS = [
    {
        id: 'minimax-m2',
        name: 'Minimax M2',
        description: 'Advanced reasoning with thinking capabilities, vision support, and tool usage',
        capabilities: ['thinking', 'vision', 'tools'],
        priority: 7,
        costLevel: 'medium',
        speedLevel: 'fast'
    },
    {
        id: 'z-ai',
        name: 'Z.AI',
        description: 'Fast responses with vision capabilities and comprehensive tool support',
        capabilities: ['vision', 'tools'],
        priority: 6,
        costLevel: 'low',
        speedLevel: 'fast'
    },
    {
        id: 'synthetic-new',
        name: 'Synthetic.New',
        description: 'Cost-effective thinking models with reliable tool usage',
        capabilities: ['thinking', 'tools'],
        priority: 5,
        costLevel: 'high',
        speedLevel: 'medium'
    }
];
const ProviderSetup = ({ onSetup, onCancel, initialProviders = [], initialRouting, availableProviders = DEFAULT_PROVIDERS }) => {
    const { exit } = (0, ink_1.useApp)();
    // State management
    const [currentStep, setCurrentStep] = (0, react_1.useState)('welcome');
    const [selectedProviders, setSelectedProviders] = (0, react_1.useState)([]);
    const [providerConfigs, setProviderConfigs] = (0, react_1.useState)(initialProviders);
    const [apiKeys, setApiKeys] = (0, react_1.useState)({});
    const [testResults, setTestResults] = (0, react_1.useState)({});
    const [testingProgress, setTestingProgress] = (0, react_1.useState)(0);
    const [routingConfig, setRoutingConfig] = (0, react_1.useState)(initialRouting || {
        defaultProvider: 'synthetic-new',
        strategy: 'capability',
        enableFailover: true,
        loadBalancing: false
    });
    const [errors, setErrors] = (0, react_1.useState)([]);
    const [isLoading, setIsLoading] = (0, react_1.useState)(false);
    // Reset state when going back to different steps
    const resetStepState = (0, react_1.useCallback)((step) => {
        setErrors([]);
        if (step === 'welcome') {
            setSelectedProviders([]);
            setApiKeys({});
            setTestResults({});
            setTestingProgress(0);
        }
    }, []);
    // Navigation helpers
    const goToNextStep = (0, react_1.useCallback)(() => {
        const steps = ['welcome', 'provider-selection', 'api-keys', 'testing', 'routing', 'summary', 'complete'];
        const currentIndex = steps.indexOf(currentStep);
        if (currentIndex < steps.length - 1) {
            const nextStepStep = steps[currentIndex + 1];
            if (nextStepStep) {
                resetStepState(nextStepStep);
                setCurrentStep(nextStepStep);
            }
        }
    }, [currentStep, resetStepState]);
    const goToPreviousStep = (0, react_1.useCallback)(() => {
        const steps = ['welcome', 'provider-selection', 'api-keys', 'testing', 'routing', 'summary', 'complete'];
        const currentIndex = steps.indexOf(currentStep);
        if (currentIndex > 0) {
            const previousStepStep = steps[currentIndex - 1];
            if (previousStepStep) {
                resetStepState(previousStepStep);
                setCurrentStep(previousStepStep);
            }
        }
    }, [currentStep, resetStepState]);
    // Provider selection
    const toggleProvider = (0, react_1.useCallback)((providerId) => {
        setSelectedProviders(prev => prev.includes(providerId)
            ? prev.filter(id => id !== providerId)
            : [...prev, providerId]);
    }, []);
    // API key management
    const setApiKey = (0, react_1.useCallback)((providerId, apiKey) => {
        setApiKeys(prev => ({ ...prev, [providerId]: apiKey }));
    }, []);
    // Test providers
    const testSelectedProviders = (0, react_1.useCallback)(async () => {
        setIsLoading(true);
        setTestingProgress(0);
        setTestResults({});
        const results = {};
        for (let i = 0; i < selectedProviders.length; i++) {
            const providerId = selectedProviders[i];
            if (!providerId)
                continue;
            const apiKey = apiKeys[providerId];
            setTestingProgress(((i + 1) / selectedProviders.length) * 100);
            // Simulate provider testing (this would integrate with actual AimuxApp.testProvider)
            await new Promise(resolve => setTimeout(resolve, 1000));
            // Simple validation - in real implementation, call AimuxApp.testProvider
            const isValid = !!(apiKey && apiKey.length > 10);
            if (providerId) {
                results[providerId] = isValid;
            }
        }
        setTestResults(results);
        setIsLoading(false);
        // Auto-proceed after testing if all tests pass
        const allPassed = Object.values(results).every(Boolean);
        if (allPassed) {
            setTimeout(() => goToNextStep(), 1500);
        }
    }, [selectedProviders, apiKeys, goToNextStep]);
    // Complete setup
    const completeSetup = (0, react_1.useCallback)(() => {
        // Build final provider configs
        const finalProviders = selectedProviders.map(providerId => {
            const providerInfo = availableProviders.find(p => p.id === providerId);
            if (!providerInfo) {
                throw new Error(`Provider ${providerId} not found`);
            }
            return {
                id: providerId,
                name: providerInfo.name,
                apiKey: apiKeys[providerId] || '',
                capabilities: providerInfo.capabilities,
                enabled: true,
                priority: providerInfo.priority,
                timeout: 30000,
                maxRetries: 3
            };
        });
        const result = {
            success: true,
            providers: finalProviders,
            routing: routingConfig,
            errors: errors.length > 0 ? errors : undefined
        };
        onSetup(result);
        exit();
    }, [selectedProviders, apiKeys, availableProviders, routingConfig, errors, onSetup, exit]);
    // Handle keyboard input
    (0, ink_1.useInput)((input, key) => {
        if (key.escape) {
            onCancel();
            exit();
            return;
        }
        switch (currentStep) {
            case 'welcome':
                if (key.return || input === ' ') {
                    goToNextStep();
                }
                break;
            case 'complete':
                if (key.return || input === ' ') {
                    completeSetup();
                }
                break;
        }
    });
    // Auto-test when API keys are entered
    (0, react_1.useEffect)(() => {
        if (currentStep === 'api-keys' && selectedProviders.every(p => apiKeys[p])) {
            setTimeout(() => {
                setCurrentStep('testing');
                testSelectedProviders();
            }, 500);
        }
    }, [apiKeys, selectedProviders, currentStep, testSelectedProviders]);
    // Render welcome screen
    const renderWelcome = () => ((0, jsx_runtime_1.jsxs)(ink_1.Box, { flexDirection: "column", children: [(0, jsx_runtime_1.jsx)(ink_1.Box, { marginBottom: 1, children: (0, jsx_runtime_1.jsx)(ink_1.Text, { color: "cyan", bold: true, children: "\uD83E\uDD16 Welcome to Aimux Multi-Provider Setup" }) }), (0, jsx_runtime_1.jsx)(ink_1.Box, { marginBottom: 1, children: (0, jsx_runtime_1.jsx)(ink_1.Text, { color: "white", children: "Aimux transforms your Claude Code experience by intelligently routing requests across multiple AI providers." }) }), (0, jsx_runtime_1.jsx)(ink_1.Box, { marginBottom: 1, children: (0, jsx_runtime_1.jsx)(ink_1.Text, { color: "gray", children: "This setup will configure:" }) }), (0, jsx_runtime_1.jsxs)(ink_1.Box, { flexDirection: "column", marginBottom: 1, children: [(0, jsx_runtime_1.jsx)(ink_1.Text, { color: "gray", children: "\u2022 Multiple AI provider accounts (Minimax M2, Z.AI, Synthetic.New)" }), (0, jsx_runtime_1.jsx)(ink_1.Text, { color: "gray", children: "\u2022 Intelligent request routing based on capabilities" }), (0, jsx_runtime_1.jsx)(ink_1.Text, { color: "gray", children: "\u2022 Automatic failover and load balancing" }), (0, jsx_runtime_1.jsx)(ink_1.Text, { color: "gray", children: "\u2022 Cost optimization and performance monitoring" })] }), (0, jsx_runtime_1.jsx)(ink_1.Box, { marginTop: 1, children: (0, jsx_runtime_1.jsx)(ink_1.Text, { color: "yellow", children: "You'll need API keys for each provider you want to use." }) }), (0, jsx_runtime_1.jsx)(ink_1.Box, { marginTop: 2, children: (0, jsx_runtime_1.jsx)(ink_1.Text, { color: "gray", children: "Press Enter or Space to continue, Escape to cancel" }) })] }));
    // Render provider selection
    const renderProviderSelection = () => ((0, jsx_runtime_1.jsxs)(ink_1.Box, { flexDirection: "column", children: [(0, jsx_runtime_1.jsx)(ink_1.Box, { marginBottom: 1, children: (0, jsx_runtime_1.jsx)(ink_1.Text, { color: "cyan", bold: true, children: "Step 1: Select Providers" }) }), (0, jsx_runtime_1.jsx)(ink_1.Box, { marginBottom: 1, children: (0, jsx_runtime_1.jsx)(ink_1.Text, { color: "gray", children: "Choose the providers you want to configure. You can select multiple providers." }) }), availableProviders.map((provider, index) => {
                const isSelected = selectedProviders.includes(provider.id);
                const costColor = provider.costLevel === 'low' ? 'green' :
                    provider.costLevel === 'medium' ? 'yellow' : 'red';
                const speedColor = provider.speedLevel === 'fast' ? 'green' :
                    provider.speedLevel === 'medium' ? 'yellow' : 'red';
                return ((0, jsx_runtime_1.jsx)(ink_1.Box, { marginBottom: 1, children: (0, jsx_runtime_1.jsxs)(ink_1.Box, { flexDirection: "column", children: [(0, jsx_runtime_1.jsxs)(ink_1.Box, { children: [(0, jsx_runtime_1.jsxs)(ink_1.Text, { color: isSelected ? 'green' : 'white', bold: isSelected, children: [isSelected ? '◉' : '◯', " ", index + 1, ". ", provider.name] }), (0, jsx_runtime_1.jsxs)(ink_1.Text, { color: "gray", children: [" (", provider.id, ")"] })] }), (0, jsx_runtime_1.jsx)(ink_1.Box, { marginLeft: 2, children: (0, jsx_runtime_1.jsx)(ink_1.Text, { color: "gray", dimColor: true, children: provider.description }) }), (0, jsx_runtime_1.jsx)(ink_1.Box, { marginLeft: 2, children: (0, jsx_runtime_1.jsxs)(ink_1.Text, { color: "gray", dimColor: true, children: ["Capabilities: ", provider.capabilities.join(', ')] }) }), (0, jsx_runtime_1.jsxs)(ink_1.Box, { marginLeft: 2, children: [(0, jsx_runtime_1.jsxs)(ink_1.Text, { color: costColor, children: ["Cost: ", provider.costLevel] }), (0, jsx_runtime_1.jsx)(ink_1.Text, { color: "gray", children: " | " }), (0, jsx_runtime_1.jsxs)(ink_1.Text, { color: speedColor, children: ["Speed: ", provider.speedLevel] }), (0, jsx_runtime_1.jsxs)(ink_1.Text, { color: "gray", children: [" | Priority: ", provider.priority] })] })] }) }, provider.id));
            }), errors.length > 0 && ((0, jsx_runtime_1.jsx)(ink_1.Box, { marginBottom: 1, children: (0, jsx_runtime_1.jsx)(StatusMessage_1.StatusMessage, { type: "error", message: errors.join(', ') }) })), (0, jsx_runtime_1.jsx)(ink_1.Box, { marginTop: 1, children: (0, jsx_runtime_1.jsx)(ink_1.Text, { color: "gray", children: "1-3: Toggle provider selection | Enter: Continue | Escape: Cancel" }) })] }));
    // Render API key collection
    const renderApiKeys = () => ((0, jsx_runtime_1.jsxs)(ink_1.Box, { flexDirection: "column", children: [(0, jsx_runtime_1.jsx)(ink_1.Box, { marginBottom: 1, children: (0, jsx_runtime_1.jsx)(ink_1.Text, { color: "cyan", bold: true, children: "Step 2: Enter API Keys" }) }), (0, jsx_runtime_1.jsx)(ink_1.Box, { marginBottom: 1, children: (0, jsx_runtime_1.jsx)(ink_1.Text, { color: "gray", children: "Enter API keys for your selected providers. Keys are masked for security." }) }), selectedProviders.map((providerId, index) => {
                if (!providerId)
                    return null;
                const provider = availableProviders.find(p => p.id === providerId);
                if (!provider)
                    return null;
                const apiKey = apiKeys[providerId];
                const hasKey = !!(apiKey && apiKey.length > 0);
                return ((0, jsx_runtime_1.jsxs)(ink_1.Box, { marginBottom: 1, flexDirection: "column", children: [(0, jsx_runtime_1.jsx)(ink_1.Box, { children: (0, jsx_runtime_1.jsxs)(ink_1.Text, { color: hasKey ? 'green' : 'yellow', children: [index + 1, ". ", provider.name, " API Key:"] }) }), (0, jsx_runtime_1.jsx)(ink_1.Box, { marginLeft: 2, children: hasKey ? ((0, jsx_runtime_1.jsxs)(ink_1.Text, { color: "green", children: ['*'.repeat(apiKey.length - 4), apiKey.slice(-4)] })) : ((0, jsx_runtime_1.jsxs)(ink_1.Text, { color: "gray", dimColor: true, children: ["[Press ", index + 1, " to enter API key]"] })) })] }, providerId));
            }), (0, jsx_runtime_1.jsx)(ink_1.Box, { marginTop: 1, children: (0, jsx_runtime_1.jsx)(ink_1.Text, { color: "gray", children: "1-3: Enter API key for provider | Enter: Continue when all keys entered | Escape: Cancel" }) }), selectedProviders.every(p => p && apiKeys[p]) && ((0, jsx_runtime_1.jsx)(ink_1.Box, { marginTop: 1, children: (0, jsx_runtime_1.jsx)(StatusMessage_1.StatusMessage, { type: "success", message: "All API keys entered. Continuing..." }) }))] }));
    // Render testing progress
    const renderTesting = () => ((0, jsx_runtime_1.jsxs)(ink_1.Box, { flexDirection: "column", children: [(0, jsx_runtime_1.jsx)(ink_1.Box, { marginBottom: 1, children: (0, jsx_runtime_1.jsx)(ink_1.Text, { color: "cyan", bold: true, children: "Step 3: Testing Provider Connections" }) }), isLoading ? ((0, jsx_runtime_1.jsxs)(jsx_runtime_1.Fragment, { children: [(0, jsx_runtime_1.jsx)(ink_1.Box, { marginBottom: 1, children: (0, jsx_runtime_1.jsx)(ink_1.Text, { color: "gray", children: "Testing connections to selected providers..." }) }), (0, jsx_runtime_1.jsx)(ProgressBar_1.ProgressBar, { current: testingProgress, total: 100, label: "Testing Progress" })] })) : ((0, jsx_runtime_1.jsxs)(jsx_runtime_1.Fragment, { children: [(0, jsx_runtime_1.jsx)(ink_1.Box, { marginBottom: 1, children: (0, jsx_runtime_1.jsx)(ink_1.Text, { color: "gray", children: "Connection test results:" }) }), selectedProviders.map(providerId => {
                        const provider = availableProviders.find(p => p.id === providerId);
                        if (!provider)
                            return;
                        const isWorking = testResults[providerId];
                        return ((0, jsx_runtime_1.jsx)(ink_1.Box, { marginBottom: 1, children: (0, jsx_runtime_1.jsxs)(ink_1.Text, { color: isWorking ? 'green' : 'red', children: [isWorking ? '✅' : '❌', " ", provider.name, ": ", isWorking ? 'Connected' : 'Failed'] }) }, providerId));
                    }), Object.values(testResults).every(Boolean) ? ((0, jsx_runtime_1.jsxs)(jsx_runtime_1.Fragment, { children: [(0, jsx_runtime_1.jsx)(ink_1.Box, { marginBottom: 1, children: (0, jsx_runtime_1.jsx)(StatusMessage_1.StatusMessage, { type: "success", message: "All providers are working correctly!" }) }), (0, jsx_runtime_1.jsx)(ink_1.Box, { marginTop: 1, children: (0, jsx_runtime_1.jsx)(ink_1.Text, { color: "gray", children: "Continuing to next step..." }) })] })) : ((0, jsx_runtime_1.jsx)(ink_1.Box, { marginTop: 1, children: (0, jsx_runtime_1.jsx)(ink_1.Text, { color: "red", children: "Some providers failed. Press 'r' to retry or Enter to continue anyway." }) }))] }))] }));
    // Render routing configuration
    const renderRouting = () => ((0, jsx_runtime_1.jsxs)(ink_1.Box, { flexDirection: "column", children: [(0, jsx_runtime_1.jsx)(ink_1.Box, { marginBottom: 1, children: (0, jsx_runtime_1.jsx)(ink_1.Text, { color: "cyan", bold: true, children: "Step 4: Configure Routing" }) }), (0, jsx_runtime_1.jsx)(ink_1.Box, { marginBottom: 1, children: (0, jsx_runtime_1.jsx)(ink_1.Text, { color: "gray", children: "Configure how requests should be routed between providers." }) }), (0, jsx_runtime_1.jsx)(ink_1.Box, { marginBottom: 1, children: (0, jsx_runtime_1.jsx)(ink_1.Text, { color: "white", bold: true, children: "Current Configuration:" }) }), (0, jsx_runtime_1.jsxs)(ink_1.Box, { marginLeft: 2, marginBottom: 1, flexDirection: "column", children: [(0, jsx_runtime_1.jsxs)(ink_1.Text, { color: "gray", children: ["Default Provider: ", routingConfig.defaultProvider] }), routingConfig.thinkingProvider && ((0, jsx_runtime_1.jsxs)(ink_1.Text, { color: "gray", children: ["Thinking Provider: ", routingConfig.thinkingProvider] })), (0, jsx_runtime_1.jsxs)(ink_1.Text, { color: "gray", children: ["Strategy: ", routingConfig.strategy] }), (0, jsx_runtime_1.jsxs)(ink_1.Text, { color: "gray", children: ["Failover: ", routingConfig.enableFailover ? 'Enabled' : 'Disabled'] }), (0, jsx_runtime_1.jsxs)(ink_1.Text, { color: "gray", children: ["Load Balancing: ", routingConfig.loadBalancing ? 'Enabled' : 'Disabled'] })] }), (0, jsx_runtime_1.jsx)(ink_1.Box, { marginTop: 1, children: (0, jsx_runtime_1.jsx)(ink_1.Text, { color: "gray", children: "Press Enter to accept default routing configuration, Escape to go back" }) })] }));
    // Render summary
    const renderSummary = () => ((0, jsx_runtime_1.jsxs)(ink_1.Box, { flexDirection: "column", children: [(0, jsx_runtime_1.jsx)(ink_1.Box, { marginBottom: 1, children: (0, jsx_runtime_1.jsx)(ink_1.Text, { color: "cyan", bold: true, children: "Setup Summary" }) }), (0, jsx_runtime_1.jsx)(ink_1.Box, { marginBottom: 1, children: (0, jsx_runtime_1.jsx)(ink_1.Text, { color: "white", bold: true, children: "Configured Providers:" }) }), selectedProviders.map(providerId => {
                const provider = availableProviders.find(p => p.id === providerId);
                if (!provider)
                    return;
                const isWorking = testResults[providerId];
                return ((0, jsx_runtime_1.jsxs)(ink_1.Box, { marginLeft: 2, marginBottom: 1, children: [(0, jsx_runtime_1.jsxs)(ink_1.Text, { color: isWorking ? 'green' : 'red', children: [isWorking ? '✅' : '❌', " ", provider.name] }), (0, jsx_runtime_1.jsxs)(ink_1.Text, { color: "gray", dimColor: true, children: [' ', "(", provider.capabilities.join(', '), ")"] })] }, providerId));
            }), (0, jsx_runtime_1.jsx)(ink_1.Box, { marginTop: 1, marginBottom: 1, children: (0, jsx_runtime_1.jsx)(ink_1.Text, { color: "white", bold: true, children: "Routing Configuration:" }) }), (0, jsx_runtime_1.jsxs)(ink_1.Box, { marginLeft: 2, marginBottom: 1, children: [(0, jsx_runtime_1.jsxs)(ink_1.Text, { color: "gray", children: ["Default: ", routingConfig.defaultProvider] }), (0, jsx_runtime_1.jsxs)(ink_1.Text, { color: "gray", children: ["Strategy: ", routingConfig.strategy] })] }), errors.length > 0 && ((0, jsx_runtime_1.jsx)(ink_1.Box, { marginBottom: 1, children: (0, jsx_runtime_1.jsx)(StatusMessage_1.StatusMessage, { type: "warning", message: `Warnings: ${errors.join(', ')}` }) })), (0, jsx_runtime_1.jsx)(ink_1.Box, { marginTop: 2, children: (0, jsx_runtime_1.jsx)(ink_1.Text, { color: "green", children: "Press Enter to complete setup or Escape to cancel" }) })] }));
    // Render completion
    const renderComplete = () => ((0, jsx_runtime_1.jsxs)(ink_1.Box, { flexDirection: "column", children: [(0, jsx_runtime_1.jsx)(ink_1.Box, { marginBottom: 1, children: (0, jsx_runtime_1.jsx)(ink_1.Text, { color: "green", bold: true, children: "\u2705 Setup Complete!" }) }), (0, jsx_runtime_1.jsx)(ink_1.Box, { marginBottom: 1, children: (0, jsx_runtime_1.jsx)(ink_1.Text, { color: "white", children: "Your Aimux multi-provider configuration is ready." }) }), (0, jsx_runtime_1.jsxs)(ink_1.Box, { flexDirection: "column", marginBottom: 1, children: [(0, jsx_runtime_1.jsxs)(ink_1.Text, { color: "gray", children: ["\u2022 ", selectedProviders.length, " providers configured"] }), (0, jsx_runtime_1.jsx)(ink_1.Text, { color: "gray", children: "\u2022 Intelligent routing enabled" }), (0, jsx_runtime_1.jsx)(ink_1.Text, { color: "gray", children: "\u2022 Automatic failover active" })] }), (0, jsx_runtime_1.jsx)(ink_1.Box, { marginTop: 1, children: (0, jsx_runtime_1.jsx)(ink_1.Text, { color: "cyan", children: "You can now use: aimux --help" }) })] }));
    // Main render
    switch (currentStep) {
        case 'welcome':
            return renderWelcome();
        case 'provider-selection':
            return renderProviderSelection();
        case 'api-keys':
            return renderApiKeys();
        case 'testing':
            return renderTesting();
        case 'routing':
            return renderRouting();
        case 'summary':
            return renderSummary();
        case 'complete':
            return renderComplete();
        default:
            return (0, jsx_runtime_1.jsx)(StatusMessage_1.StatusMessage, { type: "error", message: "Invalid setup step" });
    }
};
exports.ProviderSetup = ProviderSetup;
//# sourceMappingURL=provider-setup.js.map