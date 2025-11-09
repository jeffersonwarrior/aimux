import React, { useState, useEffect, useCallback } from 'react';
import { Box, Text, useInput, useApp } from 'ink';
import { StatusMessage } from './components/StatusMessage';
import { ProgressBar } from './components/ProgressBar';

// Types for provider status
export interface ProviderStatus {
  id: string;
  name: string;
  enabled: boolean;
  connectionStatus: 'connected' | 'disconnected' | 'testing' | 'error';
  apiKeyValid: boolean;
  lastCheck: Date;
  responseTime?: number;
  successRate?: number;
  uptimePercentage?: number;
  errorCount?: number;
  totalRequests?: number;
  capabilities: string[];
  priority: number;
}

export interface ProviderMetrics {
  ResponseTime: number;
  SuccessRate: number;
  TotalRequests: number;
  ErrorCount: number;
  Uptime: number;
}

export interface StatusSnapshot {
  timestamp: Date;
  providers: ProviderStatus[];
  globalStats: {
    totalProviders: number;
    activeProviders: number;
    totalRequests: number;
    globalSuccessRate: number;
  };
}

interface ProviderStatusProps {
  providers: ProviderStatus[];
  onRefresh: () => Promise<void>;
  onTestProvider: (providerId: string) => Promise<boolean>;
  onToggleProvider: (providerId: string) => void;
  onExit: () => void;
  autoRefresh?: boolean;
  refreshInterval?: number;
}

export const ProviderStatus: React.FC<ProviderStatusProps> = ({
  providers,
  onRefresh,
  onTestProvider,
  onToggleProvider,
  onExit,
  autoRefresh = true,
  refreshInterval = 30000 // 30 seconds
}) => {
  const { exit } = useApp();

  // State
  const [selectedIndex, setSelectedIndex] = useState(0);
  const [isRefreshing, setIsRefreshing] = useState(false);
  const [testingProvider, setTestingProvider] = useState<string | null>(null);
  const [showDetails, setShowDetails] = useState(false);
  const [lastRefresh, setLastRefresh] = useState<Date>(new Date());
  const [sortedProviders, setSortedProviders] = useState<ProviderStatus[]>(providers);

  // Sort providers by status and priority
  useEffect(() => {
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
  useEffect(() => {
    if (!autoRefresh) return;

    const interval = setInterval(async () => {
      await handleRefresh();
    }, refreshInterval);

    return () => clearInterval(interval);
  }, [autoRefresh, refreshInterval]);

  // Handle refresh
  const handleRefresh = useCallback(async () => {
    setIsRefreshing(true);
    setLastRefresh(new Date());
    await onRefresh();
    setIsRefreshing(false);
  }, [onRefresh]);

  // Handle provider testing
  const handleTestProvider = useCallback(async (providerId: string) => {
    setTestingProvider(providerId);
    try {
      await onTestProvider(providerId);
    } finally {
      setTestingProvider(null);
    }
  }, [onTestProvider]);

  // Handle provider toggle
  const handleToggleProvider = useCallback((providerId: string) => {
    onToggleProvider(providerId);
  }, [onToggleProvider]);

  // Get status indicator
  const getStatusIndicator = (status: ProviderStatus['connectionStatus']) => {
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
  const getStatusColor = (status: ProviderStatus['connectionStatus']) => {
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
  useInput((input, key) => {
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
    } else if (key.downArrow) {
      setSelectedIndex(prev => Math.min(sortedProviders.length - 1, prev + 1));
    }

    // Actions
    if (input === 'r' || (key.ctrl && input === 'r')) {
      handleRefresh();
    } else if (input === ' ') {
      const selectedProvider = sortedProviders[selectedIndex];
      if (selectedProvider) {
        handleToggleProvider(selectedProvider.id);
      }
    } else if (input === 't') {
      const selectedProvider = sortedProviders[selectedIndex];
      if (selectedProvider) {
        handleTestProvider(selectedProvider.id);
      }
    } else if (input === 'd' || key.return) {
      setShowDetails(true);
    } else if (input === 'q') {
      onExit();
      exit();
    }
  });

  // Render provider details
  const renderProviderDetails = () => {
    const provider = sortedProviders[selectedIndex];
    if (!provider) return null;

    return (
      <Box flexDirection="column">
        <Box marginBottom={1}>
          <Text color="cyan" bold>
            Provider Details: {provider.name}
          </Text>
        </Box>

        <Box flexDirection="column" marginBottom={1}>
          <Text color="white" bold>Basic Information</Text>
          <Box marginLeft={2}>
            <Text color="gray">ID: {provider.id}</Text>
          </Box>
          <Box marginLeft={2}>
            <Text color="gray">Priority: {provider.priority}</Text>
          </Box>
          <Box marginLeft={2}>
            <Text color="gray">Enabled: {provider.enabled ? 'Yes' : 'No'}</Text>
          </Box>
          <Box marginLeft={2}>
            <Text color="gray">API Key Valid: {provider.apiKeyValid ? 'Yes' : 'No'}</Text>
          </Box>
        </Box>

        <Box flexDirection="column" marginBottom={1}>
          <Text color="white" bold>Performance Metrics</Text>
          <Box marginLeft={2}>
            <Text color="gray">
              Response Time: {provider.responseTime ? `${provider.responseTime}ms` : 'N/A'}
            </Text>
          </Box>
          <Box marginLeft={2}>
            <Text color="gray">
              Success Rate: {provider.successRate ? `${(provider.successRate * 100).toFixed(1)}%` : 'N/A'}
            </Text>
          </Box>
          <Box marginLeft={2}>
            <Text color="gray">
              Total Requests: {provider.totalRequests || 0}
            </Text>
          </Box>
          <Box marginLeft={2}>
            <Text color="gray">
              Error Count: {provider.errorCount || 0}
            </Text>
          </Box>
          <Box marginLeft={2}>
            <Text color="gray">
              Uptime: {provider.uptimePercentage ? `${(provider.uptimePercentage * 100).toFixed(1)}%` : 'N/A'}
            </Text>
          </Box>
        </Box>

        <Box flexDirection="column" marginBottom={1}>
          <Text color="white" bold>Capabilities</Text>
          <Box marginLeft={2}>
            <Text color="gray">
              {provider.capabilities.length > 0 ? provider.capabilities.join(', ') : 'None'}
            </Text>
          </Box>
        </Box>

        <Box flexDirection="column" marginBottom={1}>
          <Text color="white" bold>Connection Info</Text>
          <Box marginLeft={2}>
            <Text color="gray">
              Last Check: {provider.lastCheck.toLocaleString()}
            </Text>
          </Box>
          <Box marginLeft={2}>
            <Text color={getStatusColor(provider.connectionStatus)}>
              Status: {provider.connectionStatus}
            </Text>
          </Box>
        </Box>

        <Box marginTop={1}>
          <Text color="gray">
            Press 'd', Escape, or 'q' to go back
          </Text>
        </Box>
      </Box>
    );
  };

  // Render main status view
  const renderMainStatus = () => (
    <Box flexDirection="column">
      <Box marginBottom={1}>
        <Text color="cyan" bold>
          🤖 Provider Status Dashboard
        </Text>
      </Box>

      {/* Global statistics */}
      <Box marginBottom={1}>
        <Text color="green" bold>
          Global Status: {globalStats.connectedProviders}/{globalStats.totalProviders} Connected
        </Text>
        <Text color="gray"> | </Text>
        <Text color="white">
          {globalStats.activeProviders} Active
        </Text>
        <Text color="gray"> | </Text>
        <Text color="yellow">
          Success Rate: {(globalStats.globalSuccessRate * 100).toFixed(1)}%
        </Text>
        <Text color="gray"> | </Text>
        <Text color="blue">
          Total Requests: {globalStats.totalRequests}
        </Text>
      </Box>

      {/* Refresh status */}
      {isRefreshing && (
        <Box marginBottom={1}>
          <Text color="yellow">
            🔄 Refreshing provider status...
          </Text>
        </Box>
      )}

      <Box marginBottom={1}>
        <Text color="gray" dimColor>
          Last refresh: {lastRefresh.toLocaleTimeString()}
          {autoRefresh && ` (Auto-refresh: ${(refreshInterval / 1000).toFixed(0)}s)`}
        </Text>
      </Box>

      {/* Provider list */}
      <Box marginBottom={1}>
        <Text color="white" bold>
          Providers:
        </Text>
      </Box>

      {sortedProviders.length === 0 ? (
        <Box marginBottom={1}>
          <Text color="yellow">
            No providers configured. Run 'aimux providers setup' to add providers.
          </Text>
        </Box>
      ) : (
        sortedProviders.map((provider, index) => {
          const isSelected = index === selectedIndex;
          const isTestingThis = testingProvider === provider.id;
          const indicator = getStatusIndicator(provider.connectionStatus);
          const statusColor = getStatusColor(provider.connectionStatus);

          return (
            <Box key={provider.id} marginBottom={1}>
              <Box flexDirection="column">
                <Box>
                  <Text
                    color={isSelected ? 'green' : statusColor}
                    bold={isSelected}
                  >
                    {isSelected ? '▸ ' : '  '}
                    {indicator}
                    {isTestingThis ? ' 🧪' : '   '}
                    {provider.enabled ? '●' : '○'} {provider.name}
                  </Text>
                  <Text color="gray" dimColor>
                    {' '}({provider.id})
                  </Text>
                </Box>

                {/* Quick metrics row */}
                <Box marginLeft={6}>
                  <Text color="gray" dimColor>
                    {provider.responseTime && `RT: ${provider.responseTime}ms | `}
                    {provider.successRate && `SR: ${(provider.successRate * 100).toFixed(1)}% | `}
                    {provider.totalRequests && `Req: ${provider.totalRequests}`}
                    {' | '}
                    Capabilities: {provider.capabilities.slice(0, 2).join(', ')}
                    {provider.capabilities.length > 2 && '...'}
                  </Text>
                </Box>
              </Box>
            </Box>
          );
        })
      )}

      {/* Status indicators legend */}
      <Box marginBottom={1}>
        <Text color="gray" dimColor>
          Status: ✅ Connected | ⚠️ Disconnected | ❌ Error | 🧪 Testing
        </Text>
      </Box>

      {/* Controls help */}
      <Box marginTop={1}>
        <Text color="gray">
          ↑↓ Navigate | Space: Toggle Provider | t: Test | d: Details | r: Refresh | q: Quit
        </Text>
      </Box>
    </Box>
  );

  return showDetails ? renderProviderDetails() : renderMainStatus();
};

// Helper function to create sample status data (for demonstrations)
export const createSampleProviderStatus = (): ProviderStatus[] => [
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

export default ProviderStatus;