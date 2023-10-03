#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../audio/output.h"
#include "utils.h"

ClientNode *client_list_head = NULL;
pthread_mutex_t audio_buffer_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t audio_data_cond = PTHREAD_COND_INITIALIZER;
unsigned char *audio_buffer = NULL;
ssize_t audio_buffer_size = 0;
int active_client_sock = -1;

// Define frame duration as a constant instead of a magic number.
#define FRAME_DURATION 0.040

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
