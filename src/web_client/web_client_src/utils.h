#ifndef WEB_UTILS_H
#define WEB_UTILS_H

// Required headers for thread management and data types
#include <pthread.h>
#include <sys/types.h>      // For ssize_t

// Handles the SIGINT signal to allow the program to exit gracefully.
void handle_sigint(int sig);

// Transforms the program into a daemon process.
void daemonize(void);

// Sets up signal handling for the program.
void setup_signal_handling(void);

/**
 * @brief Checks if another instance of the program is already running.
 *
 * @return 1 if another instance is running, 0 otherwise.
 */
int is_already_running(void);

// Constants for program tagging and frame duration
#define PROG_TAG "WEB_CLIENT"

#endif // WEB_UTILS_H
