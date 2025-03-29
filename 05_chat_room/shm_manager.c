#include "shm_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/mman.h>

// Global structures for the current process
static mem_pool_t* message_pool = NULL;
static ring_buffer_t* message_ring = NULL;
static participants_directory_t* participants = NULL;
static int my_participant_id = -1;
static bool is_server = false;

// For atomic operations
static inline uint32_t atomic_add_uint32(uint32_t* ptr, uint32_t val) {
    return __atomic_fetch_add(ptr, val, __ATOMIC_SEQ_CST);
}

// Get current timestamp in seconds
static uint32_t get_timestamp(void) {
    return (uint32_t)time(NULL);
}

// Initialize shared memory for chat server
bool init_chat_server(void) {
    // Clean up any existing shared memory with these names
    shm_unlink(SHM_CHAT_POOL);
    shm_unlink(SHM_CHAT_RING);
    shm_unlink(SHM_PARTICIPANTS);

    // Create shared memory for the participants directory
    int participants_fd = shm_open(SHM_PARTICIPANTS, O_CREAT | O_RDWR, 0666);
    if (participants_fd == -1) {
        perror("Failed to create participants shared memory");
        return false;
    }
    
    // Set the size of the participants directory
    if (ftruncate(participants_fd, sizeof(participants_directory_t)) == -1) {
        perror("Failed to set participants directory size");
        close(participants_fd);
        return false;
    }
    
    // Map the participants directory
    participants = mmap(NULL, sizeof(participants_directory_t), 
                        PROT_READ | PROT_WRITE, MAP_SHARED, 
                        participants_fd, 0);
    if (participants == MAP_FAILED) {
        perror("Failed to map participants directory");
        close(participants_fd);
        return false;
    }
    
    // Initialize participants directory
    memset(participants, 0, sizeof(participants_directory_t));
    participants->count = 0;
    participants->last_ping = get_timestamp();
    
    // Close file descriptor (mapping remains)
    close(participants_fd);
    
    // Create shared memory for the message pool
    mem_pool_t pool;
    if (!memory_pool_init_shared(&pool, SHM_CHAT_POOL, MEMORY_POOL_SIZE, 
                               MESSAGE_BLOCK_SIZE, true, 0666)) {
        perror("Failed to create message pool");
        munmap(participants, sizeof(participants_directory_t));
        return false;
    }
    
    message_pool = &pool;
    
    // Create shared memory for the message ring buffer
    // First create a temporary ring buffer to get the size needed
    void* buffer_array[RING_BUFFER_SIZE];
    ring_buffer_t temp_rb;
    ring_buffer_init(&temp_rb, buffer_array, RING_BUFFER_SIZE);
    
    // Now create the actual shared memory for the ring buffer
    int ring_fd = shm_open(SHM_CHAT_RING, O_CREAT | O_RDWR, 0666);
    if (ring_fd == -1) {
        perror("Failed to create ring buffer shared memory");
        memory_pool_destroy(message_pool, true);
        munmap(participants, sizeof(participants_directory_t));
        return false;
    }
    
    // Set the size of the ring buffer
    size_t ring_size = sizeof(ring_buffer_t) + RING_BUFFER_SIZE * sizeof(void*);
    if (ftruncate(ring_fd, ring_size) == -1) {
        perror("Failed to set ring buffer size");
        close(ring_fd);
        memory_pool_destroy(message_pool, true);
        munmap(participants, sizeof(participants_directory_t));
        return false;
    }
    
    // Map the ring buffer
    void* ring_memory = mmap(NULL, ring_size, PROT_READ | PROT_WRITE, 
                            MAP_SHARED, ring_fd, 0);
    if (ring_memory == MAP_FAILED) {
        perror("Failed to map ring buffer");
        close(ring_fd);
        memory_pool_destroy(message_pool, true);
        munmap(participants, sizeof(participants_directory_t));
        return false;
    }
    
    // Initialize the ring buffer
    message_ring = (ring_buffer_t*)ring_memory;
    void** buffer_ptr = (void**)(((uint8_t*)ring_memory) + sizeof(ring_buffer_t));
    ring_buffer_init(message_ring, buffer_ptr, RING_BUFFER_SIZE);
    
    // Close file descriptor (mapping remains)
    close(ring_fd);
    
    // Register the server as participant 0
    participants->participants[0].pid = getpid();
    strncpy(participants->participants[0].username, "Server", MAX_USERNAME_LENGTH);
    participants->participants[0].status = PARTICIPANT_ACTIVE;
    participants->participants[0].last_active = get_timestamp();
    participants->count = 1;
    
    is_server = true;
    my_participant_id = 0;
    
    return true;
}

// Clean up shared memory resources
void cleanup_chat_server(void) {
    if (!is_server) {
        leave_chat();
        return;
    }
    
    // Unmap the participants directory
    if (participants != NULL) {
        munmap(participants, sizeof(participants_directory_t));
        participants = NULL;
    }
    
    // Clean up message pool
    if (message_pool != NULL) {
        memory_pool_destroy(message_pool, true);
        message_pool = NULL;
    }
    
    // Unmap the ring buffer
    if (message_ring != NULL) {
        size_t ring_size = sizeof(ring_buffer_t) + RING_BUFFER_SIZE * sizeof(void*);
        munmap(message_ring, ring_size);
        message_ring = NULL;
    }
    
    // Unlink shared memory
    shm_unlink(SHM_CHAT_POOL);
    shm_unlink(SHM_CHAT_RING);
    shm_unlink(SHM_PARTICIPANTS);
}

// Join chat as a client
bool join_chat_client(const char* username) {
    if (username == NULL || strlen(username) == 0 || 
        strlen(username) >= MAX_USERNAME_LENGTH) {
        fprintf(stderr, "Invalid username\n");
        return false;
    }
    
    // Open participants directory
    int participants_fd = shm_open(SHM_PARTICIPANTS, O_RDWR, 0666);
    if (participants_fd == -1) {
        perror("Failed to open participants shared memory");
        return false;
    }
    
    // Map the participants directory
    participants = mmap(NULL, sizeof(participants_directory_t), 
                       PROT_READ | PROT_WRITE, MAP_SHARED, 
                       participants_fd, 0);
    if (participants == MAP_FAILED) {
        perror("Failed to map participants directory");
        close(participants_fd);
        return false;
    }
    
    // Close file descriptor (mapping remains)
    close(participants_fd);
    
    // Find an empty slot
    int slot = -1;
    for (int i = 0; i < MAX_PARTICIPANTS; i++) {
        if (participants->participants[i].status == PARTICIPANT_INACTIVE) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) {
        fprintf(stderr, "Chat is full\n");
        munmap(participants, sizeof(participants_directory_t));
        participants = NULL;
        return false;
    }
    
    // Check for duplicate username
    for (int i = 0; i < MAX_PARTICIPANTS; i++) {
        if (participants->participants[i].status == PARTICIPANT_ACTIVE &&
            strcmp(participants->participants[i].username, username) == 0) {
            fprintf(stderr, "Username already in use\n");
            munmap(participants, sizeof(participants_directory_t));
            participants = NULL;
            return false;
        }
    }
    
    // Connect to message pool
    mem_pool_t pool;
    if (!memory_pool_init_shared(&pool, SHM_CHAT_POOL, MEMORY_POOL_SIZE, 
                               MESSAGE_BLOCK_SIZE, false, 0666)) {
        perror("Failed to connect to message pool");
        munmap(participants, sizeof(participants_directory_t));
        participants = NULL;
        return false;
    }
    
    message_pool = &pool;
    
    // Connect to message ring buffer
    size_t ring_size = sizeof(ring_buffer_t) + RING_BUFFER_SIZE * sizeof(void*);
    int ring_fd = shm_open(SHM_CHAT_RING, O_RDWR, 0666);
    if (ring_fd == -1) {
        perror("Failed to open ring buffer shared memory");
        memory_pool_destroy(message_pool, false);
        message_pool = NULL;
        munmap(participants, sizeof(participants_directory_t));
        participants = NULL;
        return false;
    }
    
    // Map the ring buffer
    void* ring_memory = mmap(NULL, ring_size, PROT_READ | PROT_WRITE, 
                            MAP_SHARED, ring_fd, 0);
    if (ring_memory == MAP_FAILED) {
        perror("Failed to map ring buffer");
        close(ring_fd);
        memory_pool_destroy(message_pool, false);
        message_pool = NULL;
        munmap(participants, sizeof(participants_directory_t));
        participants = NULL;
        return false;
    }
    
    // Ring buffer is already initialized
    message_ring = (ring_buffer_t*)ring_memory;
    
    // Close file descriptor (mapping remains)
    close(ring_fd);
    
    // Register as a participant
    participants->participants[slot].pid = getpid();
    strncpy(participants->participants[slot].username, username, MAX_USERNAME_LENGTH);
    participants->participants[slot].status = PARTICIPANT_ACTIVE;
    participants->participants[slot].last_active = get_timestamp();
    atomic_add_uint32(&participants->count, 1);
    
    my_participant_id = slot;
    is_server = false;
    
    return true;
}

// Leave chat
void leave_chat(void) {
    if (my_participant_id < 0) {
        return;  // Not connected
    }
    
    // Mark as inactive
    if (participants != NULL) {
        participants->participants[my_participant_id].status = PARTICIPANT_INACTIVE;
        atomic_add_uint32(&participants->count, -1);
    }
    
    // Unmap the participants directory
    if (participants != NULL) {
        munmap(participants, sizeof(participants_directory_t));
        participants = NULL;
    }
    
    // Clean up message pool
    if (message_pool != NULL) {
        memory_pool_destroy(message_pool, false);
        message_pool = NULL;
    }
    
    // Unmap the ring buffer
    if (message_ring != NULL) {
        size_t ring_size = sizeof(ring_buffer_t) + RING_BUFFER_SIZE * sizeof(void*);
        munmap(message_ring, ring_size);
        message_ring = NULL;
    }
    
    my_participant_id = -1;
}

// Send a message to all participants
bool send_message(const char* message) {
    if (my_participant_id < 0 || message == NULL || message_pool == NULL || 
        message_ring == NULL) {
        return false;  // Not connected
    }
    
    size_t message_len = strlen(message);
    if (message_len == 0 || message_len >= MAX_MESSAGE_LENGTH) {
        return false;  // Empty or too long message
    }
    
    // Update last active timestamp
    participants->participants[my_participant_id].last_active = get_timestamp();
    
    // Allocate memory for the message
    void* block = memory_pool_alloc(message_pool);
    if (block == NULL) {
        fprintf(stderr, "Failed to allocate memory for message\n");
        return false;
    }
    
    // Set up message header
    message_header_t* header = (message_header_t*)block;
    header->timestamp = get_timestamp();
    strncpy(header->sender, participants->participants[my_participant_id].username, 
            MAX_USERNAME_LENGTH);
    header->message_length = message_len;
    
    // Copy message data after the header
    char* message_data = (char*)block + sizeof(message_header_t);
    strncpy(message_data, message, message_len);
    message_data[message_len] = '\0';  // Ensure null-termination
    
    // Add message to the ring buffer
    if (!ring_buffer_put(message_ring, block)) {
        fprintf(stderr, "Failed to add message to ring buffer\n");
        memory_pool_free(message_pool, block);
        return false;
    }
    
    return true;
}

// Check for and handle new messages
int process_new_messages(void (*message_callback)(const char* sender, const char* message)) {
    if (my_participant_id < 0 || message_callback == NULL || 
        message_pool == NULL || message_ring == NULL) {
        return 0;  // Not connected
    }
    
    // Update last active timestamp
    participants->participants[my_participant_id].last_active = get_timestamp();
    
    int messages_processed = 0;
    
    // Process all available messages
    while (!ring_buffer_is_empty(message_ring)) {
        // Get a message from the ring buffer
        void* block = ring_buffer_get(message_ring);
        if (block == NULL) {
            break;  // No more messages
        }
        
        // Process the message
        message_header_t* header = (message_header_t*)block;
        char* message_data = (char*)block + sizeof(message_header_t);
        
        // Call the callback function with sender and message
        message_callback(header->sender, message_data);
        
        // Free the message memory
        memory_pool_free(message_pool, block);
        
        messages_processed++;
    }
    
    return messages_processed;
}

// Check if participants are still active
void check_participants(void) {
    if (participants == NULL) {
        return;
    }
    
    uint32_t now = get_timestamp();
    uint32_t timeout = 60;  // 60 seconds timeout
    
    for (int i = 0; i < MAX_PARTICIPANTS; i++) {
        if (participants->participants[i].status == PARTICIPANT_ACTIVE) {
            if (now - participants->participants[i].last_active > timeout) {
                // Participant is inactive
                participants->participants[i].status = PARTICIPANT_INACTIVE;
                atomic_add_uint32(&participants->count, -1);
                
                // If we're the server, send a system message
                if (is_server) {
                    char disconnect_msg[MAX_MESSAGE_LENGTH];
                    snprintf(disconnect_msg, MAX_MESSAGE_LENGTH, 
                            "%s has been disconnected (timeout)", 
                            participants->participants[i].username);
                    send_message(disconnect_msg);
                }
            }
        }
    }
}

// Get list of active participants
int get_participants(char usernames[][MAX_USERNAME_LENGTH], int max_count) {
    if (participants == NULL || usernames == NULL || max_count <= 0) {
        return 0;
    }
    
    int count = 0;
    
    for (int i = 0; i < MAX_PARTICIPANTS && count < max_count; i++) {
        if (participants->participants[i].status == PARTICIPANT_ACTIVE) {
            strncpy(usernames[count], participants->participants[i].username, 
                   MAX_USERNAME_LENGTH);
            count++;
        }
    }
    
    return count;
}