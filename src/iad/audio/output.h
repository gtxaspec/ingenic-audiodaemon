#ifndef AUDIO_H
#define AUDIO_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <imp/imp_audio.h>
#include <imp/imp_log.h>

// We will now fetch these values from the JSON config or default to these constants
#define DEFAULT_AO_SAMPLE_RATE AUDIO_SAMPLE_RATE_48000
#define AO_NUM_PER_FRM compute_numPerFrm(DEFAULT_AO_SAMPLE_RATE)
#define DEFAULT_AO_MAX_FRAME_SIZE 1280
#define DEFAULT_AO_CHN_VOL 100
#define DEFAULT_AO_GAIN 25
#define DEFAULT_AO_CHN_CNT 1
#define DEFAULT_AO_FRM_NUM 20
#define DEFAULT_AO_DEV_ID 1
#define DEFAULT_AO_CHN_ID 0

// Functions
void reinitialize_audio_device(int devID, int chnID);
void *ao_test_play_thread(void *arg);
void pause_audio_output(int devID, int chnID);
void clear_audio_output_buffer(int devID, int chnID);
void resume_audio_output(int devID, int chnID);
void flush_audio_output_buffer(int devID, int chnID);

#endif // AUDIO_H
