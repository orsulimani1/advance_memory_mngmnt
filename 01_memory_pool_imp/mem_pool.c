#include "mem_pool.h"
#include <stdio.h>
#include <assert.h>

#define NEED_IMP 0XDEAD


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
    uint32_t actual_block_size = block_size + 1;

    // Calculate how many blocks we can fit
    uint32_t num_blocks = memory_size/ actual_block_size;
    // Ensure we have at least one block
    if(num_blocks == 0){
        return 0;
    }
    // Initialize pool structure
    pool->pool_start = memory;
    pool->total_size = memory_size;
    pool->block_size = block_size;
    pool->num_blocks = num_blocks;
    pool->free_count = num_blocks;

    uint8_t* current_block = (uint8_t *)memory;

    // Initialize all blocks as free
    for(uint32_t i = 0; i < num_blocks; i++){  
        // Set the first byte of each block to BLOCK_FREE
        *current_block = BLOCK_FREE;
        // Move to the next block
        current_block += actual_block_size;
    }

    return 1;
}


// Allocate a block from the pool
void* memory_pool_alloc(mem_pool* pool){
    // check pool
    if(pool == NULL || pool->free_count == 0){
        return NULL;
    }
    uint8_t* current_block = (uint8_t *)pool->pool_start;
    uint32_t actual_block_size = pool->block_size + 1; // Include status byte

    // linear search for the first free block
    for(uint32_t i = 0; i < pool->num_blocks; i++){ 
        // Check if block is free
        if (*current_block == BLOCK_FREE){

            // Mark block as used
            *current_block = BLOCK_USED;
            // reduce from free count
            pool->free_count--;
            // Return pointer to the usable portion of the block (after the status byte)
            return current_block + sizeof(uint8_t);
        }

        // Move to the next block
        current_block += actual_block_size;
    }
    // Should never reach here if free_count > 0
    return NULL;
}


// Free a block back to the pool
uint8_t memory_pool_free(mem_pool* pool, void* block){
    if (pool == NULL || block == NULL) {
        return 0;
    }
    
    // The actual block start is 1 byte before the user's pointer
    uint8_t* actual_block_start = (uint8_t *)block - sizeof(uint8_t);
    uint32_t actual_block_size = pool->block_size + 1; // Include status byte

    // Verify block is within the pool memory range
    if (actual_block_start < (uint8_t *)pool->pool_start ||
        actual_block_start > (uint8_t *)pool->pool_start  + (pool->num_blocks * actual_block_size)) {
        return 0;  // Block is outside of pool memory range
    }
    
    uint32_t offset = actual_block_start - (uint8_t *)pool->pool_start;
    // Verify block is aligned to block boundaries
    if (offset % actual_block_size != 0) {
        return 0;  // Block is not aligned to a valid block boundary
    }
    
    // Check if block is already free (double-free error)
    if (*actual_block_start ==  BLOCK_FREE) {
        return 0;  // Double-free detected
    }
    // Mark block as free
    *actual_block_start = BLOCK_FREE;
    pool->free_count++;
    return 1;

}