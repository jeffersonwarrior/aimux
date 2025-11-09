# Aimux Docker Environment

A complete Docker-based testing environment for the aimux multi-provider AI CLI system, pre-configured with Minimax M2 provider support.

## 🚀 Quick Start

```bash
# 1. Set up the environment
./scripts/setup-docker-env.sh

# 2. Run comprehensive tests
./scripts/test-docker.sh test

# 3. Start interactive testing (optional)
./scripts/test-docker.sh interactive
```

## 📋 What's Included

- ✅ **Pre-built Docker image** with aimux and all dependencies
- ✅ **Minimax M2 provider** already configured with API key
- ✅ **Automated test suite** for validation
- ✅ **Interactive testing** environment
- ✅ **Monitoring and debugging** tools
- ✅ **Comprehensive documentation**

## 🏗️ Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    Docker Host                               │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────────┐    ┌─────────────────┐                 │
│  │   aimux-contai  │    │  test-server    │                 │
│  │      ner        │    │   (nginx)       │                 │
│  │                 │    │                 │                 │
│  │  ┌─────────────┐│    │  ┌─────────────┐│                 │
│  │  │   aimux     ││    │  │   nginx     ││                 │
│  │  │   CLI       ││    │  │   server    ││                 │
│  │  └─────────────┘│    │  └─────────────┘│                 │
│  │  ┌─────────────┐│    │                 │                 │
│  │  │  Router     ││◄───┤  HTTP Requests  │                 │
│  │  │ (Port 8080) ││    │                 │                 │
│  │  └─────────────┘│    └─────────────────┘                 │
│  └─────────────────┘                                       │
│                                                             │
│  Host: localhost:8080 ◄─────────────────────────────────────┤
└─────────────────────────────────────────────────────────────┘
```

## 📁 Project Structure

```
aimux/
├── 🐳 Dockerfile                    # Docker image build
├── 🐋 docker-compose.yml           # Service orchestration
│
├── 🛠️  scripts/                     # Testing and utility scripts
│   ├── setup-docker-env.sh         # 🚀 Environment setup
│   ├── test-docker.sh              # 🧪 Main test suite
│   ├── quick-test.sh               # ⚡ Quick health checks
│   ├── view-logs.sh                # 📊 Log monitoring
│   └── provider-test.sh            # 🔍 Provider tests
│
├── ⚙️  test-config/                 # Configuration files
│   └── aimux-config.json           # Pre-configured Minimax M2
│
├── 📝 test-data/                    # Test data and samples
│   ├── test-prompt.txt             # Basic connectivity test
│   ├── thinking-test.txt           # Thinking capabilities test
│   └── samples/                    # API test payloads
│
└── 📚 Documentation/
    ├── DOCKER_TESTING.md           # 📖 Comprehensive testing guide
    └── README_DOCKER.md            # 📋 This file
```

## 🎯 Features

### 🔧 Automated Testing
- **Build Validation**: Ensures Docker image builds correctly
- **Service Health**: Verifies all containers start and stay healthy
- **CLI Functionality**: Tests aimux commands and provider integration
- **API Connectivity**: Validates Minimax M2 API access
- **Router Testing**: Checks request routing functionality

### 🧪 Provider Testing
- **Minimax M2 Integration**: Full API key validation and model testing
- **Model Capabilities**: Verify thinking, tools, and streaming support
- **Configuration Loading**: Test provider configuration parsing
- **Request Routing**: Validate HTTP proxy functionality

### 📊 Monitoring
- **Real-time Logs**: Follow container logs in real-time
- **Health Checks**: Continuous service health monitoring
- **Performance Metrics**: Resource usage and response times
- **Error Diagnostics**: Detailed error reporting and debugging

## 🚦 Available Commands

### Setup Commands
```bash
./scripts/setup-docker-env.sh    # 🚀 Initial environment setup
./scripts/test-docker.sh build   # 🏗️  Build Docker image only
./scripts/test-docker.sh start   # 🚀 Start containers only
```

### Testing Commands
```bash
./scripts/test-docker.sh test          # 🧪 Run full test suite
./scripts/test-docker.sh interactive   # 🎮 Enter interactive mode
./scripts/quick-test.sh               # ⚡ Quick health check
```

### Provider Testing
```bash
./scripts/provider-test.sh all         # 🧪 Test all provider features
./scripts/provider-test.sh minimax     # 🔑 Test Minimax M2 API
./scripts/provider-test.sh models      # 📊 Test model definitions
./scripts/provider-test.sh router      # 🌐 Test request routing
```

### Monitoring Commands
```bash
./scripts/view-logs.sh          # 📊 View live logs
./scripts/test-docker.sh logs   # 📋 Show recent logs
docker ps                       # 📋 Check container status
docker stats aimux-test         # 📊 Resource usage
```

## 🔌 API Endpoints

### Router Endpoints
```
GET  http://localhost:8080/health           # Health check
POST http://localhost:8080/v1/chat/completions  # Chat completions (proxy)
```

### Provider Direct API
```bash
# Minimax M2 Direct API
POST https://api.minimax.chat/v1/chat/completions
Headers: Authorization: Bearer <API_KEY>
```

## 🧪 Test Scenarios

### 1. ✅ Basic Connectivity Test
Tests if aimux can communicate with Minimax M2 API:
```bash
./scripts/provider-test.sh minimax
```

### 2. 🤔 Thinking Capabilities Test
Validates model's reasoning abilities:
```bash
curl -X POST https://api.minimax.chat/v1/chat/completions \
  -H "Authorization: Bearer $MINIMAX_API_KEY" \
  -H "Content-Type: application/json" \
  -d '{"model":"abab6.5s-chat","messages":[{"role":"user","content":"Solve step by step: 5*3+2"}]}'
```

### 3. 🌐 Router Request Flow Test
Tests request routing through aimux router:
```bash
curl -X POST http://localhost:8080/v1/chat/completions \
  -H "Content-Type: application/json" \
  -d '{"model":"abab6.5-chat","messages":[{"role":"user","content":"Router test"}]}'
```

## 🔧 Configuration

### Environment Variables
```bash
# Core Configuration
NODE_ENV=production
AIMUX_ROUTER_PORT=8080
AIMUX_HOST=0.0.0.0

# Logging
AIMUX_DEBUG=1
AIMUX_LOG_LEVEL=debug

# Minimax M2 (Pre-configured)
MINIMAX_API_KEY=eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9...
MINIMAX_BASE_URL=https://api.minimax.chat
```

### Provider Configuration
The Minimax M2 provider includes:
- ✅ **3 Models**: ABAB6, ABAB6.5, ABAB6.5s
- ✅ **Thinking Support**: Enhanced reasoning capabilities
- ✅ **Tool Usage**: Function calling support
- ✅ **Streaming**: Real-time response streaming
- ✅ **Rate Limiting**: 60 requests/min, 100k tokens/min

## 🚨 Troubleshooting

### Quick Diagnostics
```bash
# Run diagnostic tests
./scripts/provider-test.sh all

# Check container status
docker ps | grep aimux-test

# View recent logs
docker logs --tail 50 aimux-test
```

### Common Issues

| Issue | Symptom | Solution |
|-------|---------|----------|
| Port conflict | `Port 8080 already in use` | Change `AIMUX_ROUTER_PORT` in `.env` |
| API key invalid | `401 Unauthorized` | Verify Minimax M2 API key |
| Container won't start | Build failures | Check Node.js version, run cleanup |
| Router not responding | Connection refused | Check router logs, verify port mapping |

### Debug Mode
```bash
# Enable verbose logging
export AIMUX_DEBUG=1
export AIMUX_LOG_LEVEL=debug

# Re-run with debug
./scripts/test-docker.sh test
```

## 📖 Full Documentation

- **📋 Comprehensive Testing Guide**: `DOCKER_TESTING.md`
- **🏗️ Architecture Overview**: `ROUTING_IMPLEMENTATION.md`
- **🚀 CLI Usage**: `aimux --help` (in container)

## 🔄 Development Workflow

### 1. Initial Setup
```bash
git clone <aimux-repo>
cd aimux
./scripts/setup-docker-env.sh
```

### 2. Development Loop
```bash
# Make changes to code
./scripts/test-docker.sh build   # Rebuild image
./scripts/test-docker.sh test    # Run tests
```

### 3. Interactive Testing
```bash
./scripts/test-docker.sh interactive
docker exec -it aimux-test bash  # Shell access
```

## 🚀 Production Deployment

### Environment-Specific Config
```bash
# Production environment
export NODE_ENV=production
export AIMUX_DEBUG=0
export AIMUX_LOG_LEVEL=warn

# Deploy
docker-compose -f docker-compose.yml -f docker-compose.prod.yml up -d
```

### Security Considerations
- 🔒 API keys are set via environment variables (not in code)
- 🔒 Router listens on localhost by default
- 🔒 Container runs as non-root user
- 🔒 Input validation and request sanitization

## 🤝 Contributing

1. **Run tests**: `./scripts/test-docker.sh test`
2. **Add new features**: Update Docker image and tests
3. **Documentation**: Keep this README and `DOCKER_TESTING.md` updated
4. **Security**: Never commit API keys or sensitive data

## 📞 Support

- **Issues**: Check `DOCKER_TESTING.md` troubleshooting section
- **Logs**: Use `./scripts/view-logs.sh` for real-time monitoring
- **Tests**: Run `./scripts/provider-test.sh all` for diagnostics

---

🎉 **Ready to test aimux with Minimax M2!** 🎉

The Docker environment is now fully configured and ready for comprehensive testing of all aimux features including provider authentication, model fetching, intelligent routing, and failover capabilities.