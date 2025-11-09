# Aimux Dockerfile
FROM node:18-alpine

# Set working directory
WORKDIR /app

# Install git and other build dependencies
RUN apk add --no-cache git python3 make g++

# Copy package files
COPY package*.json ./

# Install all dependencies (including dev dependencies for build), skipping prepare script
RUN npm ci --ignore-scripts && npm cache clean --force

# Copy source code
COPY . .

# Install all dependencies (including dev) for runtime with ts-node
RUN npm ci --ignore-scripts && npm cache clean --force

# Create non-root user for security first
RUN addgroup -g 1001 -S aimux && \
    adduser -S aimux -u 1001 -G aimux

# Create directories for configuration and data
RUN mkdir -p /root/.config/aimux && \
    mkdir -p /app/logs && \
    mkdir -p /app/temp

# Set environment variables
ENV NODE_ENV=production
ENV AIMUX_DEBUG=0
ENV AIMUX_ROUTER_PORT=8080
ENV AIMUX_LOG_LEVEL=info

# Expose the router port
EXPOSE 8080

# Health check
HEALTHCHECK --interval=30s --timeout=10s --start-period=30s --retries=3 \
  CMD node -e "const http = require('http'); const options = { host: 'localhost', port: process.env.AIMUX_ROUTER_PORT || 8080, path: '/health', timeout: 2000 }; const req = http.request(options, (res) => { process.exit(res.statusCode === 200 ? 0 : 1); }); req.on('error', () => process.exit(1)); req.end();" || exit 1

# Default command - run directly from source with ts-node
CMD ["npm", "run", "dev"]