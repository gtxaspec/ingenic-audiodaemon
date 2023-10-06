#ifndef AUDIO_COMMON_H
#define AUDIO_COMMON_H

#include "cJSON.h"  // for cJSON

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

typedef struct {
    cJSON *device_idItem;
    cJSON *channel_idItem;
} PlayInputAttributes;

typedef struct {
    cJSON *aoDevIDItem;
    cJSON *channel_idItem;
} PlayAttributes;

// Functions

AudioInputAttributes get_audio_input_attributes(void);
void free_audio_input_attributes(AudioInputAttributes *attrs);
PlayInputAttributes get_audio_input_play_attributes(void);
void free_audio_play_input_attributes(PlayInputAttributes *attrs);
int disable_audio_input(void);

void pause_audio_output(void);
void clear_audio_output_buffer(void);
void resume_audio_output(void);
void flush_audio_output_buffer(void);
void mute_audio_output_device(int mute_enable);

AudioOutputAttributes get_audio_attributes(void);
void free_audio_attributes(AudioOutputAttributes *attrs);
PlayAttributes get_audio_play_attributes(void);
void free_audio_play_attributes(PlayAttributes *attrs);

/**
 * Retrieves audio attributes (device and channel IDs) either from PlayAttributes or defaults.
 * @param aoDevID Pointer to store the retrieved Device ID.
 * @param aoChnID Pointer to store the retrieved Channel ID.
 */
void get_audio_device_attributes(int *aoDevID, int *aoChnID);

#endif // AUDIO_COMMON_H

