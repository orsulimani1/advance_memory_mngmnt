#ifndef SHM_MANAGER_H
#define SHM_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "ring_buffer.h"
#include "mempool_ring.h"

// Shared memory names
#define SHM_CHAT_POOL "/chat_memory_pool"
#define SHM_CHAT_RING "/chat_message_ring"
#define SHM_PARTICIPANTS "/chat_participants"

// Constants
#define MAX_PARTICIPANTS 32
#define MAX_USERNAME_LENGTH 32
#define MAX_MESSAGE_LENGTH 256
#define MEMORY_POOL_SIZE (1024 * 1024) // 1MB
#define MESSAGE_BLOCK_SIZE (MAX_MESSAGE_LENGTH + 128) // Message plus overhead
#define RING_BUFFER_SIZE 100

// Participant status
typedef enum {
    PARTICIPANT_INACTIVE = 0,
    PARTICIPANT_ACTIVE = 1
} participant_status_t;

// Participant information
typedef struct {
    pid_t pid;                           // Process ID
    char username[MAX_USERNAME_LENGTH];  // User name
    participant_status_t status;         // Status (active/inactive)
    uint32_t last_active;                // Last active timestamp
} participant_info_t;

// Participants directory
typedef struct {
    participant_info_t participants[MAX_PARTICIPANTS];
    uint32_t count;                      // Number of active participants
    uint32_t last_ping;                  // Last ping timestamp
} participants_directory_t;

// Message header
typedef struct {
    uint32_t timestamp;                  // Message timestamp
    char sender[MAX_USERNAME_LENGTH];    // Sender username
    uint32_t message_length;             // Length of message data
} message_header_t;

// Initialize shared memory for chat
bool init_chat_server(void);

// Clean up shared memory resources
void cleanup_chat_server(void);

// Join chat as a client
bool join_chat_client(const char* username);

// Leave chat
void leave_chat(void);

// Send a message to all participants
bool send_message(const char* message);

// Check for and handle new messages
// Returns the number of new messages processed
int process_new_messages(void (*message_callback)(const char* sender, const char* message));

// Check if participants are still active
void check_participants(void);

// Get list of active participants
int get_participants(char usernames[][MAX_USERNAME_LENGTH], int max_count);

#endif // SHM_MANAGER_H