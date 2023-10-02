#include <pthread.h>
#include <sched.h>
#include "output.h"
#include "utils.h"
#include "logging.h"
#include "config.h"

#define TRUE 1

AudioAttributes get_audio_attributes() {
    AudioAttributes attrs;

    attrs.samplerateItem = get_audio_attribute(AUDIO_OUTPUT, "sample_rate");
    attrs.bitwidthItem = get_audio_attribute(AUDIO_OUTPUT, "bitwidth");
    attrs.soundmodeItem = get_audio_attribute(AUDIO_OUTPUT, "soundmode");
    attrs.frmNumItem = get_audio_attribute(AUDIO_OUTPUT, "frmNum");
    attrs.chnCntItem = get_audio_attribute(AUDIO_OUTPUT, "chnCnt");
    attrs.SetVolItem = get_audio_attribute(AUDIO_OUTPUT, "SetVol");
    attrs.SetGainItem = get_audio_attribute(AUDIO_OUTPUT, "SetGain");

    return attrs;
}

void free_audio_attributes(AudioAttributes *attrs) {
    cJSON_Delete(attrs->samplerateItem);
    cJSON_Delete(attrs->bitwidthItem);
    cJSON_Delete(attrs->soundmodeItem);
    cJSON_Delete(attrs->frmNumItem);
    cJSON_Delete(attrs->chnCntItem);
    cJSON_Delete(attrs->SetVolItem);
    cJSON_Delete(attrs->SetGainItem);
}

PlayAttributes get_audio_play_attributes() {
    PlayAttributes attrs;

    attrs.devIDItem = get_audio_attribute(AUDIO_OUTPUT, "device_id");
    attrs.channel_idItem = get_audio_attribute(AUDIO_OUTPUT, "channel_id");

    return attrs;
}

void free_audio_play_attributes(PlayAttributes *attrs) {
    cJSON_Delete(attrs->devIDItem);
    cJSON_Delete(attrs->channel_idItem);
}

/* Helper function to handle errors and potentially reinitialize */
void handle_and_reinitialize(int devID, int chnID, const char *errorMsg) {
    handle_audio_error(errorMsg);
    reinitialize_audio_device(devID, chnID);
}

void initialize_audio_device(int devID, int chnID) {
    IMPAudioIOAttr attr;
    AudioAttributes attrs = get_audio_attributes();

    attr.bitwidth = attrs.bitwidthItem ? string_to_bitwidth(attrs.bitwidthItem->valuestring) : AUDIO_BIT_WIDTH_16;
    attr.soundmode = attrs.soundmodeItem ? string_to_soundmode(attrs.soundmodeItem->valuestring) : AUDIO_SOUND_MODE_MONO;
    attr.frmNum = attrs.frmNumItem ? attrs.frmNumItem->valueint : DEFAULT_AO_FRM_NUM;
    attr.samplerate = attrs.samplerateItem ? attrs.samplerateItem->valueint : DEFAULT_AO_SAMPLE_RATE;
    attr.numPerFrm = compute_numPerFrm(attr.samplerate);
    attr.chnCnt = attrs.chnCntItem ? attrs.chnCntItem->valueint : DEFAULT_AO_CHN_CNT;

    if (IMP_AO_SetPubAttr(devID, &attr) || IMP_AO_GetPubAttr(devID, &attr) ||
        IMP_AO_Enable(devID) || IMP_AO_EnableChn(devID, chnID)) {
        handle_and_reinitialize(devID, chnID, "Failed to reinitialize audio attributes");
    }

    printf("[DEBUG] CHNID: %d\n", chnID);

    // Set chnVol and aogain again
    if (IMP_AO_SetVol(devID, chnID, attrs.SetVolItem ? attrs.SetVolItem->valueint : DEFAULT_AO_CHN_VOL) ||
        IMP_AO_SetGain(devID, chnID, attrs.SetGainItem ? attrs.SetGainItem->valueint : DEFAULT_AO_GAIN)) {
        handle_and_reinitialize(devID, chnID, "Failed to set volume or gain attributes");
    }

    printf("[DEBUG] samplerate: %d\n", attr.samplerate);
    printf("[DEBUG] bitwidth: %d\n", attr.bitwidth);
    printf("[DEBUG] soundmode: %d\n", attr.soundmode);
    printf("[DEBUG] frmNum: %d\n", attr.frmNum);
    printf("[DEBUG] numPerFrm: %d\n", attr.numPerFrm);
    printf("[DEBUG] chnCnt: %d\n", attr.chnCnt);

    free_audio_attributes(&attrs);
}

void reinitialize_audio_device(int devID, int chnID) {
    IMP_AO_DisableChn(devID, chnID);
    IMP_AO_Disable(devID);

    initialize_audio_device(devID, chnID);
}

void pause_audio_output(int devID, int chnID) {
    if (IMP_AO_PauseChn(devID, chnID)) {
        handle_and_reinitialize(devID, chnID, "Failed to pause audio output");
    }
}

void clear_audio_output_buffer(int devID, int chnID) {
    if (IMP_AO_ClearChnBuf(devID, chnID)) {
        handle_and_reinitialize(devID, chnID, "Failed to clear audio output buffer");
    }
}

void resume_audio_output(int devID, int chnID) {
    if (IMP_AO_ResumeChn(devID, chnID)) {
        handle_and_reinitialize(devID, chnID, "Failed to resume audio output");
    }
}

void flush_audio_output_buffer(int devID, int chnID) {
    if (IMP_AO_FlushChnBuf(devID, chnID)) {
        handle_and_reinitialize(devID, chnID, "Failed to flush audio output buffer");
    }
}

void *ao_test_play_thread(void *arg) {
    // Increase the thread's priority
    struct sched_param param;
    param.sched_priority = sched_get_priority_max(SCHED_FIFO);
    pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);

    PlayAttributes attrs = get_audio_play_attributes();
    int devID = attrs.devIDItem ? attrs.devIDItem->valueint : DEFAULT_AO_DEV_ID;
    int chnID = attrs.channel_idItem ? attrs.channel_idItem->valueint : DEFAULT_AO_CHN_ID;

    printf("[DEBUG] CHNID JSON: %d\n", chnID);

    initialize_audio_device(devID, chnID);  // Call initialize instead of reinitialize

    while (TRUE) {
        pthread_mutex_lock(&audio_buffer_lock);
        // Wait for new audio data
        while (audio_buffer_size == 0) {
            pthread_cond_wait(&audio_data_cond, &audio_buffer_lock);
        }

        IMPAudioFrame frm = {.virAddr = (uint32_t *)audio_buffer, .len = audio_buffer_size};
        if (IMP_AO_SendFrame(devID, chnID, &frm, BLOCK)) {
            pthread_mutex_unlock(&audio_buffer_lock);
            handle_and_reinitialize(devID, chnID, "IMP_AO_SendFrame data error");
            continue;
        }

        audio_buffer_size = 0;
        pthread_mutex_unlock(&audio_buffer_lock);
    }

    free_audio_play_attributes(&attrs);

    return NULL;
}
