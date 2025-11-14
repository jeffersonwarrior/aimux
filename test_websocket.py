#!/usr/bin/env python3
import asyncio
import websockets
import json

async def test_websocket():
    uri = "ws://localhost:18080/ws"

    try:
        async with websockets.connect(uri) as websocket:
            print("Connected to WebSocket")

            # Wait for initial data
            message = await websocket.recv()
            data = json.loads(message)

            print("Received initial data:")
            print(json.dumps(data, indent=2))

            # Send a test message
            test_message = json.dumps({"type": "ping"})
            await websocket.send(test_message)
            print(f"Sent: {test_message}")

            # Wait a moment for any response
            try:
                response = await asyncio.wait_for(websocket.recv(), timeout=2.0)
                print(f"Received: {response}")
            except asyncio.TimeoutError:
                print("No response received (expected)")

    except Exception as e:
        print(f"WebSocket error: {e}")

if __name__ == "__main__":
    asyncio.run(test_websocket())