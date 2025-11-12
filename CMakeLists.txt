cmake_minimum_required(VERSION 3.20)
project(Aimux VERSION 2.0.0 LANGUAGES CXX)

# C++ Standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

# Find required packages
find_package(PkgConfig REQUIRED)

# vcpkg toolchain if available
if(DEFINED CMAKE_TOOLCHAIN_FILE AND CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg")
    message(STATUS "Using vcpkg toolchain")
    set(CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/installed/arm64-osx;${CMAKE_PREFIX_PATH}")
endif()

find_package(nlohmann_json CONFIG)
if(NOT nlohmann_json_FOUND)
    message(STATUS "nlohmann_json not found via CONFIG, trying pkg-config")
    pkg_check_modules(NLOHMANN_JSON REQUIRED nlohmann_json)
endif()
find_package(CURL COMPONENTS HTTP REQUIRED)
find_package(Threads REQUIRED)

# Source files
set(CORE_SOURCES
    src/core/router.cpp
    src/core/failover.cpp  
    src/core/bridge.cpp
)
set(LOGGING_SOURCES
    src/logging/logger.cpp
)
file(GLOB_RECURSE PROVIDER_SOURCES "src/providers/*.cpp")
file(GLOB_RECURSE NETWORK_SOURCES "src/network/*.cpp")
file(GLOB_RECURSE CLI_SOURCES "src/cli/*.cpp")
file(GLOB_RECURSE UTILS_SOURCES "src/utils/*.cpp")

# Create main executable
add_executable(aimux
    src/main.cpp
    ${CORE_SOURCES}
    ${PROVIDER_SOURCES}
    ${NETWORK_SOURCES}
    ${LOGGING_SOURCES}
    ${CLI_SOURCES}
    ${UTILS_SOURCES}
)

# Link libraries
target_link_libraries(aimux
    nlohmann_json::nlohmann_json
    CURL::libcurl
    Threads::Threads
)

# Compiler-specific options
target_compile_options(aimux PRIVATE
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra>
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra>
)

# Installation
install(TARGETS aimux DESTINATION bin)
install(FILES version.txt DESTINATION share/aimux)

# Print configuration
message(STATUS "")
message(STATUS "Aimux ${PROJECT_VERSION} Configuration:")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Compiler: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "  Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")