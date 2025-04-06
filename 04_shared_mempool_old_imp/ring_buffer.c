#include "ring_buffer.h"
#include <time.h>      // For nanosleep in spinlock

// Helper function for spinlock with backoff
static void spinlock_acquire(atomic_uint* lock) {
    uint32_t backoff = 1;
    const uint32_t max_backoff = 1000;
    
    while (atomic_exchange(lock, 1) != 0) {
        // Use exponential backoff to reduce contention
        struct timespec ts = {0, backoff * 100};  // Nanoseconds
        nanosleep(&ts, NULL);
        
        // Increase backoff time (capped at max_backoff)
        if (backoff < max_backoff)
            backoff *= 2;
    }
}

static void spinlock_release(atomic_uint* lock) {
    atomic_store(lock, 0);
}

/**
 * Initialize a ring buffer
 * 
 * @param rb Pointer to ring buffer structure
 * @param buffer Array to use for storing pointers
 * @param capacity Maximum number of elements the buffer can hold
 * @return true on success, false on failure
 */
bool ring_buffer_init(ring_buffer_t* rb, void** buffer, uint32_t capacity) {
    // Check for null pointers
    if (rb == NULL || buffer == NULL || capacity == 0) {
        return false;
    }
    // Initialize buffer structure
    rb->buffer = buffer;
    rb->capacity = capacity;
    atomic_store(&rb->head, 0);
    atomic_store(&rb->tail, 0);
    atomic_store(&rb->count, 0);
    atomic_store(&rb->producer_lock, 0);
    atomic_store(&rb->consumer_lock, 0);
    
    return true;
}

/**
 * Add an item to the ring buffer
 * 
 * @param rb Pointer to ring buffer
 * @param item Pointer to add to the buffer
 * @return true if successful, false if buffer is full
 */
bool ring_buffer_put(ring_buffer_t* rb, void* item) {
    // Check for null pointers or full buffer without locking
    if (rb == NULL || atomic_load(&rb->count) >= rb->capacity) {
        return false;  // Buffer is full
    }
    
    // Acquire the producer lock
    spinlock_acquire(&rb->producer_lock);
    
    // Check again now that we have the lock
    bool success = false;
    if (atomic_load(&rb->count) < rb->capacity) {
        // Get current tail position
        uint32_t tail = atomic_load(&rb->tail);
        
        // Add the item
        rb->buffer[tail] = item;
        
        // Update tail position
        atomic_store(&rb->tail, (tail + 1) % rb->capacity);
        
        // Increment count atomically
        atomic_fetch_add(&rb->count, 1);
        
        success = true;
    }
    
    // Release the producer lock
    spinlock_release(&rb->producer_lock);
    
    return success;
}

/**
 * Remove and return an item from the ring buffer
 * 
 * @param rb Pointer to ring buffer
 * @return Pointer from the buffer, or NULL if buffer is empty
 */
void* ring_buffer_get(ring_buffer_t* rb) {
    // Check for null pointers or empty buffer without locking
    if (rb == NULL || atomic_load(&rb->count) == 0) {
        return NULL;  // Buffer is empty
    }
    
    // Acquire the consumer lock
    spinlock_acquire(&rb->consumer_lock);
    
    // Check again now that we have the lock
    void* item = NULL;
    if (atomic_load(&rb->count) > 0) {
        // Get current head position
        uint32_t head = atomic_load(&rb->head);
        
        // Get the item
        item = rb->buffer[head];
        
        // Update head position
        atomic_store(&rb->head, (head + 1) % rb->capacity);
        
        // Decrement count atomically
        atomic_fetch_sub(&rb->count, 1);
    }
    
    // Release the consumer lock
    spinlock_release(&rb->consumer_lock);
    
    return item;
}

/**
 * Check if ring buffer is empty
 * 
 * @param rb Pointer to ring buffer
 * @return true if empty, false otherwise
 */
bool ring_buffer_is_empty(const ring_buffer_t* rb) {
    return (rb == NULL || atomic_load(&rb->count) == 0);
}

/**
 * Check if ring buffer is full
 * 
 * @param rb Pointer to ring buffer
 * @return true if full, false otherwise
 */
bool ring_buffer_is_full(const ring_buffer_t* rb) {
    return (rb == NULL || atomic_load(&rb->count) >= rb->capacity);
}

/**
 * Get number of items in ring buffer
 * 
 * @param rb Pointer to ring buffer
 * @return Number of items in buffer
 */
uint32_t ring_buffer_count(const ring_buffer_t* rb) {
    return (rb == NULL) ? 0 : atomic_load(&rb->count);
}

/**
 * Reset ring buffer to empty state
 * 
 * @param rb Pointer to ring buffer
 */
void ring_buffer_reset(ring_buffer_t* rb) {
    if (rb != NULL) {
        // Acquire both locks for reset
        spinlock_acquire(&rb->producer_lock);
        spinlock_acquire(&rb->consumer_lock);
        
        atomic_store(&rb->head, 0);
        atomic_store(&rb->tail, 0);
        atomic_store(&rb->count, 0);
        
        // Release locks
        spinlock_release(&rb->consumer_lock);
        spinlock_release(&rb->producer_lock);
    }
}