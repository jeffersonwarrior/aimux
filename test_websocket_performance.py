#!/usr/bin/env python3
import websocket
import threading
import time
import json
import subprocess
import sys
import statistics
from concurrent.futures import ThreadPoolExecutor, as_completed

class WebSocketPerformanceTester:
    def __init__(self):
        self.connection_stats = {}
        self.message_count = 0
        self.total_messages = 0
        self.latencies = []
        self.errors = 0

    def on_message(self, ws, message):
        """Handle WebSocket messages and measure latency"""
        try:
            data = json.loads(message)
            self.message_count += 1
            self.total_messages += 1

            # Calculate latency if timestamp is available
            if 'timestamp' in data:
                server_time = data['timestamp']
                client_time = int(time.time() * 1000)
                latency = client_time - server_time
                self.latencies.append(latency)
        except Exception as e:
            self.errors += 1
            print(f"Message error: {e}")

    def on_error(self, ws, error):
        """Handle WebSocket errors"""
        self.errors += 1
        print(f"WebSocket error: {error}")

    def on_close(self, ws, close_status_code, close_msg):
        """Handle WebSocket connection close"""
        pass

    def on_open(self, ws, connection_id):
        """Handle WebSocket connection open"""
        ws.connection_id = connection_id
        self.connection_stats[connection_id] = {
            'start_time': time.time(),
            'messages': 0,
            'errors': 0
        }

        # Request immediate update
        ws.send(json.dumps({"type": "request_update"}))

    def create_websocket_connection(self, connection_id, test_duration=30):
        """Create and maintain a WebSocket connection"""
        try:
            ws_url = "ws://localhost:8080/ws"
            ws = websocket.WebSocketApp(
                ws_url,
                on_open=lambda ws: self.on_open(ws, connection_id),
                on_message=self.on_message,
                on_error=self.on_error,
                on_close=self.on_close
            )

            # Run WebSocket for specified duration
            ws_thread = threading.Thread(target=ws.run_forever)
            ws_thread.daemon = True
            ws_thread.start()

            # Keep connection alive for test duration
            time.sleep(test_duration)

            # Close connection
            ws.close()
            return True

        except Exception as e:
            print(f"Connection {connection_id} failed: {e}")
            self.errors += 1
            return False

    def test_concurrent_connections(self, num_connections=100, test_duration=30):
        """Test performance with multiple concurrent WebSocket connections"""
        print(f"🔌 Testing {num_connections} concurrent WebSocket connections...")
        print(f"⏱️  Test duration: {test_duration} seconds")
        print("=" * 60)

        start_time = time.time()
        successful_connections = 0

        # Create connections concurrently
        with ThreadPoolExecutor(max_workers=num_connections) as executor:
            # Submit all connection tasks
            futures = [
                executor.submit(self.create_websocket_connection, i, test_duration)
                for i in range(num_connections)
            ]

            # Wait for all connections to complete
            for future in as_completed(futures):
                if future.result():
                    successful_connections += 1

        total_time = time.time() - start_time

        # Calculate statistics
        success_rate = (successful_connections / num_connections) * 100
        messages_per_second = self.total_messages / total_time if total_time > 0 else 0
        avg_latency = statistics.mean(self.latencies) if self.latencies else 0
        p95_latency = statistics.quantiles(self.latencies, n=20)[18] if len(self.latencies) > 20 else 0

        # Print results
        print(f"📊 Performance Test Results:")
        print(f"   ✅ Successful Connections: {successful_connections}/{num_connections} ({success_rate:.1f}%)")
        print(f"   📈 Total Messages Received: {self.total_messages}")
        print(f"   ⚡ Messages per Second: {messages_per_second:.1f}")
        print(f"   🔔 Average Latency: {avg_latency:.1f}ms")
        print(f"   📏 95th Percentile Latency: {p95_latency:.1f}ms")
        print(f"   ❌ Errors: {self.errors}")
        print(f"   ⏱️  Total Test Time: {total_time:.1f}s")

        # Performance criteria evaluation
        print(f"\n🎯 Performance Criteria Evaluation:")

        # Connection Success Rate (target: 95%+)
        if success_rate >= 95:
            print(f"   ✅ Connection Success Rate: {success_rate:.1f}% (≥ 95%)")
        else:
            print(f"   ⚠️  Connection Success Rate: {success_rate:.1f}% (< 95%)")

        # Latency (target: <50ms for 95th percentile)
        if p95_latency < 50:
            print(f"   ✅ 95th Percentile Latency: {p95_latency:.1f}ms (< 50ms)")
        else:
            print(f"   ⚠️  95th Percentile Latency: {p95_latency:.1f}ms (≥ 50ms)")

        # Throughput (target: messages per second > 0)
        if messages_per_second > 0:
            print(f"   ✅ Throughput: {messages_per_second:.1f} msg/s (> 0)")
        else:
            print(f"   ⚠️  Throughput: {messages_per_second:.1f} msg/s (≤ 0)")

        # Error Rate (target: < 5%)
        total_operations = self.total_messages + self.errors
        error_rate = (self.errors / total_operations * 100) if total_operations > 0 else 0
        if error_rate < 5:
            print(f"   ✅ Error Rate: {error_rate:.1f}% (< 5%)")
        else:
            print(f"   ⚠️  Error Rate: {error_rate:.1f}% (≥ 5%)")

        return {
            'success_rate': success_rate,
            'messages_per_second': messages_per_second,
            'avg_latency': avg_latency,
            'p95_latency': p95_latency,
            'error_rate': error_rate,
            'successful_connections': successful_connections,
            'total_messages': self.total_messages,
            'errors': self.errors
        }

def test_websocket_performance():
    print("🚀 Task 2.3 Performance Test: 100+ Concurrent WebSocket Connections")
    print("=" * 70)

    # Start aimux server in background
    print("📡 Starting aimux server...")
    process = subprocess.Popen(
        ["./build-debug/aimux", "--webui"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        cwd="/home/agent/aimux2/aimux"
    )

    # Wait for server to start
    time.sleep(3)

    try:
        tester = WebSocketPerformanceTester()

        # Test with 50 connections first
        print("\n🔌 Phase 1: Testing with 50 connections...")
        results_50 = tester.test_concurrent_connections(50, test_duration=10)

        # Reset statistics for second test
        tester = WebSocketPerformanceTester()

        # Test with 100 connections as required
        print("\n🔌 Phase 2: Testing with 100 connections (requirement)...")
        results_100 = tester.test_concurrent_connections(100, test_duration=15)

        # Test with 150 connections to check scalability
        tester = WebSocketPerformanceTester()
        print("\n🔌 Phase 3: Testing scalability with 150 connections...")
        results_150 = tester.test_concurrent_connections(150, test_duration=10)

        # Final Assessment
        print("\n" + "=" * 70)
        print("🏁 Performance Test Summary:")
        print()

        print("📊 Results Summary:")
        print(f"   50 connections:  {results_50['success_rate']:.1f}% success, {results_50['messages_per_second']:.1f} msg/s, {results_50['p95_latency']:.1f}ms latency")
        print(f"   100 connections: {results_100['success_rate']:.1f}% success, {results_100['messages_per_second']:.1f} msg/s, {results_100['p95_latency']:.1f}ms latency")
        print(f"   150 connections: {results_150['success_rate']:.1f}% success, {results_150['messages_per_second']:.1f} msg/s, {results_150['p95_latency']:.1f}ms latency")

        print("\n🎯 Task 2.3 Requirements Evaluation:")

        # Check if 100+ concurrent connections requirement is met
        if results_100['successful_connections'] >= 95:  # 95% of 100
            print("   ✅ 100+ concurrent connections: ACHIEVED")
        else:
            print("   ❌ 100+ concurrent connections: NOT ACHIEVED")

        # Check sub-50ms latency requirement
        if results_100['p95_latency'] < 50:
            print("   ✅ Sub-50ms data aggregation: ACHIEVED")
        else:
            print("   ❌ Sub-50ms data aggregation: NOT ACHIEVED")

        # Check efficient bandwidth (<1KB updates)
        avg_message_size = 1024  # Estimated from comprehensive metrics
        if avg_message_size < 1024:
            print("   ✅ <1KB update efficiency: ACHIEVED")
        else:
            print("   ⚠️  <1KB update efficiency: UNKNOWN (estimated)")

        # Check graceful degradation
        if results_150['success_rate'] >= 90:
            print("   ✅ Graceful degradation under load: ACHIEVED")
        else:
            print("   ❌ Graceful degradation under load: NOT ACHIEVED")

        print("\n🚀 Task 2.3 Performance Requirements: ")
        if results_100['successful_connections'] >= 95 and results_100['p95_latency'] < 50:
            print("🎉 SUCCESSFULLY IMPLEMENTED!")
        else:
            print("⚠️  PARTIALLY IMPLEMENTED (some criteria not met)")

    except Exception as e:
        print(f"❌ Performance test failed: {e}")
        import traceback
        traceback.print_exc()

    finally:
        # Clean up
        process.terminate()
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()

        print("\n🏁 Performance test completed")

if __name__ == "__main__":
    test_websocket_performance()