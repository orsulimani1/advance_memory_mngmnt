// mempool_test.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "mempool_ring.h"

// Test function prototypes
void test_ring_buffer(void);
void test_memory_pool(void);
void test_stress(void);

int main(void) {
    printf("===== RING BUFFER AND MEMORY POOL TESTS =====\n\n");
    
    printf("Testing ring buffer...\n");
    test_ring_buffer();
    printf("Ring buffer tests passed!\n\n");
    
    printf("Testing memory pool...\n");
    test_memory_pool();
    printf("Memory pool tests passed!\n\n");
    
    printf("Running stress test...\n");
    test_stress();
    printf("Stress test passed!\n\n");
    
    printf("All tests passed successfully!\n");
    return 0;
}

// Test basic ring buffer operations
void test_ring_buffer(void) {
    // Calculate size needed and create memory for ring buffer
    const uint32_t capacity = 10;
    size_t rb_size = ring_buffer_size(capacity);
    void* rb_memory = malloc(rb_size);
    assert(rb_memory != NULL);
    
    // Initialize the ring buffer
    ring_buffer_t* rb = (ring_buffer_t*)rb_memory;
    assert(ring_buffer_init(rb, capacity));
    
    // Verify initial state
    assert(ring_buffer_is_empty(rb));
    assert(!ring_buffer_is_full(rb));
    assert(ring_buffer_count(rb) == 0);
    
    // Test adding items
    int values[15];  // Values to store pointers to
    for (int i = 0; i < 10; i++) {
        values[i] = i + 1;
        assert(ring_buffer_put(rb, &values[i]));
    }
    
    // Verify full state
    assert(!ring_buffer_is_empty(rb));
    assert(ring_buffer_is_full(rb));
    assert(ring_buffer_count(rb) == 10);
    
    // Test adding to a full buffer (should fail)
    assert(!ring_buffer_put(rb, &values[10]));
    
    // Test getting items
    for (int i = 0; i < 5; i++) {
        void* item = ring_buffer_get(rb);
        assert(item != NULL);
        assert(*(int*)item == i + 1);
    }
    
    // Verify partial state
    assert(!ring_buffer_is_empty(rb));
    assert(!ring_buffer_is_full(rb));
    assert(ring_buffer_count(rb) == 5);
    
    // Test adding more items (wraparound case)
    for (int i = 0; i < 5; i++) {
        values[i+10] = i + 100;
        assert(ring_buffer_put(rb, &values[i+10]));
    }
    
    // Verify full state again
    assert(ring_buffer_is_full(rb));
    
    // Test getting remaining items (including wraparound)
    for (int i = 0; i < 10; i++) {
        void* item = ring_buffer_get(rb);
        assert(item != NULL);
        // First 5 items should be original 6-10, next 5 should be the new 100-104
        if (i < 5) {
            assert(*(int*)item == i + 6);
        } else {
            assert(*(int*)item == i + 95); // 100-104
        }
    }
    
    // Verify empty state
    assert(ring_buffer_is_empty(rb));
    assert(ring_buffer_get(rb) == NULL);
    
    // Test reset
    for (int i = 0; i < 3; i++) {
        assert(ring_buffer_put(rb, &values[i]));
    }
    assert(ring_buffer_count(rb) == 3);
    
    ring_buffer_reset(rb);
    assert(ring_buffer_is_empty(rb));
    assert(ring_buffer_count(rb) == 0);
    
    free(rb_memory);
}

// Test memory pool operations
void test_memory_pool(void) {
    // Create a memory region
    const size_t memory_size = 4096;
    void* memory = malloc(memory_size);
    assert(memory != NULL);
    
    // Initialize the memory pool
    mem_pool_t pool;
    const uint32_t block_size = 64;
    assert(memory_pool_init(&pool, memory, memory_size, block_size));
    
    // Check initial state
    uint32_t expected_blocks = pool.num_blocks;  // Use the actual number from the pool
    assert(memory_pool_free_count(&pool) == expected_blocks);
    assert(memory_pool_used_count(&pool) == 0);
    
    // Test allocation
    void* blocks[100];  // Should be enough to hold all blocks
    for (uint32_t i = 0; i < expected_blocks; i++) {
        blocks[i] = memory_pool_alloc(&pool);
        assert(blocks[i] != NULL);
        
        // Write a pattern to the block to test it's usable
        memset(blocks[i], i & 0xFF, block_size);
    }
    
    // Pool should now be empty
    assert(memory_pool_free_count(&pool) == 0);
    assert(memory_pool_alloc(&pool) == NULL);
    
    // Test freeing blocks
    for (uint32_t i = 0; i < expected_blocks; i++) {
        assert(memory_pool_free(&pool, blocks[i]));
    }
    
    // Pool should be full again
    assert(memory_pool_free_count(&pool) == expected_blocks);
    
    // Test invalid frees
    assert(!memory_pool_free(&pool, NULL));
    assert(!memory_pool_free(&pool, (void*)0x12345678));  // Invalid address
    
    // Test reset functionality
    uint32_t blocks_to_alloc = expected_blocks / 2;
    for (uint32_t i = 0; i < blocks_to_alloc; i++) {
        blocks[i] = memory_pool_alloc(&pool);
    }
    // Calculate expected free blocks with correct rounding
    uint32_t expected_free = expected_blocks - blocks_to_alloc;
    assert(memory_pool_free_count(&pool) == expected_free);
    assert(memory_pool_reset(&pool));
    assert(memory_pool_free_count(&pool) == expected_blocks);
    
    free(memory);
}

// Simulate a real-world usage pattern
void test_stress(void) {
    // Create a larger memory region
    const size_t memory_size = 1024 * 1024; // 1MB
    void* memory = malloc(memory_size);
    assert(memory != NULL);
    
    // Initialize the memory pool with smaller blocks
    mem_pool_t pool;
    const uint32_t block_size = 32;
    assert(memory_pool_init(&pool, memory, memory_size, block_size));
    
    // Get block count
    uint32_t total_blocks = memory_pool_free_count(&pool);
    printf("Initialized pool with %u blocks of %u bytes each\n", 
           total_blocks, block_size);
    
    // Array to track allocated blocks
    void** blocks = malloc(total_blocks * sizeof(void*));
    assert(blocks != NULL);
    
    // Allocation patterns: allocate and free in different orders
    
    // 1. Allocate 75% of blocks
    uint32_t allocated = 0;
    for (uint32_t i = 0; i < total_blocks * 3 / 4; i++) {
        blocks[allocated] = memory_pool_alloc(&pool);
        assert(blocks[allocated] != NULL);
        
        // Write a pattern
        memset(blocks[allocated], (i & 0xFF) ^ 0xAA, block_size);
        allocated++;
    }
    
    printf("Allocated %u blocks, %u remaining\n", 
           allocated, memory_pool_free_count(&pool));
    
    // 2. Free every third block
    uint32_t freed = 0;
    for (uint32_t i = 0; i < allocated; i += 3) {
        assert(memory_pool_free(&pool, blocks[i]));
        blocks[i] = NULL;
        freed++;
    }
    
    printf("Freed %u blocks, %u remaining allocated, %u free\n", 
           freed, allocated - freed, memory_pool_free_count(&pool));
    
    // 3. Allocate more blocks until full
    while (allocated < total_blocks) {
        blocks[allocated] = memory_pool_alloc(&pool);
        if (blocks[allocated] == NULL) {
            break;
        }
        memset(blocks[allocated], (allocated & 0xFF) ^ 0x55, block_size);
        allocated++;
    }
    
    printf("Re-allocated up to %u blocks, %u remaining\n", 
           allocated, memory_pool_free_count(&pool));
    
    // 4. Free all remaining blocks
    for (uint32_t i = 0; i < allocated; i++) {
        if (blocks[i] != NULL) {
            assert(memory_pool_free(&pool, blocks[i]));
            blocks[i] = NULL;
        }
    }
    
    // Pool should be full again
    assert(memory_pool_free_count(&pool) == total_blocks);
    printf("All blocks freed successfully\n");
    
    free(blocks);
    free(memory);
}