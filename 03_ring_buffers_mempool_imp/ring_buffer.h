#ifndef RINGBUF_H
#define RINGBUF_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * Ring Buffer Structure
 * A generic circular buffer for storing pointers
 * Using flexible array member for the buffer
 */
typedef struct {
    uint32_t capacity;        // Maximum number of elements
    uint32_t head;            // Read index
    uint32_t tail;            // Write index
    uint32_t count;           // Number of elements currently in buffer
    void* buffer[];           // Flexible array member for pointers
} ring_buffer_t;

/**
 * Initialize a ring buffer
 * 
 * @param rb Pointer to ring buffer structure
 * @param capacity Maximum number of elements the buffer can hold
 * @return true on success, false on failure
 */
bool ring_buffer_init(ring_buffer_t* rb, uint32_t capacity);

/**
 * Add an item to the ring buffer
 * 
 * @param rb Pointer to ring buffer
 * @param item Pointer to add to the buffer
 * @return true if successful, false if buffer is full
 */
bool ring_buffer_put(ring_buffer_t* rb, void* item);

/**
 * Remove and return an item from the ring buffer
 * 
 * @param rb Pointer to ring buffer
 * @return Pointer from the buffer, or NULL if buffer is empty
 */
void* ring_buffer_get(ring_buffer_t* rb);

/**
 * Check if ring buffer is empty
 * 
 * @param rb Pointer to ring buffer
 * @return true if empty, false otherwise
 */
bool ring_buffer_is_empty(const ring_buffer_t* rb);
/**
 * Check if ring buffer is full
 * 
 * @param rb Pointer to ring buffer
 * @return true if full, false otherwise
 */
bool ring_buffer_is_full(const ring_buffer_t* rb);

/**
 * Get number of items in ring buffer
 * 
 * @param rb Pointer to ring buffer
 * @return Number of items in buffer
 */
uint32_t ring_buffer_count(const ring_buffer_t* rb);

/**
 * Reset ring buffer to empty state
 * 
 * @param rb Pointer to ring buffer
 */
void ring_buffer_reset(ring_buffer_t* rb);

/**
 * Get memory size required for a ring buffer with given capacity
 *
 * @param capacity Desired capacity of the ring buffer
 * @return Size in bytes needed for the ring buffer structure
 */
size_t ring_buffer_size(uint32_t capacity);

#endif