#include <pthread.h>        // for pthread_mutex_unlock, pthread_cond_wait
#include <sched.h>          // for sched_get_priority_max, SCHED_FIFO, sched...
#include <stdint.h>         // for uint32_t
#include <stdlib.h>         // for free, malloc, NULL
#include <stdio.h>          // for printf
#include "imp/imp_audio.h"  // for IMPAudioIOAttr, IMP_AO_Disable, IMP_AO_Di...
#include "imp/imp_log.h"    // for IMP_LOG_ERR
#include "audio_common.h"   // for AudioOutputAttributes, PlayAttributes
#include "config.h"         // for config_get_ao_frame_size, is_valid_sample...
#include "cJSON.h"          // for cJSON
#include "output.h"
#include "logging.h"        // for handle_audio_error
#include "utils.h"          // for audio_buffer, audio_buffer_lock, audio_bu...

#define TRUE 1
#define TAG "AO"

// Global variable to hold the maximum frame size for audio output.
int g_ao_max_frame_size = DEFAULT_AO_MAX_FRAME_SIZE;

/**
 * Set the global maximum frame size for audio output.
 * @param frame_size The desired frame size.
 */
void set_ao_max_frame_size(int frame_size) {
    g_ao_max_frame_size = frame_size;
}

/**
 * Handles errors and reinitializes the audio device.
 * @param aoDevID Device ID.
 * @param aoChnID Channel ID.
 * @param errorMsg Error message to be handled.
 */
void handle_and_reinitialize(int aoDevID, int aoChnID, const char *errorMsg) {
    handle_audio_error(errorMsg);
    reinitialize_audio_device(aoDevID, aoChnID);
}

/**
 * Initializes the audio device using the attributes from the configuration.
 * @param aoDevID Device ID.
 * @param aoChnID Channel ID.
 */
void initialize_audio_device(int aoDevID, int aoChnID) {
    IMPAudioIOAttr attr;
    AudioOutputAttributes attrs = get_audio_attributes();

    // Set audio attributes based on the configuration or default values
    attr.bitwidth = attrs.bitwidthItem ? string_to_bitwidth(attrs.bitwidthItem->valuestring) : AUDIO_BIT_WIDTH_16;
    attr.soundmode = attrs.soundmodeItem ? string_to_soundmode(attrs.soundmodeItem->valuestring) : AUDIO_SOUND_MODE_MONO;
    attr.frmNum = attrs.frmNumItem ? attrs.frmNumItem->valueint : DEFAULT_AO_FRM_NUM;

    // Validate and set samplerate for the audio device
    attr.samplerate = attrs.samplerateItem ? attrs.samplerateItem->valueint : DEFAULT_AO_SAMPLE_RATE;
    if (!is_valid_samplerate(attr.samplerate)) {
        IMP_LOG_ERR(TAG, "Invalid samplerate value: %d. Using default value: %d.\n", attr.samplerate, DEFAULT_AO_SAMPLE_RATE);
        attr.samplerate = DEFAULT_AO_SAMPLE_RATE;
    }

    attr.numPerFrm = compute_numPerFrm(attr.samplerate);

    int chnCnt = attrs.chnCntItem ? attrs.chnCntItem->valueint : DEFAULT_AO_CHN_CNT;
    if (chnCnt > 1) {
        IMP_LOG_ERR(TAG, "chnCnt value out of range: %d. Using default value: %d.\n", chnCnt, DEFAULT_AO_CHN_CNT);
        chnCnt = DEFAULT_AO_CHN_CNT;
    }
    attr.chnCnt = chnCnt;

    // Initialize the audio device
    if (IMP_AO_SetPubAttr(aoDevID, &attr) || IMP_AO_GetPubAttr(aoDevID, &attr) ||
        IMP_AO_Enable(aoDevID) || IMP_AO_EnableChn(aoDevID, aoChnID)) {
	handle_audio_error("AO: Failed to initialize audio attributes");
        exit(EXIT_FAILURE);
    }

    // Debugging prints
    printf("[DEBUG] aoChnID: %d\n", aoChnID);

    // Set volume and gain for the audio device
    int vol = attrs.SetVolItem ? attrs.SetVolItem->valueint : DEFAULT_AO_CHN_VOL;
    if (vol < -30 || vol > 120) {
        IMP_LOG_ERR(TAG, "SetVol value out of range: %d. Using default value: %d.\n", vol, DEFAULT_AO_CHN_VOL);
        vol = DEFAULT_AO_CHN_VOL;
    }
    if (IMP_AO_SetVol(aoDevID, aoChnID, vol)) {
        handle_audio_error("Failed to set volume attribute");
    }

    int gain = attrs.SetGainItem ? attrs.SetGainItem->valueint : DEFAULT_AO_GAIN;
    if (gain < 0 || gain > 31) {
        IMP_LOG_ERR(TAG, "SetGain value out of range: %d. Using default value: %d.\n", gain, DEFAULT_AO_GAIN);
        gain = DEFAULT_AO_GAIN;
    }
    if (IMP_AO_SetGain(aoDevID, aoChnID, gain)) {
        handle_audio_error("Failed to set gain attribute");
    }
    // Get frame size from config and set it
    int frame_size_from_config = config_get_ao_frame_size();
    set_ao_max_frame_size(frame_size_from_config);

    // Allocate memory for audio_buffer based on the frame size
    audio_buffer = (unsigned char*) malloc(g_ao_max_frame_size);
    if (!audio_buffer) {
        // Handle memory allocation failure
        handle_audio_error("AO: Failed to allocate memory for audio_buffer");
        exit(EXIT_FAILURE);
    }
}

/**
 * Cleans up resources used for audio output.
 * This primarily involves freeing the memory allocated for the audio buffer.
 */
void cleanup_audio_output() {
    if (audio_buffer) {
        free(audio_buffer);
        audio_buffer = NULL;
    }
}

/**
 * Reinitialize the audio device by first disabling it and then initializing.
 * @param aoDevID Device ID.
 * @param aoChnID Channel ID.
 */
void reinitialize_audio_device(int aoDevID, int aoChnID) {
    IMP_AO_DisableChn(aoDevID, aoChnID);
    IMP_AO_Disable(aoDevID);
    initialize_audio_device(aoDevID, aoChnID);
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
    int aoDevID = attrs.aoDevIDItem ? attrs.aoDevIDItem->valueint : DEFAULT_AO_DEV_ID;
    int aoChnID = attrs.channel_idItem ? attrs.channel_idItem->valueint : DEFAULT_AO_CHN_ID;

    printf("[DEBUG] aoChnID JSON: %d\n", aoChnID);

    // Initialize the audio device for playback
    initialize_audio_device(aoDevID, aoChnID);

    // Continuous loop to play audio
    while (TRUE) {
        pthread_mutex_lock(&audio_buffer_lock);

        // Wait until there's audio data in the buffer
        while (audio_buffer_size == 0) {
            pthread_cond_wait(&audio_data_cond, &audio_buffer_lock);
        }

        IMPAudioFrame frm = {.virAddr = (uint32_t *)audio_buffer, .len = audio_buffer_size};

        // Send the audio frame for playback
        if (IMP_AO_SendFrame(aoDevID, aoChnID, &frm, BLOCK)) {
            pthread_mutex_unlock(&audio_buffer_lock);
            handle_and_reinitialize(aoDevID, aoChnID, "IMP_AO_SendFrame data error");
            continue;
        }

        audio_buffer_size = 0;
        pthread_mutex_unlock(&audio_buffer_lock);
    }

    return NULL;
}

/**
 * Disables the audio channel and audio devices.
 * @return 0 on success, -1 on failure.
 */
int disable_audio_output() {
    int ret;

    int aoDevID, aoChnID;
    get_audio_device_attributes(&aoDevID, &aoChnID);

    /* Disable the audio channel */
    ret = IMP_AO_DisableChn(aoDevID, aoChnID);
    if (ret != 0) {
        IMP_LOG_ERR(TAG, "Audio channel disable error\n");
        return -1;
    }

    /* Disable the audio device */
    ret = IMP_AO_Disable(aoDevID);
    if (ret != 0) {
        IMP_LOG_ERR(TAG, "Audio device disable error\n");
        return -1;
    }

    cleanup_audio_output();

    return 0;
}
