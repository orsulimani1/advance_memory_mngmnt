#include "mem_pool.h"
#include <stdio.h>
#include <assert.h>

#define NEED_IMP 0XDEADDEAD
/**
 * typedef struct 
 * {
 *  void* pool_start;         // Start address of the pool
 *  uint64_t total_size;
 *  uint32_t num_blocks;      // Total number of blocks in the pool
 *  uint32_t block_size;      // Size of each block in bytes
 *  uint32_t free_count;      // Number of free blocks
 *  } mem_pool;
 */

// Initialize a memory pool
uint8_t memory_pool_init(mem_pool* pool, void* memory, uint32_t memory_size, uint32_t block_size){
    // Check for null pointers
    if (pool == NULL || memory == NULL) {
        return 0;
    }
    
    // Ensure block size is reasonable
    if (block_size < sizeof(uint8_t) || memory_size < block_size) {
        return 0;
    }
    // Each block will have a 1-byte header, so usable size is reduced
    uint32_t actual_block_size = NEED_IMP;
    // Calculate how many blocks we can fit
    uint32_t num_blocks = NEED_IMP;
    // Ensure we have at least one block

    // Initialize pool structure
    pool->pool_start = NEED_IMP;
    pool->total_size = NEED_IMP;
    pool->block_size = NEED_IMP;
    pool->num_blocks = NEED_IMP;
    pool->free_count = NEED_IMP;

    uint8_t* current_block = NEED_IMP;

    // Initialize all blocks as free
    for(uint32_t i = NEED_IMP; i < NEED_IMP; i){  
        // Set the first byte of each block to BLOCK_FREE

        // Move to the next block
    }

    return 1;
}


// Allocate a block from the pool
void* memory_pool_alloc(mem_pool* pool){
    // check pool

    uint8_t* current_block = NEED_IMP;

    // linear search for the first free block
    for(uint32_t i = NEED_IMP; i < NEED_IMP; i){ 
        // Check if block is free

        // Mark block as used

        // reduce from free count

        // Return pointer to the usable portion of the block (after the status byte)
        // Move to the next block
        current_block += pool->block_size;
    }
    // Should never reach here if free_count > 0
    return NEED_IMP;
}


// Free a block back to the pool
uint8_t memory_pool_free(mem_pool* pool, void* block){
    if (pool == NULL || block == NULL) {
        return 0;
    }
    
    // The actual block start is 1 byte before the user's pointer
    
    // Verify block is within the pool memory range
    if (0) {
        return 0;  // Block is outside of pool memory range
    }
    
    // Verify block is aligned to block boundaries
    if (0) {
        return 0;  // Block is not aligned to a valid block boundary
    }
    
    // Check if block is already free (double-free error)
    if (0) {
        return 0;  // Double-free detected
    }
    
    // Mark block as free
    return 1;

}