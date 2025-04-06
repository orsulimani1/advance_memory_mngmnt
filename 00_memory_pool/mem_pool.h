#ifndef MEMPOOL_H
#define MEMPOOL_H
#include <stdint.h>
#include <stddef.h>

/**
 * Key aspects of this implementation:
 * In-Place Status: Each memory block starts with a 1-byte status field (BLOCK_FREE or BLOCK_USED)
 * Memory Layout: Each block's usable memory starts immediately after the status byte
 * Allocation: Returns a pointer to the usable portion of the block (after the status byte)
 * Deallocation: Calculates the original block start by moving back 1 byte from the user's pointer
 */
typedef struct 
{
    void* pool_start;         // Start address of the pool
    uint64_t total_size;
    uint32_t num_blocks;      // Total number of blocks in the pool
    uint32_t block_size;      // Size of each block in bytes
    uint32_t actual_block_size;      // Size of each block in bytes include status
    uint32_t free_count;      // Number of free blocks
} mem_pool;

// Define block status values
#define BLOCK_FREE  0
#define BLOCK_USED  1

// Initialize a memory pool
uint8_t memory_pool_init(mem_pool* pool, void* memory, uint32_t memory_size, uint32_t block_size);

// Allocate a block from the pool
void* memory_pool_alloc(mem_pool* pool);

// Free a block back to the pool
uint8_t memory_pool_free(mem_pool* pool, void* block);

#endif
