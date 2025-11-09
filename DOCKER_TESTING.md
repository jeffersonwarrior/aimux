# Aimux Docker Testing Guide

This guide provides comprehensive instructions for testing the aimux multi-provider system in a Docker environment using the provided Minimax M2 API key.

## Overview

The Docker testing environment includes:

- **Dockerfile**: Builds aimux with all dependencies
- **docker-compose.yml**: Orchestrates services and environment
- **Test Configuration**: Pre-configured Minimax M2 provider setup
- **Test Scripts**: Automated testing and validation scripts
- **Test Data**: Sample prompts and test scenarios

## Quick Start

### Prerequisites

Ensure you have the following installed:
- Docker (version 20.10+)
- Docker Compose (version 2.0+)
- curl
- jq (optional, for JSON parsing)

### 1. Initial Setup

```bash
# Clone or navigate to aimux project directory
cd /path/to/aimux

# Run the setup script to prepare the environment
./scripts/setup-docker-env.sh
```

This creates:
- `.env` file with environment variables
- `.dockerignore` for optimized builds
- Test data structure and samples
- Helper scripts

### 2. Run Full Test Suite

```bash
# Build and test everything
./scripts/test-docker.sh test
```

This script:
- ✅ Cleans up existing containers
- ✅ Builds the Docker image
- ✅ Starts all services
- ✅ Runs comprehensive tests
- ✅ Shows results and logs (if any failures)

### 3. Quick Health Check

```bash
# Quick test of running environment
./scripts/quick-test.sh
```

## Detailed Testing Guide

## File Structure

```
aimux/
├── Dockerfile                    # Main Docker image definition
├── docker-compose.yml           # Service orchestration
├── .env                         # Environment variables (auto-created)
├── .dockerignore               # Build exclusions (auto-created)
├── scripts/
│   ├── setup-docker-env.sh     # Initial environment setup
│   ├── test-docker.sh          # Main testing script
│   ├── quick-test.sh           # Quick health checks
│   ├── view-logs.sh            # Log monitoring
│   └── provider-test.sh        # Provider-specific tests
├── test-config/
│   └── aimux-config.json       # Pre-configured aimux settings
├── test-data/
│   ├── test-prompt.txt         # Basic connectivity test
│   ├── thinking-test.txt       # Thinking capabilities test
│   └── samples/
│       ├── simple-chat.json    # API test payload
│       └── thinking-test.json  # Thinking API test
└── test/fixtures/
    └── nginx.conf             # Test server configuration
```

## Configuration

### Environment Variables

The Docker environment is configured with these key variables:

```bash
# Router Configuration
AIMUX_ROUTER_PORT=8080          # Router HTTP port
AIMUX_HOST=0.0.0.0              # Listen on all interfaces

# Provider Configuration
MINIMAX_API_KEY=eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9...  # Your Minimax M2 key
MINIMAX_BASE_URL=https://api.minimax.chat

# Logging and Debugging
AIMUX_DEBUG=1                   # Enable debug logging
AIMUX_LOG_LEVEL=debug           # Verbose logging
NODE_ENV=production             # Production mode
```

### Provider Configuration

The Minimax M2 provider is pre-configured in `test-config/aimux-config.json`:

```json
{
  "providers": {
    "minimax-m2": {
      "name": "Minimax M2",
      "enabled": true,
      "apiKey": "your-api-key-here",
      "baseUrl": "https://api.minimax.chat",
      "models": [
        {
          "id": "abab6.5s-chat",
          "name": "Minimax ABAB6.5s",
          "capabilities": ["text", "thinking"],
          "maxTokens": 24576,
          "supportsStreaming": true
        }
      ],
      "capabilities": {
        "thinking": true,
        "vision": false,
        "tools": true,
        "streaming": true
      }
    }
  }
}
```

## Testing Procedures

### 1. Comprehensive Testing

```bash
# Run all tests including build, deployment, and functionality tests
./scripts/test-docker.sh test
```

**What this tests:**
- ✅ Docker image builds successfully
- ✅ Container starts and runs
- ✅ Router starts on port 8080
- ✅ aimux CLI commands work
- ✅ Provider connectivity
- ✅ Configuration loading
- ✅ Health endpoints

### 2. Provider-Specific Testing

```bash
# Test all provider functionality
./scripts/provider-test.sh all

# Test specific aspects
./scripts/provider-test.sh minimax       # API key validation
./scripts/provider-test.sh models        # Model definitions
./scripts/provider-test.sh router        # Request routing
./scripts/provider-test.sh config        # Configuration loading
./scripts/provider-test.sh capabilities  # Capability definitions
```

### 3. Manual Testing

#### Interactive Mode

```bash
# Start containers and enter interactive mode
./scripts/test-docker.sh interactive
```

Then you can test manually:

```bash
# Test aimux CLI commands
docker exec -it aimux-test aimux --help
docker exec -it aimux-test aimux providers
docker exec -it aimux-test aimux minimax setup

# Test provider directly
docker exec -it aimux-test sh

# Inside container test Minimax API
curl -X POST https://api.minimax.chat/v1/chat/completions \
  -H "Authorization: Bearer $MINIMAX_API_KEY" \
  -H "Content-Type: application/json" \
  -d '{"model":"abab6-chat","messages":[{"role":"user","content":"Hello"}],"max_tokens":50}'
```

#### Router Testing

```bash
# Test router health endpoint
curl http://localhost:8080/health

# Test chat completions through router
curl -X POST http://localhost:8080/v1/chat/completions \
  -H "Content-Type: application/json" \
  -d @test-data/samples/simple-chat.json
```

### 4. Monitoring and Debugging

#### View Logs

```bash
# View live logs
./scripts/view-logs.sh

# Or use docker directly
docker logs -f aimux-test

# View last 50 lines
docker logs --tail 50 aimux-test
```

#### Health Status

```bash
# Check container status
docker ps

# Check container health
docker inspect aimux-test | grep Health

# Test router connectivity
curl -f http://localhost:8080/health || echo "Router not responding"
```

## Test Scenarios

### Basic Connectivity Test

Test file: `test-data/test-prompt.txt`

```bash
# Test basic communication
docker exec -it aimux-test node -e "
const axios = require('axios');
const fs = require('fs');

const prompt = fs.readFileSync('/app/test-data/test-prompt.txt', 'utf8');

axios.post('https://api.minimax.chat/v1/chat/completions', {
  model: 'abab6-chat',
  messages: [{ role: 'user', content: prompt }],
  max_tokens: 200
}, {
  headers: {
    'Authorization': 'Bearer ' + process.env.MINIMAX_API_KEY,
    'Content-Type': 'application/json'
  }
})
.then(response => {
  console.log('✅ Basic connectivity test passed');
  console.log('Response:', response.data.choices[0].message.content);
})
.catch(error => {
  console.error('❌ Basic connectivity test failed:', error.message);
});
"
```

### Thinking Capabilities Test

Test file: `test-data/thinking-test.txt`

```bash
# Test thinking model capabilities
curl -X POST https://api.minimax.chat/v1/chat/completions \
  -H "Authorization: Bearer $MINIMAX_API_KEY" \
  -H "Content-Type: application/json" \
  -d '{
    "model": "abab6.5s-chat",
    "messages": [{"role":"user","content":"Solve step by step: 2+2*3"}],
    "temperature": 0.5,
    "max_tokens": 300
  }'
```

### Router Request Flow Test

```bash
# Test request flow through router
curl -X POST http://localhost:8080/v1/chat/completions \
  -H "Content-Type: application/json" \
  -d '{
    "model": "abab6.5-chat",
    "messages": [{"role":"user","content":"Router test"}],
    "max_tokens": 50
  }'
```

## Troubleshooting

### Common Issues

#### 1. Container Won't Start

```bash
# Check Docker status
docker --version
docker-compose --version

# Check system resources
docker system df

# Clean up if needed
./scripts/test-docker.sh cleanup
```

#### 2. API Key Issues

```bash
# Test API key validity
./scripts/provider-test.sh minimax

# Check environment variables
docker exec aimux-test env | grep MINIMAX
```

#### 3. Router Not Responding

```bash
# Check router logs
docker logs aimux-test | grep -i router

# Check port mapping
docker ps | grep 8080

# Test network connectivity
docker exec aimux-test curl -f http://localhost:8080/health
```

#### 4. Build Failures

```bash
# Check build logs
docker build -t aimux:latest . --no-cache

# Check Node.js version compatibility
docker run --rm node:18-alpine node --version
```

### Debug Mode

Enable verbose debugging:

```bash
# Set debug environment variables
export AIMUX_DEBUG=1
export AIMUX_LOG_LEVEL=debug

# Re-run tests
./scripts/test-docker.sh test
```

### Port Conflicts

If port 8080 is already in use:

```bash
# Check what's using the port
netstat -tuln | grep 8080
lsof -i :8080

# Change port in .env and docker-compose.yml
AIMUX_ROUTER_PORT=8081
# Update ports mapping accordingly
```

## Performance Testing

### Load Testing

```bash
# Simple concurrent requests test
for i in {1..5}; do
  curl -s http://localhost:8080/health &
done
wait

# Measure response times
time curl -s http://localhost:8080/health
```

### Resource Monitoring

```bash
# Monitor container resource usage
docker stats aimux-test

# Check memory usage
docker exec aimux-test free -h

# Check disk usage
docker exec aimux-test df -h
```

## Cleanup

### Stop and Clean

```bash
# Stop containers
docker-compose down

# Remove images and containers
docker-compose down --rmi all

# Cleanup test data
docker volume prune -f
docker system prune -f
```

### Reset Environment

```bash
# Complete reset
./scripts/test-docker.sh cleanup
rm -rf .env test-data/ logs/
./scripts/setup-docker-env.sh
```

## Advanced Configuration

### Adding New Providers

1. Update `test-config/aimux-config.json`
2. Add API keys to `.env`
3. Update provider tests in `scripts/provider-test.sh`
4. Test with `./scripts/provider-test.sh all`

### Custom Router Configuration

Modify `docker-compose.yml`:

```yaml
services:
  aimux:
    environment:
      - AIMUX_ROUTER_PORT=9080  # Custom port
      - AIMUX_HOST=localhost     # Custom host
      - AIMUX_LOG_LEVEL=info     # Custom log level
```

### Production Deployment

For production use:

```yaml
# docker-compose.prod.yml
version: '3.8'
services:
  aimux:
    image: aimux:latest
    restart: always
    environment:
      - NODE_ENV=production
      - AIMUX_DEBUG=0
      - AIMUX_LOG_LEVEL=warn
    deploy:
      resources:
        limits:
          memory: 512M
          cpus: '0.5'
```

## Integration with CI/CD

### GitHub Actions Example

```yaml
name: Docker Test
on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2

      - name: Set up Docker
        uses: docker/setup-buildx-action@v1

      - name: Test Docker environment
        run: |
          chmod +x scripts/*.sh
          ./scripts/setup-docker-env.sh
          ./scripts/test-docker.sh test
```

## Support

For issues with the Docker testing environment:

1. Check this documentation first
2. Review container logs: `docker logs aimux-test`
3. Run diagnostic tests: `./scripts/provider-test.sh all`
4. Check GitHub Issues for known problems
5. Create a new issue with detailed logs and environment info

---

**Note:** This Docker environment is specifically configured for testing with the provided Minimax M2 API key. Ensure the API key remains secure and never commit it to version control.