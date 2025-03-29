#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <assert.h>

/**
 * Check if a pointer is valid and accessible
 * 
 * This function checks if a pointer is valid by attempting to access the memory
 * in a child process. If the memory is invalid, the child process will crash,
 * but the parent process will continue.
 * 
 * @param ptr Pointer to check
 * @param size Size of memory to check
 * @param write_access Whether to check for write access (true) or just read access (false)
 * @return true if the pointer is valid and accessible, false otherwise
 */
bool is_pointer_valid(void* ptr, size_t size, bool write_access) {
    // Invalid pointer check
    if (ptr == NULL || size == 0) {
        return false;
    }
    
    // Use fork to create a child process for testing
    pid_t child = fork();
    
    if (child == 0) {
        // Child process
        if (write_access) {
            // Try to write to the memory
            memset(ptr, 0, size);
        } else {
            // Try to read from the memory
            char* buffer = malloc(size);
            if (buffer) {
                memcpy(buffer, ptr, size);
                free(buffer);
            }
        }
        // If we reach here, the memory access was successful
        exit(EXIT_SUCCESS);
    } else if (child > 0) {
        // Parent process
        int status;
        pid_t result = waitpid(child, &status, 0);
        assert(result >= 0);
        
        // Check if child exited normally with success
        return WIFEXITED(status) && WEXITSTATUS(status) == EXIT_SUCCESS;
    } else {
        // Fork failed
        perror("fork");
        return false;
    }
}

// Example usage in your memory management class
void memory_leak_detection_example() {
    printf("=== Memory Pointer Validation Example ===\n");
    
    // Allocate a valid pointer
    int* valid_ptr = malloc(sizeof(int));
    *valid_ptr = 42;
    
    // Check valid pointer
    printf("Valid pointer check (write): %s\n", 
           is_pointer_valid(valid_ptr, sizeof(int), true) ? "PASS" : "FAIL");
    printf("Valid pointer check (read): %s\n", 
           is_pointer_valid(valid_ptr, sizeof(int), false) ? "PASS" : "FAIL");
    
    // Create an invalid pointer
    int* invalid_ptr = (int*)0xdeadbeef;
    
    // Check invalid pointer
    printf("Invalid pointer check (write): %s\n", 
           is_pointer_valid(invalid_ptr, sizeof(int), true) ? "PASS" : "FAIL");
    printf("Invalid pointer check (read): %s\n", 
           is_pointer_valid(invalid_ptr, sizeof(int), false) ? "PASS" : "FAIL");
    
    // Clean up
    free(valid_ptr);
}