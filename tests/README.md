# Aimux Test Infrastructure

This directory contains the migrated and enhanced test infrastructure for the aimux project, upgraded from the original synclaude tests.

## Test Structure

### Core Test Files

- **`setup.ts`** - Jest configuration and global test setup
- **`config.test.ts`** - Configuration management tests (migrated from synclaude)
- **`models.test.ts`** - Model information and handling tests (migrated from synclaude)

### New Multi-Provider Tests

- **`providers.test.ts`** - Multi-provider configuration and capability testing
- **`router.test.ts`** - Router logic, failover, and load balancing tests
- **`integration.test.ts`** - End-to-end multi-provider workflow tests
- **`utils.test.ts`** - Utility functions and helper tests

## Key Features

### Multi-Provider Configuration Testing
- Provider registration and validation
- Capability-based routing rules
- Provider enable/disable state management
- Configuration schema validation

### Router Logic Testing
- Request type detection (thinking vs regular)
- Provider routing based on capabilities
- Failover scenario handling
- Load balancing algorithms
- Request transformation between providers

### IntegrationWorkflow Testing
- Complete multi-provider setup workflows
- Hybrid mode configuration (thinking + regular models)
- Legacy configuration migration
- Error handling and fallback scenarios

### Utility Function Testing
- Model ID parsing and normalization
- Provider capability detection
- Performance metric calculations
- Error categorization and retry logic

## Running Tests

```bash
# Run all tests
npm test

# Run tests with coverage
npm run test:coverage

# Run tests in watch mode
npm run test:watch

# Run specific test file
npx jest tests/config.test.ts
```

## Migration Changes

### From Synclaude to Aimux

1. **Configuration Path**: Updated from `~/.config/synclaude` to `~/.config/aimux`
2. **Test Filenames**: All references updated from `synclaude-test-` to `aimux-test-`
3. **Schema Extensions**: Added multi-provider configuration fields:
   - `providers`: Record of provider configurations
   - `routingRules`: Request routing rules
   - `selectedThinkingModel`: Thinking-specific model selection

### Enhanced Test Coverage

The aimux test suite includes all original synclaude tests plus:

- **43 additional test cases** for multi-provider functionality
- **Integration scenarios** covering complete workflows
- **Performance monitoring** and metric calculation tests
- **Error handling** and failover scenario tests

## Test Results

All tests are currently passing:
- **46 tests total** (3 migrated, 43 new)
- **6 test suites**
- **100% pass rate**

## Recommendations for Multi-Provider Testing

### 1. Provider-Specific Testing
As new providers are implemented, add provider-specific test files:

```typescript
// tests/providers/minimax.test.ts
// tests/providers/zai.test.ts
// tests/providers/synthetic.test.ts
```

### 2. Performance Benchmarking
Consider adding performance tests:
- Request latency benchmarks
- Throughput measurements
- Resource usage monitoring
- Cost optimization validation

### 3. End-to-End Testing
Add comprehensive E2E tests:
- Real provider API integration
- Multi-request scenario testing
- Concurrent request handling
- Provider outage simulation

### 4. Contract Testing
Implement API contract tests:
- Provider response format validation
- Schema compatibility verification
- Backward compatibility testing

### 5. Stress Testing
Create stress test scenarios:
- High-volume request handling
- Memory usage under load
- Connection pool management
- Error recovery under stress

## Future Enhancements

### CI/CD Integration
- Automated test runs on PR submissions
- Coverage reporting and thresholds
- Performance regression detection
- Multi-environment testing (dev/staging/prod)

### Mock Providers
- Provider mocking for isolated testing
- Error scenario simulation
- Performance testing without real API calls
- Offline development testing

### Test Data Management
- Centralized test data fixtures
- Provider-specific test configurations
- Performance benchmark data
- Test scenario templates

This foundation provides comprehensive test coverage for aimux's multi-provider architecture while maintaining compatibility with the original synclaude test base.