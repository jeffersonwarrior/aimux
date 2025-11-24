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

# Try to find nlohmann_json - prefer system package over vcpkg
find_package(nlohmann_json CONFIG QUIET)
if(NOT nlohmann_json_FOUND)
    # Try vcpkg if system package not found
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake")
        set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake"
            CACHE STRING "Vcpkg toolchain file")
        set(nlohmann_json_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/packages/nlohmann-json_x64-linux/share/nlohmann_json")
        find_package(nlohmann_json CONFIG QUIET)
    endif()

    # Fallback to header-only if still not found
    if(NOT nlohmann_json_FOUND)
        message(STATUS "Using system nlohmann-json header-only")
        add_library(nlohmann_json::nlohmann_json INTERFACE IMPORTED)
        set_target_properties(nlohmann_json::nlohmann_json PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "/usr/include")
    endif()
endif()
# Force manual CURL configuration
set(CURL_FOUND TRUE)
set(CURL_INCLUDE_DIRS "/usr/include")
set(CURL_LIBRARIES "/usr/lib/libcurl.so")
set(CURL_VERSION_STRING "8.5.0")
add_library(CURL::libcurl SHARED IMPORTED)
set_target_properties(CURL::libcurl PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${CURL_INCLUDE_DIRS}"
    IMPORTED_LOCATION "${CURL_LIBRARIES}")
# Force manual OpenSSL configuration
set(OPENSSL_FOUND TRUE)
set(OPENSSL_INCLUDE_DIR "/usr/include")
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

# Try vcpkg packages first, fallback to system or manual configuration
find_package(asio CONFIG QUIET)
if(NOT asio_FOUND)
    # Check if system asio exists
    if(EXISTS "/usr/include/asio.hpp")
        message(STATUS "Using system asio from /usr/include")
        set(asio_INCLUDE_DIRS "/usr/include")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/packages/asio_x64-linux/include")
        message(STATUS "Using vcpkg asio")
        set(asio_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/packages/asio_x64-linux/include")
    else()
        message(FATAL_ERROR "asio not found in system or vcpkg")
    endif()
    add_library(asio::asio INTERFACE IMPORTED)
    set_target_properties(asio::asio PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${asio_INCLUDE_DIRS}")
endif()

find_package(Crow CONFIG QUIET)
if(NOT Crow_FOUND)
    # Check vcpkg first, then download if needed
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/packages/crow_x64-linux/include")
        message(STATUS "Using vcpkg Crow")
        set(Crow_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/packages/crow_x64-linux/include")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/include/crow.h")
        message(STATUS "Using local Crow from include/")
        set(Crow_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/include")
    else()
        message(STATUS "Crow not found - will download if needed or skip web features")
        set(Crow_INCLUDE_DIRS "")
    endif()

    if(Crow_INCLUDE_DIRS)
        add_library(Crow::Crow INTERFACE IMPORTED)
        set_target_properties(Crow::Crow PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${Crow_INCLUDE_DIRS};${asio_INCLUDE_DIRS}")
    endif()
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
    src/core/model_registry.cpp
    src/core/api_initializer.cpp
)

# Prettifier sources
set(PRETTIFIER_SOURCES
    src/prettifier/plugin_registry.cpp
    src/prettifier/prettifier_plugin.cpp
    src/prettifier/toon_formatter.cpp
    src/prettifier/cerebras_formatter.cpp
    src/prettifier/openai_formatter.cpp
    src/prettifier/anthropic_formatter.cpp
    src/prettifier/synthetic_formatter.cpp
    src/prettifier/streaming_processor.cpp
    src/prettifier/markdown_normalizer.cpp
    src/prettifier/tool_call_extractor.cpp
)

# Distribution sources
set(DISTRIBUTION_SOURCES
    src/distribution/github_registry.cpp
    src/distribution/plugin_downloader.cpp
    src/distribution/version_resolver.cpp
)
set(LOGGING_SOURCES
        src/logging/correlation_context.cpp
        src/logging/logger.cpp
)
set(PROVIDER_SOURCES
    src/providers/provider_impl.cpp
    src/providers/anthropic_model_query.cpp
    src/providers/openai_model_query.cpp
    src/providers/cerebras_model_query.cpp
)
set(DAEMON_SOURCES 
    src/daemon/daemon.cpp
)
# WebUI sources with embedded resources and advanced streaming
set(WEBUI_SOURCES
    src/webui/web_server.cpp
    src/webui/metrics_streamer.cpp
    src/webui/resource_loader.cpp
    src/webui/prettifier_api.cpp
    src/webui/config_validator.cpp
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

# Phase 4 Advanced Features Sources

set(METRICS_SOURCES
    src/metrics/metrics_collector.cpp
    src/metrics/time_series_db.cpp
    src/metrics/performance_monitor.cpp
)

set(AB_TESTING_SOURCES
    src/ab_testing/ab_testing_framework.cpp
)

set(ML_OPTIMIZATION_SOURCES
    src/ml_optimization/ml_optimizer_test_minimal.cpp
)
set(CONFIG_SOURCES
        src/config/production_config.cpp  # Production configuration implementation
    src/config/startup_validator.cpp  # Startup validation implementation
    src/config/toon_parser.cpp  # TOON format parser implementation
    src/config/global_config.cpp  # Global configuration variables
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

# TODO: Add utils when available
# file(GLOB_RECURSE UTILS_SOURCES "src/utils/*.cpp")

# Create main executable
add_executable(aimux
    src/main.cpp
    ${CORE_SOURCES}
    ${PRETTIFIER_SOURCES}
    ${PROVIDER_SOURCES}
    ${DAEMON_SOURCES}
    ${WEBUI_SOURCES}
    ${NETWORK_SOURCES}
    ${LOGGING_SOURCES}
    ${CLI_SOURCES}
    ${SECURITY_SOURCES}
    ${VALIDATION_SOURCES}
    ${MONITORING_SOURCES}
    ${METRICS_SOURCES}
    ${AB_TESTING_SOURCES}
    ${ML_OPTIMIZATION_SOURCES}
    ${CONFIG_SOURCES}
    ${GATEWAY_SOURCES}
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

# Compiler-specific options - C++23 ready, less pedantic for now
target_compile_options(aimux PRIVATE
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra -Wunused>
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wunused>
)

# Create provider integration test executable
# TODO: Fix missing source file
# add_executable(provider_integration_tests
#     provider_integration_tests.cpp
#     ${PROVIDER_SOURCES}
#     ${NETWORK_SOURCES}
#     ${LOGGING_SOURCES}
# )

# target_link_libraries(provider_integration_tests
#     nlohmann_json::nlohmann_json
#     asio::asio
#     Crow::Crow
#     CURL::libcurl
#     OpenSSL::SSL
#     OpenSSL::Crypto
#     Threads::Threads
# )

# target_compile_options(provider_integration_tests PRIVATE
#     $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra>
#     $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra>
# )

# Create memory performance test executable
# TODO: Fix missing source file
# add_executable(memory_performance_test
#     memory_performance_test.cpp
#     ${PROVIDER_SOURCES}
#     ${NETWORK_SOURCES}
#     ${LOGGING_SOURCES}
# )

# target_link_libraries(memory_performance_test
#     nlohmann_json::nlohmann_json
#     Crow::Crow
#     CURL::libcurl
#     OpenSSL::SSL
#     OpenSSL::Crypto
#     Threads::Threads
# )

# target_compile_options(memory_performance_test PRIVATE
#     $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra>
#     $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra>
# )

# Create dashboard test executable
# TODO: Fix missing source file
# add_executable(test_dashboard
#     test_dashboard.cpp
#     src/webui/resource_loader.cpp
# )

# target_link_libraries(test_dashboard
#     nlohmann_json::nlohmann_json
#     Crow::Crow
#     OpenSSL::SSL
#     OpenSSL::Crypto
#     Threads::Threads
# )

# target_compile_options(test_dashboard PRIVATE
#     $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra>
#     $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra>
# )

# Create format detection test executable
add_executable(format_detection_tests
    tests/format_detection_tests.cpp
    ${GATEWAY_SOURCES}
    ${LOGGING_SOURCES}
)

# Create GitHub registry test executable
add_executable(github_registry_test
    test/github_registry_test.cpp
    src/distribution/github_registry.cpp
)

target_link_libraries(github_registry_test
    nlohmann_json::nlohmann_json
    GTest::gtest
    GTest::gmock
    Threads::Threads
)

# Create Plugin downloader test executable
add_executable(plugin_downloader_test
    test/plugin_downloader_test.cpp
    src/distribution/github_registry.cpp
    src/distribution/plugin_downloader.cpp
)

target_link_libraries(plugin_downloader_test
    nlohmann_json::nlohmann_json
    GTest::gtest
    GTest::gmock
    Threads::Threads
    CURL::libcurl
    OpenSSL::SSL
    OpenSSL::Crypto
)

target_include_directories(github_registry_test PRIVATE
    ${GTEST_INCLUDE_DIRS}
    ${GMOCK_INCLUDE_DIRS}
)

target_include_directories(plugin_downloader_test PRIVATE
    ${GTEST_INCLUDE_DIRS}
    ${GMOCK_INCLUDE_DIRS}
)

target_compile_options(github_registry_test PRIVATE
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra>
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra>
)

target_compile_options(plugin_downloader_test PRIVATE
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra>
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra>
)

# Create Version resolver test executable
add_executable(version_resolver_test
    test/version_resolver_test.cpp
    src/distribution/version_resolver.cpp
)

target_link_libraries(version_resolver_test
    nlohmann_json::nlohmann_json
    GTest::gtest
    GTest::gmock
    Threads::Threads
)

target_include_directories(version_resolver_test PRIVATE
    ${GTEST_INCLUDE_DIRS}
    ${GMOCK_INCLUDE_DIRS}
)

target_compile_options(version_resolver_test PRIVATE
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra>
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra>
)

# Create Phase 3 integration test executable
add_executable(phase3_integration_test
    test/phase3_integration_test.cpp
    src/distribution/github_registry.cpp
    src/distribution/plugin_downloader.cpp
    src/distribution/version_resolver.cpp
)

target_link_libraries(phase3_integration_test
    nlohmann_json::nlohmann_json
    GTest::gtest
    GTest::gmock
    Threads::Threads
    CURL::libcurl
    OpenSSL::SSL
    OpenSSL::Crypto
)

target_include_directories(phase3_integration_test PRIVATE
    ${GTEST_INCLUDE_DIRS}
    ${GMOCK_INCLUDE_DIRS}
)

target_compile_options(phase3_integration_test PRIVATE
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra>
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra>
)

# Create Security test executable
add_executable(security_test
    test/security_test.cpp
    src/distribution/github_registry.cpp
    src/distribution/plugin_downloader.cpp
    src/distribution/version_resolver.cpp
)

target_link_libraries(security_test
    nlohmann_json::nlohmann_json
    GTest::gtest
    GTest::gmock
    Threads::Threads
    CURL::libcurl
    OpenSSL::SSL
    OpenSSL::Crypto
)

target_include_directories(security_test PRIVATE
    ${GTEST_INCLUDE_DIRS}
    ${GMOCK_INCLUDE_DIRS}
)

target_compile_options(security_test PRIVATE
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra>
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra>
)

# CLI Tests
add_executable(cli_test
    test/cli_test.cpp
    src/cli/plugin_cli.cpp
    src/cli/cli_utils.cpp
    src/cli/command_dispatcher.cpp
    src/cli/interactive_installation.cpp
    src/cli/batch_operations.cpp
    src/distribution/github_registry.cpp
    src/distribution/plugin_downloader.cpp
    src/distribution/version_resolver.cpp
)

target_link_libraries(cli_test
    aimux_core
    aimux_prettifier
    aimux_distribution
    nlohmann_json::nlohmann_json
    GTest::gtest
    GTest::gtest_main
    GTest::gmock
    Threads::Threads
    CURL::libcurl
    OpenSSL::SSL
    OpenSSL::Crypto
)

target_include_directories(cli_test PRIVATE
    ${GTEST_INCLUDE_DIRS}
    ${GMOCK_INCLUDE_DIRS}
)

target_compile_options(cli_test PRIVATE
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra>
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra>
)

add_executable(cli_security_test
    test/cli_security_test.cpp
    src/cli/plugin_cli.cpp
    src/cli/cli_utils.cpp
    src/cli/command_dispatcher.cpp
    src/cli/interactive_installation.cpp
    src/cli/batch_operations.cpp
    src/distribution/github_registry.cpp
    src/distribution/plugin_downloader.cpp
    src/distribution/version_resolver.cpp
)

target_link_libraries(cli_security_test
    aimux_core
    aimux_prettifier
    aimux_distribution
    nlohmann_json::nlohmann_json
    GTest::gtest
    GTest::gtest_main
    GTest::gmock
    Threads::Threads
    CURL::libcurl
    OpenSSL::SSL
    OpenSSL::Crypto
)

target_include_directories(cli_security_test PRIVATE
    ${GTEST_INCLUDE_DIRS}
    ${GMOCK_INCLUDE_DIRS}
)

target_compile_options(cli_security_test PRIVATE
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra>
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra>
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
# TODO: Fix missing source file
# add_executable(test_claude_gateway
#     test_claude_gateway.cpp
#     ${GATEWAY_SOURCES}
#     ${LOGGING_SOURCES}
#     ${NETWORK_SOURCES}
#     ${CONFIG_SOURCES}
# )

# target_link_libraries(test_claude_gateway
#     nlohmann_json::nlohmann_json
#     Crow::Crow
#     CURL::libcurl
#     OpenSSL::SSL
#     OpenSSL::Crypto
#     Threads::Threads
# )

# target_compile_options(test_claude_gateway PRIVATE
#     $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra>
#     $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra>
# )

# Create gateway integration test executable
add_executable(gateway_integration_tests
    tests/gateway_integration_tests.cpp
    ${GATEWAY_SOURCES}
    ${LOGGING_SOURCES}
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

# Create Phase 4 metrics collection tests
add_executable(metrics_collector_tests
    test/metrics_collector_test.cpp
    ${METRICS_SOURCES}
)

target_link_libraries(metrics_collector_tests
    nlohmann_json::nlohmann_json
    asio::asio
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
    GTest::gtest
    GTest::gtest_main
    GTest::gmock
    GTest::gmock_main
)

target_include_directories(metrics_collector_tests PRIVATE
    include
    ${GTEST_INCLUDE_DIRS}
    ${GMOCK_INCLUDE_DIRS}
)

target_compile_options(metrics_collector_tests PRIVATE
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra -Wno-unused-parameter>
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wno-unused-parameter>
)

# Create Phase 4 A/B testing framework tests
add_executable(ab_testing_framework_tests
    test/ab_testing_framework_test.cpp
    ${METRICS_SOURCES}
    ${AB_TESTING_SOURCES}
)

target_link_libraries(ab_testing_framework_tests
    nlohmann_json::nlohmann_json
    asio::asio
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
    GTest::gtest
    GTest::gtest_main
    GTest::gmock
    GTest::gmock_main
)

target_include_directories(ab_testing_framework_tests PRIVATE
    include
    ${GTEST_INCLUDE_DIRS}
    ${GMOCK_INCLUDE_DIRS}
)

target_compile_options(ab_testing_framework_tests PRIVATE
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra -Wno-unused-parameter>
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wno-unused-parameter>
)

# Create prettifier plugin tests
add_executable(prettifier_plugin_tests
    test/prettifier_plugin_test.cpp
    test/toon_formatter_test.cpp
    test/cerebras_formatter_test.cpp
    test/openai_formatter_test.cpp
    test/anthropic_formatter_test.cpp
    test/synthetic_formatter_test.cpp
    test/streaming_processor_test.cpp
    test/phase2_integration_test.cpp
    src/prettifier/plugin_registry.cpp
    src/prettifier/prettifier_plugin.cpp
    src/prettifier/toon_formatter.cpp
    src/prettifier/cerebras_formatter.cpp
    src/prettifier/openai_formatter.cpp
    src/prettifier/anthropic_formatter.cpp
    src/prettifier/synthetic_formatter.cpp
    src/prettifier/streaming_processor.cpp
)

target_link_libraries(prettifier_plugin_tests
    nlohmann_json::nlohmann_json
    asio::asio
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
    GTest::gtest
    GTest::gtest_main
    GTest::gmock
    GTest::gmock_main
)

target_include_directories(prettifier_plugin_tests PRIVATE
    include
    ${GTEST_INCLUDE_DIRS}
)

target_compile_options(prettifier_plugin_tests PRIVATE
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra -Wno-unused-parameter>
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wno-unused-parameter>
)

# Create streaming processor synchronization tests
add_executable(streaming_processor_sync_test
    test/streaming_processor_sync_test.cpp
    src/prettifier/streaming_processor.cpp
    src/prettifier/prettifier_plugin.cpp
    src/prettifier/anthropic_formatter.cpp
)

target_link_libraries(streaming_processor_sync_test
    nlohmann_json::nlohmann_json
    asio::asio
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
    GTest::gtest
    GTest::gtest_main
    GTest::gmock
    GTest::gmock_main
)

target_include_directories(streaming_processor_sync_test PRIVATE
    include
    ${GTEST_INCLUDE_DIRS}
)

target_compile_options(streaming_processor_sync_test PRIVATE
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra -Wno-unused-parameter>
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wno-unused-parameter>
)

# Create prettifier config tests
add_executable(prettifier_config_tests
    test/prettifier_config_test.cpp
    src/config/production_config.cpp
    src/config/startup_validator.cpp
    src/security/secure_config.cpp
    src/prettifier/toon_formatter.cpp
    )

target_link_libraries(prettifier_config_tests
    nlohmann_json::nlohmann_json
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
    GTest::gtest
    GTest::gtest_main
    GTest::gmock
    GTest::gmock_main
)

target_include_directories(prettifier_config_tests PRIVATE
    include
    ${GTEST_INCLUDE_DIRS}
)

target_compile_options(prettifier_config_tests PRIVATE
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

# Create Phase 3.1 prettifier pipeline integration test
add_executable(phase3_prettifier_pipeline_test
    tests/integration/phase3_prettifier_pipeline_test.cpp
    ${PRETTIFIER_SOURCES}
    ${LOGGING_SOURCES}
)

target_link_libraries(phase3_prettifier_pipeline_test
    nlohmann_json::nlohmann_json
    Threads::Threads
    GTest::gtest
    GTest::gtest_main
    GTest::gmock
    GTest::gmock_main
)

target_include_directories(phase3_prettifier_pipeline_test PRIVATE
    include
    ${GTEST_INCLUDE_DIRS}
)

target_compile_options(phase3_prettifier_pipeline_test PRIVATE
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra -Wno-unused-parameter>
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wno-unused-parameter>
)

# Create Real Provider API Integration Test (v2.1 Production Validation)
add_executable(real_provider_api_integration_test
    test/real_provider_api_integration_test.cpp
    ${PRETTIFIER_SOURCES}
    ${LOGGING_SOURCES}
)

target_link_libraries(real_provider_api_integration_test
    nlohmann_json::nlohmann_json
    CURL::libcurl
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
    GTest::gtest
    GTest::gmock
)

target_include_directories(real_provider_api_integration_test PRIVATE
    include
    ${GTEST_INCLUDE_DIRS}
)

target_compile_options(real_provider_api_integration_test PRIVATE
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra -Wno-unused-parameter>
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wno-unused-parameter>
)

# Create Live API Integration Test (Anthropic + OpenAI with Real APIs)
add_executable(live_api_integration_test
    test/live_api_integration_test.cpp
    ${PRETTIFIER_SOURCES}
    ${LOGGING_SOURCES}
)

target_link_libraries(live_api_integration_test
    nlohmann_json::nlohmann_json
    CURL::libcurl
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
    GTest::gtest
    GTest::gmock
)

target_include_directories(live_api_integration_test PRIVATE
    include
    ${GTEST_INCLUDE_DIRS}
)

target_compile_options(live_api_integration_test PRIVATE
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra -Wno-unused-parameter>
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wno-unused-parameter>
)

# ============================================================================
# v2.2 Test Executables
# ============================================================================

# v2.2 Logging Test (Global logging functions)
add_executable(v2_2_logging_test
    test/v2_2_logging_test.cpp
    ${LOGGING_SOURCES}
)

target_link_libraries(v2_2_logging_test
    nlohmann_json::nlohmann_json
    Threads::Threads
    GTest::gtest
    GTest::gmock
)

target_include_directories(v2_2_logging_test PRIVATE
    include
    ${GTEST_INCLUDE_DIRS}
)

target_compile_options(v2_2_logging_test PRIVATE
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra -Wno-unused-parameter>
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wno-unused-parameter>
)

# v2.2 ConfigValidator Test
add_executable(v2_2_config_validator_test
    test/v2_2_config_validator_test.cpp
    ${WEBUI_SOURCES}
    ${LOGGING_SOURCES}
)

target_link_libraries(v2_2_config_validator_test
    nlohmann_json::nlohmann_json
    Crow::Crow
    Threads::Threads
    GTest::gtest
    GTest::gmock
)

target_include_directories(v2_2_config_validator_test PRIVATE
    include
    ${GTEST_INCLUDE_DIRS}
)

target_compile_options(v2_2_config_validator_test PRIVATE
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra -Wno-unused-parameter>
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wno-unused-parameter>
)

# v2.2 PrettifierAPI Test
add_executable(v2_2_prettifier_api_test
    test/v2_2_prettifier_api_test.cpp
    ${WEBUI_SOURCES}
    ${PRETTIFIER_SOURCES}
    ${LOGGING_SOURCES}
)

target_link_libraries(v2_2_prettifier_api_test
    nlohmann_json::nlohmann_json
    Crow::Crow
    Threads::Threads
    GTest::gtest
    GTest::gmock
)

target_include_directories(v2_2_prettifier_api_test PRIVATE
    include
    ${GTEST_INCLUDE_DIRS}
)

target_compile_options(v2_2_prettifier_api_test PRIVATE
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra -Wno-unused-parameter>
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wno-unused-parameter>
)

# v2.2 Task 1: Crow HTTP Route Integration Tests
add_executable(v2_2_crow_integration_test
    test/v2_2_crow_integration_test.cpp
)

target_link_libraries(v2_2_crow_integration_test
    nlohmann_json::nlohmann_json
    CURL::libcurl
    Threads::Threads
    GTest::gtest
    GTest::gmock
)

target_include_directories(v2_2_crow_integration_test PRIVATE
    include
    ${GTEST_INCLUDE_DIRS}
)

target_compile_options(v2_2_crow_integration_test PRIVATE
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra -Wno-unused-parameter>
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wno-unused-parameter>
)

# v2.2 Task 2: Frontend CSS Validation Tests
add_executable(v2_2_frontend_css_test
    test/v2_2_frontend_css_test.cpp
)

target_link_libraries(v2_2_frontend_css_test
    Threads::Threads
    GTest::gtest
    GTest::gmock
)

target_include_directories(v2_2_frontend_css_test PRIVATE
    include
    ${GTEST_INCLUDE_DIRS}
)

target_compile_options(v2_2_frontend_css_test PRIVATE
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra -Wno-unused-parameter>
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wno-unused-parameter>
)

# v2.2 Task 3: JavaScript API Client Tests
add_executable(v2_2_javascript_api_test
    test/v2_2_javascript_api_test.cpp
)

target_link_libraries(v2_2_javascript_api_test
    Threads::Threads
    GTest::gtest
    GTest::gmock
)

target_include_directories(v2_2_javascript_api_test PRIVATE
    include
    ${GTEST_INCLUDE_DIRS}
)

target_compile_options(v2_2_javascript_api_test PRIVATE
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra -Wno-unused-parameter>
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wno-unused-parameter>
)

# v2.2 Task 4: UI Controller Tests
add_executable(v2_2_ui_controller_test
    test/v2_2_ui_controller_test.cpp
)

target_link_libraries(v2_2_ui_controller_test
    Threads::Threads
    GTest::gtest
    GTest::gmock
)

target_include_directories(v2_2_ui_controller_test PRIVATE
    include
    ${GTEST_INCLUDE_DIRS}
)

target_compile_options(v2_2_ui_controller_test PRIVATE
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra -Wno-unused-parameter>
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wno-unused-parameter>
)

# v2.2 Task 5: HTML Structure Tests
add_executable(v2_2_html_structure_test
    test/v2_2_html_structure_test.cpp
)

target_link_libraries(v2_2_html_structure_test
    Threads::Threads
    GTest::gtest
    GTest::gmock
)

target_include_directories(v2_2_html_structure_test PRIVATE
    include
    ${GTEST_INCLUDE_DIRS}
)

target_compile_options(v2_2_html_structure_test PRIVATE
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra -Wno-unused-parameter>
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wno-unused-parameter>
)

# ============================================================================
# v3.0 Model Discovery System Tests
# ============================================================================

# Model Registry Test
add_executable(model_registry_test
    test/model_registry_test.cpp
    src/core/model_registry.cpp
)

target_link_libraries(model_registry_test
    nlohmann_json::nlohmann_json
    Threads::Threads
    GTest::gtest
    GTest::gtest_main
    GTest::gmock
)

target_include_directories(model_registry_test PRIVATE
    include
    ${GTEST_INCLUDE_DIRS}
)

target_compile_options(model_registry_test PRIVATE
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra -Wno-unused-parameter>
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wno-unused-parameter>
)

# Anthropic Model Query Test
add_executable(anthropic_model_query_test
    test/anthropic_model_query_test.cpp
    src/core/model_registry.cpp
    src/providers/anthropic_model_query.cpp
)

target_link_libraries(anthropic_model_query_test
    nlohmann_json::nlohmann_json
    CURL::libcurl
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
    GTest::gtest
    GTest::gtest_main
    GTest::gmock
)

target_include_directories(anthropic_model_query_test PRIVATE
    include
    ${GTEST_INCLUDE_DIRS}
)

target_compile_options(anthropic_model_query_test PRIVATE
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra -Wno-unused-parameter>
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wno-unused-parameter>
)

# OpenAI Model Query Test
add_executable(openai_model_query_test
    test/openai_model_query_test.cpp
    src/core/model_registry.cpp
    src/providers/openai_model_query.cpp
)

target_link_libraries(openai_model_query_test
    nlohmann_json::nlohmann_json
    CURL::libcurl
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
    GTest::gtest
    GTest::gtest_main
    GTest::gmock
)

target_include_directories(openai_model_query_test PRIVATE
    include
    ${GTEST_INCLUDE_DIRS}
)

target_compile_options(openai_model_query_test PRIVATE
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra -Wno-unused-parameter>
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wno-unused-parameter>
)

# Cerebras Model Query Test
add_executable(cerebras_model_query_test
    test/cerebras_model_query_test.cpp
    src/core/model_registry.cpp
    src/providers/cerebras_model_query.cpp
)

target_link_libraries(cerebras_model_query_test
    nlohmann_json::nlohmann_json
    CURL::libcurl
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
    GTest::gtest
    GTest::gtest_main
    GTest::gmock
)

target_include_directories(cerebras_model_query_test PRIVATE
    include
    ${GTEST_INCLUDE_DIRS}
)

target_compile_options(cerebras_model_query_test PRIVATE
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra -Wno-unused-parameter>
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wno-unused-parameter>
)

# API Initializer Test
add_executable(api_initializer_test
    test/api_initializer_test.cpp
    src/core/api_initializer.cpp
    src/core/model_registry.cpp
    src/providers/anthropic_model_query.cpp
    src/providers/openai_model_query.cpp
    src/providers/cerebras_model_query.cpp
)

target_link_libraries(api_initializer_test
    nlohmann_json::nlohmann_json
    CURL::libcurl
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
    GTest::gtest
    GTest::gtest_main
    GTest::gmock
)

target_include_directories(api_initializer_test PRIVATE
    include
    ${GTEST_INCLUDE_DIRS}
)

target_compile_options(api_initializer_test PRIVATE
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra -Wno-unused-parameter>
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wno-unused-parameter>
)

# Main Startup Integration Test (Phase 4.4)
add_executable(main_startup_integration_test
    test/main_startup_integration_test.cpp
    src/core/api_initializer.cpp
    src/core/model_registry.cpp
    src/providers/anthropic_model_query.cpp
    src/providers/openai_model_query.cpp
    src/providers/cerebras_model_query.cpp
    src/config/global_config.cpp
    ${PRETTIFIER_SOURCES}
)

target_link_libraries(main_startup_integration_test
    nlohmann_json::nlohmann_json
    CURL::libcurl
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
    GTest::gtest
    GTest::gtest_main
    GTest::gmock
)

target_include_directories(main_startup_integration_test PRIVATE
    include
    ${GTEST_INCLUDE_DIRS}
)

target_compile_options(main_startup_integration_test PRIVATE
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra -Wno-unused-parameter>
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wno-unused-parameter>
)

# Live API Dynamic Models Test (Phase 5)
add_executable(live_api_dynamic_models_test
    test/live_api_dynamic_models_test.cpp
    src/core/api_initializer.cpp
    src/core/model_registry.cpp
    src/providers/anthropic_model_query.cpp
    src/providers/openai_model_query.cpp
    src/providers/cerebras_model_query.cpp
    ${PRETTIFIER_SOURCES}
)

target_link_libraries(live_api_dynamic_models_test
    nlohmann_json::nlohmann_json
    CURL::libcurl
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
    GTest::gtest
    GTest::gtest_main
    GTest::gmock
)

target_include_directories(live_api_dynamic_models_test PRIVATE
    include
    ${GTEST_INCLUDE_DIRS}
)

target_compile_options(live_api_dynamic_models_test PRIVATE
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

add_custom_target(test_prettifier
    COMMAND prettifier_plugin_tests
    DEPENDS prettifier_plugin_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running prettifier plugin tests"
)

add_custom_target(test_prettifier_config
    COMMAND prettifier_config_tests
    DEPENDS prettifier_config_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running prettifier config tests"
)

add_custom_target(test_integration
    COMMAND integration_tests
    DEPENDS integration_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running integration tests"
)

add_custom_target(test_phase3
    COMMAND phase3_prettifier_pipeline_test
    DEPENDS phase3_prettifier_pipeline_test
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running Phase 3.1 prettifier pipeline integration tests"
)

add_custom_target(test_real_api
    COMMAND real_provider_api_integration_test
    DEPENDS real_provider_api_integration_test
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running Real Provider API Integration Tests (v2.1 Production Validation)"
)

add_custom_target(test_live_api
    COMMAND live_api_integration_test
    DEPENDS live_api_integration_test
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running Live API Integration Tests (Anthropic + OpenAI with Real API Keys)"
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

add_custom_target(test_metrics
    COMMAND metrics_collector_tests
    DEPENDS metrics_collector_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running metrics collection tests"
)

add_custom_target(test_ab_testing
    COMMAND ab_testing_framework_tests
    DEPENDS ab_testing_framework_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running A/B testing framework tests"
)

add_custom_target(test_all
    COMMAND ${CMAKE_COMMAND} --build . --target test_unit
    COMMAND ${CMAKE_COMMAND} --build . --target test_prettifier
    COMMAND ${CMAKE_COMMAND} --build . --target test_prettifier_config
    COMMAND ${CMAKE_COMMAND} --build . --target test_integration
    COMMAND ${CMAKE_COMMAND} --build . --target test_advanced
    COMMAND ${CMAKE_COMMAND} --build . --target test_metrics
    COMMAND ${CMAKE_COMMAND} --build . --target test_ab_testing
    DEPENDS unit_tests prettifier_plugin_tests prettifier_config_tests integration_tests advanced_test_runner metrics_collector_tests ab_testing_framework_tests
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