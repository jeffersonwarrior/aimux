cmake_minimum_required(VERSION 3.20)
project(Aimux VERSION 2.0.0 LANGUAGES CXX)

# C++ Standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Production build optimizations
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    # Optimize for performance
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native -mtune=native")

    # Strip symbols for smaller binary
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -s")

    # Enable LTO (Link Time Optimization)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

    # Static linking for easier deployment
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
    set(BUILD_SHARED_LIBS OFF)

    message(STATUS "Production build optimizations enabled")
endif()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

# Find required packages
find_package(PkgConfig REQUIRED)

# vcpkg toolchain
if(DEFINED CMAKE_TOOLCHAIN_FILE AND CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg")
    message(STATUS "Using vcpkg toolchain")
else()
    # Set vcpkg toolchain if not defined
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file")
    message(STATUS "Setting vcpkg toolchain to: ${CMAKE_TOOLCHAIN_FILE}")
endif()

# Use vcpkg packages with system fallback
set(nlohmann_json_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/packages/nlohmann-json_x64-linux/share/nlohmann_json")
find_package(nlohmann_json CONFIG REQUIRED)
# Force manual CURL configuration
set(CURL_FOUND TRUE)
set(CURL_INCLUDE_DIRS "/usr/include/curl")
set(CURL_LIBRARIES "/usr/lib/libcurl.so")
set(CURL_VERSION_STRING "8.17.0")
add_library(CURL::libcurl SHARED IMPORTED)
set_target_properties(CURL::libcurl PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${CURL_INCLUDE_DIRS}"
    IMPORTED_LOCATION "${CURL_LIBRARIES}")
# Force manual OpenSSL configuration
set(OPENSSL_FOUND TRUE)
set(OPENSSL_INCLUDE_DIR "/usr/include/openssl")
set(OPENSSL_SSL_LIBRARY "/usr/lib/libssl.so")
set(OPENSSL_CRYPTO_LIBRARY "/usr/lib/libcrypto.so")
set(OPENSSL_LIBRARIES "${OPENSSL_SSL_LIBRARY} ${OPENSSL_CRYPTO_LIBRARY}")
set(OPENSSL_INCLUDE_DIRS "${OPENSSL_INCLUDE_DIR}")
add_library(OpenSSL::SSL SHARED IMPORTED)
add_library(OpenSSL::Crypto SHARED IMPORTED)
set_target_properties(OpenSSL::SSL PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_INCLUDE_DIR}"
    IMPORTED_LOCATION "${OPENSSL_SSL_LIBRARY}")
set_target_properties(OpenSSL::Crypto PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_INCLUDE_DIR}"
    IMPORTED_LOCATION "${OPENSSL_CRYPTO_LIBRARY}")
find_package(Threads REQUIRED)

# Try vcpkg packages first, fallback to manual configuration
find_package(asio CONFIG QUIET)
if(NOT asio_FOUND)
    message(STATUS "Using header-only asio")
    set(asio_LIBRARIES "")
    set(asio_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/packages/asio_x64-linux/include")
    add_library(asio::asio INTERFACE IMPORTED)
    set_target_properties(asio::asio PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${asio_INCLUDE_DIRS}")
endif()

find_package(Crow CONFIG QUIET)
if(NOT Crow_FOUND)
    message(STATUS "Using header-only Crow with manual configuration")
    set(Crow_LIBRARIES "")
    set(Crow_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/packages/crow_x64-linux/include")
    set(asio_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/packages/asio_x64-linux/include")
    add_library(Crow::Crow INTERFACE IMPORTED)
    set_target_properties(Crow::Crow PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${Crow_INCLUDE_DIRS};${asio_INCLUDE_DIRS}")
endif()

# Create CURL target if config not found
if(CURL_FOUND AND NOT TARGET CURL::libcurl)
    add_library(CURL::libcurl UNKNOWN IMPORTED)
    set_target_properties(CURL::libcurl PROPERTIES
        IMPORTED_LOCATION "${CURL_LIBRARIES}"
        INTERFACE_INCLUDE_DIRECTORIES "${CURL_INCLUDE_DIRS}"
    )
endif()

# Simple HTTP server without crow for now
# TODO: Add proper HTTP server later

# Source files
set(CORE_SOURCES
    src/core/router.cpp
    src/core/failover.cpp
    src/core/bridge.cpp
    src/core/thread_manager.cpp
    src/core/error_handler.cpp
)
set(LOGGING_SOURCES
    src/logging/logger.cpp
    src/logging/production_logger.cpp
    src/logging/correlation_context.cpp
)
set(PROVIDER_SOURCES
    src/providers/provider_impl.cpp
)
set(DAEMON_SOURCES 
    src/daemon/daemon.cpp
)
# WebUI sources with embedded resources and advanced streaming
set(WEBUI_SOURCES
    src/webui/web_server.cpp
    src/webui/metrics_streamer.cpp
    src/webui/resource_loader.cpp
)
set(NETWORK_SOURCES
    src/network/http_client.cpp
    src/network/connection_pool.cpp
    src/network/ssl_config.cpp
)
set(CLI_SOURCES
    src/cli/migrate.cpp
)
set(SECURITY_SOURCES
    src/security/secure_config.cpp
)
set(VALIDATION_SOURCES
    src/validation/input_validator.cpp
)
set(MONITORING_SOURCES
    src/monitoring/performance_monitor.cpp
    src/monitoring/enhanced_performance_endpoints.cpp
)
set(CONFIG_SOURCES
    src/config/simple_config.cpp  # Simple fallback implementation
    src/config/production_config.cpp  # Production configuration implementation
    src/config/startup_validator.cpp  # Startup validation implementation
    src/config/toon_parser.cpp  # TOON format parser implementation
)

# Gateway sources for unified API gateway
set(GATEWAY_SOURCES
    src/gateway/format_detector.cpp
    src/gateway/api_transformer.cpp
    src/gateway/gateway_manager.cpp
    src/gateway/routing_logic.cpp
    src/gateway/provider_health.cpp
    src/gateway/claude_gateway.cpp
)

file(GLOB_RECURSE UTILS_SOURCES "src/utils/*.cpp")

# Create main executable
add_executable(aimux
    src/main.cpp
    ${CORE_SOURCES}
    ${PROVIDER_SOURCES}
    ${DAEMON_SOURCES}
    ${WEBUI_SOURCES}
    ${NETWORK_SOURCES}
    ${LOGGING_SOURCES}
    ${CLI_SOURCES}
    ${SECURITY_SOURCES}
    ${VALIDATION_SOURCES}
    ${MONITORING_SOURCES}
    ${CONFIG_SOURCES}
    ${GATEWAY_SOURCES}
    ${UTILS_SOURCES}
)

# vcpkg Crow includes are handled automatically by find_package(Crow CONFIG REQUIRED)

# Link libraries
target_link_libraries(aimux
    nlohmann_json::nlohmann_json
    asio::asio
    Crow::Crow
    CURL::libcurl
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
)

# Compiler-specific options with strict warning levels and warnings as errors
target_compile_options(aimux PRIVATE
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra -Werror -Wpedantic -Wcast-align -Wcast-qual -Wformat=2 -Wmissing-include-dirs -Wnon-virtual-dtor -Wold-style-cast -Woverloaded-virtual -Wredundant-decls -Wshadow -Wsign-conversion -Wunused>
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Werror -Wpedantic -Wcast-align -Wcast-qual -Wformat=2 -Wmissing-include-dirs -Wnon-virtual-dtor -Wold-style-cast -Woverloaded-virtual -Wredundant-decls -Wshadow -Wsign-conversion -Wunused>
)

# Create provider integration test executable
add_executable(provider_integration_tests
    provider_integration_tests.cpp
    ${PROVIDER_SOURCES}
    ${NETWORK_SOURCES}
    ${LOGGING_SOURCES}
    ${UTILS_SOURCES}
)

target_link_libraries(provider_integration_tests
    nlohmann_json::nlohmann_json
    asio::asio
    Crow::Crow
    CURL::libcurl
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
)

target_compile_options(provider_integration_tests PRIVATE
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra>
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra>
)

# Create memory performance test executable
add_executable(memory_performance_test
    memory_performance_test.cpp
    ${PROVIDER_SOURCES}
    ${NETWORK_SOURCES}
    ${LOGGING_SOURCES}
    ${UTILS_SOURCES}
)

target_link_libraries(memory_performance_test
    nlohmann_json::nlohmann_json
    Crow::Crow
    CURL::libcurl
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
)

target_compile_options(memory_performance_test PRIVATE
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra>
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra>
)

# Create dashboard test executable
add_executable(test_dashboard
    test_dashboard.cpp
    src/webui/resource_loader.cpp
)

target_link_libraries(test_dashboard
    nlohmann_json::nlohmann_json
    Crow::Crow
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
)

target_compile_options(test_dashboard PRIVATE
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra>
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra>
)

# Create format detection test executable
add_executable(format_detection_tests
    tests/format_detection_tests.cpp
    ${GATEWAY_SOURCES}
    ${LOGGING_SOURCES}
    ${UTILS_SOURCES}
)

target_link_libraries(format_detection_tests
    nlohmann_json::nlohmann_json
    Crow::Crow
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
)

target_compile_options(format_detection_tests PRIVATE
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra>
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra>
)

# Create API transformation test executable
add_executable(api_transformation_tests
    tests/api_transformation_tests.cpp
    ${GATEWAY_SOURCES}
    ${LOGGING_SOURCES}
    ${UTILS_SOURCES}
)

target_link_libraries(api_transformation_tests
    nlohmann_json::nlohmann_json
    Crow::Crow
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
)

target_compile_options(api_transformation_tests PRIVATE
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra>
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra>
)

# Create ClaudeGateway V3.2 Service executable
add_executable(claude_gateway
    src/claude_gateway_main.cpp
    ${CORE_SOURCES}
    ${PROVIDER_SOURCES}
    ${NETWORK_SOURCES}
    ${LOGGING_SOURCES}
    ${GATEWAY_SOURCES}
    ${CONFIG_SOURCES}
    ${SECURITY_SOURCES}
    ${UTILS_SOURCES}
)

target_link_libraries(claude_gateway
    nlohmann_json::nlohmann_json
    Crow::Crow
    CURL::libcurl
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
)

target_compile_options(claude_gateway PRIVATE
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra>
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra>
)

# Set installation target for ClaudeGateway
install(TARGETS claude_gateway
    RUNTIME DESTINATION bin
)

# Create ClaudeGateway test executable
add_executable(test_claude_gateway
    test_claude_gateway.cpp
    ${GATEWAY_SOURCES}
    ${LOGGING_SOURCES}
    ${NETWORK_SOURCES}
    ${CONFIG_SOURCES}
    ${UTILS_SOURCES}
)

target_link_libraries(test_claude_gateway
    nlohmann_json::nlohmann_json
    Crow::Crow
    CURL::libcurl
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
)

target_compile_options(test_claude_gateway PRIVATE
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra>
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra>
)

# Create gateway integration test executable
add_executable(gateway_integration_tests
    tests/gateway_integration_tests.cpp
    ${GATEWAY_SOURCES}
    ${LOGGING_SOURCES}
    ${UTILS_SOURCES}
)

target_link_libraries(gateway_integration_tests
    nlohmann_json::nlohmann_json
    Crow::Crow
    CURL::libcurl
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
)

target_compile_options(gateway_integration_tests PRIVATE
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra>
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra>
)

# Create provider compatibility test executable
add_executable(provider_compatibility_tests
    tests/provider_compatibility_tests.cpp
    ${GATEWAY_SOURCES}
    ${LOGGING_SOURCES}
    ${UTILS_SOURCES}
)

target_link_libraries(provider_compatibility_tests
    nlohmann_json::nlohmann_json
    Crow::Crow
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
)

target_compile_options(provider_compatibility_tests PRIVATE
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra>
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra>
)

# Create validation example executable
add_executable(validation_example
    examples/validation_example.cpp
    ${VALIDATION_SOURCES}
    ${MONITORING_SOURCES}
    ${LOGGING_SOURCES}
    ${UTILS_SOURCES}
)

target_link_libraries(validation_example
    nlohmann_json::nlohmann_json
    Crow::Crow
    CURL::libcurl
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
)

target_compile_options(validation_example PRIVATE
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra>
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra>
)

# Add Google Test for advanced testing framework
find_package(GTest REQUIRED)

# Advanced Testing Framework
set(TESTING_SOURCES
    tests/test_runner.cpp
    tests/integration/test_router_provider_integration.cpp
    tests/performance/test_performance_regression.cpp
)

# Create advanced test runner
add_executable(advanced_test_runner
    ${TESTING_SOURCES}
    ${CORE_SOURCES}
    ${PROVIDER_SOURCES}
    ${LOGGING_SOURCES}
    ${GATEWAY_SOURCES}
    ${NETWORK_SOURCES}
    ${CONFIG_SOURCES}
    ${UTILS_SOURCES}
)

target_link_libraries(advanced_test_runner
    nlohmann_json::nlohmann_json
    Crow::Crow
    CURL::libcurl
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
    GTest::gtest
    GTest::gtest_main
    GTest::gmock
    GTest::gmock_main
)

target_include_directories(advanced_test_runner PRIVATE
    include
    ${GTEST_INCLUDE_DIRS}
)

target_compile_options(advanced_test_runner PRIVATE
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra -Wno-unused-parameter>
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wno-unused-parameter>
)

# Create unit tests
add_executable(unit_tests
    tests/unit/test_production_logger.cpp
    tests/unit/test_router_comprehensive.cpp
    ${CORE_SOURCES}
    ${LOGGING_SOURCES}
    ${UTILS_SOURCES}
)

target_link_libraries(unit_tests
    nlohmann_json::nlohmann_json
    Crow::Crow
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
    GTest::gtest
    GTest::gtest_main
    GTest::gmock
    GTest::gmock_main
)

target_include_directories(unit_tests PRIVATE
    include
    ${GTEST_INCLUDE_DIRS}
)

target_compile_options(unit_tests PRIVATE
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra -Wno-unused-parameter>
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wno-unused-parameter>
)

# Create integration tests
add_executable(integration_tests
    tests/integration/test_providers_comprehensive.cpp
    tests/integration/test_http_client_comprehensive.cpp
    ${GATEWAY_SOURCES}
    ${NETWORK_SOURCES}
    ${LOGGING_SOURCES}
    ${UTILS_SOURCES}
)

target_link_libraries(integration_tests
    nlohmann_json::nlohmann_json
    Crow::Crow
    CURL::libcurl
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
    GTest::gtest
    GTest::gtest_main
    GTest::gmock
    GTest::gmock_main
)

target_include_directories(integration_tests PRIVATE
    include
    ${GTEST_INCLUDE_DIRS}
)

target_compile_options(integration_tests PRIVATE
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra -Wno-unused-parameter>
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wno-unused-parameter>
)

# Test targets for easy execution
add_custom_target(test_unit
    COMMAND unit_tests
    DEPENDS unit_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running unit tests"
)

add_custom_target(test_integration
    COMMAND integration_tests
    DEPENDS integration_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running integration tests"
)

add_custom_target(test_advanced
    COMMAND advanced_test_runner --mode=all --format=json --output=advanced_test_results
    DEPENDS advanced_test_runner
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running advanced test suite"
)

add_custom_target(test_performance
    COMMAND advanced_test_runner --mode=performance --format=json --output=performance_test_results --baseline
    DEPENDS advanced_test_runner
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running performance regression tests"
)

add_custom_target(test_all
    COMMAND ${CMAKE_COMMAND} --build . --target test_unit
    COMMAND ${CMAKE_COMMAND} --build . --target test_integration
    COMMAND ${CMAKE_COMMAND} --build . --target test_advanced
    DEPENDS unit_tests integration_tests advanced_test_runner
    COMMENT "Running all test suites"
)

# Installation
install(TARGETS aimux DESTINATION bin)
install(FILES version.txt DESTINATION share/aimux)

# Version management
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/version.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/version.h"
)
include_directories("${CMAKE_CURRENT_BINARY_DIR}/include")

# Generate build info
find_package(Git)
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --always --dirty
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    execute_process(
        COMMAND ${GIT_EXECUTABLE} log -1 --format=%cd --date=iso
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_DATE
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
else()
    set(GIT_VERSION "${PROJECT_VERSION}")
    set(GIT_COMMIT "unknown")
    set(GIT_DATE "unknown")
endif()

# Print configuration
message(STATUS "")
message(STATUS "Aimux ${PROJECT_VERSION} Configuration:")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Compiler: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "  Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  Git Version: ${GIT_VERSION}")
message(STATUS "  Git Commit: ${GIT_COMMIT}")
message(STATUS "  Build Date: ${GIT_DATE}")
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "  Production Build: YES")
endif()
message(STATUS "")