// tester.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/wait.h>
#include "mempool_ring.h"

#define NUM_THREADS 4
#define OPERATIONS_PER_THREAD 1000
#define SHM_NAME "/mempool_test"
#define SHM_SIZE (1024 * 1024)  // 1MB
#define BLOCK_SIZE 32

// Struct for thread worker function arguments
typedef struct {
    ring_buffer_t* rb;
    int thread_id;
    int* values;
} thread_args_t;

// Thread worker function prototypes
void* producer_thread(void* arg);
void* consumer_thread(void* arg);

// Test function prototypes
void test_ring_buffer(void);
void test_memory_pool(void);
void test_mpmc_ring_buffer(void);
void test_shared_memory_pool(void);

int main(void) {
    printf("===== RING BUFFER AND MEMORY POOL TESTS =====\n\n");
    
    printf("Testing basic ring buffer...\n");
    test_ring_buffer();
    printf("Basic ring buffer tests passed!\n\n");
    
    printf("Testing memory pool...\n");
    test_memory_pool();
    printf("Memory pool tests passed!\n\n");
    
    printf("Testing MPMC ring buffer...\n");
    test_mpmc_ring_buffer();
    printf("MPMC ring buffer tests passed!\n\n");
    
    printf("Testing shared memory pool...\n");
    test_shared_memory_pool();
    printf("Shared memory pool tests passed!\n\n");
    
    printf("All tests passed successfully!\n");
    return 0;
}

// Test basic ring buffer operations
void test_ring_buffer(void) {
    // Create a buffer for 10 pointers
    void* items[10];
    ring_buffer_t rb;
    
    // Initialize the ring buffer
    assert(ring_buffer_init(&rb, items, 10));
    
    // Verify initial state
    assert(ring_buffer_is_empty(&rb));
    assert(!ring_buffer_is_full(&rb));
    assert(ring_buffer_count(&rb) == 0);
    
    // Test adding items
    int values[15];  // Values to store pointers to
    for (int i = 0; i < 10; i++) {
        values[i] = i + 1;
        assert(ring_buffer_put(&rb, &values[i]));
    }
    
    // Verify full state
    assert(!ring_buffer_is_empty(&rb));
    assert(ring_buffer_is_full(&rb));
    assert(ring_buffer_count(&rb) == 10);
    
    // Test adding to a full buffer (should fail)
    assert(!ring_buffer_put(&rb, &values[10]));
    
    // Test getting items
    for (int i = 0; i < 5; i++) {
        void* item = ring_buffer_get(&rb);
        assert(item != NULL);
        assert(*(int*)item == i + 1);
    }
    
    // Verify partial state
    assert(!ring_buffer_is_empty(&rb));
    assert(!ring_buffer_is_full(&rb));
    assert(ring_buffer_count(&rb) == 5);
    
    // Test adding more items (wraparound case)
    for (int i = 0; i < 5; i++) {
        values[i+10] = i + 100;
        assert(ring_buffer_put(&rb, &values[i+10]));
    }
    
    // Verify full state again
    assert(ring_buffer_is_full(&rb));
    
    // Test getting remaining items (including wraparound)
    for (int i = 0; i < 10; i++) {
        void* item = ring_buffer_get(&rb);
        assert(item != NULL);
        // First 5 items should be original 6-10, next 5 should be the new 100-104
        if (i < 5) {
            assert(*(int*)item == i + 6);
        } else {
            assert(*(int*)item == i + 95); // 100-104
        }
    }
    
    // Verify empty state
    assert(ring_buffer_is_empty(&rb));
    assert(ring_buffer_get(&rb) == NULL);
    
    // Test reset
    for (int i = 0; i < 3; i++) {
        assert(ring_buffer_put(&rb, &values[i]));
    }
    assert(ring_buffer_count(&rb) == 3);
    
    ring_buffer_reset(&rb);
    assert(ring_buffer_is_empty(&rb));
    assert(ring_buffer_count(&rb) == 0);
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
    uint32_t overhead_size = sizeof(ring_buffer_t);
    uint32_t potential_blocks = (memory_size - overhead_size) / block_size;
    uint32_t array_size = potential_blocks * sizeof(void*);
    uint32_t total_overhead = overhead_size + array_size;
    uint32_t expected_blocks = (memory_size - total_overhead) / block_size;
    
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
    for (uint32_t i = 0; i < expected_blocks / 2; i++) {
        blocks[i] = memory_pool_alloc(&pool);
    }
    assert(memory_pool_free_count(&pool) == expected_blocks / 2);
    
    assert(memory_pool_reset(&pool));
    assert(memory_pool_free_count(&pool) == expected_blocks);
    
    // Clean up
    memory_pool_destroy(&pool, false);
    free(memory);
}

// Test multi-producer multi-consumer ring buffer
void test_mpmc_ring_buffer(void) {
    printf("Starting MPMC ring buffer test with %d threads...\n", NUM_THREADS * 2);
    
    // Create a buffer for the ring buffer
    const int capacity = OPERATIONS_PER_THREAD * NUM_THREADS;
    void** items = malloc(capacity * sizeof(void*));
    assert(items != NULL);
    
    // Create ring buffer
    ring_buffer_t rb;
    assert(ring_buffer_init(&rb, items, capacity));
    
    // Create thread arguments
    thread_args_t producer_args[NUM_THREADS];
    thread_args_t consumer_args[NUM_THREADS];
    
    // Create value arrays for producers
    int* values = malloc(OPERATIONS_PER_THREAD * NUM_THREADS * sizeof(int));
    assert(values != NULL);
    
    // Initialize values
    for (int i = 0; i < OPERATIONS_PER_THREAD * NUM_THREADS; i++) {
        values[i] = i + 1;
    }
    
    // Create threads
    pthread_t producer_threads[NUM_THREADS];
    pthread_t consumer_threads[NUM_THREADS];
    
    // Initialize thread arguments
    for (int i = 0; i < NUM_THREADS; i++) {
        producer_args[i].rb = &rb;
        producer_args[i].thread_id = i;
        producer_args[i].values = values + (i * OPERATIONS_PER_THREAD);
        
        consumer_args[i].rb = &rb;
        consumer_args[i].thread_id = i;
        consumer_args[i].values = NULL;
    }
    
    // Start consumer threads first
    for (int i = 0; i < NUM_THREADS; i++) {
        assert(pthread_create(&consumer_threads[i], NULL, consumer_thread, &consumer_args[i]) == 0);
    }
    
    // Start producer threads
    for (int i = 0; i < NUM_THREADS; i++) {
        assert(pthread_create(&producer_threads[i], NULL, producer_thread, &producer_args[i]) == 0);
    }
    
    // Wait for producer threads to complete
    for (int i = 0; i < NUM_THREADS; i++) {
        assert(pthread_join(producer_threads[i], NULL) == 0);
    }
    
    printf("All producer threads completed\n");
    
    // Wait for consumer threads to complete
    for (int i = 0; i < NUM_THREADS; i++) {
        assert(pthread_join(consumer_threads[i], NULL) == 0);
    }
    
    printf("All consumer threads completed\n");
    
    // Verify ring buffer is empty
    assert(ring_buffer_is_empty(&rb));
    
    // Clean up
    free(items);
    free(values);
}

// Producer thread function
void* producer_thread(void* arg) {
    thread_args_t* args = (thread_args_t*)arg;
    
    printf("Producer %d starting\n", args->thread_id);
    
    // Add items to the ring buffer
    for (int i = 0; i < OPERATIONS_PER_THREAD; i++) {
        int* value = &args->values[i];
        
        // Try until successful
        while (!ring_buffer_put(args->rb, value)) {
            // Small delay if buffer is full
            usleep(1);
        }
        
        // Occasional status update
        if (i % (OPERATIONS_PER_THREAD / 10) == 0) {
            printf("Producer %d: %d%%\n", args->thread_id, i * 100 / OPERATIONS_PER_THREAD);
        }
    }
    
    printf("Producer %d completed\n", args->thread_id);
    return NULL;
}

// Consumer thread function
void* consumer_thread(void* arg) {
    thread_args_t* args = (thread_args_t*)arg;
    
    printf("Consumer %d starting\n", args->thread_id);
    
    int items_consumed = 0;
    int consecutive_empty = 0;
    const int max_consecutive_empty = 1000;
    
    // Consume items until all producers have finished
    while (items_consumed < OPERATIONS_PER_THREAD || consecutive_empty < max_consecutive_empty) {
        void* item = ring_buffer_get(args->rb);
        
        if (item != NULL) {
            // Successfully got an item
            consecutive_empty = 0;
            items_consumed++;
            
            // Use the item (just a read to verify it's valid)
            int value = *(int*)item;
            assert(value > 0);
            
            // Occasional status update
            if (items_consumed % (OPERATIONS_PER_THREAD / 10) == 0) {
                printf("Consumer %d: %d items\n", args->thread_id, items_consumed);
            }
        } else {
            // Buffer is empty, wait a bit
            consecutive_empty++;
            usleep(1);
        }
    }
    
    printf("Consumer %d completed, consumed %d items\n", args->thread_id, items_consumed);
    return NULL;
}

// Test shared memory pool
void test_shared_memory_pool(void) {
    printf("Testing shared memory pool using fork...\n");
    
    // Clean up any previous shared memory
    shm_unlink(SHM_NAME);
    
    pid_t pid = fork();
    assert(pid >= 0);
    
    if (pid == 0) {
        // Child process - create the shared memory pool
        printf("Child: Creating shared memory pool\n");
        
        mem_pool_t pool;
        assert(memory_pool_init_shared(&pool, SHM_NAME, SHM_SIZE, BLOCK_SIZE, true, 0666));
        
        printf("Child: Pool created with %u blocks\n", pool.num_blocks);
        
        // Allocate half the blocks
        uint32_t half_blocks = pool.num_blocks / 2;
        void** blocks = malloc(half_blocks * sizeof(void*));
        assert(blocks != NULL);
        
        printf("Child: Allocating %u blocks\n", half_blocks);
        for (uint32_t i = 0; i < half_blocks; i++) {
            blocks[i] = memory_pool_alloc(&pool);
            assert(blocks[i] != NULL);
            
            // Write a pattern to the block
            memset(blocks[i], i & 0xFF, BLOCK_SIZE);
        }
        
        printf("Child: Blocks allocated, remaining: %u\n", memory_pool_free_count(&pool));
        
        // Signal to parent that it can proceed
        printf("Child: Waiting for parent to attach...\n");
        sleep(2);
        
        // Free blocks in child
        printf("Child: Freeing blocks\n");
        for (uint32_t i = 0; i < half_blocks; i++) {
            assert(memory_pool_free(&pool, blocks[i]));
        }
        
        printf("Child: Blocks freed, free count: %u\n", memory_pool_free_count(&pool));
        
        // Clean up
        free(blocks);
        memory_pool_destroy(&pool, false);
        
        printf("Child: Exiting\n");
        exit(0);
    } else {
        // Parent process - attach to the shared memory pool
        printf("Parent: Waiting for child to create pool\n");
        sleep(1);  // Give child time to create the pool
        
        printf("Parent: Attaching to shared memory pool\n");
        
        mem_pool_t pool;
        assert(memory_pool_init_shared(&pool, SHM_NAME, SHM_SIZE, BLOCK_SIZE, false, 0));
        
        printf("Parent: Attached to pool with %u blocks\n", pool.num_blocks);
        printf("Parent: Free blocks: %u\n", memory_pool_free_count(&pool));
        
        // Allocate remaining blocks
        uint32_t free_blocks = memory_pool_free_count(&pool);
        void** blocks = malloc(free_blocks * sizeof(void*));
        assert(blocks != NULL);
        
        printf("Parent: Allocating %u blocks\n", free_blocks);
        for (uint32_t i = 0; i < free_blocks; i++) {
            blocks[i] = memory_pool_alloc(&pool);
            assert(blocks[i] != NULL);
            
            // Write a pattern to the block
            memset(blocks[i], (i + 128) & 0xFF, BLOCK_SIZE);
        }
        
        printf("Parent: Blocks allocated, remaining: %u\n", memory_pool_free_count(&pool));
        
        // Pool should now be empty
        assert(memory_pool_free_count(&pool) == 0);
        
        // Wait for child to free its blocks
        printf("Parent: Waiting for child to free blocks\n");
        sleep(3);
        
        printf("Parent: Child has freed blocks, free count: %u\n", memory_pool_free_count(&pool));
        
        // Free parent's blocks too
        printf("Parent: Freeing blocks\n");
        for (uint32_t i = 0; i < free_blocks; i++) {
            assert(memory_pool_free(&pool, blocks[i]));
        }
        
        printf("Parent: All blocks freed, free count: %u\n", memory_pool_free_count(&pool));
        
        // Clean up
        free(blocks);
        memory_pool_destroy(&pool, true);  // Unlink shared memory
        
        // Wait for child to exit
        int status;
        waitpid(pid, &status, 0);
        assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
        
        printf("Parent: Child exited successfully\n");
    }
}