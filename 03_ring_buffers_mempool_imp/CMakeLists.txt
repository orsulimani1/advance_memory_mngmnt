cmake_minimum_required(VERSION 3.10)
project(MemoryPoolRingBuffer C)

# Set C standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Enable debugging
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -O0")

# Set output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

# Create the ring buffer library
add_library(ring_buffer STATIC
    ring_buffer.c
)
target_include_directories(ring_buffer PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Create the memory pool library
add_library(mempool_ring STATIC
    mempool_ring.c
)
target_include_directories(mempool_ring PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(mempool_ring PRIVATE
    ring_buffer
)

# Create the test executable
add_executable(mempool_test
    mempool_test.c
)
target_link_libraries(mempool_test PRIVATE
    mempool_ring
    ring_buffer
)

# Add compiler warnings
target_compile_options(ring_buffer PRIVATE -Wall -Wextra)
target_compile_options(mempool_ring PRIVATE -Wall -Wextra)
target_compile_options(mempool_test PRIVATE -Wall -Wextra)

# Enable testing
enable_testing()
add_test(NAME RingBufferMemPoolTest COMMAND mempool_test)