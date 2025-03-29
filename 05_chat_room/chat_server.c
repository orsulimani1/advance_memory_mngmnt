#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include "shm_manager.h"

// Flag to indicate if the server should continue running
static volatile int running = 1;

// Signal handler to handle Ctrl+C
void handle_signal(int sig) {
    printf("\nShutting down chat server...\n");
    running = 0;
}

// Callback function for handling messages
void print_message(const char* sender, const char* message) {
    printf("[%s] %s\n", sender, message);
}

int main() {
    // Set up signal handler
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    
    printf("Starting chat server...\n");
    
    // Initialize the chat server
    if (!init_chat_server()) {
        fprintf(stderr, "Failed to initialize chat server\n");
        return 1;
    }
    
    printf("Chat server initialized. Press Ctrl+C to exit.\n");
    
    // Send welcome message
    send_message("Chat server is now online");
    
    // Main server loop
    while (running) {
        // Process new messages
        process_new_messages(print_message);
        
        // Check for inactive participants
        check_participants();
        
        // Print active participants every 10 seconds
        static time_t last_status = 0;
        time_t now = time(NULL);
        
        if (now - last_status >= 10) {
            char usernames[MAX_PARTICIPANTS][MAX_USERNAME_LENGTH];
            int count = get_participants(usernames, MAX_PARTICIPANTS);
            
            printf("\nActive participants (%d):\n", count);
            for (int i = 0; i < count; i++) {
                printf("- %s\n", usernames[i]);
            }
            printf("\n");
            
            last_status = now;
        }
        
        // Sleep to avoid busy waiting
        usleep(100000);  // 100ms
    }
    
    // Clean up resources
    cleanup_chat_server();
    
    printf("Chat server shut down\n");
    return 0;
}