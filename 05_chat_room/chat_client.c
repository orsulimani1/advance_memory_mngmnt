#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include "shm_manager.h"

// Flag to indicate if the client should continue running
static volatile int running = 1;

// Signal handler to handle Ctrl+C
void handle_signal(int sig) {
    printf("\nLeaving chat...\n");
    running = 0;
}

// Callback function for handling messages
void print_message(const char* sender, const char* message) {
    printf("[%s] %s\n", sender, message);
}

// Thread function to read messages
void* read_messages_thread(void* arg) {
    while (running) {
        // Process new messages
        process_new_messages(print_message);
        
        // Sleep to avoid busy waiting
        usleep(100000);  // 100ms
    }
    
    return NULL;
}

int main(int argc, char* argv[]) {
    // Check command line arguments
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <username>\n", argv[0]);
        return 1;
    }
    
    // Set up signal handler
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    
    // Get username from command line
    const char* username = argv[1];
    
    printf("Joining chat as '%s'...\n", username);
    
    // Join the chat
    if (!join_chat_client(username)) {
        fprintf(stderr, "Failed to join chat\n");
        return 1;
    }
    
    printf("Joined chat. Type your messages and press Enter. Press Ctrl+C to exit.\n");
    
    // Send join message
    char join_message[MAX_MESSAGE_LENGTH];
    snprintf(join_message, MAX_MESSAGE_LENGTH, "has joined the chat");
    send_message(join_message);
    
    // Create thread to read messages
    pthread_t thread;
    if (pthread_create(&thread, NULL, read_messages_thread, NULL) != 0) {
        fprintf(stderr, "Failed to create message reader thread\n");
        leave_chat();
        return 1;
    }
    
    // Main loop to read user input
    char input[MAX_MESSAGE_LENGTH];
    
    while (running) {
        printf("> ");
        fflush(stdout);
        
        if (fgets(input, MAX_MESSAGE_LENGTH, stdin) == NULL) {
            break;  // EOF
        }
        
        // Remove trailing newline
        size_t len = strlen(input);
        if (len > 0 && input[len - 1] == '\n') {
            input[len - 1] = '\0';
        }
        
        // Check for empty input or exit command
        if (strlen(input) == 0) {
            continue;
        }
        
        if (strcmp(input, "/exit") == 0 || strcmp(input, "/quit") == 0) {
            running = 0;
            break;
        }
        
        if (strcmp(input, "/list") == 0) {
            char usernames[MAX_PARTICIPANTS][MAX_USERNAME_LENGTH];
            int count = get_participants(usernames, MAX_PARTICIPANTS);
            
            printf("\nActive participants (%d):\n", count);
            for (int i = 0; i < count; i++) {
                printf("- %s\n", usernames[i]);
            }
            printf("\n");
            continue;
        }
        
        // Send the message
        if (!send_message(input)) {
            fprintf(stderr, "Failed to send message\n");
        }
    }
    
    // Send leave message
    snprintf(join_message, MAX_MESSAGE_LENGTH, "has left the chat");
    send_message(join_message);
    
    // Wait for message reader thread to finish
    pthread_cancel(thread);
    pthread_join(thread, NULL);
    
    // Leave the chat
    leave_chat();
    
    printf("Left chat\n");
    return 0;
}