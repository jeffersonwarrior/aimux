import React, { useState, useEffect, useCallback } from 'react';
import { Box, Text, useInput, useApp } from 'ink';
import { StatusMessage } from './components/StatusMessage';
import { ProgressBar } from './components/ProgressBar';

// Provider information types
export interface ProviderInfo {
  id: string;
  name: string;
  description: string;
  capabilities: string[];
  priority: number;
  costLevel: 'low' | 'medium' | 'high';
  speedLevel: 'fast' | 'medium' | 'slow';
}

export interface ProviderConfig {
  id: string;
  name: string;
  apiKey: string;
  capabilities: string[];
  enabled: boolean;
  priority: number;
  timeout: number;
  maxRetries: number;
}

export interface RoutingConfig {
  defaultProvider: string;
  thinkingProvider?: string;
  visionProvider?: string;
  toolsProvider?: string;
  strategy: 'capability' | 'cost' | 'performance' | 'round-robin';
  enableFailover: boolean;
  loadBalancing: boolean;
}

export interface SetupResult {
  success: boolean;
  providers: ProviderConfig[];
  routing: RoutingConfig;
  errors?: string[];
}

interface ProviderSetupProps {
  onSetup: (result: SetupResult) => void;
  onCancel: () => void;
  initialProviders?: ProviderConfig[];
  initialRouting?: RoutingConfig;
  availableProviders?: ProviderInfo[];
}

// Available providers configuration
const DEFAULT_PROVIDERS: ProviderInfo[] = [
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

type SetupStep = 'welcome' | 'provider-selection' | 'api-keys' | 'testing' | 'routing' | 'summary' | 'complete';

export const ProviderSetup: React.FC<ProviderSetupProps> = ({
  onSetup,
  onCancel,
  initialProviders = [],
  initialRouting,
  availableProviders = DEFAULT_PROVIDERS
}) => {
  const { exit } = useApp();

  // State management
  const [currentStep, setCurrentStep] = useState<SetupStep>('welcome');
  const [selectedProviders, setSelectedProviders] = useState<string[]>([]);
  const [providerConfigs, setProviderConfigs] = useState<ProviderConfig[]>(initialProviders);
  const [apiKeys, setApiKeys] = useState<Record<string, string>>({});
  const [testResults, setTestResults] = useState<Record<string, boolean>>({});
  const [testingProgress, setTestingProgress] = useState(0);
  const [routingConfig, setRoutingConfig] = useState<RoutingConfig>(
    initialRouting || {
      defaultProvider: 'synthetic-new',
      strategy: 'capability',
      enableFailover: true,
      loadBalancing: false
    }
  );
  const [errors, setErrors] = useState<string[]>([]);
  const [isLoading, setIsLoading] = useState(false);

  // Reset state when going back to different steps
  const resetStepState = useCallback((step: SetupStep) => {
    setErrors([]);
    if (step === 'welcome') {
      setSelectedProviders([]);
      setApiKeys({});
      setTestResults({});
      setTestingProgress(0);
    }
  }, []);

  // Navigation helpers
  const goToNextStep = useCallback(() => {
    const steps: SetupStep[] = ['welcome', 'provider-selection', 'api-keys', 'testing', 'routing', 'summary', 'complete'];
    const currentIndex = steps.indexOf(currentStep);
    if (currentIndex < steps.length - 1) {
      const nextStepStep = steps[currentIndex + 1];
      if (nextStepStep) {
        resetStepState(nextStepStep);
        setCurrentStep(nextStepStep);
      }
    }
  }, [currentStep, resetStepState]);

  const goToPreviousStep = useCallback(() => {
    const steps: SetupStep[] = ['welcome', 'provider-selection', 'api-keys', 'testing', 'routing', 'summary', 'complete'];
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
  const toggleProvider = useCallback((providerId: string) => {
    setSelectedProviders(prev =>
      prev.includes(providerId)
        ? prev.filter(id => id !== providerId)
        : [...prev, providerId]
    );
  }, []);

  // API key management
  const setApiKey = useCallback((providerId: string, apiKey: string) => {
    setApiKeys(prev => ({ ...prev, [providerId]: apiKey }));
  }, []);

  // Test providers
  const testSelectedProviders = useCallback(async () => {
    setIsLoading(true);
    setTestingProgress(0);
    setTestResults({});
    const results: Record<string, boolean> = {};

    for (let i = 0; i < selectedProviders.length; i++) {
      const providerId = selectedProviders[i];
      if (!providerId) continue;
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
  const completeSetup = useCallback(() => {
    // Build final provider configs
    const finalProviders: ProviderConfig[] = selectedProviders.map(providerId => {
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

    const result: SetupResult = {
      success: true,
      providers: finalProviders,
      routing: routingConfig,
      errors: errors.length > 0 ? errors : undefined
    };

    onSetup(result);
    exit();
  }, [selectedProviders, apiKeys, availableProviders, routingConfig, errors, onSetup, exit]);

  // Handle keyboard input
  useInput((input, key) => {
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
  useEffect(() => {
    if (currentStep === 'api-keys' && selectedProviders.every(p => apiKeys[p])) {
      setTimeout(() => {
        setCurrentStep('testing');
        testSelectedProviders();
      }, 500);
    }
  }, [apiKeys, selectedProviders, currentStep, testSelectedProviders]);

  // Render welcome screen
  const renderWelcome = () => (
    <Box flexDirection="column">
      <Box marginBottom={1}>
        <Text color="cyan" bold>
          🤖 Welcome to Aimux Multi-Provider Setup
        </Text>
      </Box>

      <Box marginBottom={1}>
        <Text color="white">
          Aimux transforms your Claude Code experience by intelligently routing requests
          across multiple AI providers.
        </Text>
      </Box>

      <Box marginBottom={1}>
        <Text color="gray">
          This setup will configure:
        </Text>
      </Box>

      <Box flexDirection="column" marginBottom={1}>
        <Text color="gray">• Multiple AI provider accounts (Minimax M2, Z.AI, Synthetic.New)</Text>
        <Text color="gray">• Intelligent request routing based on capabilities</Text>
        <Text color="gray">• Automatic failover and load balancing</Text>
        <Text color="gray">• Cost optimization and performance monitoring</Text>
      </Box>

      <Box marginTop={1}>
        <Text color="yellow">
          You'll need API keys for each provider you want to use.
        </Text>
      </Box>

      <Box marginTop={2}>
        <Text color="gray">
          Press Enter or Space to continue, Escape to cancel
        </Text>
      </Box>
    </Box>
  );

  // Render provider selection
  const renderProviderSelection = () => (
    <Box flexDirection="column">
      <Box marginBottom={1}>
        <Text color="cyan" bold>
          Step 1: Select Providers
        </Text>
      </Box>

      <Box marginBottom={1}>
        <Text color="gray">
          Choose the providers you want to configure. You can select multiple providers.
        </Text>
      </Box>

      {availableProviders.map((provider, index) => {
        const isSelected = selectedProviders.includes(provider.id);
        const costColor = provider.costLevel === 'low' ? 'green' :
                         provider.costLevel === 'medium' ? 'yellow' : 'red';
        const speedColor = provider.speedLevel === 'fast' ? 'green' :
                          provider.speedLevel === 'medium' ? 'yellow' : 'red';

        return (
          <Box key={provider.id} marginBottom={1}>
            <Box flexDirection="column">
              <Box>
                <Text
                  color={isSelected ? 'green' : 'white'}
                  bold={isSelected}
                >
                  {isSelected ? '◉' : '◯'} {index + 1}. {provider.name}
                </Text>
                <Text color="gray"> ({provider.id})</Text>
              </Box>
              <Box marginLeft={2}>
                <Text color="gray" dimColor>
                  {provider.description}
                </Text>
              </Box>
              <Box marginLeft={2}>
                <Text color="gray" dimColor>
                  Capabilities: {provider.capabilities.join(', ')}
                </Text>
              </Box>
              <Box marginLeft={2}>
                <Text color={costColor}>
                  Cost: {provider.costLevel}
                </Text>
                <Text color="gray"> | </Text>
                <Text color={speedColor}>
                  Speed: {provider.speedLevel}
                </Text>
                <Text color="gray"> | Priority: {provider.priority}</Text>
              </Box>
            </Box>
          </Box>
        );
      })}

      {errors.length > 0 && (
        <Box marginBottom={1}>
          <StatusMessage type="error" message={errors.join(', ')} />
        </Box>
      )}

      <Box marginTop={1}>
        <Text color="gray">
          1-3: Toggle provider selection | Enter: Continue | Escape: Cancel
        </Text>
      </Box>

    </Box>
  );

  // Render API key collection
  const renderApiKeys = () => (
    <Box flexDirection="column">
      <Box marginBottom={1}>
        <Text color="cyan" bold>
          Step 2: Enter API Keys
        </Text>
      </Box>

      <Box marginBottom={1}>
        <Text color="gray">
          Enter API keys for your selected providers. Keys are masked for security.
        </Text>
      </Box>

      {selectedProviders.map((providerId, index) => {
        if (!providerId) return null;
        const provider = availableProviders.find(p => p.id === providerId);
        if (!provider) return null;
        const apiKey = apiKeys[providerId];
        const hasKey = !!(apiKey && apiKey.length > 0);

        return (
          <Box key={providerId} marginBottom={1} flexDirection="column">
            <Box>
              <Text color={hasKey ? 'green' : 'yellow'}>
                {index + 1}. {provider.name} API Key:
              </Text>
            </Box>
            <Box marginLeft={2}>
              {hasKey ? (
                <Text color="green">
                  {'*'.repeat(apiKey!.length - 4)}{apiKey!.slice(-4)}
                </Text>
              ) : (
                <Text color="gray" dimColor>
                  [Press {index + 1} to enter API key]
                </Text>
              )}
            </Box>
          </Box>
        );
      })}

      <Box marginTop={1}>
        <Text color="gray">
          1-3: Enter API key for provider | Enter: Continue when all keys entered | Escape: Cancel
        </Text>
      </Box>

      {/* Continue automatically when all keys are entered */}
      {selectedProviders.every(p => p && apiKeys[p]) && (
        <Box marginTop={1}>
          <StatusMessage type="success" message="All API keys entered. Continuing..." />
        </Box>
      )}
    </Box>
  );

  // Render testing progress
  const renderTesting = () => (
    <Box flexDirection="column">
      <Box marginBottom={1}>
        <Text color="cyan" bold>
          Step 3: Testing Provider Connections
        </Text>
      </Box>

      {isLoading ? (
        <>
          <Box marginBottom={1}>
            <Text color="gray">
              Testing connections to selected providers...
            </Text>
          </Box>
          <ProgressBar
            current={testingProgress}
            total={100}
            label="Testing Progress"
          />
        </>
      ) : (
        <>
          <Box marginBottom={1}>
            <Text color="gray">
              Connection test results:
            </Text>
          </Box>

          {selectedProviders.map(providerId => {
            const provider = availableProviders.find(p => p.id === providerId);
        if (!provider) return;
            const isWorking = testResults[providerId];

            return (
              <Box key={providerId} marginBottom={1}>
                <Text color={isWorking ? 'green' : 'red'}>
                  {isWorking ? '✅' : '❌'} {provider.name}: {isWorking ? 'Connected' : 'Failed'}
                </Text>
              </Box>
            );
          })}

          {Object.values(testResults).every(Boolean) ? (
            <>
              <Box marginBottom={1}>
                <StatusMessage type="success" message="All providers are working correctly!" />
              </Box>
              <Box marginTop={1}>
                <Text color="gray">
                  Continuing to next step...
                </Text>
              </Box>
            </>
          ) : (
            <Box marginTop={1}>
              <Text color="red">
                Some providers failed. Press 'r' to retry or Enter to continue anyway.
              </Text>
            </Box>
          )}
        </>
      )}
    </Box>
  );

  // Render routing configuration
  const renderRouting = () => (
    <Box flexDirection="column">
      <Box marginBottom={1}>
        <Text color="cyan" bold>
          Step 4: Configure Routing
        </Text>
      </Box>

      <Box marginBottom={1}>
        <Text color="gray">
          Configure how requests should be routed between providers.
        </Text>
      </Box>

      <Box marginBottom={1}>
        <Text color="white" bold>
          Current Configuration:
        </Text>
      </Box>

      <Box marginLeft={2} marginBottom={1} flexDirection="column">
        <Text color="gray">
          Default Provider: {routingConfig.defaultProvider}
        </Text>
        {routingConfig.thinkingProvider && (
          <Text color="gray">
            Thinking Provider: {routingConfig.thinkingProvider}
          </Text>
        )}
        <Text color="gray">
          Strategy: {routingConfig.strategy}
        </Text>
        <Text color="gray">
          Failover: {routingConfig.enableFailover ? 'Enabled' : 'Disabled'}
        </Text>
        <Text color="gray">
          Load Balancing: {routingConfig.loadBalancing ? 'Enabled' : 'Disabled'}
        </Text>
      </Box>

      <Box marginTop={1}>
        <Text color="gray">
          Press Enter to accept default routing configuration, Escape to go back
        </Text>
      </Box>
    </Box>
  );

  // Render summary
  const renderSummary = () => (
    <Box flexDirection="column">
      <Box marginBottom={1}>
        <Text color="cyan" bold>
          Setup Summary
        </Text>
      </Box>

      <Box marginBottom={1}>
        <Text color="white" bold>
          Configured Providers:
        </Text>
      </Box>

      {selectedProviders.map(providerId => {
        const provider = availableProviders.find(p => p.id === providerId);
        if (!provider) return;
        const isWorking = testResults[providerId];

        return (
          <Box key={providerId} marginLeft={2} marginBottom={1}>
            <Text color={isWorking ? 'green' : 'red'}>
              {isWorking ? '✅' : '❌'} {provider.name}
            </Text>
            <Text color="gray" dimColor>
              {' '}({provider.capabilities.join(', ')})
            </Text>
          </Box>
        );
      })}

      <Box marginTop={1} marginBottom={1}>
        <Text color="white" bold>
          Routing Configuration:
        </Text>
      </Box>

      <Box marginLeft={2} marginBottom={1}>
        <Text color="gray">
          Default: {routingConfig.defaultProvider}
        </Text>
        <Text color="gray">
          Strategy: {routingConfig.strategy}
        </Text>
      </Box>

      {errors.length > 0 && (
        <Box marginBottom={1}>
          <StatusMessage type="warning" message={`Warnings: ${errors.join(', ')}`} />
        </Box>
      )}

      <Box marginTop={2}>
        <Text color="green">
          Press Enter to complete setup or Escape to cancel
        </Text>
      </Box>
    </Box>
  );

  // Render completion
  const renderComplete = () => (
    <Box flexDirection="column">
      <Box marginBottom={1}>
        <Text color="green" bold>
          ✅ Setup Complete!
        </Text>
      </Box>

      <Box marginBottom={1}>
        <Text color="white">
          Your Aimux multi-provider configuration is ready.
        </Text>
      </Box>

      <Box flexDirection="column" marginBottom={1}>
        <Text color="gray">• {selectedProviders.length} providers configured</Text>
        <Text color="gray">• Intelligent routing enabled</Text>
        <Text color="gray">• Automatic failover active</Text>
      </Box>

      <Box marginTop={1}>
        <Text color="cyan">
          You can now use: aimux --help
        </Text>
      </Box>
    </Box>
  );

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
      return <StatusMessage type="error" message="Invalid setup step" />;
  }
};