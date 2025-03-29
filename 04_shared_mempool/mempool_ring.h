#ifndef MEM_RING_H
#define MEM_RING_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>  // For mode_t
#include "ring_buffer.h"

/**
 * Memory Pool Structure
 */
typedef struct {
    void* pool_start;         // Start address of the memory pool
    uint64_t total_size;      // Total size of the memory pool
    uint32_t block_size;      // Size of each block in bytes
    uint32_t num_blocks;      // Total number of blocks in the pool
    ring_buffer_t* free_blocks; // Ring buffer to track free blocks
    void** block_array;       // Array used by the ring buffer
    int shm_id;               // Shared memory ID when using shared memory
    char* shm_name;           // Shared memory name
} mem_pool_t;

/**
 * Initialize a memory pool in private memory
 * 
 * @param pool Pointer to memory pool structure
 * @param memory Pointer to memory region to use
 * @param memory_size Size of memory region in bytes
 * @param block_size Size of each block in bytes
 * @return true on success, false on failure
 */
bool memory_pool_init(mem_pool_t* pool, void* memory, uint32_t memory_size, uint32_t block_size);

/**
 * Initialize a memory pool in shared memory
 * 
 * @param pool Pointer to memory pool structure
 * @param shm_name Name for the shared memory segment
 * @param memory_size Size of memory region in bytes
 * @param block_size Size of each block in bytes
 * @param create Whether to create the segment (true) or attach to existing (false)
 * @param mode Permission mode when creating shared memory
 * @return true on success, false on failure
 */
bool memory_pool_init_shared(mem_pool_t* pool, const char* shm_name, uint32_t memory_size, 
                           uint32_t block_size, bool create, mode_t mode);

/**
 * Allocate a memory block from the pool
 * 
 * @param pool Pointer to memory pool
 * @return Pointer to allocated block, or NULL if none available
 */
void* memory_pool_alloc(mem_pool_t* pool);

/**
 * Return a memory block to the pool
 * 
 * @param pool Pointer to memory pool
 * @param block Pointer to block being returned
 * @return true if successful, false on error
 */
bool memory_pool_free(mem_pool_t* pool, void* block);

/**
 * Get number of free blocks in the pool
 * 
 * @param pool Pointer to memory pool
 * @return Number of free blocks
 */
uint32_t memory_pool_free_count(mem_pool_t* pool);

/**
 * Get number of allocated blocks in the pool
 * 
 * @param pool Pointer to memory pool
 * @return Number of allocated blocks
 */
uint32_t memory_pool_used_count(mem_pool_t* pool);

/**
 * Reset memory pool to initial state
 * 
 * @param pool Pointer to memory pool
 * @return true if successful, false on error
 */
bool memory_pool_reset(mem_pool_t* pool);

/**
 * Destroy memory pool and release resources
 * 
 * @param pool Pointer to memory pool
 * @param unlink Whether to unlink shared memory (only for creator)
 * @return true if successful, false on error
 */
bool memory_pool_destroy(mem_pool_t* pool, bool unlink);

#endif