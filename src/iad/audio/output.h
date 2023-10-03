#ifndef OUTPUT_H
#define OUTPUT_H

#include <imp/imp_audio.h>
#include <imp/imp_log.h>
#include <../include/cJSON.h>

#define DEFAULT_AO_SAMPLE_RATE AUDIO_SAMPLE_RATE_48000
#define DEFAULT_AO_MAX_FRAME_SIZE 1280
#define DEFAULT_AO_CHN_VOL 100
#define DEFAULT_AO_GAIN 25
#define DEFAULT_AO_CHN_CNT 1
#define DEFAULT_AO_FRM_NUM 20
#define DEFAULT_AO_DEV_ID 0
#define DEFAULT_AO_CHN_ID 0

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
    cJSON *devIDItem;
    cJSON *channel_idItem;
} PlayAttributes;

// Functions
void reinitialize_audio_device(int devID, int chnID);
void *ao_test_play_thread(void *arg);
void pause_audio_output(int devID, int chnID);
void clear_audio_output_buffer(int devID, int chnID);
void resume_audio_output(int devID, int chnID);
void flush_audio_output_buffer(int devID, int chnID);

AudioOutputAttributes get_audio_attributes(void);
void free_audio_attributes(AudioOutputAttributes *attrs);
PlayAttributes get_audio_play_attributes(void);
void free_audio_play_attributes(PlayAttributes *attrs);

extern int g_ao_max_frame_size;
void set_ao_max_frame_size(int frame_size);
void cleanup_audio_output();

#endif // OUTPUT_H
