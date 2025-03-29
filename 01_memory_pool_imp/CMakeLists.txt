# Memory pool implementation library
add_library(memory_pool_imp STATIC
    mem_pool.c
)
target_include_directories(memory_pool_imp PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Memory pool implementation tester executable
add_executable(memory_pool_tester
    memory_pool_tester.c
)
target_link_libraries(memory_pool_tester PRIVATE
    memory_pool_imp
)

# Add compiler warnings
target_compile_options(memory_pool_imp PRIVATE -Wall -Wextra)
target_compile_options(memory_pool_tester PRIVATE -Wall -Wextra)

