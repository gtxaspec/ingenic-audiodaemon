#include <pthread.h>
#include <sched.h>
#include "output.h"
#include "utils.h"
#include "logging.h"
#include "config.h"

/* function seems to be error-tolerant. If there's an error in setting audio attributes, the function prints an error and continues.
consider whether we want to continue executing the function or return early when an error is encountered. */

void reinitialize_audio_device(int devID, int chnID) {
    // Create another function to initialize, at boot.

    IMP_AO_DisableChn(devID, chnID);
    IMP_AO_Disable(devID);

    IMPAudioIOAttr attr;

    cJSON* samplerateItem = get_audio_attribute(AUDIO_OUTPUT, "sample_rate");
    cJSON* bitwidthItem = get_audio_attribute(AUDIO_OUTPUT, "bitwidth");
    cJSON* soundmodeItem = get_audio_attribute(AUDIO_OUTPUT, "soundmode");
    cJSON* frmNumItem = get_audio_attribute(AUDIO_OUTPUT, "frmNum");
    cJSON* chnCntItem = get_audio_attribute(AUDIO_OUTPUT, "chnCnt");


    cJSON* SetVolItem = get_audio_attribute(AUDIO_OUTPUT, "SetVol");
    cJSON* SetGainItem = get_audio_attribute(AUDIO_OUTPUT, "SetGain");

    attr.bitwidth = bitwidthItem ? string_to_bitwidth(bitwidthItem->valuestring) : AUDIO_BIT_WIDTH_16;
    attr.soundmode = soundmodeItem ? string_to_soundmode(soundmodeItem->valuestring) : AUDIO_SOUND_MODE_MONO;
    attr.frmNum = frmNumItem ? frmNumItem->valueint : DEFAULT_AO_FRM_NUM;
    attr.samplerate = samplerateItem ? samplerateItem->valueint : DEFAULT_AO_SAMPLE_RATE;
    attr.numPerFrm = compute_numPerFrm(attr.samplerate); // Updated this line to compute at runtime
    attr.chnCnt = chnCntItem ? chnCntItem->valueint : DEFAULT_AO_CHN_CNT;

    if (IMP_AO_SetPubAttr(devID, &attr) || IMP_AO_GetPubAttr(devID, &attr) || IMP_AO_Enable(devID) || IMP_AO_EnableChn(devID, chnID)) {
        handle_audio_error("Failed to reinitialize audio attributes");
    }
	    printf("[DEBUG] CHNID: %d\n", chnID);

    // Set chnVol and aogain again
    if (IMP_AO_SetVol(devID, chnID, SetVolItem ? SetVolItem->valueint : DEFAULT_AO_CHN_VOL) || IMP_AO_SetGain(devID, chnID, SetGainItem ? SetGainItem->valueint : DEFAULT_AO_GAIN)) {

        handle_audio_error("Failed to set volume or gain attributes");
    }

    printf("[DEBUG] samplerate: %d\n", attr.samplerate);
    printf("[DEBUG] bitwidth: %d\n", attr.bitwidth);
    printf("[DEBUG] soundmode: %d\n", attr.soundmode);
    printf("[DEBUG] frmNum: %d\n", attr.frmNum);
    printf("[DEBUG] numPerFrm: %d\n", attr.numPerFrm);
    printf("[DEBUG] chnCnt: %d\n", attr.chnCnt);

}

void pause_audio_output(int devID, int chnID) {
    if (IMP_AO_PauseChn(devID, chnID)) {
        handle_audio_error("Failed to pause audio output");
        reinitialize_audio_device(devID, chnID);
    }
}

void clear_audio_output_buffer(int devID, int chnID) {
    if (IMP_AO_ClearChnBuf(devID, chnID)) {
        handle_audio_error("Failed to clear audio output buffer");
        reinitialize_audio_device(devID, chnID);
    }
}

void resume_audio_output(int devID, int chnID) {
    if (IMP_AO_ResumeChn(devID, chnID)) {
        handle_audio_error("Failed to resume audio output");
        reinitialize_audio_device(devID, chnID);
    }
}

void flush_audio_output_buffer(int devID, int chnID) {
    if (IMP_AO_FlushChnBuf(devID, chnID)) {
        handle_audio_error("Failed to flush audio output buffer");
        reinitialize_audio_device(devID, chnID);
    }
}

void *ao_test_play_thread(void *arg) {
    // Increase the thread's priority
    struct sched_param param;
    param.sched_priority = sched_get_priority_max(SCHED_FIFO);
    pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);

    cJSON* devIDItem = get_audio_attribute(AUDIO_OUTPUT, "device_id");

    int devID = devIDItem ? devIDItem->valueint : DEFAULT_AO_GAIN;

    int chnID;

    cJSON* channel_idItem = get_audio_attribute(AUDIO_OUTPUT, "channel_id");
    chnID = channel_idItem ? channel_idItem->valueint : DEFAULT_AO_CHN_ID;
	    printf("[DEBUG] CHNID JSON: %d\n", chnID);

    reinitialize_audio_device(devID, chnID);

    while (1) {
        pthread_mutex_lock(&audio_buffer_lock);
        // Wait for new audio data
        while (audio_buffer_size == 0) {
            pthread_cond_wait(&audio_data_cond, &audio_buffer_lock);
        }

        IMPAudioFrame frm = {.virAddr = (uint32_t *)audio_buffer, .len = audio_buffer_size};
        if (IMP_AO_SendFrame(devID, chnID, &frm, BLOCK)) {
            pthread_mutex_unlock(&audio_buffer_lock);
            handle_audio_error("IMP_AO_SendFrame data error");
            reinitialize_audio_device(devID, chnID);
            continue;
        }
        audio_buffer_size = 0;
        pthread_mutex_unlock(&audio_buffer_lock);
    }
    return NULL;
}
