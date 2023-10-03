#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <output.h>

#define PROG_TAG "AO_T31"

typedef struct ClientNode {
    int sockfd;  // Socket descriptor for the client
    struct ClientNode *next;  // Pointer to the next client node
} ClientNode;

extern ClientNode *client_list_head;
extern pthread_mutex_t audio_buffer_lock;
extern pthread_cond_t audio_data_cond;
extern unsigned char audio_buffer[DEFAULT_AO_MAX_FRAME_SIZE];
extern ssize_t audio_buffer_size;
extern int active_client_sock;

int create_thread(pthread_t *thread_id, void *(*start_routine) (void *), void *arg);

// Sample rates
#define AUDIO_SAMPLE_RATE_8000	 8000
#define AUDIO_SAMPLE_RATE_16000  16000
#define AUDIO_SAMPLE_RATE_24000  24000
#define AUDIO_SAMPLE_RATE_32000  32000
#define AUDIO_SAMPLE_RATE_44100  44100
#define AUDIO_SAMPLE_RATE_48000  48000
#define AUDIO_SAMPLE_RATE_96000  96000

// Compute numPerFrm based on sample rate
int compute_numPerFrm(int sample_rate);

IMPAudioBitWidth string_to_bitwidth(const char* str);
IMPAudioSoundMode string_to_soundmode(const char* str);

#endif // UTILS_H
