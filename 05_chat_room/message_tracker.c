#include "message_tracker.h"
#include <string.h>
#include <time.h>

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

// Initialize the message tracker
bool tracker_init(message_tracker_t* tracker) {
    if (tracker == NULL) {
        return false;
    }
    
    // Clear the entire structure
    memset(tracker, 0, sizeof(message_tracker_t));
    
    // Initialize atomic fields
    atomic_store(&tracker->count, 0);
    atomic_store(&tracker->next_index, 0);
    atomic_store(&tracker->tracker_lock, 0);
    
    // Initialize reference counts for all messages
    for (int i = 0; i < MAX_TRACKED_MESSAGES; i++) {
        atomic_store(&tracker->messages[i].ref_count, 0);
        atomic_store(&tracker->messages[i].participants_mask, 0);
        tracker->messages[i].block_ptr = NULL;
        tracker->messages[i].timestamp = 0;
    }
    
    return true;
}

// Track a new message
bool tracker_add_message(message_tracker_t* tracker, void* block, uint32_t active_mask) {
    if (tracker == NULL || block == NULL) {
        return false;
    }
    
    // Acquire the tracker lock
    spinlock_acquire(&tracker->tracker_lock);
    
    // Check if tracker is full
    if (atomic_load(&tracker->count) >= MAX_TRACKED_MESSAGES) {
        spinlock_release(&tracker->tracker_lock);
        return false;
    }
    
    // Find next available slot
    uint32_t index = atomic_load(&tracker->next_index);
    uint32_t original_index = index;
    
    do {
        // Check if slot is available
        if (tracker->messages[index].block_ptr == NULL) {
            // Found an empty slot, use it
            tracker->messages[index].block_ptr = block;
            tracker->messages[index].timestamp = (uint32_t)time(NULL);
            atomic_store(&tracker->messages[index].ref_count, __builtin_popcount(active_mask));
            atomic_store(&tracker->messages[index].participants_mask, active_mask);
            
            // Increment count
            atomic_fetch_add(&tracker->count, 1);
            
            // Update next index
            atomic_store(&tracker->next_index, (index + 1) % MAX_TRACKED_MESSAGES);
            
            spinlock_release(&tracker->tracker_lock);
            return true;
        }
        
        // Move to next slot
        index = (index + 1) % MAX_TRACKED_MESSAGES;
    } while (index != original_index); // Wrapped around
    
    // No available slots
    spinlock_release(&tracker->tracker_lock);
    return false;
}

// Mark a message as read by a participant
bool tracker_mark_read(message_tracker_t* tracker, int message_index, int participant_id) {
    if (tracker == NULL || message_index < 0 || message_index >= MAX_TRACKED_MESSAGES ||
        participant_id < 0 || participant_id >= 32) {
        return false;
    }
    
    // Check if message exists
    void* block = tracker->messages[message_index].block_ptr;
    if (block == NULL) {
        return false;
    }
    
    // Calculate participant mask bit
    uint32_t participant_bit = 1 << participant_id;
    
    // Check if already marked as read
    uint32_t current_mask = atomic_load(&tracker->messages[message_index].participants_mask);
    if ((current_mask & participant_bit) == 0) {
        // Already marked as read
        return true;
    }
    
    // Update mask to remove participant bit
    uint32_t new_mask = current_mask & ~participant_bit;
    atomic_store(&tracker->messages[message_index].participants_mask, new_mask);
    
    // Decrement reference count
    uint32_t old_count = atomic_fetch_sub(&tracker->messages[message_index].ref_count, 1);
    
    return true;
}

// Check if a participant has read a message
bool tracker_has_read(message_tracker_t* tracker, int message_index, int participant_id) {
    if (tracker == NULL || message_index < 0 || message_index >= MAX_TRACKED_MESSAGES ||
        participant_id < 0 || participant_id >= 32) {
        return true; // Default to "has read" for invalid parameters
    }
    
    // Check if message exists
    void* block = tracker->messages[message_index].block_ptr;
    if (block == NULL) {
        return true; // Message doesn't exist, so consider it read
    }
    
    // Calculate participant mask bit
    uint32_t participant_bit = 1 << participant_id;
    
    // Check if bit is set in mask (1 means not read yet)
    uint32_t current_mask = atomic_load(&tracker->messages[message_index].participants_mask);
    return (current_mask & participant_bit) == 0;
}

// Get next unread message for a participant
int tracker_get_next_unread(message_tracker_t* tracker, int participant_id) {
    if (tracker == NULL || participant_id < 0 || participant_id >= 32) {
        return -1;
    }
    
    // Calculate participant mask bit
    uint32_t participant_bit = 1 << participant_id;
    
    // Find oldest unread message
    uint32_t oldest_timestamp = UINT32_MAX;
    int oldest_index = -1;
    
    for (int i = 0; i < MAX_TRACKED_MESSAGES; i++) {
        void* block = tracker->messages[i].block_ptr;
        if (block != NULL) {
            uint32_t mask = atomic_load(&tracker->messages[i].participants_mask);
            if ((mask & participant_bit) != 0) {
                // This message is unread by this participant
                if (tracker->messages[i].timestamp < oldest_timestamp) {
                    oldest_timestamp = tracker->messages[i].timestamp;
                    oldest_index = i;
                }
            }
        }
    }
    
    return oldest_index;
}

// Get the message block for a tracked message
void* tracker_get_message(message_tracker_t* tracker, int message_index) {
    if (tracker == NULL || message_index < 0 || message_index >= MAX_TRACKED_MESSAGES) {
        return NULL;
    }
    
    return tracker->messages[message_index].block_ptr;
}

// Free a message if all participants have read it
bool tracker_try_free_message(message_tracker_t* tracker, int message_index, mem_pool_t* pool) {
    if (tracker == NULL || message_index < 0 || message_index >= MAX_TRACKED_MESSAGES || pool == NULL) {
        return false;
    }
    
    // Check if message exists
    void* block = tracker->messages[message_index].block_ptr;
    if (block == NULL) {
        return false;
    }
    
    // Check if reference count is zero
    if (atomic_load(&tracker->messages[message_index].ref_count) > 0) {
        return false; // Still being read by some participants
    }
    
    // Acquire the tracker lock
    spinlock_acquire(&tracker->tracker_lock);
    
    // Double-check that reference count is still zero
    if (atomic_load(&tracker->messages[message_index].ref_count) == 0) {
        // Free the memory block
        if (memory_pool_free(pool, block)) {
            // Clear the tracker entry
            tracker->messages[message_index].block_ptr = NULL;
            tracker->messages[message_index].timestamp = 0;
            atomic_store(&tracker->messages[message_index].participants_mask, 0);
            
            // Decrement count
            atomic_fetch_sub(&tracker->count, 1);
            
            spinlock_release(&tracker->tracker_lock);
            return true;
        }
    }
    
    spinlock_release(&tracker->tracker_lock);
    return false;
}

// Reset the tracker
void tracker_reset(message_tracker_t* tracker) {
    if (tracker == NULL) {
        return;
    }
    
    // Acquire the tracker lock
    spinlock_acquire(&tracker->tracker_lock);
    
    // Reset all entries
    for (int i = 0; i < MAX_TRACKED_MESSAGES; i++) {
        tracker->messages[i].block_ptr = NULL;
        tracker->messages[i].timestamp = 0;
        atomic_store(&tracker->messages[i].ref_count, 0);
        atomic_store(&tracker->messages[i].participants_mask, 0);
    }
    
    // Reset counters
    atomic_store(&tracker->count, 0);
    atomic_store(&tracker->next_index, 0);
    
    spinlock_release(&tracker->tracker_lock);
}