import React, { useState, useEffect, useCallback } from 'react';
import { Box, Text, useInput, useApp } from 'ink';
import { StatusMessage } from './StatusMessage';

// Types for provider configuration
export interface ProviderInfo {
  id: string;
  name: string;
  apiKey: string;
  enabled: boolean;
  capabilities: string[];
  priority: number;
  timeout: number;
  maxRetries: number;
  costLevel: 'low' | 'medium' | 'high';
  speedLevel: 'fast' | 'medium' | 'slow';
}

export interface ProviderRoutingConfig {
  defaultProvider: string;
  thinkingProvider?: string;
  visionProvider?: string;
  toolsProvider?: string;
  strategy: 'capability' | 'cost' | 'performance' | 'round-robin';
  enableFailover: boolean;
  loadBalancing: boolean;
}

interface ProviderConfigProps {
  providers: ProviderInfo[];
  routing: ProviderRoutingConfig;
  onSave: (providers: ProviderInfo[], routing: ProviderRoutingConfig) => void;
  onCancel: () => void;
  onTestProvider: (providerId: string) => Promise<boolean>;
}

type ConfigStep = 'overview' | 'edit-provider' | 'edit-routing' | 'test-connection' | 'confirm-save';
type EditField = 'enabled' | 'priority' | 'timeout' | 'maxRetries';

export const ProviderConfig: React.FC<ProviderConfigProps> = ({
  providers,
  routing,
  onSave,
  onCancel,
  onTestProvider
}) => {
  const { exit } = useApp();

  // State
  const [currentStep, setCurrentStep] = useState<ConfigStep>('overview');
  const [selectedIndex, setSelectedIndex] = useState(0);
  const [editedProviders, setEditedProviders] = useState<ProviderInfo[]>(providers);
  const [editedRouting, setEditedRouting] = useState<ProviderRoutingConfig>(routing);
  const [editingField, setEditingField] = useState<EditField | null>(null);
  const [testResults, setTestResults] = useState<Record<string, boolean>>({});
  const [isTesting, setIsTesting] = useState(false);

  // Reset state when changing steps
  const resetStepState = useCallback((step: ConfigStep) => {
    setSelectedIndex(0);
    setEditingField(null);
    if (step === 'overview') {
      setEditedProviders(providers);
      setEditedRouting(routing);
    }
  }, [providers, routing]);

  // Navigation helpers
  const goToStep = useCallback((step: ConfigStep) => {
    resetStepState(step);
    setCurrentStep(step);
  }, [resetStepState]);

  // Provider editing helpers
  const toggleProviderEnabled = useCallback((providerId: string) => {
    setEditedProviders(prev =>
      prev.map(p =>
        p.id === providerId ? { ...p, enabled: !p.enabled } : p
      )
    );
  }, []);

  const adjustProviderPriority = useCallback((providerId: string, delta: number) => {
    setEditedProviders(prev =>
      prev.map(p =>
        p.id === providerId ? { ...p, priority: Math.max(1, Math.min(10, p.priority + delta)) } : p
      )
    );
  }, []);

  const adjustProviderTimeout = useCallback((providerId: string, delta: number) => {
    setEditedProviders(prev =>
      prev.map(p =>
        p.id === providerId ? { ...p, timeout: Math.max(5000, p.timeout + delta * 1000) } : p
      )
    );
  }, []);

  const adjustProviderRetries = useCallback((providerId: string, delta: number) => {
    setEditedProviders(prev =>
      prev.map(p =>
        p.id === providerId ? { ...p, maxRetries: Math.max(0, Math.min(10, p.maxRetries + delta)) } : p
      )
    );
  }, []);

  // Test all providers
  const testAllProviders = useCallback(async () => {
    setIsTesting(true);
    setTestResults({});
    const results: Record<string, boolean> = {};

    for (const provider of editedProviders) {
      if (provider.apiKey && provider.apiKey.length > 0) {
        try {
          const result = await onTestProvider(provider.id);
          results[provider.id] = result;
        } catch (error) {
          results[provider.id] = false;
        }
      }
    }

    setTestResults(results);
    setIsTesting(false);
  }, [editedProviders, onTestProvider]);

  // Save configuration
  const saveConfiguration = useCallback(() => {
    onSave(editedProviders, editedRouting);
    exit();
  }, [editedProviders, editedRouting, onSave, exit]);

  // Get status color for provider
  const getProviderStatusColor = (provider: ProviderInfo) => {
    if (!provider.enabled) return 'gray';
    if (!provider.apiKey || provider.apiKey.length === 0) return 'red';
    if (testResults[provider.id] === false) return 'red';
    if (testResults[provider.id] === true) return 'green';
    return 'yellow';
  };

  // Handle keyboard input
  useInput((input, key) => {
    if (key.escape) {
      onCancel();
      exit();
      return;
    }

    switch (currentStep) {
      case 'overview':
        if (key.upArrow) {
          setSelectedIndex(prev => Math.max(0, prev - 1));
        } else if (key.downArrow) {
          setSelectedIndex(prev => Math.min(editedProviders.length - 1, prev + 1));
        } else if (key.return) {
          goToStep('edit-provider');
        } else if (input === 'r') {
          goToStep('edit-routing');
        } else if (input === 't') {
          goToStep('test-connection');
        } else if (input === 's') {
          goToStep('confirm-save');
        } else if (input === 'q') {
          onCancel();
          exit();
        }
        break;

      case 'edit-provider':
        if (key.escape || input === 'b') {
          goToStep('overview');
        } else if (key.upArrow) {
          setSelectedIndex(prev => Math.max(0, prev - 1));
        } else if (key.downArrow) {
          setSelectedIndex(prev => Math.min(editedProviders.length - 1, prev + 1));
        } else if (input === ' ') {
          const provider = editedProviders[selectedIndex];
          if (provider) {
            toggleProviderEnabled(provider.id);
          }
        } else if (key.leftArrow && editingField === 'priority') {
          const provider = editedProviders[selectedIndex];
          if (provider) {
            adjustProviderPriority(provider.id, -1);
          }
        } else if (key.rightArrow && editingField === 'priority') {
          const provider = editedProviders[selectedIndex];
          if (provider) {
            adjustProviderPriority(provider.id, 1);
          }
        } else if (input === '1') {
          setEditingField('enabled');
        } else if (input === '2') {
          setEditingField('priority');
        } else if (input === '3') {
          setEditingField('timeout');
        } else if (input === '4') {
          setEditingField('maxRetries');
        }
        break;

      case 'edit-routing':
        if (key.escape || input === 'b') {
          goToStep('overview');
        } else if (key.upArrow) {
          setSelectedIndex(prev => Math.max(0, prev - 1));
        } else if (key.downArrow) {
          setSelectedIndex(prev => Math.min(3, prev + 1)); // 4 routing options
        } else if (input === ' ') {
          // Toggle boolean routing options
          if (selectedIndex === 2) {
            setEditedRouting(prev => ({ ...prev, enableFailover: !prev.enableFailover }));
          } else if (selectedIndex === 3) {
            setEditedRouting(prev => ({ ...prev, loadBalancing: !prev.loadBalancing }));
          }
        } else if (key.leftArrow || key.rightArrow) {
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
        } else if (key.return || input === 't') {
          testAllProviders();
        }
        break;

      case 'confirm-save':
        if (key.return || input === 'y') {
          saveConfiguration();
        } else if (input === 'n' || key.escape) {
          goToStep('overview');
        }
        break;
    }
  });

  // Render overview
  const renderOverview = () => (
    <Box flexDirection="column">
      <Box marginBottom={1}>
        <Text color="cyan" bold>
          🔧 Provider Configuration
        </Text>
      </Box>

      <Box marginBottom={1}>
        <Text color="gray">
          Configure providers, routing, and test connections.
        </Text>
      </Box>

      <Box marginBottom={1}>
        <Text color="white" bold>
          Configured Providers:
        </Text>
      </Box>

      {editedProviders.map((provider, index) => {
        const statusColor = getProviderStatusColor(provider);
        const status = provider.enabled ? '✓' : '✗';
        const testStatus = testResults[provider.id] === true ? '✅' :
                          testResults[provider.id] === false ? '❌' : '❓';

        return (
          <Box key={provider.id} marginBottom={1}>
            <Text
              color={index === selectedIndex ? 'green' : statusColor}
              bold={index === selectedIndex}
            >
              {index === selectedIndex ? '▸ ' : '  '}
              {status} {provider.name} ({provider.id})
            </Text>
            <Text color="gray"> | Priority: {provider.priority}</Text>
            <Text color="gray"> | {testStatus}</Text>
          </Box>
        );
      })}

      <Box marginTop={1} marginBottom={1}>
        <Text color="white" bold>
          Routing Configuration:
        </Text>
      </Box>

      <Box marginLeft={2} flexDirection="column">
        <Text color="gray">
          Default: {editedRouting.defaultProvider} | Strategy: {editedRouting.strategy}
        </Text>
        <Text color="gray">
          Failover: {editedRouting.enableFailover ? 'Enabled' : 'Disabled'} |
          Load Balancing: {editedRouting.loadBalancing ? 'Enabled' : 'Disabled'}
        </Text>
      </Box>

      <Box marginTop={1}>
        <Text color="gray">
          ↑↓ Navigate | Enter: Edit Provider | r: Routing | t: Test | s: Save | q: Quit
        </Text>
      </Box>
    </Box>
  );

  // Render provider edit
  const renderEditProvider = () => {
    if (editedProviders.length === 0) {
      return (
        <Box flexDirection="column">
          <StatusMessage type="error" message="No providers configured" />
          <Text color="gray">Press 'b' to go back</Text>
        </Box>
      );
    }

    const provider = editedProviders[selectedIndex];
    if (!provider) return null;

    return (
      <Box flexDirection="column">
        <Box marginBottom={1}>
          <Text color="cyan" bold>
            Edit Provider: {provider.name}
          </Text>
        </Box>

        <Box flexDirection="column" marginBottom={1}>
          <Box marginBottom={1}>
            <Text color={editingField === 'enabled' ? 'green' : 'white'}>
              1. Enabled: {provider.enabled ? 'Yes' : 'No'} {editingField === 'enabled' && '(Space to toggle)'}
            </Text>
          </Box>

          <Box marginBottom={1}>
            <Text color={editingField === 'priority' ? 'green' : 'white'}>
              2. Priority: {provider.priority} {editingField === 'priority' && '(← → to adjust)'}
            </Text>
          </Box>

          <Box marginBottom={1}>
            <Text color={editingField === 'timeout' ? 'green' : 'white'}>
              3. Timeout: {provider.timeout}ms {editingField === 'timeout' && '(← → to adjust)'}
            </Text>
          </Box>

          <Box marginBottom={1}>
            <Text color={editingField === 'maxRetries' ? 'green' : 'white'}>
              4. Max Retries: {provider.maxRetries} {editingField === 'maxRetries' && '(← → to adjust)'}
            </Text>
          </Box>
        </Box>

        <Box flexDirection="column" marginBottom={1}>
          <Text color="white" bold>Provider Information:</Text>
          <Text color="gray">Capabilities: {provider.capabilities.join(', ')}</Text>
          <Text color="gray">Cost Level: {provider.costLevel}</Text>
          <Text color="gray">Speed Level: {provider.speedLevel}</Text>
          <Text color="gray">API Key: {provider.apiKey ? `${'*'.repeat(provider.apiKey.length - 4)}${provider.apiKey.slice(-4)}` : 'Not set'}</Text>
        </Box>

        <Box marginTop={1}>
          <Text color="gray">
            1-4: Select field | Space: Toggle enabled | ← →: Adjust value | ↑↓: Change provider | b: Back
          </Text>
        </Box>
      </Box>
    );
  };

  // Render routing edit
  const renderEditRouting = () => {
    const routingOptions = [
      `Default Provider: ${editedRouting.defaultProvider}`,
      `Strategy: ${editedRouting.strategy}`,
      `Enable Failover: ${editedRouting.enableFailover ? 'Yes' : 'No'}`,
      `Enable Load Balancing: ${editedRouting.loadBalancing ? 'Yes' : 'No'}`
    ];

    return (
      <Box flexDirection="column">
        <Box marginBottom={1}>
          <Text color="cyan" bold>
            Edit Routing Configuration
          </Text>
        </Box>

        {routingOptions.map((option, index) => (
          <Box key={index} marginBottom={1}>
            <Text
              color={index === selectedIndex ? 'green' : 'white'}
              bold={index === selectedIndex}
            >
              {index === selectedIndex ? '▸ ' : '  '}
              {index + 1}. {option}
            </Text>
            {index === 0 && (
              <Text color="gray"> (← → to change provider)</Text>
            )}
            {(index === 2 || index === 3) && (
              <Text color="gray"> (Space to toggle)</Text>
            )}
          </Box>
        ))}

        <Box marginTop={1}>
          <Text color="gray">
            ↑↓: Navigate | Space: Toggle option | ← →: Change provider | b: Back
          </Text>
        </Box>
      </Box>
    );
  };

  // Render test connection
  const renderTestConnection = () => (
    <Box flexDirection="column">
      <Box marginBottom={1}>
        <Text color="cyan" bold>
          Test Provider Connections
        </Text>
      </Box>

      {isTesting ? (
        <Box marginBottom={1}>
          <Text color="yellow">
            Testing connections to all providers...
          </Text>
        </Box>
      ) : (
        <>
          <Box marginBottom={1}>
            <Text color="gray">
              Connection test results:
            </Text>
          </Box>

          {editedProviders.map(provider => {
            const result = testResults[provider.id];
            const status = result === true ? '✅ Connected' :
                          result === false ? '❌ Failed' : '❓ Not Tested';

            return (
              <Box key={provider.id} marginBottom={1}>
                <Text>
                  {provider.name}: {status}
                </Text>
              </Box>
            );
          })}

          {Object.keys(testResults).length > 0 && (
            <Box marginTop={1}>
              <StatusMessage
                type={Object.values(testResults).every(Boolean) ? 'success' : 'warning'}
                message={`${Object.values(testResults).filter(Boolean).length}/${editedProviders.length} providers connected`}
              />
            </Box>
          )}
        </>
      )}

      <Box marginTop={1}>
        <Text color="gray">
          t: Test Connections | b: Back
        </Text>
      </Box>
    </Box>
  );

  // Render confirm save
  const renderConfirmSave = () => (
    <Box flexDirection="column">
      <Box marginBottom={1}>
        <Text color="cyan" bold>
          Confirm Save Configuration
        </Text>
      </Box>

      <Box marginBottom={1}>
        <Text color="white">
          Are you sure you want to save these configuration changes?
        </Text>
      </Box>

      <Box marginTop={1}>
        <Text color="gray">
          y: Yes, save | n: No, go back | Escape: Cancel
        </Text>
      </Box>
    </Box>
  );

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
      return <StatusMessage type="error" message="Invalid configuration step" />;
  }
};

export default ProviderConfig;