#ifndef INPUT_H
#define INPUT_H

#include "imp/imp_audio.h"  // for AUDIO_SAMPLE_RATE_48000

#define DEFAULT_AI_SAMPLE_RATE AUDIO_SAMPLE_RATE_48000
#define DEFAULT_AI_CHN_VOL 100
#define DEFAULT_AI_GAIN 25
#define DEFAULT_AI_CHN_CNT 1
#define DEFAULT_AI_FRM_NUM 40
#define DEFAULT_AI_DEV_ID 0
#define DEFAULT_AI_CHN_ID 0
#define DEFAULT_AI_USR_FRM_DEPTH 40

typedef struct {
    int sockfd;
} AiThreadArg;

// Functions
int initialize_audio_input_device(int aiDevID, int aiChnID);
void *ai_record_thread(void *output_file_path);
int disable_audio_input(void);

#endif // INPUT_H
