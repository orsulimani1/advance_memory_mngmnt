# Find pthread library
find_package(Threads REQUIRED)

# Create the shared memory manager library
add_library(shm_manager STATIC
    shm_manager.c
)
target_include_directories(shm_manager PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/04_shared_mempool  # Include directory for shared memory implementation
)
target_link_libraries(shm_manager PRIVATE
    shared_mempool_ring  # Use library from 04_shared_mempool
    shared_ring_buffer   # Use library from 04_shared_mempool
    rt                   # For shared memory functions
)

# Create the chat server executable
add_executable(chat_server
    chat_server.c
)
target_link_libraries(chat_server PRIVATE
    shm_manager
    shared_mempool_ring
    shared_ring_buffer
    Threads::Threads  # For pthread
    rt                # For shared memory functions
)

# Create the chat client executable
add_executable(chat_client
    chat_client.c
)
target_link_libraries(chat_client PRIVATE
    shm_manager
    shared_mempool_ring
    shared_ring_buffer
    Threads::Threads  # For pthread
    rt                # For shared memory functions
)

# Add compiler warnings
target_compile_options(shm_manager PRIVATE -Wall -Wextra)
target_compile_options(chat_server PRIVATE -Wall -Wextra)
target_compile_options(chat_client PRIVATE -Wall -Wextra)