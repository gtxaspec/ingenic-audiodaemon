#ifndef UTILS_H
#define UTILS_H

// Required headers for thread management and data types
#include <pthread.h>
#include <sys/types.h>      // For ssize_t

// Audio headers for handling audio data and configurations
#include "imp/imp_audio.h"  // For IMPAudioBitWidth, IMPAudioSoundMode

// Constants for program tagging and frame duration
#define PROG_TAG "AO_T31"
#define FRAME_DURATION 0.040

/**
 * @brief Represents a connected client with its socket descriptor.
 *
 * This struct is used to manage connected clients in a linked list.
 */
typedef struct ClientNode {
    int sockfd;  // Socket descriptor for the client
    struct ClientNode *next;  // Pointer to the next client node
} ClientNode;

// Head of the linked list that contains all connected clients
extern ClientNode *client_list_head;

// Mutex lock for audio buffer synchronization
extern pthread_mutex_t audio_buffer_lock;

// Condition variable for signaling availability of audio data
extern pthread_cond_t audio_data_cond;

// Global buffer to store audio data
extern unsigned char *audio_buffer;

// Size of the current audio data in the buffer
extern ssize_t audio_buffer_size;

// Socket descriptor of the currently active client
extern int active_client_sock;

/**
 * @brief Creates a new thread.
 *
 * @param thread_id Pointer to the thread ID.
 * @param start_routine Pointer to the function to run in the new thread.
 * @param arg Argument to pass to the start_routine.
 * @return 0 on success, error code on failure.
 */
int create_thread(pthread_t *thread_id, void *(*start_routine) (void *), void *arg);

/**
 * @brief Computes the number of samples per frame based on sample rate.
 *
 * @param sample_rate The sample rate to use for the computation.
 * @return Number of samples per frame.
 */
int compute_numPerFrm(int sample_rate);

/**
 * @brief Converts a string representation of bit width to its enum value.
 *
 * @param str String representation of the bit width.
 * @return Corresponding enum value.
 */
IMPAudioBitWidth string_to_bitwidth(const char* str);

/**
 * @brief Converts a string representation of sound mode to its enum value.
 *
 * @param str String representation of the sound mode.
 * @return Corresponding enum value.
 */
IMPAudioSoundMode string_to_soundmode(const char* str);

// Cleans up all resources and prepares for program termination.
void perform_cleanup(void);

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

// Flag to indicate if the thread should stop.
extern volatile int g_stop_thread;

// Mutex associated with the stop thread flag.
extern pthread_mutex_t g_stop_thread_mutex;

#endif // UTILS_H
