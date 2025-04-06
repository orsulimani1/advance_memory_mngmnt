
#include "ring_buffer.h"
#define NEED_IMP 0XDEADDEAD

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
    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;
    
    return true;
}

/**
 * Check if ring buffer is full
 * 
 * @param rb Pointer to ring buffer
 * @return true if full, false otherwise
 */
bool ring_buffer_is_full(const ring_buffer_t* rb) {
    return rb->count == rb->capacity;
}


/**
 * Add an item to the ring buffer
 * 
 * @param rb Pointer to ring buffer
 * @param item Pointer to add to the buffer
 * @return true if successful, false if buffer is full
 */
bool ring_buffer_put(ring_buffer_t* rb, void* item) {
    // Check for null pointers
    if (rb == NULL || ring_buffer_is_full(rb)) {
        return false;
    }
    // assign item
    rb->buffer[rb->tail] = item;
    // advance tail
    rb->tail = (rb->tail + 1) % rb->capacity;
    rb->count++;
    return true;
}
/**
 * Check if ring buffer is empty
 * 
 * @param rb Pointer to ring buffer
 * @return true if empty, false otherwise
 */
bool ring_buffer_is_empty(const ring_buffer_t* rb) {
    return rb->count == 0;
}

/**
 * Remove and return an item from the ring buffer
 * 
 * @param rb Pointer to ring buffer
 * @return Pointer from the buffer, or NULL if buffer is empty
 */
void* ring_buffer_get(ring_buffer_t* rb) {
    // Check for null pointers
    if (rb == NULL || ring_buffer_is_empty(false)) {
        return false;
    }
    // get item
    void * item = rb->buffer[rb->head];
    // advance head
    rb->head = (rb->head + 1) % rb->capacity;
    rb->count--;
    return item;
}


/**
 * Get number of items in ring buffer
 * 
 * @param rb Pointer to ring buffer
 * @return Number of items in buffer
 */
uint32_t ring_buffer_count(const ring_buffer_t* rb) {
    return rb->count;
}

/**
 * Reset ring buffer to empty state
 * 
 * @param rb Pointer to ring buffer
 */
void ring_buffer_reset(ring_buffer_t* rb) {
    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;
}