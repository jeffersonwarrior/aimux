# CLAUDE.md

## Project Overview
Aimux v2.0.0 - Router-bridge-claude code system. Refactored from synclaude into C++ for speed. Claude Code will have aimux for the models. AIMUX will dynamically manage all providers and keys. Automatic high speed retry / failover to other providers with the same model class. Automatic key management (rotation of keys with same provider). For example, if Cerebras token limit reached (per minute) then switch to z.ai or synthetic. Automatic speed measurement (always pick the highest speed provider). All performance data logged for tracking over time. Charts/Provider Management via local WebUI.

## Development Commands
```bash
npm install && npm run build && npm link  # Setup
aimux --help                              # Test CLI
npm test                                  # Run tests
npm run lint && npm test && npm run build # Full quality check
```

## Architecture
- **CLI**: `src/cli/` (Commander.js)
- **Core**: `src/core/app.ts` (AimuxApp orchestrator)
- **Config**: `src/config/` (Zod validation, ~/.config/aimux/)
- **Models**: `src/models/` (API sync, caching)
- **UI**: `src/ui/` (Ink/React terminal)
- **API**: `src/api/` (Axios client, aimux/2.0.0)

## Key Paths
- Config: `~/.config/aimux/config.json`
- Cache: `~/.config/aimux/models_cache.json`
- Executable: `dist/cli/index.js`
- API: `https://api.synthetic.new`

## Code Standards
- TypeScript strict mode
- Zod validation everywhere
- Jest tests for all components
- ESLint + Prettier
- npm for all package management

## File Management
- Use `.claudeignore` to exclude archives and build artifacts
- Environment files (`.env*`) are accessible for configuration
- Archived files in `./archives/` contain old TypeScript implementation

## Credits
Forked from Synclaude by Paranjay Singh (parnexcodes) to Aimux v2.0.0 by Jefferson Nunn, Claude, and Codex.