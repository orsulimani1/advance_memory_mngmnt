cmake_minimum_required(VERSION 3.10)
project(AdvancedMemManagement C)

# Set C standard
set(CMAKE_C_STANDARD 11)  # For atomic operations in newer implementations
set(CMAKE_C_STANDARD_REQUIRED ON)

# Enable debugging
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -O0")

# Set output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

# Add subdirectories
add_subdirectory(00_memory_pool)
add_subdirectory(01_memory_pool_imp)
add_subdirectory(03_ring_buffers_mempool)
add_subdirectory(04_shared_mempool)
add_subdirectory(05_chat_room)

# Enable testing
enable_testing()