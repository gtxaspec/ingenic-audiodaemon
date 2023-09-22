#ifndef INPUT_H
#define INPUT_H

#include <stdio.h>
#include <imp/imp_audio.h>
#include "output.h"   // Add this line to include output.h

#define AI_SAMPLE_RATE 16000
#define AI_CHN_VOL 100
#define AI_GAIN 25

// Initialize the audio input device
int initialize_audio_input_device(int devID);

// Thread function to handle audio input, taking output file path as an argument
void *ai_record_thread(void *output_file_path);

typedef struct {
    int sockfd;
    char *output_file_path;
} AiThreadArg;

#endif // INPUT_H
