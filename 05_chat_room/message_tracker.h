#ifndef MESSAGE_TRACKER_H
#define MESSAGE_TRACKER_H

#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include "mempool_ring.h"

// Maximum number of tracked messages
#define MAX_TRACKED_MESSAGES 100

// Message tracking structure
typedef struct {
    void* block_ptr;             // Pointer to memory block
    atomic_uint ref_count;       // Reference count for this message
    atomic_uint participants_mask;  // Bitmask of participants who have seen the message
    uint32_t timestamp;          // Message timestamp
} tracked_message_t;

// Message tracker
typedef struct {
    tracked_message_t messages[MAX_TRACKED_MESSAGES];
    atomic_uint count;           // Number of active tracked messages
    atomic_uint next_index;      // Next index to use
    atomic_uint tracker_lock;    // Lock for the tracker
} message_tracker_t;

// Initialize the message tracker
bool tracker_init(message_tracker_t* tracker);

// Track a new message
bool tracker_add_message(message_tracker_t* tracker, void* block, uint32_t active_mask);

// Mark a message as read by a participant
bool tracker_mark_read(message_tracker_t* tracker, int message_index, int participant_id);

// Check if a participant has read a message
bool tracker_has_read(message_tracker_t* tracker, int message_index, int participant_id);

// Get next unread message for a participant
int tracker_get_next_unread(message_tracker_t* tracker, int participant_id);

// Get the message block for a tracked message
void* tracker_get_message(message_tracker_t* tracker, int message_index);

// Free a message if all participants have read it
bool tracker_try_free_message(message_tracker_t* tracker, int message_index, mem_pool_t* pool);

// Reset the tracker
void tracker_reset(message_tracker_t* tracker);

#endif // MESSAGE_TRACKER_H