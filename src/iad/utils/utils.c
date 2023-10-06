#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "utils.h"
#include "config.h"
#include "output.h"
#include "input.h"

ClientNode *client_list_head = NULL;
pthread_mutex_t audio_buffer_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t audio_data_cond = PTHREAD_COND_INITIALIZER;
unsigned char *audio_buffer = NULL;
ssize_t audio_buffer_size = 0;
int active_client_sock = -1;

// Flag and associated mutex for thread termination
volatile int g_stop_thread = 0;
pthread_mutex_t g_stop_thread_mutex = PTHREAD_MUTEX_INITIALIZER;

int create_thread(pthread_t *thread_id, void *(*start_routine) (void *), void *arg) {
    int ret = pthread_create(thread_id, NULL, start_routine, arg);
    if (ret) {
        fprintf(stderr, "[ERROR] pthread_create for thread failed with error code: %d\n", ret);
    }
    return ret;
}

int compute_numPerFrm(int sample_rate) {
    // Use the FRAME_DURATION constant instead of magic number.
    return sample_rate * FRAME_DURATION;
}

IMPAudioBitWidth string_to_bitwidth(const char* str) {
    if (strcmp(str, "AUDIO_BIT_WIDTH_16") == 0) {
        return AUDIO_BIT_WIDTH_16;
    }
    // Log a warning if an unexpected value is encountered.
    fprintf(stderr, "[WARNING] Unexpected bitwidth string: %s. Defaulting to AUDIO_BIT_WIDTH_16.\n", str);
    return AUDIO_BIT_WIDTH_16;  // Default value
}

IMPAudioSoundMode string_to_soundmode(const char* str) {
    if (strcmp(str, "AUDIO_SOUND_MODE_MONO") == 0) {
        return AUDIO_SOUND_MODE_MONO;
    }
    // Log a warning if an unexpected value is encountered.
    fprintf(stderr, "[WARNING] Unexpected sound mode string: %s. Defaulting to AUDIO_SOUND_MODE_MONO.\n", str);
    return AUDIO_SOUND_MODE_MONO;  // Default value
}

/**
 * @brief Clean up resources.
 *
 * This function is responsible for cleaning up any allocated resources
 * and restoring the system to its initial state.
 */
void perform_cleanup() {
    pthread_mutex_destroy(&audio_buffer_lock);
    pthread_cond_destroy(&audio_data_cond);

    pthread_mutex_lock(&g_stop_thread_mutex);
    g_stop_thread = 1;
    pthread_mutex_unlock(&g_stop_thread_mutex);

    // Signal the condition variable to wake up any waiting threads
    pthread_cond_signal(&audio_data_cond);

    disable_audio_input();
    disable_audio_output();

    config_cleanup();
}

/**
 * @brief Signal handler for SIGINT.
 *
 * This function handles the SIGINT signal (typically sent from the
 * command line via CTRL+C). It ensures that the daemon exits gracefully.
 *
 * @param sig Signal number (expected to be SIGINT).
 */
void handle_sigint(int sig) {
    printf("Caught signal %d. Exiting gracefully...\n", sig);
    perform_cleanup();
    exit(0);
}
