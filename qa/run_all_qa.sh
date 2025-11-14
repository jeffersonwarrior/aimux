#!/bin/bash

# Aimux2 Master QA Test Runner
# Runs all QA procedures and generates comprehensive report

echo "🧪 Running Aimux2 Master QA Suite..."

# Check if we're in the right directory
if [ ! -f "CMakeLists.txt" ]; then
    echo "❌ ERROR: CMakeLists.txt not found. Please run from project root."
    exit 1
fi

# Create results directory
mkdir -p qa/results
mkdir -p qa/logs

# Initialize overall results
OVERALL_RESULTS_FILE="qa/results/master_qa_$(date +%Y%m%d_%H%M%S).json"
echo "{" > $OVERALL_RESULTS_FILE
echo '  "test_run": "'$(date -Iseconds)'"' >> $OVERALL_RESULTS_FILE
echo '  "version": "'$(./build-debug/aimux --version 2>/dev/null | head -1 || echo "unknown")'"' >> $OVERALL_RESULTS_FILE
echo '  "test_suites": {' >> $OVERALL_RESULTS_FILE

# Function to run test suite
run_test_suite() {
    local suite_name=$1
    local test_script=$2
    local status=0
    
    echo "🔨 Running $suite_name..."
    
    # Run test with timeout and error handling
    timeout 300 bash $test_script > qa/logs/${suite_name}_$(date +%Y%m%d_%H%M%S).log 2>&1
    status=$?
    
    if [ $status -eq 0 ]; then
        echo "✅ $suite_name PASSED"
        echo '    "'$suite_name'": {"status": "PASS"},' >> $OVERALL_RESULTS_FILE
        return 0
    elif [ $status -eq 124 ]; then
        echo "⏰ $suite_name TIMEOUT"
        echo '    "'$suite_name'": {"status": "TIMEOUT"},' >> $OVERALL_RESULTS_FILE
        return 1
    else
        echo "❌ $suite_name FAILED (exit code: $status)"
        echo '    "'$suite_name'": {"status": "FAIL", "exit_code": '$status'},' >> $OVERALL_RESULTS_FILE
        return 1
    fi
}

# Track overall status
PASSED_SUITES=0
FAILED_SUITES=0
TOTAL_SUITES=0

# 1. Build Test
TOTAL_SUITES=$((TOTAL_SUITES + 1))
if run_test_suite "build_system" "qa/build_test.sh"; then
    PASSED_SUITES=$((PASSED_SUITES + 1))
else
    FAILED_SUITES=$((FAILED_SUITES + 1))
fi

# 2. Installation Test
TOTAL_SUITES=$((TOTAL_SUITES + 1))
if run_test_suite "installation" "qa/installation_test.sh"; then
    PASSED_SUITES=$((PASSED_SUITES + 1))
else
    FAILED_SUITES=$((FAILED_SUITES + 1))
fi

# 3. Performance Test
TOTAL_SUITES=$((TOTAL_SUITES + 1))
if run_test_suite "performance" "qa/performance_test.sh"; then
    PASSED_SUITES=$((PASSED_SUITES + 1))
else
    FAILED_SUITES=$((FAILED_SUITES + 1))
fi

# 4. Security Test
TOTAL_SUITES=$((TOTAL_SUITES + 1))
if run_test_suite "security" "qa/security_test.sh"; then
    PASSED_SUITES=$((PASSED_SUITES + 1))
else
    FAILED_SUITES=$((FAILED_SUITES + 1))
fi

# 5. Integration Tests (if they exist)
if [ -f "./build-debug/aimux" ]; then
    TOTAL_SUITES=$((TOTAL_SUITES + 1))
    echo "🔨 Running integration_tests..."
    
    # Run built-in integration tests
    ./build-debug/aimux --test > qa/logs/integration_tests_$(date +%Y%m%d_%H%M%S).log 2>&1
    if [ $? -eq 0 ]; then
        echo "✅ integration_tests PASSED"
        echo '    "integration_tests": {"status": "PASS"},' >> $OVERALL_RESULTS_FILE
        PASSED_SUITES=$((PASSED_SUITES + 1))
    else
        echo "❌ integration_tests FAILED"
        echo '    "integration_tests": {"status": "FAIL"},' >> $OVERALL_RESULTS_FILE
        FAILED_SUITES=$((FAILED_SUITES + 1))
    fi
fi

# 6. WebUI Test
if [ -f "./build-debug/aimux" ]; then
    TOTAL_SUITES=$((TOTAL_SUITES + 1))
    echo "🔨 Running webui..."
    
    # Start WebUI and test it
    ./build-debug/aimux --webui > qa/logs/webui_$(date +%Y%m%d_%H%M%S).log 2>&1 &
    WEBUI_PID=$!
    sleep 5
    
    # Test if WebUI is responsive
    if curl -s "http://localhost:8080" > /dev/null 2>&1; then
        echo "✅ webui PASSED"
        echo '    "webui": {"status": "PASS"},' >> $OVERALL_RESULTS_FILE
        PASSED_SUITES=$((PASSED_SUITES + 1))
    else
        echo "❌ webui FAILED"
        echo '    "webui": {"status": "FAIL"},' >> $OVERALL_RESULTS_FILE
        FAILED_SUITES=$((FAILED_SUITES + 1))
    fi
    
    # Stop WebUI
    kill $WEBUI_PID 2>/dev/null
fi

# Remove trailing comma and close test_suites object
if command -v sed >/dev/null 2>&1; then
    sed -i '' '$ s/,$//' $OVERALL_RESULTS_FILE 2>/dev/null || sed -i '$ s/,$//' $OVERALL_RESULTS_FILE 2>/dev/null
fi
echo '' >> $OVERALL_RESULTS_FILE
echo '  },' >> $OVERALL_RESULTS_FILE

# Generate summary
echo '  "summary": {' >> $OVERALL_RESULTS_FILE
echo '    "total_suites": '$TOTAL_SUITES',' >> $OVERALL_RESULTS_FILE
echo '    "passed_suites": '$PASSED_SUITES',' >> $OVERALL_RESULTS_FILE
echo '    "failed_suites": '$FAILED_SUITES',' >> $OVERALL_RESULTS_FILE
PASS_RATE=$((PASSED_SUITES * 100 / TOTAL_SUITES))
echo '    "pass_rate": '$PASS_RATE',' >> $OVERALL_RESULTS_FILE

# Determine overall status
if [ $PASS_RATE -ge 80 ]; then
    echo '    "overall_status": "PASS",' >> $OVERALL_RESULTS_FILE
    echo '    "passing_threshold": 80,' >> $OVERALL_RESULTS_FILE
    echo '    "test_passed": true' >> $OVERALL_RESULTS_FILE
else
    echo '    "overall_status": "FAIL",' >> $OVERALL_RESULTS_FILE
    echo '    "passing_threshold": 80,' >> $OVERALL_RESULTS_FILE
    echo '    "test_passed": false' >> $OVERALL_RESULTS_FILE
fi

echo '    "duration_seconds": '$SECONDS',' >> $OVERALL_RESULTS_FILE
echo '    "timestamp": "'$(date -Iseconds)'"' >> $OVERALL_RESULTS_FILE
echo '  }' >> $OVERALL_RESULTS_FILE
echo '}' >> $OVERALL_RESULTS_FILE

# Final summary
echo ""
echo "🎯 QA Test Results Summary:"
echo "  Total Test Suites: $TOTAL_SUITES"
echo "  Passed: $PASSED_SUITES"
echo "  Failed: $FAILED_SUITES"
echo "  Pass Rate: ${PASS_RATE}%"
echo "  Duration: ${SECONDS} seconds"
echo "  Results File: $OVERALL_RESULTS_FILE"

# Show log directory
echo ""
echo "📂 Log files saved to: qa/logs/"
ls -la qa/logs/ | tail -5

if [ $PASS_RATE -ge 80 ]; then
    echo ""
    echo "🎉 MASTER QA TEST PASSED"
    echo "✅ Aimux2 is ready for production"
    exit 0
else
    echo ""
    echo "❌ MASTER QA TEST FAILED"
    echo "🔍 Please review failed test suites in qa/logs/"
    exit 1
fi