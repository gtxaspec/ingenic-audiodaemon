#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define TAG "AO_T31"

extern pthread_mutex_t audio_buffer_lock;
extern pthread_cond_t audio_data_cond;
extern unsigned char audio_buffer[AO_MAX_FRAME_SIZE];
extern ssize_t audio_buffer_size;
extern int active_client_sock;

int create_thread(pthread_t *thread_id, void *(*start_routine) (void *), void *arg);

#endif // UTILS_H
