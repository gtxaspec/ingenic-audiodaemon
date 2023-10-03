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

int create_thread(pthread_t *thread_id, void *(*start_routine) (void *), void *arg) {
    int ret = pthread_create(thread_id, NULL, start_routine, arg);
    if (ret) {
        fprintf(stderr, "[ERROR] pthread_create for thread failed with error code: %d\n", ret);
    }
    return ret;
}

int compute_numPerFrm(int sample_rate) {
    return sample_rate * 0.040;  // Assuming a 40ms frame duration.  Output currently has 20ms frame duration.  Investigate.
}

// Convert string representation of bitwidth to the corresponding enum value
IMPAudioBitWidth string_to_bitwidth(const char* str) {
    if (strcmp(str, "AUDIO_BIT_WIDTH_16") == 0) {
        return AUDIO_BIT_WIDTH_16;
    }
    // We should add more mappings if there are other possible values in the JSON.
    return AUDIO_BIT_WIDTH_16;  // Default value
}

// Convert string representation of sound mode to the corresponding enum value
IMPAudioSoundMode string_to_soundmode(const char* str) {
    if (strcmp(str, "AUDIO_SOUND_MODE_MONO") == 0) {
        return AUDIO_SOUND_MODE_MONO;
    }
    // We should add more mappings if there are other possible values in the JSON.
    return AUDIO_SOUND_MODE_MONO;  // Default value
}
