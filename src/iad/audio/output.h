#ifndef OUTPUT_H
#define OUTPUT_H

#include <pthread.h>
#include "imp/imp_audio.h"  // for AUDIO_SAMPLE_RATE_48000

#define DEFAULT_AO_SAMPLE_RATE AUDIO_SAMPLE_RATE_48000
#define DEFAULT_AO_MAX_FRAME_SIZE 1280
#define DEFAULT_AO_CHN_VOL 90
#define DEFAULT_AO_GAIN 25
#define DEFAULT_AO_CHN_CNT 1
#define DEFAULT_AO_FRM_NUM 20
#define DEFAULT_AO_DEV_ID 0
#define DEFAULT_AO_CHN_ID 0

// Functions
void reinitialize_audio_output_device(int aoDevID, int aoChnID);
void *ao_play_thread(void *arg);

extern int g_ao_max_frame_size;
void set_ao_max_frame_size(int frame_size);
void cleanup_audio_output();
int disable_audio_output(void);

// Global variable declaration for the maximum frame size for audio output.
extern int g_ao_max_frame_size;

// Global flag and mutex to control thread termination
extern volatile int g_stop_thread;
extern pthread_mutex_t g_stop_thread_mutex;

#endif // OUTPUT_H
