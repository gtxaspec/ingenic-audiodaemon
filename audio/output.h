#ifndef AUDIO_H
#define AUDIO_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <imp/imp_audio.h>
#include <imp/imp_log.h>

#define AO_TEST_SAMPLE_RATE 16000
#define AO_MAX_FRAME_SIZE 1280
#define CHN_VOL 100
#define AO_GAIN 24

// Functions
void reinitialize_audio_device(int devID);
void *ao_test_play_thread(void *arg);

#endif // AUDIO_H
