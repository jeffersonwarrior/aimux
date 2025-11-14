# Multi-stage build for Aimux v2.0.0
FROM ubuntu:22.04 AS builder

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    pkg-config \
    git \
    curl \
    libssl-dev \
    libcurl4-openssl-dev \
    nlohmann-json3-dev \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /app

# Copy source code
COPY . .

# Build configuration
RUN mkdir build && cd build && \
    cmake .. -GNinja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr/local

# Build application
RUN cd build && ninja

# Production stage
FROM ubuntu:22.04

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
    libssl3 \
    libcurl4 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Create non-root user
RUN useradd -m -u 1000 aimux && \
    mkdir -p /app/config /app/logs /app/data && \
    chown -R aimux:aimux /app

WORKDIR /app

# Copy built applications from builder stage
COPY --from=builder /app/build/aimux /usr/local/bin/
COPY --from=builder /app/build/claude_gateway /usr/local/bin/
COPY --from=builder /app/build/test_dashboard /usr/local/bin/

# Copy configuration files
COPY config.json /app/config/
COPY production-config.json /app/config/
COPY scripts/ /app/scripts/

# Set permissions
RUN chmod +x /app/scripts/*.sh && \
    chmod +x /usr/local/bin/*

# Switch to non-root user
USER aimux

# Expose ports
EXPOSE 8080 8081 8888 9443

# Health check
HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD /usr/local/bin/aimux --status || exit 1

# Default command
ENTRYPOINT ["/usr/local/bin/aimux"]
CMD ["--config", "/app/config/config.json", "--webui"]

# Labels
LABEL version="2.0.0" \
      description="Aimux AI service router v2.0.0" \
      maintainer="Jefferson Nunn <jefferson@example.com>" \
      org.opencontainers.image.title="Aimux v2.0.0" \
      org.opencontainers.image.description="High-performance AI service router with multi-provider support" \
      org.opencontainers.image.version="2.0.0" \
      org.opencontainers.image.vendor="Aimux Team"