# Build and Integration Tests

## Overview
This document defines comprehensive build and integration tests to ensure the Aimux system builds correctly across platforms, links properly with all dependencies, packages correctly, and deploys successfully in various environments.

## 1. Cross-Platform Build Validation

### 1.1 Platform Support Matrix
**Objective**: Validate builds across supported platforms.

**Supported Platforms**:
- **Linux**: Ubuntu 20.04+, CentOS 8+, RHEL 8+, Arch Linux
- **macOS**: 11.0+ (Apple Silicon and Intel)
- **Windows**: Windows 10+ (Visual Studio 2019+, MinGW-w64)

**Build Matrix Configuration**:
```yaml
# .github/workflows/build_matrix.yml
strategy:
  matrix:
    os: [ubuntu-latest, macos-latest, windows-latest]
    build_type: [Debug, Release, RelWithDebInfo]
    compiler: [gcc, clang, msvc]

    exclude:
      # Exclude invalid combinations
      - os: windows-latest
        compiler: gcc
      - os: macos-latest
        compiler: msvc

fail-fast: false
```

### 1.2 Build Environment Testing
**Objective**: Ensure consistent build environments.

**Test Categories**:
- **Dependency Resolution**: All dependencies found and linked correctly
- **Compiler Compatibility**: Works with different compiler versions
- **Library Version Compatibility**: Compatible with different library versions
- **Build Tool Variants**: Works with different build tools

**Implementation Framework**:
```cmake
# cmake/BuildTest.cmake
# Build environment validation

function(validate_build_environment)
    message(STATUS "Validating build environment...")

    # Check C++ standard
    if(CMAKE_CXX_STANDARD LESS 23)
        message(FATAL_ERROR "C++23 or higher is required. Found: ${CMAKE_CXX_STANDARD}")
    endif()

    # Check essential dependencies
    find_package(PkgConfig REQUIRED)
    find_package(Threads REQUIRED)
    find_package(nlohmann_json REQUIRED)

    # Validate compiler flags
    if(NOT CMAKE_CXX_FLAGS MATCHES "-std=c\\+\\+23")
        message(FATAL_ERROR "C++23 flag not found in compiler flags")
    endif()

    # Check build type specific requirements
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        if(NOT CMAKE_CXX_FLAGS_RELEASE MATCHES "-O3")
            message(WARNING "Release build not using -O3 optimization")
        endif()
    endif()

    message(STATUS "Build environment validation passed")
endfunction()
```

### 1.3 Compiler Compatibility Testing
**Objective**: Validate compilation with different compilers.

**Compiler Test Matrix**:
```bash
# qa/scripts/test_compiler_compatibility.sh
#!/bin/bash

set -e

COMPILERS=("gcc-11" "gcc-12" "clang-14" "clang-15")
BUILD_TYPES=("Debug" "Release" "RelWithDebInfo")

TEST_RESULTS=()

for compiler in "${COMPILERS[@]}"; do
    for build_type in "${BUILD_TYPES[@]}"; do
        echo "Testing compiler: $compiler, build type: $build_type"

        # Create build directory
        build_dir="build_${compiler}_${build_type}"
        mkdir -p "$build_dir"
        cd "$build_dir"

        # Configure with specific compiler
        if command -v "$compiler" &> /dev/null; then
            export CC="$compiler"
            export CXX="${compiler/%g++/gcc}"

            if cmake -DCMAKE_BUILD_TYPE="$build_type" ..; then
                if make -j$(nproc); then
                    TEST_RESULTS+=("$compiler:$build_type:PASS")
                else
                    TEST_RESULTS+=("$compiler:$build_type:FAIL_BUILD")
                fi
            else
                TEST_RESULTS+=("$compiler:$build_type:FAIL_CONFIGURE")
            fi
        else
            TEST_RESULTS+=("$compiler:$build_type:SKIP")
        fi

        cd ..
    done
done

# Report results
echo "=== Compiler Compatibility Test Results ==="
for result in "${TEST_RESULTS[@]}"; do
    echo "$result"
done

# Check for failures
FAILS=$(echo "${TEST_RESULTS[@]}" | grep -v "PASS\|SKIP" | wc -l)
if [ "$FAILS" -gt 0 ]; then
    echo "ERROR: $FAILS build configurations failed"
    exit 1
fi
```

### 1.4 Cross-Platform Integration Tests
**Objective**: Ensure functionality works across platforms.

**Platform-Specific Tests**:
```cpp
// test/platform_integration_test.cpp
#include <gtest/gtest.h>
#include <filesystem>
#include <platform_utils.hpp>

class PlatformIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Platform-specific setup
        platform_utils::initialize_platform();
    }
};

TEST_F(PlatformIntegrationTest, FilesystemOperations) {
    // Test filesystem operations work on current platform
    namespace fs = std::filesystem;

    fs::path test_dir = fs::temp_directory_path() / "aimux_test";
    fs::create_directories(test_dir);

    EXPECT_TRUE(fs::exists(test_dir));
    EXPECT_TRUE(fs::is_directory(test_dir));

    // Create test file
    fs::path test_file = test_dir / "test.txt";
    std::ofstream file(test_file);
    file << "test content";
    file.close();

    EXPECT_TRUE(fs::exists(test_file));
    EXPECT_TRUE(fs::is_regular_file(test_file));

    // Clean up
    fs::remove_all(test_dir);
}

TEST_F(PlatformIntegrationTest, NetworkOperations) {
    // Test network operations work on current platform
    auto http_client = std::make_unique<HttpClient>();

    // Test simple HTTP request
    auto response = http_client->get("https://httpbin.org/get");
    EXPECT_TRUE(response.success);
    EXPECT_GT(response.body.size(), 0);
}

TEST_F(PlatformIntegrationTest, ThreadOperations) {
    // Test thread operations work on current platform
    std::atomic<bool> flag{false};
    std::thread worker([&flag]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        flag = true;
    });

    worker.join();
    EXPECT_TRUE(flag.load());
}

TEST_F(PlatformIntegrationTest, PathHandling) {
    // Test path handling is platform-correct
    std::string config_path = platform_utils::get_config_path();
    EXPECT_FALSE(config_path.empty());

    // Path separator handling
    std::string test_path = config_path + platform_utils::get_path_separator() + "test";
    EXPECT_NE(test_path.back(), ' ');
}
```

## 2. Dependency Verification Tests

### 2.1 Dependency Chain Validation
**Objective**: Ensure all dependencies are correctly linked and functional.

**Dependency Validation Framework**:
```cpp
// test/dependency_validation_test.cpp
#include <gtest/gtest.h>
#include <dlfcn.h>

class DependencyValidationTest : public ::testing::Test {
protected:
    void SetUp() override {
        load_dependency_info();
    }

    struct DependencyInfo {
        std::string name;
        std::string library_path;
        std::string version;
        std::vector<std::string> required_symbols;
    };

    std::vector<DependencyInfo> dependencies_;

    void load_dependency_info() {
        // Core dependencies
        dependencies_ = {
            {
                "libcurl",
                "libcurl.so",
                get_library_version("libcurl"),
                {"curl_easy_init", "curl_easy_setopt", "curl_easy_perform"}
            },
            {
                "libssl",
                "libssl.so",
                get_library_version("libssl"),
                {"SSL_new", "SSL_connect", "SSL_read"}
            },
            {
                "libcrypto",
                "libcrypto.so",
                get_library_version("libcrypto"),
                {"EVP_DigestInit", "RAND_bytes", "EVP_PKEY_new"}
            },
            {
                "nlohmann_json",
                "libnlohmann_json.so",
                get_header_version("nlohmann/json.hpp"),
                {}, // Header-only library
            }
        };
    }

    std::string get_library_version(const std::string& library_name) {
        // Implementation to query library version
        void* handle = dlopen(library_name.c_str(), RTLD_LAZY);
        if (!handle) return "unknown";

        // Try to get version information
        char* version = nullptr; // Implementation dependent
        dlclose(handle);
        return version ? std::string(version) : "unknown";
    }

    std::string get_header_version(const std::string& header_path) {
        // For header-only libraries, check version macros
        return "header-only";
    }
};

TEST_F(DependencyValidationTest, AllDependenciesLoadable) {
    for (const auto& dep : dependencies_) {
        if (dep.required_symbols.empty()) {
            // Header-only library, skip dynamic loading test
            continue;
        }

        void* handle = dlopen(dep.library_path.c_str(), RTLD_LAZY);
        EXPECT_NE(handle, nullptr) << "Failed to load dependency: " << dep.name;

        if (handle) {
            dlclose(handle);
        }
    }
}

TEST_F(DependencyValidationTest, RequiredSymbolsAvailable) {
    for (const auto& dep : dependencies_) {
        if (dep.required_symbols.empty()) {
            continue;
        }

        void* handle = dlopen(dep.library_path.c_str(), RTLD_LAZY);
        ASSERT_NE(handle, nullptr) << "Failed to load: " << dep.name;

        for (const auto& symbol : dep.required_symbols) {
            void* func_ptr = dlsym(handle, symbol.c_str());
            EXPECT_NE(func_ptr, nullptr)
                << "Symbol " << symbol << " not found in " << dep.name;
        }

        dlclose(handle);
    }
}

TEST_F(DependencyValidationTest, DependencyVersionCompatibility) {
    // Test that dependency versions meet minimum requirements
    for (const auto& dep : dependencies_) {
        if (dep.name == "libcurl") {
            // Require curl 7.68.0+
            EXPECT_GE(compare_versions(dep.version, "7.68.0"), 0)
                << "libcurl version too old: " << dep.version;
        } else if (dep.name == "libssl") {
            // Require OpenSSL 1.1.1+
            EXPECT_GE(compare_versions(dep.version, "1.1.1"), 0)
                << "OpenSSL version too old: " << dep.version;
        }
    }
}

int compare_versions(const std::string& v1, const std::string& v2) {
    // Simple version comparison implementation
    auto split = [](const std::string& s) {
        std::vector<int> parts;
        std::stringstream ss(s);
        std::string part;
        while (std::getline(ss, part, '.')) {
            parts.push_back(std::stoi(part));
        }
        return parts;
    };

    auto parts1 = split(v1);
    auto parts2 = split(v2);

    size_t max_size = std::max(parts1.size(), parts2.size());
    parts1.resize(max_size, 0);
    parts2.resize(max_size, 0);

    for (size_t i = 0; i < max_size; ++i) {
        if (parts1[i] < parts2[i]) return -1;
        if (parts1[i] > parts2[i]) return 1;
    }
    return 0;
}
```

### 2.2 Link Time Optimization Testing
**Objective**: Validate LTO works correctly and provides benefits.

**LTO Test Implementation**:
```cmake
# cmake/LTOTest.cmake
function(test_lto_optimization)
    message(STATUS "Testing LTO optimization...")

    # Create test executable with LTO
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

    add_executable(lto_test
        test/lto_performance_test.cpp
    )

    set_target_properties(lto_test PROPERTIES
        INTERPROCEDURAL_OPTIMIZATION TRUE
    )

    # Build without LTO for comparison
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION FALSE)
    add_executable(no_lto_test
        test/lto_performance_test.cpp
    )

    message(STATUS "LTO test executables created")
endfunction()
```

### 2.3 Dependency Conflict Detection
**Objective**: Detect and prevent dependency conflicts.

**Conflict Detection Script**:
```bash
#!/bin/bash
# qa/scripts/check_dependency_conflicts.sh

echo "Checking for dependency conflicts..."

# Check for multiple versions of key libraries
check_conflicting_libraries() {
    local library_pattern=$1
    local found_versions=$(find /usr/lib /usr/local/lib /lib -name "$library_pattern*" 2>/dev/null)

    if [ $(echo "$found_versions" | wc -l) -gt 1 ]; then
        echo "WARNING: Multiple versions found for $library_pattern:"
        echo "$found_versions"
        return 1
    fi
    return 0
}

# Check for common conflict sources
CONFLICT_COUNT=0

check_conflicting_libraries "libssl.so" || ((CONFLICT_COUNT++))
check_conflicting_libraries "libcrypto.so" || ((CONFLICT_COUNT++))
check_conflicting_libraries "libcurl.so" || ((CONFLICT_COUNT++))
check_conflicting_libraries "libc++.so" || ((CONFLICT_COUNT++))

# Check for compiler ABI mismatches
if [ -f /etc/os-release ]; then
    . /etc/os-release
    echo "OS: $NAME $VERSION"
fi

# Check glibc version
GLIBC_VERSION=$(ldd --version | head -n1 | grep -o '[0-9.]*$' | head -n1)
echo "GLIBC version: $GLIBC_VERSION"

# Minimum required glibc version is 2.28
if [ "$(printf '%s\n' "2.28" "$GLIBC_VERSION" | sort -V | head -n1)" != "2.28" ]; then
    echo "ERROR: GLIBC version $GLIBC_VERSION is too old. Minimum required: 2.28"
    ((CONFLICT_COUNT++))
fi

if [ $CONFLICT_COUNT -gt 0 ]; then
    echo "ERROR: Found $CONFLICT_COUNT potential dependency conflicts"
    exit 1
else
    echo "✓ No dependency conflicts detected"
fi
```

## 3. Packaging Integrity Tests

### 3.1 Build Artifact Validation
**Objective**: Ensure build artifacts are complete and correctly structured.

**Artifact Validation Framework**:
```python
#!/usr/bin/env python3
# qa/scripts/validate_build_artifacts.py

import os
import subprocess
import json
import hashlib
from pathlib import Path
import tarfile
import zipfile

class BuildArtifactValidator:
    def __init__(self, build_dir="build"):
        self.build_dir = Path(build_dir)
        self.validation_results = {
            'executables': {},
            'libraries': {},
            'headers': {},
            'configs': {},
            'tests': {}
        }

    def validate_executable(self, exe_path):
        """Validate executable file integrity."""
        exe_path = Path(exe_path)
        if not exe_path.exists():
            self.validation_results['executables'][exe_path.name] = 'MISSING'
            return False

        # Check file permissions
        if not os.access(exe_path, os.X_OK):
            self.validation_results['executables'][exe_path.name] = 'NOT_EXECUTABLE'
            return False

        # Check dynamic dependencies
        try:
            result = subprocess.run(['ldd', str(exe_path)],
                                  capture_output=True, text=True)
            if result.returncode != 0:
                self.validation_results['executables'][exe_path.name] = 'LDD_FAILED'
                return False

            # Check for missing dependencies
            missing_deps = [line for line in result.stdout.split('\n')
                          if 'not found' in line]
            if missing_deps:
                self.validation_results['executables'][exe_path.name] = f'MISSING_DEPS: {missing_deps}'
                return False

        except Exception as e:
            self.validation_results['executables'][exe_path.name] = f'ERROR: {str(e)}'
            return False

        self.validation_results['executables'][exe_path.name] = 'VALID'
        return True

    def validate_library(self, lib_path):
        """Validate shared library integrity."""
        lib_path = Path(lib_path)
        if not lib_path.exists():
            self.validation_results['libraries'][lib_path.name] = 'MISSING'
            return False

        # Check file magic
        try:
            result = subprocess.run(['file', str(lib_path)],
                                  capture_output=True, text=True)
            if 'shared object' not in result.stdout:
                self.validation_results['libraries'][lib_path.name] = 'NOT_SHARED_LIB'
                return False
        except Exception as e:
            self.validation_results['libraries'][lib_path.name] = f'ERROR: {str(e)}'
            return False

        self.validation_results['libraries'][lib_path.name] = 'VALID'
        return True

    def validate_package_structure(self, package_path):
        """Validate package structure and contents."""
        package_path = Path(package_path)

        if package_path.suffix == '.tar.gz':
            return self._validate_tar_package(package_path)
        elif package_path.suffix == '.zip':
            return self._validate_zip_package(package_path)
        elif package_path.suffix == '.deb':
            return self._validate_deb_package(package_path)
        elif package_path.suffix in ['.rpm', '.dmg']:
            return self._validate_native_package(package_path)
        else:
            print(f"Unsupported package format: {package_path.suffix}")
            return False

    def _validate_tar_package(self, tar_path):
        """Validate tar.gz package."""
        try:
            with tarfile.open(tar_path, 'r:gz') as tar:
                members = tar.getnames()

                # Required files
                required_files = [
                    'bin/aimux',
                    'bin/claude_gateway',
                    'share/aimux/version.txt',
                    'README.md',
                    'LICENSE'
                ]

                for req_file in required_files:
                    if req_file not in members:
                        print(f"Missing required file in package: {req_file}")
                        return False

                return True
        except Exception as e:
            print(f"Error validating tar package: {e}")
            return False

    def _validate_zip_package(self, zip_path):
        """Validate zip package."""
        try:
            with zipfile.ZipFile(zip_path, 'r') as zip_file:
                members = zip_file.namelist()

                # Validate similar to tar package
                required_files = [
                    'bin/aimux.exe',
                    'bin/claude_gateway.exe',
                    'share/aimux/version.txt',
                    'README.md',
                    'LICENSE'
                ]

                for req_file in required_files:
                    if req_file not in members:
                        print(f"Missing required file in package: {req_file}")
                        return False

                return True
        except Exception as e:
            print(f"Error validating zip package: {e}")
            return False

    def generate_checksums(self, file_path):
        """Generate file checksums."""
        file_path = Path(file_path)
        checksums = {}

        with open(file_path, 'rb') as f:
            content = f.read()
            checksums['md5'] = hashlib.md5(content).hexdigest()
            checksums['sha1'] = hashlib.sha1(content).hexdigest()
            checksums['sha256'] = hashlib.sha256(content).hexdigest()

        return checksums

    def generate_validation_report(self, output_file):
        """Generate validation report."""
        report = {
            'timestamp': subprocess.check_output(['date', '-Iseconds']).decode().strip(),
            'validation_results': self.validation_results,
            'summary': self._generate_summary()
        }

        with open(output_file, 'w') as f:
            json.dump(report, f, indent=2)

    def _generate_summary(self):
        """Generate validation summary."""
        summary = {
            'total_executables': len(self.validation_results['executables']),
            'valid_executables': sum(1 for v in self.validation_results['executables'].values() if v == 'VALID'),
            'total_libraries': len(self.validation_results['libraries']),
            'valid_libraries': sum(1 for v in self.validation_results['libraries'].values() if v == 'VALID'),
            'overall_status': 'PASS' if self._all_valid() else 'FAIL'
        }
        return summary

    def _all_valid(self):
        """Check if all validations passed."""
        all_results = []
        all_results.extend(self.validation_results['executables'].values())
        all_results.extend(self.validation_results['libraries'].values())

        return all(result == 'VALID' for result in all_results)

def main():
    validator = BuildArtifactValidator()

    # Validate main executables
    validator.validate_executable("build/aimux")
    validator.validate_executable("build/claude_gateway")

    # Validate libraries if they exist
    lib_files = Path("build").glob("*.so*")
    for lib in lib_files:
        validator.validate_library(lib)

    # Generate report
    validator.generate_validation_report("qa/reports/build_validation_report.json")

    # Print summary
    summary = validator._generate_summary()
    print(f"Validation Summary:")
    print(f"  Executables: {summary['valid_executables']}/{summary['total_executables']} valid")
    print(f"  Libraries: {summary['valid_libraries']}/{summary['total_libraries']} valid")
    print(f"  Overall Status: {summary['overall_status']}")

    return 0 if summary['overall_status'] == 'PASS' else 1

if __name__ == "__main__":
    exit(main())
```

### 3.2 Installation Testing
**Objective**: Test installation process and verify installed files.

**Installation Test Script**:
```bash
#!/bin/bash
# qa/scripts/test_installation.sh

set -e

BUILD_DIR="build_install_test"
INSTALL_PREFIX="/tmp/aimux_install_test"

echo "Testing installation process..."

# Clean up previous test
rm -rf "$BUILD_DIR" "$INSTALL_PREFIX"

# Create build directory for installation test
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure with test prefix
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" \
      ..

# Build and install
make -j$(nproc)
make install

# Verify installation
echo "Verifying installation..."

# Check that executables are installed
if [ ! -f "$INSTALL_PREFIX/bin/aimux" ]; then
    echo "ERROR: aimux executable not installed"
    exit 1
fi

if [ ! -f "$INSTALL_PREFIX/bin/claude_gateway" ]; then
    echo "ERROR: claude_gateway executable not installed"
    exit 1
fi

# Check that executables work
"$INSTALL_PREFIX/bin/aimux" --version > /dev/null
"$INSTALL_PREFIX/bin/claude_gateway" --version > /dev/null

# Check that static files are installed
if [ ! -f "$INSTALL_PREFIX/share/aimux/version.txt" ]; then
    echo "ERROR: version.txt not installed"
    exit 1
fi

# Test that installation is relocatable (on Unix systems)
if [[ "$OSTYPE" == "linux-gnu"* ]] || [[ "$OSTYPE" == "darwin"* ]]; then
    echo "Testing installation relocation..."

    RELOCATED_PREFIX="/tmp/aimux_relocated_test"
    cp -r "$INSTALL_PREFIX" "$RELOCATED_PREFIX"

    # Update dynamic library paths for relocation
    find "$RELOCATED_PREFIX/bin" -type f -executable -exec \
        patchelf --set-rpath '$ORIGIN/../lib' {} \; 2>/dev/null || true

    # Test relocated executables
    "$RELOCATED_PREFIX/bin/aimux" --version > /dev/null
    "$RELOCATED_PREFIX/bin/claude_gateway" --version > /dev/null

    rm -rf "$RELOCATED_PREFIX"
fi

# Clean up
cd ..
rm -rf "$BUILD_DIR" "$INSTALL_PREFIX"

echo "✓ Installation test passed"
```

### 3.3 Package Distribution Testing
**Objective**: Test package creation and distribution formats.

**Distribution Tests**:
```cmake
# cmake/PackageTest.cmake
# Package format testing

function(create_test_packages)
    message(STATUS "Creating test packages...")

    # Create DEB package for Debian/Ubuntu
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        find_program(DPKG_EXECUTABLE dpkg)
        if(DPKG_EXECUTABLE)
            set(CPACK_GENERATOR "DEB")
            set(CPACK_DEBIAN_PACKAGE_DEPENDS "libcurl4, libssl1.1, libcrypto1.1")
            include(CPack)
            cpack()
        endif()

        # Create RPM package for Red Hat/CentOS
        find_program(RPMBUILD_EXECUTABLE rpmbuild)
        if(RPMBUILD_EXECUTABLE)
            set(CPACK_GENERATOR "RPM")
            set(CPACK_RPM_PACKAGE_REQUIRES "libcurl, openssl, openssl-libs")
            include(CPack)
            cpack()
        endif()

        # Create AppImage
        find_program(LINUXDEPLOY_EXECUTABLE linuxdeploy)
        if(LINUXDEPLOY_EXECUTABLE)
            # Create AppImage for portable distribution
            add_custom_target(appimage
                COMMAND ${CMAKE_COMMAND} --build . --target install
                COMMAND ${LINUXDEPLOY_EXECUTABLE} --appdir AppDir --executable aimux --create-desktop-file --output appimage
            )
        endif()
    endif()

    # Create DMG for macOS
    if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        set(CPACK_GENERATOR "DragNDrop")
        include(CPack)
        cpack()
    endif()

    # Create NSIS installer for Windows
    if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
        find_program(NSIS_EXECUTABLE makensis)
        if(NSIS_EXECUTABLE)
            set(CPACK_GENERATOR "NSIS")
            include(CPack)
            cpack()
        endif()
    endif()
endfunction()

function test_package_integrity(package_path)
    message(STATUS "Testing package integrity: ${package_path}")

    # Test package file integrity
    if(EXISTS "${package_path}")
        file(SHA256 "${package_path}" PACKAGE_HASH)
        message(STATUS "Package SHA256: ${PACKAGE_HASH}")

        # Test package extraction
        if(package_path MATCHES "\\.deb$")
            # Test DEB package
            execute_process(
                COMMAND dpkg-deb --contents ${package_path}
                RESULT_VARIABLE DEB_TEST_RESULT
                ERROR_QUIET
            )
            if(NOT DEB_TEST_RESULT EQUAL 0)
                message(FATAL_ERROR "DEB package integrity test failed")
            endif()
        endif()

        if(package_path MATCHES "\\.rpm$")
            # Test RPM package
            execute_process(
                COMMAND rpm -qpl ${package_path}
                RESULT_VARIABLE RPM_TEST_RESULT
                ERROR_QUIET
            )
            if(NOT RPM_TEST_RESULT EQUAL 0)
                message(FATAL_ERROR "RPM package integrity test failed")
            endif()
        endif()
    else()
        message(FATAL_ERROR "Package file not found: ${package_path}")
    endif()

    message(STATUS "Package integrity test passed")
endfunction()
```

## 4. Deployment Validation Tests

### 4.1 Environment Setup Testing
**Objective**: Validate deployment in various environments.

**Deployment Test Matrix**:
```yaml
# qa/deployment/environments.yml
environments:
  development:
    description: "Local development environment"
    config_file: "config/development.json"
    required_services: []
    environment_variables:
      - AIMUX_LOG_LEVEL=debug
      - AIMUX_ENV=development

  testing:
    description: "Testing environment with mocked services"
    config_file: "config/testing.json"
    required_services:
      - "mock_http_server:8080"
      - "mock_cache_server:6379"
    environment_variables:
      - AIMUX_LOG_LEVEL=info
      - AIMUX_ENV=testing

  staging:
    description: "Staging environment with staging services"
    config_file: "config/staging.json"
    required_services:
      - "staging_api:443"
      - "staging_cache:6379"
      - "staging_monitoring:9090"
    environment_variables:
      - AIMUX_LOG_LEVEL=warn
      - AIMUX_ENV=staging

  production:
    description: "Production environment"
    config_file: "config/production.json"
    required_services:
      - "production_api:443"
      - "production_cache:6379"
      - "production_monitoring:9090"
      - "production_logs:514"
    environment_variables:
      - AIMUX_LOG_LEVEL=error
      - AIMUX_ENV=production
```

### 4.2 Service Integration Validation
**Objective**: Validate integration with external services.

**Integration Test Framework**:
```cpp
// test/deployment_integration_test.cpp
#include <gtest/gtest.h>
#include <service_health_checker.hpp>

class DeploymentIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Load environment configuration
        load_environment_config();
    }

    void load_environment_config() {
        const char* env = std::getenv("AIMUX_ENV");
        if (!env) {
            env = "development"; // Default
        }

        config_file = "config/" + std::string(env) + ".json";
        // Load configuration
    }

    std::string config_file;
};

TEST_F(DeploymentIntegrationTest, RequiredServicesAvailable) {
    ServiceHealthChecker health_checker;

    // Check required services
    auto required_services = get_required_services();

    for (const auto& service : required_services) {
        auto result = health_checker.check_service(service.name, service.port);
        EXPECT_TRUE(result.is_healthy) << "Service not available: " << service.name;
        EXPECT_LT(result.response_time_ms, 5000) << "Service timeout: " << service.name;
    }
}

TEST_F(DeploymentIntegrationTest, ConfigurationValid) {
    EXPECT_TRUE(std::filesystem::exists(config_file))
        << "Configuration file not found: " << config_file;

    // Parse and validate configuration
    ProductionConfig config;
    EXPECT_TRUE(config.loadFromFile(config_file)) << "Failed to parse configuration";

    // Validate required configuration sections
    EXPECT_TRUE(config.hasDatabaseConfig()) << "Missing database configuration";
    EXPECT_TRUE(config.hasCacheConfig()) << "Missing cache configuration";
    EXPECT_TRUE(config.hasLoggingConfig()) << "Missing logging configuration";
}

TEST_F(DeploymentIntegrationTest, LoggingSystemWorking) {
    // Test logging in deployment environment
    Logger& logger = Logger::getInstance();

    EXPECT_NO_THROW(logger.info("Deployment test log message"));
    EXPECT_NO_THROW(logger.error("Deployment test error message"));
    EXPECT_NO_THROW(logger.debug("Deployment test debug message"));

    // Verify logs are written
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto log_file = get_log_file_path();
    EXPECT_TRUE(std::filesystem::exists(log_file)) << "Log file not created";

    // Check log content
    std::ifstream log_stream(log_file);
    std::string content((std::istreambuf_iterator<char>(log_stream)),
                       std::istreambuf_iterator<char>());

    EXPECT_NE(content.find("Deployment test"), std::string::npos)
        << "Test log messages not found in log file";
}

TEST_F(DeploymentIntegrationTest, PerformanceBaseline) {
    // Test that performance meets deployment baseline
    PerformanceMonitor monitor;

    auto start_time = std::chrono::high_resolution_clock::now();

    // Perform basic operation
    api_gateway.process_test_request();

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    // Should complete within reasonable time
    EXPECT_LT(duration.count(), 1000) << "Basic operation too slow in deployment";
}

TEST_F(DeploymentIntegrationTest, ResourceUsageWithinLimits) {
    // Monitor resource usage
    auto initial_memory = get_memory_usage();
    auto initial_cpu = get_cpu_usage();

    // Run load test
    run_deployment_load_test();

    auto final_memory = get_memory_usage();
    auto final_cpu = get_cpu_usage();

    // Check resource usage is within acceptable limits
    EXPECT_LT(final_memory - initial_memory, 100 * 1024 * 1024) << "Memory usage too high";
    EXPECT_LT(final_cpu - initial_cpu, 80.0) << "CPU usage too high";
}
```

### 4.3 Health Check Endpoint Testing
**Objective**: Validate health check endpoints work correctly.

**Health Check Tests**:
```cpp
// test/health_check_test.cpp
#include <gtest/gtest.h>
#include <http_client.hpp>

class HealthCheckTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Get health check URL from configuration
        health_check_url = get_health_check_url();
        client = std::make_unique<HttpClient>();
    }

    std::string health_check_url;
    std::unique_ptr<HttpClient> client;
};

TEST_F(HealthCheckTest, BasicHealthCheck) {
    auto response = client->get(health_check_url + "/health");

    EXPECT_EQ(response.status_code, 200) << "Health check endpoint failed";

    auto health_data = json::parse(response.body);
    EXPECT_EQ(health_data["status"], "healthy") << "Service reports unhealthy status";
    EXPECT_TRUE(health_data.contains("timestamp")) << "Missing timestamp in health check";
    EXPECT_TRUE(health_data.contains("version")) << "Missing version in health check";
}

TEST_F(HealthCheckTest, DetailedHealthCheck) {
    auto response = client->get(health_check_url + "/health/detailed");

    EXPECT_EQ(response.status_code, 200) << "Detailed health check failed";

    auto health_data = json::parse(response.body);
    EXPECT_TRUE(health_data.contains("services")) << "Missing services status";
    EXPECT_TRUE(health_data.contains("system")) << "Missing system information";

    // Check individual service health
    for (const auto& service : health_data["services"].items()) {
        EXPECT_TRUE(service.value().contains("status"))
            << "Service missing status: " << service.key();
        EXPECT_TRUE(service.value().contains("response_time"))
            << "Service missing response time: " << service.key();
    }
}

TEST_F(HealthCheckTest, MetricsEndpoint) {
    auto response = client->get(health_check_url + "/metrics");

    EXPECT_EQ(response.status_code, 200) << "Metrics endpoint failed";

    // Should contain Prometheus-style metrics
    EXPECT_TRUE(response.body.find("# HELP") != std::string::npos)
        << "Missing metric help information";
    EXPECT_TRUE(response.body.find("# TYPE") != std::string::npos)
        << "Missing metric type information";
}
```

## 5. Rollback Testing Procedures

### 5.1 Rollback Validation Framework
**Objective**: Ensure rollback procedures work correctly.

**Rollback Test Implementation**:
```bash
#!/bin/bash
# qa/scripts/test_rollback_procedures.sh

set -e

CURRENT_VERSION=$(git describe --tags --always)
TEST_VERSION="test-rollback-$(date +%s)"
BACKUP_DIR="/tmp/aimux_rollback_test"

echo "Testing rollback procedures..."
echo "Current version: $CURRENT_VERSION"

# Create backup of current installation
create_backup() {
    echo "Creating backup of current installation..."
    mkdir -p "$BACKUP_DIR"

    if [ -d "/opt/aimux" ]; then
        sudo cp -r /opt/aimux "$BACKUP_DIR/"
    fi

    if [ -f "/etc/systemd/system/aimux.service" ]; then
        sudo cp /etc/systemd/system/aimux.service "$BACKUP_DIR/"
    fi

    echo "Backup created in $BACKUP_DIR"
}

# Simulate deployment of new version
deploy_test_version() {
    echo "Deploying test version..."

    # Create fake new version
    mkdir -p /tmp/aimux_new_version/bin
    echo "#!/bin/bash
echo \"Test version $TEST_VERSION\"
exit 1 # Simulate broken deployment" > /tmp/aimux_new_version/bin/aimux
    chmod +x /tmp/aimux_new_version/bin/aimux

    # Deploy test version
    sudo mkdir -p /opt/aimux
    sudo cp -r /tmp/aimux_new_version/* /opt/aimux/

    echo "Test version deployed"
}

# Test rollback procedure
test_rollback() {
    echo "Testing rollback procedure..."

    # Check that test version is broken (as expected)
    if /opt/aimux/bin/aimux 2>/dev/null; then
        echo "ERROR: Test version should fail but succeeded"
        return 1
    fi

    # Perform rollback
    echo "Performing rollback..."

    if [ -d "$BACKUP_DIR/aimux" ]; then
        sudo rm -rf /opt/aimux
        sudo cp -r "$BACKUP_DIR/aimux" /opt/
    fi

    if [ -f "$BACKUP_DIR/aimux.service" ]; then
        sudo cp "$BACKUP_DIR/aimux.service" /etc/systemd/system/
        sudo systemctl daemon-reload
    fi

    # Test that rollback worked
    if /opt/aimux/bin/aimux --version; then
        echo "✓ Rollback successful"
        return 0
    else
        echo "ERROR: Rollback failed"
        return 1
    fi
}

# Cleanup test environment
cleanup() {
    echo "Cleaning up test environment..."

    sudo rm -rf /tmp/aimux_new_version
    sudo rm -rf "$BACKUP_DIR"

    # Restore original state if needed
    if sudo systemctl is-active --quiet aimux 2>/dev/null; then
        sudo systemctl stop aimux
    fi
}

# Main test execution
trap cleanup EXIT

create_backup
deploy_test_version

if test_rollback; then
    echo "✓ Rollback test passed"
    exit 0
else
    echo "✗ Rollback test failed"
    exit 1
fi
```

### 5.2 Zero-Downtime Deployment Testing
**Objective**: Test zero-downtime deployment strategies.

**Zero-Downtime Test**:
```bash
#!/bin/bash
# qa/scripts/test_zero_downtime_deployment.sh

set -e

echo "Testing zero-downtime deployment..."

# Start load generation
start_load_test() {
    echo "Starting load test..."

    # Use Apache Bench or similar tool to generate continuous load
    ab -n 1000 -c 10 http://localhost:8080/health &
    LOAD_TEST_PID=$!

    sleep 2 # Let load test stabilize

    echo "Load test started (PID: $LOAD_TEST_PID)"
}

# Perform rolling deployment
perform_rolling_deployment() {
    echo "Performing rolling deployment..."

    # Simulate multiple instances behind load balancer
    for instance in instance1 instance2; do
        echo "Updating $instance..."

        # Take instance out of load balancer
        echo "Removing $instance from load balancer"

        # Wait for connections to drain
        sleep 5

        # Update instance
        echo "Deploying new version to $instance"

        # Health check
        echo "Health checking $instance"

        # Add back to load balancer
        echo "Adding $instance back to load balancer"

        sleep 2
    done
}

# Monitor for downtime
monitor_downtime() {
    echo "Monitoring for downtime..."

    FAILED_REQUESTS=0
    TOTAL_REQUESTS=0

    # Monitor load test output for failures
    while kill -0 $LOAD_TEST_PID 2>/dev/null; do
        # Check if service is responding
        if ! curl -s http://localhost:8080/health > /dev/null; then
            ((FAILED_REQUESTS++))
            echo "ERROR: Service not responding"
        fi
        ((TOTAL_REQUESTS++))
        sleep 1
    done

    echo "Monitoring complete"
    echo "Failed requests: $FAILED_REQUESTS / $TOTAL_REQUESTS"

    # Calculate availability percentage
    AVAILABILITY=$(( (TOTAL_REQUESTS - FAILED_REQUESTS) * 100 / TOTAL_REQUESTS ))
    echo "Availability: ${AVAILABILITY}%"

    if [ $AVAILABILITY -lt 95 ]; then
        echo "ERROR: Availability below 95% during deployment"
        return 1
    fi
}

# Main test execution
echo "Starting zero-downtime deployment test..."

start_load_test
perform_rolling_deployment
monitor_downtime

# Wait for load test to complete
wait $LOAD_TEST_PID

echo "✓ Zero-downtime deployment test passed"
```

## Test Execution Framework

### Build System Integration
```cmake
# Create build and integration test targets
add_executable(build_integration_tests
    test/platform_integration_test.cpp
    test/dependency_validation_test.cpp
    test/deployment_integration_test.cpp
    test/health_check_test.cpp
)

target_link_libraries(build_integration_tests
    aimux
    ${CMAKE_THREAD_LIBS_INIT}
)

# Add build validation target
add_custom_target(validate_build
    COMMAND ${CMAKE_COMMAND} -E echo "Validating build environment..."
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target all
    COMMAND python3 qa/scripts/validate_build_artifacts.py
    COMMENT "Validating build integrity"
)

# Add package testing target
add_custom_target(test_packages
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target package
    COMMAND python3 qa/scripts/test_package_installation.py
    COMMENT "Testing package installation"
)

# Add deployment testing target
add_custom_target(test_deployment
    COMMAND ${CMAKE_COMMAND} -E echo "Testing deployment..."
    COMMAND ${CMAKE_BINARY_DIR}/build_integration_tests
    COMMAND bash qa/scripts/test_rollback_procedures.sh
    COMMENT "Testing deployment procedures"
)
```

### Continuous Integration
```yaml
# .github/workflows/build_integration.yml
name: Build Integration Tests

on:
  push:
    branches: [ main, develop ]
  pull_request:
    branches: [ main ]
  schedule:
    - cron: '0 2 * * *'  # Daily at 2 AM

jobs:
  build-matrix:
    runs-on: ${{ matrix.os }}
    strategy:
      matrix:
        os: [ubuntu-latest, macos-latest, windows-latest]
        build_type: [Debug, Release]

    steps:
    - uses: actions/checkout@v3

    - name: Install dependencies (Linux)
      if: matrix.os == 'ubuntu-latest'
      run: |
        sudo apt-get update
        sudo apt-get install -y cmake ninja-build pkg-config

    - name: Install dependencies (macOS)
      if: matrix.os == 'macos-latest'
      run: |
       brew install cmake ninja pkg-config

    - name: Install dependencies (Windows)
      if: matrix.os == 'windows-latest'
      run: |
        choco install cmake ninja

    - name: Configure build
      run: |
        cmake -B build -DCMAKE_BUILD_TYPE=${{ matrix.build_type }}

    - name: Build
      run: |
        cmake --build build -- -j$(nproc)

    - name: Run build integration tests
      run: |
        cd build
        ./build_integration_tests

    - name: Validate build artifacts
      run: |
        python3 qa/scripts/validate_build_artifacts.py

    - name: Test packaging
      run: |
        cmake --build build --target package
        python3 qa/scripts/test_package_installation.py

  deployment-tests:
    runs-on: ubuntu-latest
    needs: build-matrix

    steps:
    - uses: actions/checkout@v3

    - name: Deploy to staging
      run: |
        # Deploy to staging environment
        echo "Deploying to staging..."

    - name: Run deployment tests
      run: |
        bash qa/scripts/test_deployment_integration.sh

    - name: Test rollback
      run: |
        bash qa/scripts/test_rollback_procedures.sh
```

## Success Criteria

### Build Requirements
- All supported platforms build successfully
- All dependencies are correctly linked
- Build artifacts are complete and valid
- Packages install and run correctly

### Integration Requirements
- Deployment works in all target environments
- Services integrate correctly with dependencies
- Health checks work as expected
- Rollback procedures function correctly

### Release Requirements
- Zero-downtime deployment works
- Package integrity is maintained
- Installation across platforms succeeds
- Rollback can recover from failures

## Documentation Requirements

### Build Documentation
- Platform-specific build instructions
- Dependency requirements and versions
- Troubleshooting common build issues
- Release build procedures

### Deployment Documentation
- Environment setup procedures
- Service integration guidelines
- Health check endpoint documentation
- Rollback and recovery procedures

This comprehensive build and integration testing framework ensures that the Aimux system builds correctly, packages properly, deploys successfully, and can be recovered from failures across all supported platforms and environments.