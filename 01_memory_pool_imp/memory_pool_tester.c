#include "mem_pool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// Define block status values if not defined in mem_pool.h
#ifndef BLOCK_FREE
#define BLOCK_FREE  0
#endif

#ifndef BLOCK_USED
#define BLOCK_USED  1
#endif

// Test function declarations
void test_init();
void test_alloc_free();
void test_full_pool();
void test_boundary_conditions();
void test_invalid_free();

int main() {
    printf("Running memory pool tests...\n");
    
    test_init();
    test_alloc_free();
    test_full_pool();
    test_boundary_conditions();
    test_invalid_free();
    
    printf("All tests passed!\n");
    return 0;
}

// Test memory pool initialization
void test_init() {
    printf("Testing initialization...\n");
    
    // Create a memory region
    const uint32_t memory_size = 1024;
    void* memory = malloc(memory_size);
    assert(memory != NULL);
    
    // Initialize a pool
    mem_pool pool;
    uint32_t block_size = 16;
    uint8_t result = memory_pool_init(&pool, memory, memory_size, block_size);
    
    // Check initialization succeeded
    assert(result == 1);
    
    // Check pool parameters
    uint32_t expected_num_blocks = memory_size / (block_size + 1); // +1 for status byte
    assert(pool.num_blocks == expected_num_blocks);
    assert(pool.free_count == expected_num_blocks);
    assert(pool.block_size == block_size);
    assert(pool.pool_start == memory);
    
    // Check all blocks are marked as free
    uint8_t* current_block = (uint8_t*)memory;
    for (uint32_t i = 0; i < expected_num_blocks; i++) {
        assert(*current_block == BLOCK_FREE);
        current_block += (block_size + 1); // Move to next block (including status byte)
    }
    
    // Test initialization with invalid parameters
    result = memory_pool_init(NULL, memory, memory_size, block_size);
    assert(result == 0); // Should fail with NULL pool
    
    result = memory_pool_init(&pool, NULL, memory_size, block_size);
    assert(result == 0); // Should fail with NULL memory
    
    result = memory_pool_init(&pool, memory, 0, block_size);
    assert(result == 0); // Should fail with zero memory size
    
    result = memory_pool_init(&pool, memory, memory_size, 0);
    assert(result == 0); // Should fail with zero block size
    
    // Too small block size (must fit status byte)
    result = memory_pool_init(&pool, memory, memory_size, sizeof(uint8_t) - 1);
    assert(result == 0);
    
    // Clean up
    free(memory);
    printf("Initialization tests passed!\n");
}

// Test allocation and deallocation
void test_alloc_free() {
    printf("Testing allocation and deallocation...\n");
    
    // Create a memory region
    const uint32_t memory_size = 1024;
    void* memory = malloc(memory_size);
    assert(memory != NULL);
    
    // Initialize a pool with 16-byte blocks
    mem_pool pool;
    uint32_t block_size = 16;
    memory_pool_init(&pool, memory, memory_size, block_size);
    
    // Calculate expected block count
    uint32_t expected_num_blocks = memory_size / (block_size + 1);
    uint32_t initial_free_count = pool.free_count;
    
    // Allocate a block
    void* block1 = memory_pool_alloc(&pool);
    assert(block1 != NULL);
    assert(pool.free_count == initial_free_count - 1);
    
    // Check that the block status is marked as used
    uint8_t* status_byte = ((uint8_t*)block1) - 1;
    assert(*status_byte == BLOCK_USED);
    
    // Write to the block
    memset(block1, 0xAA, block_size);
    
    // Allocate a second block
    void* block2 = memory_pool_alloc(&pool);
    assert(block2 != NULL);
    assert(pool.free_count == initial_free_count - 2);
    assert(block2 != block1); // Should be different blocks
    
    // Free the first block
    uint8_t free_result = memory_pool_free(&pool, block1);
    assert(free_result == 1);
    assert(pool.free_count == initial_free_count - 1);
    
    // Verify block1 is now marked as free
    assert(*status_byte == BLOCK_FREE);
    
    // Allocate again - should get block1 back
    void* block3 = memory_pool_alloc(&pool);
    assert(block3 != NULL);
    assert(block3 == block1); // Should reuse the first block
    assert(pool.free_count == initial_free_count - 2);
    
    // Free all blocks
    memory_pool_free(&pool, block2);
    memory_pool_free(&pool, block3);
    assert(pool.free_count == initial_free_count);
    
    // Clean up
    free(memory);
    printf("Allocation and deallocation tests passed!\n");
}

// Test allocating until the pool is full
void test_full_pool() {
    printf("Testing full pool behavior...\n");
    
    // Create a memory region
    const uint32_t memory_size = 512;
    void* memory = malloc(memory_size);
    assert(memory != NULL);
    
    // Initialize a pool
    mem_pool pool;
    uint32_t block_size = 15; // Small blocks to get several of them
    memory_pool_init(&pool, memory, memory_size, block_size);
    
    // Calculate expected block count
    uint32_t expected_num_blocks = memory_size / (block_size + 1);
    
    // Allocate all blocks
    void* blocks[100]; // Assume we won't have more than 100 blocks
    for (uint32_t i = 0; i < expected_num_blocks; i++) {
        blocks[i] = memory_pool_alloc(&pool);
        assert(blocks[i] != NULL);
    }
    
    // Pool should now be empty
    assert(pool.free_count == 0);
    
    // Try to allocate one more - should fail
    void* extra_block = memory_pool_alloc(&pool);
    assert(extra_block == NULL);
    
    // Free a block
    memory_pool_free(&pool, blocks[0]);
    assert(pool.free_count == 1);
    
    // Should be able to allocate again
    extra_block = memory_pool_alloc(&pool);
    assert(extra_block != NULL);
    assert(extra_block == blocks[0]);
    assert(pool.free_count == 0);
    
    // Free all blocks
    memory_pool_free(&pool, extra_block);
    for (uint32_t i = 1; i < expected_num_blocks; i++) {
        memory_pool_free(&pool, blocks[i]);
    }
    
    // Pool should be full again
    assert(pool.free_count == expected_num_blocks);
    
    // Clean up
    free(memory);
    printf("Full pool tests passed!\n");
}

// Test boundary conditions
void test_boundary_conditions() {
    printf("Testing boundary conditions...\n");
    
    // Create a memory region
    const uint32_t memory_size = 512;
    void* memory = malloc(memory_size);
    assert(memory != NULL);
    
    // Initialize a pool
    mem_pool pool;
    uint32_t block_size = 15;
    memory_pool_init(&pool, memory, memory_size, block_size);
    
    // Calculate actual block size (including status byte)
    uint32_t actual_block_size = block_size + 1;
    uint32_t expected_num_blocks = memory_size / actual_block_size;
    
    // Allocate first and last block
    void* first_block = memory_pool_alloc(&pool);
    
    void* blocks[100]; // Temp storage
    for (uint32_t i = 0; i < expected_num_blocks - 2; i++) {
        blocks[i] = memory_pool_alloc(&pool);
    }
    
    void* last_block = memory_pool_alloc(&pool);
    
    // Verify first block is at the expected location
    uint8_t* expected_first = (uint8_t*)memory + 1; // +1 for status byte
    assert(first_block == expected_first);
    
    // Verify last block is at the expected location
    uint8_t* expected_last = (uint8_t*)memory + (expected_num_blocks - 1) * actual_block_size + 1;
    assert(last_block == expected_last);
    
    // Free all blocks
    memory_pool_free(&pool, first_block);
    memory_pool_free(&pool, last_block);
    for (uint32_t i = 0; i < expected_num_blocks - 2; i++) {
        memory_pool_free(&pool, blocks[i]);
    }
    
    // Clean up
    free(memory);
    printf("Boundary condition tests passed!\n");
}

// Test invalid free operations
void test_invalid_free() {
    printf("Testing invalid free operations...\n");
    
    // Create a memory region
    const uint32_t memory_size = 512;
    void* memory = malloc(memory_size);
    assert(memory != NULL);
    
    // Initialize a pool
    mem_pool pool;
    uint32_t block_size = 16;
    memory_pool_init(&pool, memory, memory_size, block_size);
    
    // Allocate a block
    void* block = memory_pool_alloc(&pool);
    assert(block != NULL);
    
    // Test NULL parameters
    uint8_t result = memory_pool_free(NULL, block);
    assert(result == 0); // Should fail with NULL pool
    
    result = memory_pool_free(&pool, NULL);
    assert(result == 0); // Should fail with NULL block
    
    // Test double free
    result = memory_pool_free(&pool, block);
    assert(result == 1); // First free should succeed
    
    result = memory_pool_free(&pool, block);
    assert(result == 0); // Second free should fail
    
    // Test freeing memory outside the pool
    char external_memory[16];
    result = memory_pool_free(&pool, external_memory);
    assert(result == 0); // Should fail for external memory
    
    // Test freeing unaligned memory
    void* aligned_block = memory_pool_alloc(&pool);
    assert(aligned_block != NULL);
    
    // Try to free with offset pointer
    uint8_t* offset_ptr = (uint8_t*)aligned_block + 1;
    result = memory_pool_free(&pool, offset_ptr);
    assert(result == 0); // Should fail for unaligned pointer
    
    // Clean up properly
    memory_pool_free(&pool, aligned_block);
    free(memory);
    
    printf("Invalid free tests passed!\n");
}