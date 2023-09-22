#ifndef INPUT_H
#define INPUT_H

#include <stdio.h>
#include <imp/imp_audio.h>
#include "output.h"   // Add this line to include output.h

// Initialize the audio input device
int initialize_audio_input_device(int devID);

// Thread function to handle audio input, taking output file path as an argument
void *ai_record_thread(void *output_file_path);

typedef struct {
    int sockfd;
    char *output_file_path;
} AiThreadArg;

#endif // INPUT_H
