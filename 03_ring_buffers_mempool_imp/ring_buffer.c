#include "ring_buffer.h"
#include <stdlib.h>

/**
 * Get memory size required for a ring buffer with given capacity
 *
 * @param capacity Desired capacity of the ring buffer
 * @return Size in bytes needed for the ring buffer structure
 */
size_t ring_buffer_size(uint32_t capacity) {
    return sizeof(ring_buffer_t) + (capacity * sizeof(void*));
}

/**
 * Initialize a ring buffer
 * 
 * @param rb Pointer to ring buffer structure
 * @param capacity Maximum number of elements the buffer can hold
 * @return true on success, false on failure
 */
bool ring_buffer_init(ring_buffer_t* rb, uint32_t capacity) {
    // Check for null pointers
    if (rb == NULL || capacity == 0) {
        return false;
    }
    
    // Initialize buffer structure
    rb->capacity = capacity;
    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;
    
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
    // Check for null pointers or full buffer
    if (rb == NULL || rb->count >= rb->capacity) {
        return false;  // Buffer is full
    }
    
    // Assign item
    rb->buffer[rb->tail] = item;
    
    // Advance tail
    rb->tail = (rb->tail + 1) % rb->capacity;
    rb->count++;
    
    return true;
}

/**
 * Remove and return an item from the ring buffer
 * 
 * @param rb Pointer to ring buffer
 * @return Pointer from the buffer, or NULL if buffer is empty
 */
void* ring_buffer_get(ring_buffer_t* rb) {
    // Check for null pointers or empty buffer
    if (rb == NULL || rb->count == 0) {
        return NULL;  // Buffer is empty
    }
    
    // Get item
    void* item = rb->buffer[rb->head];
    
    // Advance head
    rb->head = (rb->head + 1) % rb->capacity;
    rb->count--;
    
    return item;
}

/**
 * Check if ring buffer is empty
 * 
 * @param rb Pointer to ring buffer
 * @return true if empty, false otherwise
 */
bool ring_buffer_is_empty(const ring_buffer_t* rb) {
    return (rb == NULL || rb->count == 0);
}

/**
 * Check if ring buffer is full
 * 
 * @param rb Pointer to ring buffer
 * @return true if full, false otherwise
 */
bool ring_buffer_is_full(const ring_buffer_t* rb) {
    return (rb == NULL || rb->count >= rb->capacity);
}

/**
 * Get number of items in ring buffer
 * 
 * @param rb Pointer to ring buffer
 * @return Number of items in buffer
 */
uint32_t ring_buffer_count(const ring_buffer_t* rb) {
    return (rb == NULL) ? 0 : rb->count;
}

/**
 * Reset ring buffer to empty state
 * 
 * @param rb Pointer to ring buffer
 */
void ring_buffer_reset(ring_buffer_t* rb) {
    if (rb != NULL) {
        rb->head = 0;
        rb->tail = 0;
        rb->count = 0;
    }
}