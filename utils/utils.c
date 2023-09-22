#include "../audio/output.h"
#include "utils.h"
#include <pthread.h>
#include <stdio.h>

pthread_mutex_t audio_buffer_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t audio_data_cond = PTHREAD_COND_INITIALIZER;
unsigned char audio_buffer[AO_MAX_FRAME_SIZE];
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
    return sample_rate * 0.040;  // Assuming a 40ms frame duration
}
