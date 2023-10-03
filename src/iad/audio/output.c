#include <pthread.h>
#include <sched.h>
#include "output.h"
#include "utils.h"
#include "logging.h"
#include "config.h"

#define TRUE 1
#define TAG "AO"

/**
 * Fetches the audio attributes from the configuration.
 * @return A structure containing the audio attributes.
 */
AudioOutputAttributes get_audio_attributes() {
    AudioOutputAttributes attrs;

    // Populate the structure with audio attributes from the configuration
    attrs.samplerateItem = get_audio_attribute(AUDIO_OUTPUT, "sample_rate");
    attrs.bitwidthItem = get_audio_attribute(AUDIO_OUTPUT, "bitwidth");
    attrs.soundmodeItem = get_audio_attribute(AUDIO_OUTPUT, "soundmode");
    attrs.frmNumItem = get_audio_attribute(AUDIO_OUTPUT, "frmNum");
    attrs.chnCntItem = get_audio_attribute(AUDIO_OUTPUT, "chnCnt");
    attrs.SetVolItem = get_audio_attribute(AUDIO_OUTPUT, "SetVol");
    attrs.SetGainItem = get_audio_attribute(AUDIO_OUTPUT, "SetGain");

    return attrs;
}

/**
 * Frees the memory allocated for the audio attributes.
 * @param attrs Pointer to the audio attributes structure.
 */
void free_audio_attributes(AudioOutputAttributes *attrs) {
    cJSON_Delete(attrs->samplerateItem);
    cJSON_Delete(attrs->bitwidthItem);
    cJSON_Delete(attrs->soundmodeItem);
    cJSON_Delete(attrs->frmNumItem);
    cJSON_Delete(attrs->chnCntItem);
    cJSON_Delete(attrs->SetVolItem);
    cJSON_Delete(attrs->SetGainItem);
}

/**
 * Fetches the play attributes from the configuration.
 * @return A structure containing the play attributes.
 */
PlayAttributes get_audio_play_attributes() {
    PlayAttributes attrs;

    // Populate the structure with play attributes from the configuration
    attrs.devIDItem = get_audio_attribute(AUDIO_OUTPUT, "device_id");
    attrs.channel_idItem = get_audio_attribute(AUDIO_OUTPUT, "channel_id");

    return attrs;
}

/**
 * Frees the memory allocated for the play attributes.
 * @param attrs Pointer to the play attributes structure.
 */
void free_audio_play_attributes(PlayAttributes *attrs) {
    cJSON_Delete(attrs->devIDItem);
    cJSON_Delete(attrs->channel_idItem);
}

/**
 * Handles errors and reinitializes the audio device.
 * @param devID Device ID.
 * @param chnID Channel ID.
 * @param errorMsg Error message to be handled.
 */
void handle_and_reinitialize(int devID, int chnID, const char *errorMsg) {
    handle_audio_error(errorMsg);
    reinitialize_audio_device(devID, chnID);
}

/**
 * Initializes the audio device using the attributes from the configuration.
 * @param devID Device ID.
 * @param chnID Channel ID.
 */
void initialize_audio_device(int devID, int chnID) {
    IMPAudioIOAttr attr;
    AudioOutputAttributes attrs = get_audio_attributes();

    // Set audio attributes based on the configuration or default values
    attr.bitwidth = attrs.bitwidthItem ? string_to_bitwidth(attrs.bitwidthItem->valuestring) : AUDIO_BIT_WIDTH_16;
    attr.soundmode = attrs.soundmodeItem ? string_to_soundmode(attrs.soundmodeItem->valuestring) : AUDIO_SOUND_MODE_MONO;
    attr.frmNum = attrs.frmNumItem ? attrs.frmNumItem->valueint : DEFAULT_AO_FRM_NUM;
    attr.samplerate = attrs.samplerateItem ? attrs.samplerateItem->valueint : DEFAULT_AO_SAMPLE_RATE;
    attr.numPerFrm = compute_numPerFrm(attr.samplerate);
    attr.chnCnt = attrs.chnCntItem ? attrs.chnCntItem->valueint : DEFAULT_AO_CHN_CNT;

    // Initialize the audio device
    if (IMP_AO_SetPubAttr(devID, &attr) || IMP_AO_GetPubAttr(devID, &attr) ||
        IMP_AO_Enable(devID) || IMP_AO_EnableChn(devID, chnID)) {
	handle_audio_error("AO: Failed to initialize audio attributes");
        exit(EXIT_FAILURE);
    }

    // Debugging prints
    printf("[DEBUG] CHNID: %d\n", chnID);

    // Set volume and gain for the audio device
    if (IMP_AO_SetVol(devID, chnID, attrs.SetVolItem ? attrs.SetVolItem->valueint : DEFAULT_AO_CHN_VOL) ||
        IMP_AO_SetGain(devID, chnID, attrs.SetGainItem ? attrs.SetGainItem->valueint : DEFAULT_AO_GAIN)) {
        handle_and_reinitialize(devID, chnID, "Failed to set volume or gain attributes");
    }
}

/**
 * Reinitialize the audio device by first disabling it and then initializing.
 * @param devID Device ID.
 * @param chnID Channel ID.
 */
void reinitialize_audio_device(int devID, int chnID) {
    IMP_AO_DisableChn(devID, chnID);
    IMP_AO_Disable(devID);
    initialize_audio_device(devID, chnID);
}

// The following functions pause, clear, resume, and flush the audio output respectively.

void pause_audio_output(int devID, int chnID) {
    if (IMP_AO_PauseChn(devID, chnID)) {
        handle_audio_error("AO: Failed to pause audio output");
    }
}

void clear_audio_output_buffer(int devID, int chnID) {
    if (IMP_AO_ClearChnBuf(devID, chnID)) {
        handle_audio_error("AO: Failed to clear audio output buffer");
    }
}

void resume_audio_output(int devID, int chnID) {
    if (IMP_AO_ResumeChn(devID, chnID)) {
        handle_audio_error("AO: Failed to resume audio output");
    }
}

void flush_audio_output_buffer(int devID, int chnID) {
    if (IMP_AO_FlushChnBuf(devID, chnID)) {
        handle_audio_error("AO: Failed to flush audio output buffer");
    }
}

/**
 * Thread function to continuously play audio.
 * @param arg Thread arguments.
 * @return NULL.
 */
void *ao_test_play_thread(void *arg) {
    // Boost the thread priority for real-time audio playback
    struct sched_param param;
    param.sched_priority = sched_get_priority_max(SCHED_FIFO);
    pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);

    PlayAttributes attrs = get_audio_play_attributes();
    int devID = attrs.devIDItem ? attrs.devIDItem->valueint : DEFAULT_AO_DEV_ID;
    int chnID = attrs.channel_idItem ? attrs.channel_idItem->valueint : DEFAULT_AO_CHN_ID;

    printf("[DEBUG] CHNID JSON: %d\n", chnID);

    // Initialize the audio device for playback
    initialize_audio_device(devID, chnID);

    // Continuous loop to play audio
    while (TRUE) {
        pthread_mutex_lock(&audio_buffer_lock);

        // Wait until there's audio data in the buffer
        while (audio_buffer_size == 0) {
            pthread_cond_wait(&audio_data_cond, &audio_buffer_lock);
        }

        IMPAudioFrame frm = {.virAddr = (uint32_t *)audio_buffer, .len = audio_buffer_size};

        // Send the audio frame for playback
        if (IMP_AO_SendFrame(devID, chnID, &frm, BLOCK)) {
            pthread_mutex_unlock(&audio_buffer_lock);
            handle_and_reinitialize(devID, chnID, "IMP_AO_SendFrame data error");
            continue;
        }

        audio_buffer_size = 0;
        pthread_mutex_unlock(&audio_buffer_lock);
    }

    return NULL;
}
