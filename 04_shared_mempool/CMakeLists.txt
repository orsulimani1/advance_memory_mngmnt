# Rename targets to avoid conflicts
add_library(shared_ring_buffer STATIC
    ring_buffer.c
)
target_include_directories(shared_ring_buffer PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Find pthread library
find_package(Threads REQUIRED)

# Create the memory pool library with a unique name
add_library(shared_mempool_ring STATIC
    mempool_ring.c
)
target_include_directories(shared_mempool_ring PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(shared_mempool_ring PRIVATE
    shared_ring_buffer
    rt  # For shared memory functions
)

# Create the test executable with a unique name
add_executable(shared_mempool_test
    mempool_test.c
)
target_link_libraries(shared_mempool_test PRIVATE
    shared_mempool_ring
    shared_ring_buffer
    Threads::Threads  # For pthread
    rt  # For shared memory functions
)

# Add compiler warnings
target_compile_options(shared_ring_buffer PRIVATE -Wall -Wextra)
target_compile_options(shared_mempool_ring PRIVATE -Wall -Wextra)
target_compile_options(shared_mempool_test PRIVATE -Wall -Wextra)

# Add test
add_test(NAME SharedMemPoolTest COMMAND shared_mempool_test)