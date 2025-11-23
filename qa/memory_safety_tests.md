# Memory Safety Regression Tests

## Overview
This document defines comprehensive memory safety regression tests to prevent memory corruption, leaks, and unsafe memory access patterns in the Aimux codebase.

## Test Categories

### 1. Valgrind Integration Tests

#### 1.1 Memory Leak Detection
**Objective**: Detect and prevent memory leaks using Valgrind Memcheck.

**Test Cases**:
- All allocated memory is properly freed
- No reachable leaks at program exit
- No definitely lost blocks
- No possibly lost blocks
- No indirectly lost blocks

**Valgrind Command**:
```bash
valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes \
         --suppressions=valgrind.supp ./memory_safety_tests
```

**Expected Output**:
```
HEAP SUMMARY:
    in use at exit: 0 bytes in 0 blocks
  total heap usage: X allocs, X frees, Y bytes allocated
All heap blocks were freed -- no leaks are possible
```

#### 1.2 Invalid Memory Access Detection
**Objective**: Detect invalid reads/writes and use of uninitialized values.

**Test Cases**:
- No invalid reads of size X
- No invalid writes of size X
- No use of uninitialized values
- No invalid free/delete/delete[]
- No mismatched free/delete

#### 1.3 Memory Errors in Threaded Code
**Objective**: Detect memory errors specific to multi-threaded operations.

**Test Cases**:
- Race condition detection with Helgrind
- Thread stack usage validation
- Synchronization primitive misuse

**Helgrind Command**:
```bash
valgrind --tool=helgrind ./thread_safety_tests
```

### 2. Address Sanitizer Setup

#### 2.1 Compiler Configuration
**Objective**: Enable comprehensive address sanitization for development builds.

**CMake Configuration**:
```cmake
if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
endif()
```

#### 2.2 ASan Runtime Options
**Objective**: Configure AddressSanitizer for optimal error detection.

**Environment Variables**:
```bash
export ASAN_OPTIONS=detect_leaks=1:detect_odr_violation=1:strict_string_checks=1:detect_stack_use_after_return=1:check_initialization_order=1:strict_init_order=1
```

#### 2.3 Suppression Files
**Objective**: Suppress false positives from third-party libraries.

**asan.supp**:
```
# Suppress known false positives from external libraries
leak:libc.so
leak:libcurl.so
leak:libssl.so
```

### 3. Memory Leak Detection Tests

#### 3.1 Smart Pointer Validation
**Objective**: Ensure proper use of smart pointers and RAII patterns.

**Test Cases**:
- All heap allocations use smart pointers where appropriate
- No raw pointers with unclear ownership
- Proper use of std::unique_ptr, std::shared_ptr
- No circular references with shared_ptr

**Implementation Test**:
```cpp
// test/memory_safety_test.cpp
TEST(MemorySafetyTest, SmartPointerLeakPrevention) {
    {
        auto ptr = std::make_unique<TestObject>();
        // ptr should be automatically freed when leaving scope
    }

    // Verify no leaks using ASan or custom leak detector
    EXPECT_EQ(memory_tracker::get_alloc_count(), 0);
}
```

#### 3.2 Container Memory Management
**Objective**: Validate memory management in STL containers.

**Test Cases**:
- Large vector allocations don't leak
- Container clear() properly frees memory
- Custom allocators don't leak
- Reserve/resize operations are memory safe

**Implementation Test**:
```cpp
TEST(MemorySafetyTest, ContainerMemorySafety) {
    std::vector<std::string> large_vector;
    large_vector.reserve(10000);

    for (int i = 0; i < 10000; ++i) {
        large_vector.emplace_back("test_string_" + std::to_string(i));
    }

    large_vector.clear();
    large_vector.shrink_to_fit();

    // Verify all memory is freed
    EXPECT_TRUE(memory_validator::no_leaks_detected());
}
```

#### 3.3 Cache Memory Leak Detection
**Objective**: Ensure response cache doesn't accumulate leaked memory.

**Test Cases**:
- Cache eviction frees memory properly
- Time-based expiration doesn't leak
- Size-based limits prevent memory growth
- Cache destruction cleans up all entries

**Implementation Test**:
```cpp
TEST(MemorySafetyTest, CacheLeakPrevention) {
    {
        response_cache cache(100); // 100 entry limit

        // Fill cache beyond capacity
        for (int i = 0; i < 200; ++i) {
            cache.put("key_" + std::to_string(i), "value_" + std::to_string(i));
        }

        // Cache should have evicted old entries without leaks
    }

    EXPECT_TRUE(memory_tracker::no_active_allocations());
}
```

### 4. Use-After-Free Detection Tests

#### 4.1 Dangling Pointer Prevention
**Objective**: Detect use-after-free errors in pointer management.

**Test Cases**:
- No access to deleted heap memory
- No access to freed stack memory
- Iterator invalidation prevention
- Observer pattern safety

**Implementation Test**:
```cpp
TEST(MemorySafetyTest, UseAfterFreeDetection) {
    char* ptr = new char[100];
    strcpy(ptr, "test");
    delete[] ptr;

    // This should crash under ASan or be caught by our validation
    EXPECT_DEATH(ptr[0] = 'x', "heap-use-after-free");
}
```

#### 4.2 Iterator Invalidation Prevention
**Objective**: Prevent iterator invalidation in container operations.

**Test Cases**:
- Iterator remains valid during expected operations
- No use of invalidated iterators
- Safe iteration while modifying containers

**Implementation Test**:
```cpp
TEST(MemorySafetyTest, IteratorInvalidationPrevention) {
    std::vector<int> numbers = {1, 2, 3, 4, 5};

    // This should be safe - using indices instead of iterators
    for (size_t i = 0; i < numbers.size(); ++i) {
        if (numbers[i] % 2 == 0) {
            numbers.erase(numbers.begin() + i);
            i--; // Adjust index after erase
        }
    }

    EXPECT_EQ(numbers.size(), 3); // Only odd numbers remain
}
```

### 5. Buffer Overflow Simulation Tests

#### 5.1 String Buffer Overflows
**Objective**: Detect and prevent buffer overflows in string operations.

**Test Cases**:
- String copy with proper bounds checking
- Safe string concatenation
- Buffer size validation before operations
- Use of safe string functions

**Implementation Test**:
```cpp
TEST(MemorySafetyTest, BufferOverflowPrevention) {
    char small_buffer[10];
    std::string large_input(100, 'A');

    // Should use safe string operations
    EXPECT_THROW(strncpy(small_buffer, large_input.c_str(), sizeof(small_buffer)),
                 std::length_error);

    // Or use safe alternatives
    auto result = string_utils::safe_copy(small_buffer, large_input, sizeof(small_buffer));
    EXPECT_TRUE(result.success);
    EXPECT_LT(result.copied_bytes, sizeof(small_buffer));
}
```

#### 5.2 Array Bound Validation
**Objective**: Ensure array access stays within valid bounds.

**Test Cases**:
- Array index validation
- Vector bounds checking
- Multi-dimensional array safety
- Stack buffer overflow prevention

**Implementation Test**:
```cpp
TEST(MemorySafetyTest, ArrayBoundsValidation) {
    std::array<int, 10> safe_array = {0};

    constexpr int valid_index = 5;
    constexpr int invalid_index = 15;

    // Valid access should work
    EXPECT_NO_THROW(safe_array[valid_index] = 42);
    EXPECT_EQ(safe_array[valid_index], 42);

    // Invalid access should be caught
    #ifdef DEBUG
    EXPECT_THROW(safe_array.at(invalid_index) = 99, std::out_of_range);
    #else
    // In release, this should be caught by ASan
    EXPECT_DEATH(safe_array[invalid_index] = 99, "heap-buffer-overflow");
    #endif
}
```

### 6. RAII Validation Tests

#### 6.1 Constructor-Destructor Exception Safety
**Objective**: Ensure RAII classes are exception-safe.

**Test Cases**:
- Constructors don't leak on failure
- Destructors called for successfully constructed objects
- Exception propagation doesn't skip cleanup
- Move operations maintain invariants

**Implementation Test**:
```cpp
TEST(MemorySafetyTest, RAIISafetyValidation) {
    auto memory_before = memory_tracker::get_usage();

    try {
        risky_raii_object obj("test_param");
        throw std::runtime_error("test exception");
    } catch (const std::exception&) {
        // Object destructor should be called here
    }

    auto memory_after = memory_tracker::get_usage();
    EXPECT_EQ(memory_before, memory_after); // No leak
}
```

#### 6.2 Resource Management Validation
**Objective**: Ensure all resources are properly managed by RAII.

**Test Cases**:
- File handles are closed automatically
- Network connections are closed
- Database connections are returned to pool
- Memory-mapped files are unmapped

**Implementation Test**:
```cpp
TEST(MemorySafetyTest, ResourceManagementValidation) {
    {
        file_resource file("/tmp/test_file.txt");
        EXPECT_TRUE(file.is_open());

        // File should be automatically closed when leaving scope
    }

    // Verify file is closed and resources freed
    EXPECT_FALSE(file_utils::is_open("/tmp/test_file.txt"));
}
```

### 7. Large Allocation Scenarios

#### 7.1 Memory Pressure Testing
**Objective**: Test behavior under memory pressure conditions.

**Test Cases**:
- Large allocations are properly handled
- Out-of-memory conditions don't corrupt state
- Memory allocation failures are handled gracefully
- No memory fragmentation issues

**Implementation Test**:
```cpp
TEST(MemorySafetyTest, MemoryPressureTesting) {
    const size_t large_size = 1024 * 1024 * 100; // 100MB

    std::vector<std::unique_ptr<char[]>> large_allocations;

    try {
        for (int i = 0; i < 10; ++i) {
            large_allocations.emplace_back(std::make_unique<char[]>(large_size));
            memset(large_allocations.back().get(), 0xAA, large_size);
        }
    } catch (const std::bad_alloc&) {
        // Handle OOM gracefully
    }

    // Cleanup should be automatic
}
```

#### 7.2 Cache Size Limit Testing
**Objective**: Ensure cache respects memory limits.

**Test Cases**:
- Cache enforces maximum size limits
- Memory usage stays within bounds
- Cache eviction prevents memory exhaustion

## Test Execution Framework

### Build Configuration
```cmake
# Memory Safety Test Target
add_executable(memory_safety_tests
    test/memory_safety_test.cpp
    # ... other test files
)

# Compile with sanitizers for debug builds
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(memory_safety_tests PRIVATE
        -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer)
    target_link_libraries(memory_safety_tests PRIVATE
        -fsanitize=address -fsanitize=undefined)
endif()
```

### Test Execution
```bash
# Build with sanitizers
cmake -DCMAKE_BUILD_TYPE=Debug ..
make memory_safety_tests

# Run with ASan options
ASAN_OPTIONS=detect_leaks=1:strict_string_checks=1 ./memory_safety_tests

# Run with Valgrind
valgrind --leak-check=full ./memory_safety_tests
```

### Continuous Integration
```yaml
# .github/workflows/memory_safety.yml
- name: Run Memory Safety Tests
  run: |
    cmake -DCMAKE_BUILD_TYPE=Debug ..
    make memory_safety_tests
    ASAN_OPTIONS=detect_leaks=1 ./memory_safety_tests
    valgrind --error-exitcode=1 --leak-check=full ./memory_safety_tests
```

## Success Criteria

### Must Pass
- No memory leaks detected by Valgrind
- No address sanitizer errors
- All RAII classes properly clean up resources
- Buffer overflow prevention tests pass

### Should Pass
- No use-after-free errors
- Iterator safety validation
- Memory pressure tests complete successfully

### Performance Requirements
- Memory safety tests complete within 2 minutes
- Sanitizer overhead < 50% performance impact
- Memory usage stays within expected bounds

## Monitoring and Reporting

### Metrics Collection
- Memory usage trends over time
- Leak detection frequency
- Memory safety test coverage
- Performance impact of safety measures

### Alert Conditions
- New memory leaks detected
- Increasing memory usage trends
- Memory unsafe code patterns detected
- Sanitizer errors in CI/CD pipeline

## Documentation Requirements

### Test Documentation
- Each test includes rationale for memory safety
- Expected memory usage patterns documented
- Known limitations and false positives noted

### Developer Guidelines
- Memory management best practices
- When to use smart pointers vs raw pointers
- Container usage guidelines for memory safety
- Exception safety requirements