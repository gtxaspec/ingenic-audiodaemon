#ifndef INPUT_H
#define INPUT_H

#include <stdio.h>
#include <imp/imp_audio.h>
#include "config.h"
#include "output.h"

// We will now fetch these values from the JSON config or default to these constants
#define DEFAULT_AI_SAMPLE_RATE AUDIO_SAMPLE_RATE_48000
//#define DEFAULT_AI_NUM_PER_FRM compute_numPerFrm(DEFAULT_AI_SAMPLE_RATE)
#define AI_NUM_PER_FRM compute_numPerFrm(DEFAULT_AI_SAMPLE_RATE)
#define DEFAULT_AI_CHN_VOL 100
#define DEFAULT_AI_GAIN 25
#define DEFAULT_AI_CHN_CNT 1
#define DEFAULT_AI_FRM_NUM 40
#define DEFAULT_AI_DEV_ID 1
#define DEFAULT_AI_CHN_ID 0

// Initialize the audio input device
int initialize_audio_input_device(int devID, int chnID);

// Thread function to handle audio input, taking output file path as an argument
void *ai_record_thread(void *output_file_path);

typedef struct {
    int sockfd;
} AiThreadArg;

#endif // INPUT_H
