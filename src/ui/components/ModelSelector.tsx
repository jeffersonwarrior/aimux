import React, { useState, useEffect, useCallback } from 'react';
import { Box, Text, useInput, useApp, useStdout } from 'ink';
import { ModelInfoImpl } from '../../models';

interface ModelSelectorProps {
  models: ModelInfoImpl[];
  onSelect: (regularModel: ModelInfoImpl | null, thinkingModel: ModelInfoImpl | null) => void;
  onCancel: () => void;
  searchPlaceholder?: string;
  initialRegularModel?: ModelInfoImpl | null;
  initialThinkingModel?: ModelInfoImpl | null;
  showProviderBadges?: boolean;
  providerCapabilities?: Record<string, string[]>;
  enableMultiProvider?: boolean;
}

export const ModelSelector: React.FC<ModelSelectorProps> = ({
  models,
  onSelect,
  onCancel,
  searchPlaceholder = 'Search models...',
  initialRegularModel = null,
  initialThinkingModel = null,
  showProviderBadges = true,
  providerCapabilities = {},
  enableMultiProvider = true
}) => {
  const [searchQuery, setSearchQuery] = useState('');
  const [selectedIndex, setSelectedIndex] = useState(0);
  const [filteredModels, setFilteredModels] = useState<ModelInfoImpl[]>(models);
  const [selectedRegularModel, setSelectedRegularModel] = useState<ModelInfoImpl | null>(initialRegularModel);
  const [selectedThinkingModel, setSelectedThinkingModel] = useState<ModelInfoImpl | null>(initialThinkingModel);

  const { exit } = useApp();
  const { write } = useStdout();

  // Helper function to get provider color and badge
  const getProviderBadge = (provider: string) => {
    if (!showProviderBadges) return null;

    const providerColors: Record<string, string> = {
      'minimax-m2': 'cyan',
      'z-ai': 'green',
      'synthetic-new': 'yellow',
      'anthropic': 'blue',
      'openai': 'magenta'
    };

    const providerShortNames: Record<string, string> = {
      'minimax-m2': 'M2',
      'z-ai': 'Z.A',
      'synthetic-new': 'S.N',
      'anthropic': 'ANT',
      'openai': 'OAI'
    };

    const color = providerColors[provider] || 'gray';
    const shortName = providerShortNames[provider] || provider.slice(0, 3).toUpperCase();

    return { color, shortName };
  };

  // Helper function to get capability badges
  const getCapabilityBadges = (model: ModelInfoImpl) => {
    if (!enableMultiProvider) return [];

    const badges: string[] = [];
    const capabilities = providerCapabilities[model.getProvider()] || [];

    if (model.id.toLowerCase().includes('thinking') || capabilities.includes('thinking')) {
      badges.push('🤔');
    }
    if (model.id.toLowerCase().includes('vision') || capabilities.includes('vision')) {
      badges.push('👁️');
    }
    if (model.id.toLowerCase().includes('tools') || capabilities.includes('tools')) {
      badges.push('🛠️');
    }

    return badges;
  };

  // Filter models based on search query
  useEffect(() => {
    if (!searchQuery) {
      setFilteredModels(models);
      return;
    }

    const query = searchQuery.toLowerCase();
    const filtered = models.filter(model => {
      const searchText = [
        model.id.toLowerCase(),
        model.getProvider().toLowerCase(),
        model.getModelName().toLowerCase()
      ].join(' ');

      return searchText.includes(query);
    });

    setFilteredModels(filtered);
    setSelectedIndex(0); // Reset selection when filter changes
  }, [searchQuery, models]);

  // Calculate visible range for better scrolling
  const visibleStartIndex = Math.max(0, selectedIndex - 5);
  const visibleEndIndex = Math.min(filteredModels.length, selectedIndex + 6);
  const visibleModels = filteredModels.slice(visibleStartIndex, visibleEndIndex);

  // Handle keyboard input
  useInput((input, key) => {
    // Handle special 't' key for thinking model selection when no search query exists
    if (input === 't' && !searchQuery && !key.ctrl && !key.meta) {
      if (filteredModels.length > 0 && selectedIndex < filteredModels.length) {
        const selectedModel = filteredModels[selectedIndex];
        if (selectedModel) {
          // Toggle thinking model selection
          if (selectedThinkingModel?.id === selectedModel.id) {
            setSelectedThinkingModel(null);
          } else {
            setSelectedThinkingModel(selectedModel);
          }
        }
      }
      return;
    }

    // Handle text input for search
    if (input && !key.ctrl && !key.meta && !key.return && !key.escape && !key.tab &&
        !key.upArrow && !key.downArrow && !key.leftArrow && !key.rightArrow &&
        !key.delete && !key.backspace && input !== 'q' && !(input === 't' && !searchQuery)) {
      setSearchQuery(prev => prev + input);
      return;
    }

    // Handle backspace
    if (key.backspace || key.delete) {
      setSearchQuery(prev => prev.slice(0, -1));
      return;
    }

    if (key.escape) {
      onCancel();
      exit();
      return;
    }

    // Space to launch with selections
    if (input === ' ') {
      if (selectedRegularModel || selectedThinkingModel) {
        onSelect(selectedRegularModel, selectedThinkingModel);
        exit();
      }
      return;
    }

    // Enter to select as regular model and launch
    if (key.return) {
      if (filteredModels.length > 0 && selectedIndex < filteredModels.length) {
        const selectedModel = filteredModels[selectedIndex];
        if (selectedModel) {
          onSelect(selectedModel, selectedThinkingModel);
          exit();
        }
      }
      return;
    }

    if (key.upArrow) {
      setSelectedIndex(prev => Math.max(0, prev - 1));
      return;
    }

    if (key.downArrow) {
      setSelectedIndex(prev => Math.min(filteredModels.length - 1, prev + 1));
      return;
    }


    // 'q' to quit
    if (input === 'q') {
      onCancel();
      exit();
    }
  });

  if (models.length === 0) {
    return (
      <Box flexDirection="column">
        <Text color="red">Error: No models available</Text>
        <Text color="gray">Press 'q' to quit or Escape to cancel</Text>
      </Box>
    );
  }

  return (
    <Box flexDirection="column">
      <Box marginBottom={1}>
        <Text color="cyan">Select Models:</Text>
      </Box>

      {/* Selection status */}
      <Box marginBottom={1}>
        <Text color="gray">
          Regular: {selectedRegularModel ? selectedRegularModel.getDisplayName() : "none"} |
          Thinking: {selectedThinkingModel ? selectedThinkingModel.getDisplayName() : "none"}
        </Text>
      </Box>

      <Box marginBottom={1}>
        <Text color="gray">Search: {searchQuery || "(type to search)"} </Text>
      </Box>

      {filteredModels.length > 0 ? (
        <>
          <Box marginBottom={1}>
            <Text color="gray">
              Found {filteredModels.length} model{filteredModels.length !== 1 ? 's' : ''}
            </Text>
          </Box>

          {/* Show scroll indicators if needed */}
          {visibleStartIndex > 0 && (
            <Box marginBottom={1}>
              <Text color="gray">▲ {visibleStartIndex} more above</Text>
            </Box>
          )}

          {visibleModels.map((model, index) => {
            const actualIndex = visibleStartIndex + index;
            const isRegularSelected = selectedRegularModel?.id === model.id;
            const isThinkingSelected = selectedThinkingModel?.id === model.id;
            const providerBadge = getProviderBadge(model.getProvider());
            const capabilityBadges = getCapabilityBadges(model);

            // Selection indicators
            const getSelectionIndicator = () => {
              if (isRegularSelected && isThinkingSelected) return '[R,T] ';
              if (isRegularSelected) return '[R] ';
              if (isThinkingSelected) return '[T] ';
              return '    ';
            };

            return (
              <Box key={model.id} marginBottom={1}>
                <Box flexDirection="column">
                  <Box>
                    <Text
                      color={actualIndex === selectedIndex ? 'green' :
                             isRegularSelected ? 'cyan' :
                             isThinkingSelected ? 'yellow' : 'white'}
                      bold={actualIndex === selectedIndex || isRegularSelected || isThinkingSelected}
                    >
                      {actualIndex === selectedIndex ? '▸ ' : '  '}
                      {getSelectionIndicator()}
                      {actualIndex + 1}. {model.getDisplayName()}
                    </Text>
                    {providerBadge && (
                      <Text color={providerBadge.color} bold backgroundColor={providerBadge.color === 'gray' ? undefined : providerBadge.color}>
                        {providerBadge.shortName}
                      </Text>
                    )}
                    {capabilityBadges.length > 0 && (
                      <Text color="gray">
                        {' '}{capabilityBadges.join(' ')}
                      </Text>
                    )}
                  </Box>
                  <Box marginLeft={4}>
                    <Text color="gray" dimColor>
                      Provider: {model.getProvider()}
                      {(model as any).context_length && ` | Context: ${Math.round((model as any).context_length / 1024)}K`}
                      {(model as any).quantization && ` | ${model.quantization}`}
                      {model.id.toLowerCase().includes('thinking') && !enableMultiProvider && ' | 🤔 Thinking'}
                    </Text>
                  </Box>
                </Box>
              </Box>
            );
          })}

          {/* Show scroll indicators if needed */}
          {visibleEndIndex < filteredModels.length && (
            <Box marginBottom={1}>
              <Text color="gray">▼ {filteredModels.length - visibleEndIndex} more below</Text>
            </Box>
          )}

          <Box marginTop={1}>
            <Text color="gray">
              ↑↓ Navigate | Enter: Regular Model + Launch | t: Toggle Thinking Model | Space: Launch | q: Quit
            </Text>
          </Box>
        </>
      ) : (
        <Box>
          <Text color="yellow">No models match your search.</Text>
          <Text color="gray">Try different search terms.</Text>
        </Box>
      )}
    </Box>
  );
};