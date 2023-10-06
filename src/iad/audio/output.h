#ifndef OUTPUT_H
#define OUTPUT_H

#include <imp/imp_audio.h>  // for AUDIO_SAMPLE_RATE_48000

#define DEFAULT_AO_SAMPLE_RATE AUDIO_SAMPLE_RATE_48000
#define DEFAULT_AO_MAX_FRAME_SIZE 1280
#define DEFAULT_AO_CHN_VOL 100
#define DEFAULT_AO_GAIN 25
#define DEFAULT_AO_CHN_CNT 1
#define DEFAULT_AO_FRM_NUM 20
#define DEFAULT_AO_DEV_ID 0
#define DEFAULT_AO_CHN_ID 0

// Functions
void reinitialize_audio_device(int aoDevID, int aoChnID);
void *ao_test_play_thread(void *arg);

extern int g_ao_max_frame_size;
void set_ao_max_frame_size(int frame_size);
void cleanup_audio_output();
int disable_audio_output(void);

#endif // OUTPUT_H
