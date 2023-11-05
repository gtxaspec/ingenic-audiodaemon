#ifndef AUDIO_COMMON_H
#define AUDIO_COMMON_H

#include "cJSON.h"

#define DEFAULT_AI_SAMPLE_RATE AUDIO_SAMPLE_RATE_48000
#define DEFAULT_AI_CHN_VOL 100
#define DEFAULT_AI_GAIN 25
#define DEFAULT_AI_CHN_CNT 1
#define DEFAULT_AI_FRM_NUM 40
#define DEFAULT_AI_DEV_ID 0
#define DEFAULT_AI_CHN_ID 0
#define DEFAULT_AI_USR_FRM_DEPTH 40

typedef struct {
    cJSON *samplerateItem;
    cJSON *bitwidthItem;
    cJSON *soundmodeItem;
    cJSON *frmNumItem;
    cJSON *chnCntItem;
    cJSON *SetVolItem;
    cJSON *SetGainItem;
    cJSON *usrFrmDepthItem;
} AudioInputAttributes;

typedef struct {
    cJSON *samplerateItem;
    cJSON *bitwidthItem;
    cJSON *soundmodeItem;
    cJSON *frmNumItem;
    cJSON *chnCntItem;
    cJSON *SetVolItem;
    cJSON *SetGainItem;
} AudioOutputAttributes;

// Functions for audio input attributes
AudioInputAttributes get_audio_input_attributes(void);
void free_audio_input_attributes(AudioInputAttributes *attrs);

// Functions for audio output attributes
AudioOutputAttributes get_audio_attributes(void);
void free_audio_output_attributes(AudioOutputAttributes *attrs);

// Functions for device attributes
void get_audio_input_device_attributes(int *aiDevID, int *aiChnID);
void free_audio_input_device_attributes(cJSON *aiDevIDItem, cJSON *aiChnIDItem);
void get_audio_output_device_attributes(int *aoDevID, int *aoChnID);
void free_audio_output_device_attributes(cJSON *aoDevIDItem, cJSON *aoChnIDItem);

// Functions for controlling audio output
void pause_audio_output(void);
void clear_audio_output_buffer(void);
void resume_audio_output(void);
void flush_audio_output_buffer(void);
void enable_output_channel(void);
void mute_audio_output_device(int mute_enable);

#endif // AUDIO_COMMON_H
