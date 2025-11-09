#!/bin/bash

# View aimux container logs
CONTAINER_NAME="aimux-test"

if docker ps --format '{{.Names}}' | grep -q "^${CONTAINER_NAME}$"; then
    docker logs -f "${CONTAINER_NAME}"
else
    echo "Container ${CONTAINER_NAME} is not running"
    exit 1
fi
