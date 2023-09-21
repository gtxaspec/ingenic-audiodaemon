#include "../audio/output.h"
#include "utils.h"

pthread_mutex_t audio_buffer_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t audio_data_cond = PTHREAD_COND_INITIALIZER;
unsigned char audio_buffer[AO_MAX_FRAME_SIZE];
ssize_t audio_buffer_size = 0;
int active_client_sock = -1;
