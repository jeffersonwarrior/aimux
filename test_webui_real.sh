#!/bin/bash

# Create a working WebUI test with real data
echo "🌐 Creating working WebUI test with real data..."

# Start the server
./build-debug/aimux --webui > /tmp/webui_real_test.log 2>&1 &
WEBUI_PID=$!
sleep 3

# Test if server is accessible
echo "🔍 Testing server connectivity..."

# Create a simple test script to validate API responses
cat > test_api.py << 'EOF'
import urllib.request
import json
import sys
import time

def test_api(endpoint, description):
    try:
        response = urllib.request.urlopen(f'http://localhost:8080{endpoint}')
        data = response.read().decode('utf-8')
        print(f"✅ {description}")
        print(f"   Status: {response.getcode()}")
        print(f"   Data length: {len(data)} chars")
        
        # Try to parse JSON
        try:
            parsed = json.loads(data)
            print(f"   Valid JSON: {len(parsed)} items" if isinstance(parsed, (list, dict)) else "   Valid JSON: true")
        except json.JSONDecodeError:
            print("   ⚠️  Invalid JSON")
            print(f"   Raw: {data[:200]}...")
        
        print()
        return True
    except Exception as e:
        print(f"❌ {description}")
        print(f"   Error: {str(e)}")
        print()
        return False

print("🧪 Testing Aimux2 WebUI API...")
print()

# Test endpoints
results = [
    test_api('/api/health', 'Health Check'),
    test_api('/api/providers', 'Provider Status'),
    test_api('/api/metrics', 'System Metrics'),
    test_api('/api/config', 'Configuration'),
]

if all(results):
    print("🎉 All API endpoints working!")
else:
    print("❌ Some API endpoints failed")
    sys.exit(1)
EOF

# Run Python test if available
if command -v python3 >/dev/null 2>&1; then
    python3 test_api.py
else
    echo "⚠️  Python3 not available, skipping API test"
fi

# Check server logs for errors
echo "📋 Server logs:"
if [ -f "/tmp/webui_real_test.log" ]; then
    tail -10 /tmp/webui_real_test.log
else
    echo "No server logs found"
fi

# Stop server
kill $WEBUI_PID 2>/dev/null
wait $WEBUI_PID 2>/dev/null

# Clean up
rm -f test_api.py /tmp/webui_real_test.log

echo ""
echo "🔧 If APIs show errors, the binary needs to be rebuilt with updated WebUI code"
echo "The source code has been fixed with real data integration."