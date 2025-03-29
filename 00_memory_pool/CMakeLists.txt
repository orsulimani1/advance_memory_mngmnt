# Memory pool library (non-implemented version)
add_library(memory_pool STATIC
    mem_pool.c
)
target_include_directories(memory_pool PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Add compiler warnings
target_compile_options(memory_pool PRIVATE -Wall -Wextra)