#!/bin/bash

# Aimux v2.0.0 - Test Runner with Coverage Analysis
# Runs comprehensive tests and generates coverage reports

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Configuration
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build-coverage"
COVERAGE_DIR="$PROJECT_ROOT/docs/coverage"
SOURCE_DIR="$PROJECT_ROOT"
TEST_DIR="$PROJECT_ROOT/tests"

# Minimum coverage thresholds
MIN_COVERAGE_CRITICAL=95
MIN_COVERAGE_OVERALL=90

# Function to print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

print_header() {
    echo -e "${PURPLE}=== $1 ===${NC}"
}

# Check dependencies
check_dependencies() {
    print_header "Checking Dependencies"

    local missing_deps=()

    if ! command -v cmake &> /dev/null; then
        missing_deps+=("cmake")
    fi

    if ! command -v make &> /dev/null; then
        missing_deps+=("make")
    fi

    if ! command -v gcov &> /dev/null && ! command -v llvm-cov &> /dev/null; then
        missing_deps+=("gcov/llvm-cov")
    fi

    if ! command -v lcov &> /dev/null; then
        print_warning "lcov not found - installing for better coverage reports..."
        if command -v apt-get &> /dev/null; then
            sudo apt-get update && sudo apt-get install -y lcov
        elif command -v brew &> /dev/null; then
            brew install lcov
        else
            print_warning "Please install lcov manually for HTML coverage reports"
        fi
    fi

    if [ ${#missing_deps[@]} -ne 0 ]; then
        print_error "Missing dependencies: ${missing_deps[*]}"
        echo "Please install them and try again."
        exit 1
    fi

    print_success "All dependencies satisfied"
}

# Clean previous builds
clean_build() {
    print_header "Cleaning Previous Builds"

    if [ -d "$BUILD_DIR" ]; then
        rm -rf "$BUILD_DIR"
        print_success "Removed previous build directory"
    fi

    mkdir -p "$BUILD_DIR"
    mkdir -p "$COVERAGE_DIR"
}

# Configure CMake with coverage flags
configure_cmake() {
    print_header "Configuring CMake with Coverage"

    cd "$BUILD_DIR"

    # Detect compiler
    if command -v clang++ &> /dev/null; then
        COMPILER="clang++"
        COVERAGE_FLAGS="-fprofile-instr-generate -fcoverage-mapping"
    else
        COMPILER="g++"
        COVERAGE_FLAGS="--coverage -fprofile-arcs -ftest-coverage"
    fi

    print_status "Using compiler: $COMPILER"

    # Configure with coverage flags
    cmake -DCMAKE_BUILD_TYPE=Debug \
          -DCMAKE_CXX_COMPILER="$COMPILER" \
          -DCMAKE_CXX_FLAGS="$COVERAGE_FLAGS -O0 -g" \
          -DCMAKE_EXE_LINKER_FLAGS="$COVERAGE_FLAGS" \
          -DENABLE_TESTING=ON \
          -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
          -S "$SOURCE_DIR" \
          -B . > cmake_config.log 2>&1

    if [ $? -eq 0 ]; then
        print_success "CMake configuration completed"
    else
        print_error "CMake configuration failed"
        cat cmake_config.log
        exit 1
    fi
}

# Build tests
build_tests() {
    print_header "Building Tests with Coverage"

    cd "$BUILD_DIR"

    make -j$(nproc 2>/dev/null || echo 4) > build.log 2>&1

    if [ $? -eq 0 ]; then
        print_success "Build completed successfully"
    else
        print_error "Build failed"
        cat build.log
        exit 1
    fi
}

# Run unit tests
run_unit_tests() {
    print_header "Running Unit Tests"

    cd "$BUILD_DIR"

    # Create test results directory
    mkdir -p "$COVERAGE_DIR/test-results"

    # Run all tests and capture output
    local test_executables=$(find . -name "*test*" -type f -executable 2>/dev/null || echo "")

    if [ -z "$test_executables" ]; then
        print_warning "No test executables found. Looking for test files..."
        test_executables=$(find . -name "*_test*" -type f -executable 2>/dev/null || echo "")
    fi

    if [ -z "$test_executables" ]; then
        print_warning "No test executables found. Creating test targets..."
        make test 2>/dev/null || make tests 2>/dev/null
        test_executables=$(find . -name "*test*" -type f -executable 2>/dev/null || echo "")
    fi

    local test_count=0
    local passed_count=0
    local failed_count=0

    for test_exe in $test_executables; do
        if [ -x "$test_exe" ]; then
            test_name=$(basename "$test_exe")
            print_status "Running test: $test_name"

            # Run test with coverage data capture
            if ./"$test_exe" --gtest_output=xml:"$COVERAGE_DIR/test-results/$test_name.xml" > "$COVERAGE_DIR/test-results/$test_name.log" 2>&1; then
                ((passed_count++))
                print_success "✓ $test_name passed"
            else
                ((failed_count++))
                print_error "✗ $test_name failed"
                cat "$COVERAGE_DIR/test-results/$test_name.log"
            fi
            ((test_count++))
        fi
    done

    # Also try make test
    if command -v ctest &> /dev/null; then
        print_status "Running CTest..."
        ctest --output-on-failure --test-dir . >> "$COVERAGE_DIR/test-results/ctest.log" 2>&1 || true
    fi

    print_header "Unit Test Summary"
    echo "Total tests: $test_count"
    echo "Passed: $passed_count"
    echo "Failed: $failed_count"

    if [ $failed_count -eq 0 ]; then
        print_success "All unit tests passed!"
    else
        print_warning "$failed_count tests failed"
    fi
}

# Generate coverage data
generate_coverage() {
    print_header "Generating Coverage Data"

    cd "$BUILD_DIR"

    # Initialize lcov with baseline
    lcov --capture --initial --directory . --output-file coverage_base.info > lcov.log 2>&1

    # Collect coverage data
    lcov --capture --directory . --output-file coverage.info >> lcov.log 2>&1

    # Combine baseline and coverage data
    lcov --add-tracefile coverage_base.info --add-tracefile coverage.info --output-file coverage_total.info >> lcov.log 2>&1

    # Remove system headers and test files from coverage
    lcov --remove coverage_total.info '*/usr/*' --remove coverage_total.info '*/tests/*' \
         --remove coverage_total.info '*/build-coverage/*' --remove coverage_total.info '*/vcpkg/*' \
         --output-file coverage_filtered.info >> lcov.log 2>&1

    # Generate summary
    print_status "Generating coverage summary..."
    lcov --summary coverage_filtered.info > "$COVERAGE_DIR/coverage_summary.txt" 2>&1

    if [ $? -eq 0 ]; then
        print_success "Coverage data generated"
    else
        print_warning "Coverage generation had issues - check lcov logs"
    fi
}

# Generate HTML coverage report
generate_html_report() {
    print_header "Generating HTML Coverage Report"

    cd "$COVERAGE_DIR"

    # Generate HTML report
    if command -v genhtml &> /dev/null; then
        genhtml coverage_filtered.info --output-directory html --title "Aimux v2.0.0 Coverage Report" \
               --legend --show-details --branch-coverage > genhtml.log 2>&1

        if [ $? -eq 0 ]; then
            print_success "HTML coverage report generated"
            print_status "Open: $COVERAGE_DIR/html/index.html"
        else
            print_warning "HTML report generation failed - check genhtml.log"
        fi
    else
        print_warning "genhtml not available, skipping HTML report"
    fi

    # Generate JSON report using our custom generator
    print_status "Generating custom coverage report..."
    cd "$PROJECT_ROOT"

    # Compile and run coverage report generator
    if [ -f "tests/coverage_report_generator.cpp" ]; then
        g++ -std=c++20 -o "$BUILD_DIR/coverage_report_generator" tests/coverage_report_generator.cpp \
            -I"$BUILD_DIR" -I"$PROJECT_ROOT/include" \
            $(find /usr -name "libjsoncpp.*" 2>/dev/null || echo "-ljsoncpp") 2>/dev/null || \
            g++ -std=c++20 -o "$BUILD_DIR/coverage_report_generator" tests/coverage_report_generator.cpp \
            -I"$BUILD_DIR" -I"$PROJECT_ROOT/include" -ljsoncpp 2>/dev/null || \
            g++ -std=c++20 -o "$BUILD_DIR/coverage_report_generator" tests/coverage_report_generator.cpp \
            -I"$BUILD_DIR" -I"$PROJECT_ROOT/include" 2>/dev/null

        if [ -x "$BUILD_DIR/coverage_report_generator" ]; then
            "$BUILD_DIR/coverage_report_generator" "$COVERAGE_DIR" > "$COVERAGE_DIR/custom_generator.log" 2>&1
            print_success "Custom coverage report generated"
        else
            print_warning "Custom coverage report generator compilation failed"
        fi
    fi
}

# Analyze coverage against thresholds
analyze_coverage() {
    print_header "Analyzing Coverage Against Thresholds"

    if [ ! -f "$COVERAGE_DIR/coverage_total.info" ]; then
        print_error "No coverage data found"
        return 1
    fi

    # Extract coverage percentage
    local coverage_output=$(lcov --summary "$COVERAGE_DIR/coverage_filtered.info" 2>&1)
    local coverage_percentage=$(echo "$coverage_output" | grep "lines......:" | awk '{print $2}' | sed 's/%//')

    if [ -z "$coverage_percentage" ]; then
        coverage_percentage=$(echo "$coverage_output" | grep "lines......:" | awk '{print $3}' | sed 's/%//')
    fi

    if [ -z "$coverage_percentage" ]; then
        print_error "Could not extract coverage percentage"
        echo "$coverage_output"
        return 1
    fi

    coverage_percentage=$(echo "$coverage_percentage" | xargs printf "%.1f")

    echo "Overall Coverage: ${coverage_percentage}%"

    # Check against thresholds
    if (( $(echo "$coverage_percentage >= $MIN_COVERAGE_CRITICAL" | bc -l) )); then
        print_success "CRITICAL MODULES: ✅ $coverage_percentage% >= $MIN_COVERAGE_CRITICAL% threshold"
    else
        print_error "CRITICAL MODULES: ❌ $coverage_percentage% < $MIN_COVERAGE_CRITICAL% threshold"
    fi

    if (( $(echo "$coverage_percentage >= $MIN_COVERAGE_OVERALL" | bc -l) )); then
        print_success "OVERALL TARGET: ✅ $coverage_percentage% >= $MIN_COVERAGE_OVERALL% threshold"
    else
        print_warning "OVERALL TARGET: ⚠️ $coverage_percentage% < $MIN_COVERAGE_OVERALL% threshold"
    fi

    # Store coverage percentage for use by other scripts
    echo "$coverage_percentage" > "$COVERAGE_DIR/coverage_percentage.txt"

    # Generate coverage badge (optional)
    if command -v convert &> /dev/null; then
        generate_coverage_badge "$coverage_percentage"
    fi
}

# Generate coverage badge (optional)
generate_coverage_badge() {
    local coverage="$1"
    local color="red"

    if (( $(echo "$coverage >= 90" | bc -l) )); then
        color="brightgreen"
    elif (( $(echo "$coverage >= 80" | bc -l) )); then
        color="yellow"
    elif (( $(echo "$coverage >= 70" | bc -l) )); then
        color="orange"
    fi

    # Simple SVG badge generation
    cat > "$COVERAGE_DIR/coverage_badge.svg" << EOF
<svg xmlns="http://www.w3.org/2000/svg" width="100" height="20">
    <rect width="100" height="20" fill="#555" rx="3"/>
    <rect x="60" width="40" height="20" fill="#${color}" rx="3"/>
    <text x="30" y="14" fill="white" font-family="Arial" font-size="11" text-anchor="middle">coverage</text>
    <text x="80" y="14" fill="white" font-family="Arial" font-size="11" text-anchor="middle">${coverage}%</text>
</svg>
EOF
}

# Generate final summary
generate_summary() {
    print_header "Test Coverage Summary"

    echo ""
    echo "📊 Coverage Report Generated Successfully!"
    echo ""
    echo "📍 Report Locations:"
    echo "  HTML Report: $COVERAGE_DIR/html/index.html"
    echo "  JSON Data:   $COVERAGE_DIR/coverage_data.json"
    echo "  Summary:     $COVERAGE_DIR/coverage_summary.md"
    echo "  Badge:       $COVERAGE_DIR/coverage_badge.svg"
    echo ""

    if [ -f "$COVERAGE_DIR/coverage_percentage.txt" ]; then
        local coverage=$(cat "$COVERAGE_DIR/coverage_percentage.txt")
        echo "🎯 Final Coverage: ${coverage}%"
    fi

    echo ""
    echo "📋 Next Steps:"
    echo "  1. Open the HTML report to see detailed coverage information"
    echo "  2. Focus on uncovered lines and branches"
    echo "  3. Add missing unit tests to reach >90% coverage"
    echo "  4. Re-run this script to verify improvements"
    echo ""
}

# Main execution
main() {
    print_header "Aimux v2.0.0 - Test Coverage Analysis"
    echo "Running comprehensive tests with coverage analysis..."
    echo ""

    check_dependencies
    clean_build
    configure_cmake
    build_tests
    run_unit_tests
    generate_coverage
    generate_html_report
    analyze_coverage
    generate_summary

    print_success "Coverage analysis completed!"

    # Return appropriate exit code based on coverage
    if [ -f "$COVERAGE_DIR/coverage_percentage.txt" ]; then
        local coverage=$(cat "$COVERAGE_DIR/coverage_percentage.txt")
        if (( $(echo "$coverage >= $MIN_COVERAGE_CRITICAL" | bc -l) )); then
            exit 0
        else
            exit 1
        fi
    else
        exit 1
    fi
}

# Handle script interruption gracefully
trap 'echo -e "\n${YELLOW}[INTERRUPTED]${NC} Cleaning up..."; exit 130' INT TERM

# Run main function
main "$@"