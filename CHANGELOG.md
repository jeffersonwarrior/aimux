# Changelog

All notable changes to this project will be documented in this file.

## [2.0.0-rc7] - 2025-11-09

### Fixed
- **Critical Bug**: Z.AI interceptor module resolution error `Cannot find module '../interceptors/zai-interceptor'`
- **Enhancement**: Implemented robust path resolution with multiple fallback strategies to make aimux installable from any location
- **Reliability**: Z.AI interceptor now works correctly in global, local, and development installations

### Technical Details
- Added multi-path resolution strategy that tries:
  - Relative path from compiled core directory (`../interceptors/zai-interceptor`)
  - Source directory fallback (`../../src/interceptors/zai-interceptor`)
  - Node_modules absolute path fallback (`require.resolve('aimux/dist/interceptors/zai-interceptor')`)
- Enhanced debugging output showing resolved interceptor path
- Z.AI proxy server starts successfully on localhost:8123

## [2.0.0-rc6] - 2025-11-08

### Added
- Z.AI HTTP Request Interceptor for model transformation fixes
- Automatic model format conversion: `hf:zai-org/GLM-4.6` → `GLM-4.6`
- Proxy server implementation for seamless Z.AI integration
- Enhanced error handling and debugging output

## [2.0.0-rc2] - 2025-01-08

### Added
- Comprehensive ESLint configuration and fixes
- TypeScript strict mode compilation
- Full test suite coverage (46 tests passing)
- Robust error handling improvements
- Critical bug fixes for minimax-m2 provider selection
- Dangerous flag passthrough functionality fixed
- Performance and memory leak assessments
- Quality assurance and code polish
- Build process reliability improvements
- npm linking verification and global installation tests

### Fixed
- **Critical Bug**: minimax-m2 provider selection issues
- **Critical Bug**: dangerous flag passthrough functionality
- ESLint configuration incompatibilities
- TypeScript compilation errors and type issues
- Require/require() statement conversion to ES imports
- Unused imports and variables cleanup
- Code formatting and prettier integration
- CLI argument validation improvements

### Improved
- Code quality and maintainability
- Build process reliability
- Test coverage and automation
- Error handling robustness across all components
- CLI command reliability and crash prevention
- Memory performance optimization
- Multi-provider stability

### Technical Changes
- Migrated from CommonJS require() to ES6 imports
- Fixed circular dependency issues in configuration modules
- Improved type safety with stricter TypeScript settings
- Enhanced linting rules with better error detection
- Optimized build output and distribution structure

### Security
- No security changes in this release
- Maintained existing API key security practices
- Verified no sensitive data leaks in error messages

### Tests
- All 46 tests now passing
- Added comprehensive integration tests
- Verified all provider commands functionality
- Tested edge cases and error conditions
- Performance benchmarking completed

## [2.0.0-rc.1] - Previous Release
- Initial v2.0 MVP release candidate
- Multi-provider support (Minimax M2, Z.AI, Synthetic.New)
- Basic routing and failover mechanisms
- TypeScript migration completion
- React-based terminal UI implementation

---

## Release Notes for v2.0.0-rc2

### Overview
This release candidate focuses on stability, reliability, and production readiness. Major emphasis has been placed on quality assurance, bug fixes, and ensuring the CLI does not crash under normal usage conditions.

### What's Changed
- **Stability**: Fixed critical crash-causing bugs
- **Code Quality**: Comprehensive linting and type safety improvements
- **Testing**: Full test suite coverage with 46 passing tests
- **Build**: Reliable build process with global installation verification
- **Performance**: Memory leak assessments and optimizations

### Migration from v2.0.0-rc.1
- No breaking changes
- Safe upgrade path for existing installations
- All existing functionality preserved
- Improved reliability for production use

### Installation
```bash
npm install -g aimux@2.0.0-rc2
```

### Support
- All provider commands tested and verified
- Comprehensive help system available
- Configuration management improved
- Error messages enhanced for better user experience