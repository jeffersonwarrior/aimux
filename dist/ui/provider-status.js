"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.createSampleProviderStatus = exports.ProviderStatus = void 0;
const jsx_runtime_1 = require("react/jsx-runtime");
const react_1 = require("react");
const ink_1 = require("ink");
const ProviderStatus = ({ providers, onRefresh, onTestProvider, onToggleProvider, onExit, autoRefresh = true, refreshInterval = 30000 // 30 seconds
 }) => {
    const { exit } = (0, ink_1.useApp)();
    // State
    const [selectedIndex, setSelectedIndex] = (0, react_1.useState)(0);
    const [isRefreshing, setIsRefreshing] = (0, react_1.useState)(false);
    const [testingProvider, setTestingProvider] = (0, react_1.useState)(null);
    const [showDetails, setShowDetails] = (0, react_1.useState)(false);
    const [lastRefresh, setLastRefresh] = (0, react_1.useState)(new Date());
    const [sortedProviders, setSortedProviders] = (0, react_1.useState)(providers);
    // Sort providers by status and priority
    (0, react_1.useEffect)(() => {
        const sorted = [...providers].sort((a, b) => {
            // Active providers first
            if (a.enabled !== b.enabled) {
                return b.enabled ? 1 : -1;
            }
            // Then by connection status
            const statusOrder = { connected: 0, testing: 1, disconnected: 2, error: 3 };
            const statusA = statusOrder[a.connectionStatus];
            const statusB = statusOrder[b.connectionStatus];
            if (statusA !== statusB) {
                return statusA - statusB;
            }
            // Finally by priority (highest first)
            return b.priority - a.priority;
        });
        setSortedProviders(sorted);
    }, [providers]);
    // Auto-refresh
    (0, react_1.useEffect)(() => {
        if (!autoRefresh)
            return;
        const interval = setInterval(async () => {
            await handleRefresh();
        }, refreshInterval);
        return () => clearInterval(interval);
    }, [autoRefresh, refreshInterval]);
    // Handle refresh
    const handleRefresh = (0, react_1.useCallback)(async () => {
        setIsRefreshing(true);
        setLastRefresh(new Date());
        await onRefresh();
        setIsRefreshing(false);
    }, [onRefresh]);
    // Handle provider testing
    const handleTestProvider = (0, react_1.useCallback)(async (providerId) => {
        setTestingProvider(providerId);
        try {
            await onTestProvider(providerId);
        }
        finally {
            setTestingProvider(null);
        }
    }, [onTestProvider]);
    // Handle provider toggle
    const handleToggleProvider = (0, react_1.useCallback)((providerId) => {
        onToggleProvider(providerId);
    }, [onToggleProvider]);
    // Get status indicator
    const getStatusIndicator = (status) => {
        switch (status) {
            case 'connected':
                return '✅';
            case 'testing':
                return '🧪';
            case 'disconnected':
                return '⚠️';
            case 'error':
                return '❌';
            default:
                return '❓';
        }
    };
    // Get status color
    const getStatusColor = (status) => {
        switch (status) {
            case 'connected':
                return 'green';
            case 'testing':
                return 'yellow';
            case 'disconnected':
                return 'yellow';
            case 'error':
                return 'red';
            default:
                return 'gray';
        }
    };
    // Calculate global statistics
    const globalStats = {
        totalProviders: providers.length,
        activeProviders: providers.filter(p => p.enabled).length,
        connectedProviders: providers.filter(p => p.connectionStatus === 'connected').length,
        totalRequests: providers.reduce((sum, p) => sum + (p.totalRequests || 0), 0),
        globalSuccessRate: providers.length > 0
            ? providers.reduce((sum, p) => sum + (p.successRate || 0), 0) / providers.length
            : 0
    };
    // Handle keyboard input
    (0, ink_1.useInput)((input, key) => {
        if (key.escape) {
            onExit();
            exit();
            return;
        }
        if (showDetails) {
            if (key.escape || input === 'q' || input === 'd') {
                setShowDetails(false);
            }
            return;
        }
        // Navigation
        if (key.upArrow) {
            setSelectedIndex(prev => Math.max(0, prev - 1));
        }
        else if (key.downArrow) {
            setSelectedIndex(prev => Math.min(sortedProviders.length - 1, prev + 1));
        }
        // Actions
        if (input === 'r' || (key.ctrl && input === 'r')) {
            handleRefresh();
        }
        else if (input === ' ') {
            const selectedProvider = sortedProviders[selectedIndex];
            if (selectedProvider) {
                handleToggleProvider(selectedProvider.id);
            }
        }
        else if (input === 't') {
            const selectedProvider = sortedProviders[selectedIndex];
            if (selectedProvider) {
                handleTestProvider(selectedProvider.id);
            }
        }
        else if (input === 'd' || key.return) {
            setShowDetails(true);
        }
        else if (input === 'q') {
            onExit();
            exit();
        }
    });
    // Render provider details
    const renderProviderDetails = () => {
        const provider = sortedProviders[selectedIndex];
        if (!provider)
            return null;
        return ((0, jsx_runtime_1.jsxs)(ink_1.Box, { flexDirection: "column", children: [(0, jsx_runtime_1.jsx)(ink_1.Box, { marginBottom: 1, children: (0, jsx_runtime_1.jsxs)(ink_1.Text, { color: "cyan", bold: true, children: ["Provider Details: ", provider.name] }) }), (0, jsx_runtime_1.jsxs)(ink_1.Box, { flexDirection: "column", marginBottom: 1, children: [(0, jsx_runtime_1.jsx)(ink_1.Text, { color: "white", bold: true, children: "Basic Information" }), (0, jsx_runtime_1.jsx)(ink_1.Box, { marginLeft: 2, children: (0, jsx_runtime_1.jsxs)(ink_1.Text, { color: "gray", children: ["ID: ", provider.id] }) }), (0, jsx_runtime_1.jsx)(ink_1.Box, { marginLeft: 2, children: (0, jsx_runtime_1.jsxs)(ink_1.Text, { color: "gray", children: ["Priority: ", provider.priority] }) }), (0, jsx_runtime_1.jsx)(ink_1.Box, { marginLeft: 2, children: (0, jsx_runtime_1.jsxs)(ink_1.Text, { color: "gray", children: ["Enabled: ", provider.enabled ? 'Yes' : 'No'] }) }), (0, jsx_runtime_1.jsx)(ink_1.Box, { marginLeft: 2, children: (0, jsx_runtime_1.jsxs)(ink_1.Text, { color: "gray", children: ["API Key Valid: ", provider.apiKeyValid ? 'Yes' : 'No'] }) })] }), (0, jsx_runtime_1.jsxs)(ink_1.Box, { flexDirection: "column", marginBottom: 1, children: [(0, jsx_runtime_1.jsx)(ink_1.Text, { color: "white", bold: true, children: "Performance Metrics" }), (0, jsx_runtime_1.jsx)(ink_1.Box, { marginLeft: 2, children: (0, jsx_runtime_1.jsxs)(ink_1.Text, { color: "gray", children: ["Response Time: ", provider.responseTime ? `${provider.responseTime}ms` : 'N/A'] }) }), (0, jsx_runtime_1.jsx)(ink_1.Box, { marginLeft: 2, children: (0, jsx_runtime_1.jsxs)(ink_1.Text, { color: "gray", children: ["Success Rate: ", provider.successRate ? `${(provider.successRate * 100).toFixed(1)}%` : 'N/A'] }) }), (0, jsx_runtime_1.jsx)(ink_1.Box, { marginLeft: 2, children: (0, jsx_runtime_1.jsxs)(ink_1.Text, { color: "gray", children: ["Total Requests: ", provider.totalRequests || 0] }) }), (0, jsx_runtime_1.jsx)(ink_1.Box, { marginLeft: 2, children: (0, jsx_runtime_1.jsxs)(ink_1.Text, { color: "gray", children: ["Error Count: ", provider.errorCount || 0] }) }), (0, jsx_runtime_1.jsx)(ink_1.Box, { marginLeft: 2, children: (0, jsx_runtime_1.jsxs)(ink_1.Text, { color: "gray", children: ["Uptime: ", provider.uptimePercentage ? `${(provider.uptimePercentage * 100).toFixed(1)}%` : 'N/A'] }) })] }), (0, jsx_runtime_1.jsxs)(ink_1.Box, { flexDirection: "column", marginBottom: 1, children: [(0, jsx_runtime_1.jsx)(ink_1.Text, { color: "white", bold: true, children: "Capabilities" }), (0, jsx_runtime_1.jsx)(ink_1.Box, { marginLeft: 2, children: (0, jsx_runtime_1.jsx)(ink_1.Text, { color: "gray", children: provider.capabilities.length > 0 ? provider.capabilities.join(', ') : 'None' }) })] }), (0, jsx_runtime_1.jsxs)(ink_1.Box, { flexDirection: "column", marginBottom: 1, children: [(0, jsx_runtime_1.jsx)(ink_1.Text, { color: "white", bold: true, children: "Connection Info" }), (0, jsx_runtime_1.jsx)(ink_1.Box, { marginLeft: 2, children: (0, jsx_runtime_1.jsxs)(ink_1.Text, { color: "gray", children: ["Last Check: ", provider.lastCheck.toLocaleString()] }) }), (0, jsx_runtime_1.jsx)(ink_1.Box, { marginLeft: 2, children: (0, jsx_runtime_1.jsxs)(ink_1.Text, { color: getStatusColor(provider.connectionStatus), children: ["Status: ", provider.connectionStatus] }) })] }), (0, jsx_runtime_1.jsx)(ink_1.Box, { marginTop: 1, children: (0, jsx_runtime_1.jsx)(ink_1.Text, { color: "gray", children: "Press 'd', Escape, or 'q' to go back" }) })] }));
    };
    // Render main status view
    const renderMainStatus = () => ((0, jsx_runtime_1.jsxs)(ink_1.Box, { flexDirection: "column", children: [(0, jsx_runtime_1.jsx)(ink_1.Box, { marginBottom: 1, children: (0, jsx_runtime_1.jsx)(ink_1.Text, { color: "cyan", bold: true, children: "\uD83E\uDD16 Provider Status Dashboard" }) }), (0, jsx_runtime_1.jsxs)(ink_1.Box, { marginBottom: 1, children: [(0, jsx_runtime_1.jsxs)(ink_1.Text, { color: "green", bold: true, children: ["Global Status: ", globalStats.connectedProviders, "/", globalStats.totalProviders, " Connected"] }), (0, jsx_runtime_1.jsx)(ink_1.Text, { color: "gray", children: " | " }), (0, jsx_runtime_1.jsxs)(ink_1.Text, { color: "white", children: [globalStats.activeProviders, " Active"] }), (0, jsx_runtime_1.jsx)(ink_1.Text, { color: "gray", children: " | " }), (0, jsx_runtime_1.jsxs)(ink_1.Text, { color: "yellow", children: ["Success Rate: ", (globalStats.globalSuccessRate * 100).toFixed(1), "%"] }), (0, jsx_runtime_1.jsx)(ink_1.Text, { color: "gray", children: " | " }), (0, jsx_runtime_1.jsxs)(ink_1.Text, { color: "blue", children: ["Total Requests: ", globalStats.totalRequests] })] }), isRefreshing && ((0, jsx_runtime_1.jsx)(ink_1.Box, { marginBottom: 1, children: (0, jsx_runtime_1.jsx)(ink_1.Text, { color: "yellow", children: "\uD83D\uDD04 Refreshing provider status..." }) })), (0, jsx_runtime_1.jsx)(ink_1.Box, { marginBottom: 1, children: (0, jsx_runtime_1.jsxs)(ink_1.Text, { color: "gray", dimColor: true, children: ["Last refresh: ", lastRefresh.toLocaleTimeString(), autoRefresh && ` (Auto-refresh: ${(refreshInterval / 1000).toFixed(0)}s)`] }) }), (0, jsx_runtime_1.jsx)(ink_1.Box, { marginBottom: 1, children: (0, jsx_runtime_1.jsx)(ink_1.Text, { color: "white", bold: true, children: "Providers:" }) }), sortedProviders.length === 0 ? ((0, jsx_runtime_1.jsx)(ink_1.Box, { marginBottom: 1, children: (0, jsx_runtime_1.jsx)(ink_1.Text, { color: "yellow", children: "No providers configured. Run 'aimux providers setup' to add providers." }) })) : (sortedProviders.map((provider, index) => {
                const isSelected = index === selectedIndex;
                const isTestingThis = testingProvider === provider.id;
                const indicator = getStatusIndicator(provider.connectionStatus);
                const statusColor = getStatusColor(provider.connectionStatus);
                return ((0, jsx_runtime_1.jsx)(ink_1.Box, { marginBottom: 1, children: (0, jsx_runtime_1.jsxs)(ink_1.Box, { flexDirection: "column", children: [(0, jsx_runtime_1.jsxs)(ink_1.Box, { children: [(0, jsx_runtime_1.jsxs)(ink_1.Text, { color: isSelected ? 'green' : statusColor, bold: isSelected, children: [isSelected ? '▸ ' : '  ', indicator, isTestingThis ? ' 🧪' : '   ', provider.enabled ? '●' : '○', " ", provider.name] }), (0, jsx_runtime_1.jsxs)(ink_1.Text, { color: "gray", dimColor: true, children: [' ', "(", provider.id, ")"] })] }), (0, jsx_runtime_1.jsx)(ink_1.Box, { marginLeft: 6, children: (0, jsx_runtime_1.jsxs)(ink_1.Text, { color: "gray", dimColor: true, children: [provider.responseTime && `RT: ${provider.responseTime}ms | `, provider.successRate && `SR: ${(provider.successRate * 100).toFixed(1)}% | `, provider.totalRequests && `Req: ${provider.totalRequests}`, ' | ', "Capabilities: ", provider.capabilities.slice(0, 2).join(', '), provider.capabilities.length > 2 && '...'] }) })] }) }, provider.id));
            })), (0, jsx_runtime_1.jsx)(ink_1.Box, { marginBottom: 1, children: (0, jsx_runtime_1.jsx)(ink_1.Text, { color: "gray", dimColor: true, children: "Status: \u2705 Connected | \u26A0\uFE0F Disconnected | \u274C Error | \uD83E\uDDEA Testing" }) }), (0, jsx_runtime_1.jsx)(ink_1.Box, { marginTop: 1, children: (0, jsx_runtime_1.jsx)(ink_1.Text, { color: "gray", children: "\u2191\u2193 Navigate | Space: Toggle Provider | t: Test | d: Details | r: Refresh | q: Quit" }) })] }));
    return showDetails ? renderProviderDetails() : renderMainStatus();
};
exports.ProviderStatus = ProviderStatus;
// Helper function to create sample status data (for demonstrations)
const createSampleProviderStatus = () => [
    {
        id: 'minimax-m2',
        name: 'Minimax M2',
        enabled: true,
        connectionStatus: 'connected',
        apiKeyValid: true,
        lastCheck: new Date(),
        responseTime: 850,
        successRate: 0.98,
        uptimePercentage: 0.995,
        errorCount: 12,
        totalRequests: 1250,
        capabilities: ['thinking', 'vision', 'tools'],
        priority: 7
    },
    {
        id: 'z-ai',
        name: 'Z.AI',
        enabled: true,
        connectionStatus: 'connected',
        apiKeyValid: true,
        lastCheck: new Date(),
        responseTime: 450,
        successRate: 0.96,
        uptimePercentage: 0.988,
        errorCount: 24,
        totalRequests: 890,
        capabilities: ['vision', 'tools'],
        priority: 6
    },
    {
        id: 'synthetic-new',
        name: 'Synthetic.New',
        enabled: false,
        connectionStatus: 'disconnected',
        apiKeyValid: true,
        lastCheck: new Date(),
        responseTime: 1200,
        successRate: 0.94,
        uptimePercentage: 0.972,
        errorCount: 35,
        totalRequests: 670,
        capabilities: ['thinking', 'tools'],
        priority: 5
    }
];
exports.createSampleProviderStatus = createSampleProviderStatus;
exports.default = exports.ProviderStatus;
//# sourceMappingURL=provider-status.js.map