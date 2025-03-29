#include "mempool_ring.h"
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>           // For O_* constants
#include <sys/mman.h>         // For shm_open, mmap
#include <unistd.h>           // For ftruncate
#include <sys/stat.h>         // For mode constants

/**
 * Initialize a memory pool
 * 
 * @param pool Pointer to memory pool structure
 * @param memory Pointer to memory region to use
 * @param memory_size Size of memory region in bytes
 * @param block_size Size of each block in bytes
 * @return true on success, false on failure
 */
bool memory_pool_init(mem_pool_t* pool, void* memory, uint32_t memory_size, uint32_t block_size) {
    if (pool == NULL || memory == NULL) {
        return false;
    }
    
    // Ensure block size is reasonable
    if (block_size < sizeof(void*) || memory_size < block_size) {
        return false;
    }
    
    // Reserve space at the beginning of memory for:
    // 1. Ring buffer structure
    // 2. Array of block pointers for the ring buffer
    
    uint32_t rb_struct_size = sizeof(ring_buffer_t);
    uint32_t potential_blocks = (memory_size - rb_struct_size) / block_size;
    uint32_t array_size = potential_blocks * sizeof(void*);
    
    // Total overhead size
    uint32_t overhead_size = rb_struct_size + array_size;
    
    // Check if we have enough memory after overhead
    if (memory_size <= overhead_size + block_size) {
        return false;  // Not enough memory for even one block
    }
    
    // Calculate how many blocks we can actually fit
    uint32_t actual_blocks = (memory_size - overhead_size) / block_size;
    
    // Set up memory layout
    uint8_t* mem_ptr = (uint8_t*)memory;
    
    // 1. Ring buffer structure at the beginning
    ring_buffer_t* rb = (ring_buffer_t*)mem_ptr;
    mem_ptr += rb_struct_size;
    
    // 2. Array of block pointers
    void** block_array = (void**)mem_ptr;
    mem_ptr += actual_blocks * sizeof(void*);
    
    // 3. Actual memory blocks start here
    void* blocks_start = mem_ptr;
    
    // Initialize the pool structure
    pool->pool_start = blocks_start;
    pool->total_size = memory_size;
    pool->block_size = block_size;
    pool->num_blocks = actual_blocks;
    pool->free_blocks = rb;
    pool->block_array = block_array;
    pool->shm_id = -1;        // Not using shared memory
    pool->shm_name = NULL;    // No shared memory name
    
    // Initialize the ring buffer
    if (!ring_buffer_init(rb, block_array, actual_blocks)) {
        return false;
    }
    
    // Add all blocks to the ring buffer
    for (uint32_t i = 0; i < actual_blocks; i++) {
        void* block = (uint8_t*)blocks_start + (i * block_size);
        ring_buffer_put(rb, block);
    }
    
    return true;
}

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
                           uint32_t block_size, bool create, mode_t mode) {
    if (pool == NULL || shm_name == NULL) {
        return false;
    }
    
    // Ensure block size is reasonable
    if (block_size < sizeof(void*) || memory_size < block_size) {
        return false;
    }
    
    // Save the shared memory name
    pool->shm_name = strdup(shm_name);
    if (pool->shm_name == NULL) {
        return false;
    }
    
    // Open or create shared memory
    int flags = O_RDWR;
    if (create) {
        flags |= O_CREAT | O_EXCL;
    }
    
    int shm_fd = shm_open(shm_name, flags, mode);
    if (shm_fd == -1) {
        free(pool->shm_name);
        pool->shm_name = NULL;
        return false;
    }
    
    // Set the size of the shared memory object (only if creating)
    if (create && ftruncate(shm_fd, memory_size) == -1) {
        close(shm_fd);
        shm_unlink(shm_name);
        free(pool->shm_name);
        pool->shm_name = NULL;
        return false;
    }
    
    // Map the shared memory object
    void* memory = mmap(NULL, memory_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (memory == MAP_FAILED) {
        close(shm_fd);
        if (create) {
            shm_unlink(shm_name);
        }
        free(pool->shm_name);
        pool->shm_name = NULL;
        return false;
    }
    
    // Save the file descriptor
    pool->shm_id = shm_fd;
    
    // If creating, initialize the memory pool
    if (create) {
        if (!memory_pool_init(pool, memory, memory_size, block_size)) {
            munmap(memory, memory_size);
            close(shm_fd);
            shm_unlink(shm_name);
            free(pool->shm_name);
            pool->shm_name = NULL;
            return false;
        }
    } else {
        // If attaching, just set up the pointers
        
        // Calculate the same memory layout as in memory_pool_init
        uint32_t rb_struct_size = sizeof(ring_buffer_t);
        uint32_t potential_blocks = (memory_size - rb_struct_size) / block_size;
        uint32_t array_size = potential_blocks * sizeof(void*);
        uint32_t overhead_size = rb_struct_size + array_size;
        uint32_t actual_blocks = (memory_size - overhead_size) / block_size;
        
        uint8_t* mem_ptr = (uint8_t*)memory;
        
        // Get pointers from shared memory
        ring_buffer_t* rb = (ring_buffer_t*)mem_ptr;
        mem_ptr += rb_struct_size;
        
        void** block_array = (void**)mem_ptr;
        mem_ptr += actual_blocks * sizeof(void*);
        
        void* blocks_start = mem_ptr;
        
        // Set up pool structure
        pool->pool_start = blocks_start;
        pool->total_size = memory_size;
        pool->block_size = block_size;
        pool->num_blocks = actual_blocks;
        pool->free_blocks = rb;
        pool->block_array = block_array;
    }
    
    // Close the file descriptor (the mapping remains valid)
    close(shm_fd);
    
    return true;
}

/**
 * Allocate a memory block from the pool
 * 
 * @param pool Pointer to memory pool
 * @return Pointer to allocated block, or NULL if none available
 */
void* memory_pool_alloc(mem_pool_t* pool) {
    if (pool == NULL || pool->free_blocks == NULL) {
        return NULL;
    }
    
    // Get a block from the ring buffer
    return ring_buffer_get(pool->free_blocks);
}

/**
 * Return a memory block to the pool
 * 
 * @param pool Pointer to memory pool
 * @param block Pointer to block being returned
 * @return true if successful, false on error
 */
bool memory_pool_free(mem_pool_t* pool, void* block) {
    if (pool == NULL || pool->free_blocks == NULL || block == NULL) {
        return false;
    }
    
    // Validate block is within our pool
    if (block < pool->pool_start || 
        block >= (void*)((uint8_t*)pool->pool_start + (pool->num_blocks * pool->block_size))) {
        return false;
    }
    
    // Validate block alignment
    uint32_t offset = (uint8_t*)block - (uint8_t*)pool->pool_start;
    if (offset % pool->block_size != 0) {
        return false;
    }
    
    // Add block back to the ring buffer
    return ring_buffer_put(pool->free_blocks, block);
}

/**
 * Get number of free blocks in the pool
 * 
 * @param pool Pointer to memory pool
 * @return Number of free blocks
 */
uint32_t memory_pool_free_count(mem_pool_t* pool) {
    if (pool == NULL || pool->free_blocks == NULL) {
        return 0;
    }
    
    return ring_buffer_count(pool->free_blocks);
}

/**
 * Get number of allocated blocks in the pool
 * 
 * @param pool Pointer to memory pool
 * @return Number of allocated blocks
 */
uint32_t memory_pool_used_count(mem_pool_t* pool) {
    if (pool == NULL) {
        return 0;
    }
    
    return pool->num_blocks - memory_pool_free_count(pool);
}

/**
 * Reset memory pool to initial state
 * 
 * @param pool Pointer to memory pool
 * @return true if successful, false on error
 */
bool memory_pool_reset(mem_pool_t* pool) {
    if (pool == NULL || pool->free_blocks == NULL) {
        return false;
    }
    
    // Reset the ring buffer
    ring_buffer_reset(pool->free_blocks);
    
    // Add all blocks back to the ring buffer
    for (uint32_t i = 0; i < pool->num_blocks; i++) {
        void* block = (uint8_t*)pool->pool_start + (i * pool->block_size);
        if (!ring_buffer_put(pool->free_blocks, block)) {
            return false;  // Ring buffer is full (shouldn't happen)
        }
    }
    
    return true;
}

/**
 * Destroy memory pool and release resources
 * 
 * @param pool Pointer to memory pool
 * @param unlink Whether to unlink shared memory (only for creator)
 * @return true if successful, false on error
 */
bool memory_pool_destroy(mem_pool_t* pool, bool unlink) {
    if (pool == NULL) {
        return false;
    }
    
    bool success = true;
    
    // If using shared memory
    if (pool->shm_name != NULL) {
        // Unmap the shared memory
        if (munmap(pool->free_blocks, pool->total_size) != 0) {
            success = false;
        }
        
        // Unlink the shared memory if requested
        if (unlink && shm_unlink(pool->shm_name) != 0) {
            success = false;
        }
        
        // Free the name
        free(pool->shm_name);
        pool->shm_name = NULL;
    }
    
    // Reset the pool structure
    pool->pool_start = NULL;
    pool->free_blocks = NULL;
    pool->block_array = NULL;
    pool->shm_id = -1;
    
    return success;
}