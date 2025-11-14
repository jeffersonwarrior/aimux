cmake_minimum_required(VERSION 3.20)
project(CrowTest LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)

# Use Crow as header-only library
include_directories(../build-debug/vcpkg_installed/x64-linux/include)

# Find required dependencies for Crow
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

add_executable(crow_test crow_test.cpp)
target_link_libraries(crow_test OpenSSL::SSL OpenSSL::Crypto Threads::Threads)
