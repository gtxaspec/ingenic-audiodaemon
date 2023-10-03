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
extern pthread_mutex_t audio_buffer_lock;   // Declaration for the global variable
extern pthread_cond_t audio_data_cond;      // Declaration for the global variable
extern unsigned char *audio_buffer;
extern ssize_t audio_buffer_size;
extern int active_client_sock;

int create_thread(pthread_t *thread_id, void *(*start_routine) (void *), void *arg);

// Sample rates definitions removed as there's a conflicting enum in imp_audio.h
// they were adjusted to not conflict.

int compute_numPerFrm(int sample_rate);

IMPAudioBitWidth string_to_bitwidth(const char* str);
IMPAudioSoundMode string_to_soundmode(const char* str);

#endif // UTILS_H
