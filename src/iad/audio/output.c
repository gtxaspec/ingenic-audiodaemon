#include <pthread.h>
#include <sched.h>
#include "output.h"
#include "utils.h"
#include "logging.h"
#include "config.h"

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
 * Retrieves audio attributes (device and channel IDs) either from PlayAttributes or defaults.
 * @param devID Pointer to store the retrieved Device ID.
 * @param chnID Pointer to store the retrieved Channel ID.
 */
void get_audio_device_attributes(int *devID, int *chnID) {
    PlayAttributes attrs = get_audio_play_attributes();
    *devID = attrs.devIDItem ? attrs.devIDItem->valueint : DEFAULT_AO_DEV_ID;
    *chnID = attrs.channel_idItem ? attrs.channel_idItem->valueint : DEFAULT_AO_CHN_ID;
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
    if (IMP_AO_SetPubAttr(devID, &attr) || IMP_AO_GetPubAttr(devID, &attr) ||
        IMP_AO_Enable(devID) || IMP_AO_EnableChn(devID, chnID)) {
	handle_audio_error("AO: Failed to initialize audio attributes");
        exit(EXIT_FAILURE);
    }

    // Debugging prints
    printf("[DEBUG] CHNID: %d\n", chnID);

    // Set volume and gain for the audio device
    int vol = attrs.SetVolItem ? attrs.SetVolItem->valueint : DEFAULT_AO_CHN_VOL;
    if (vol < -30 || vol > 120) {
        IMP_LOG_ERR(TAG, "SetVol value out of range: %d. Using default value: %d.\n", vol, DEFAULT_AO_CHN_VOL);
        vol = DEFAULT_AO_CHN_VOL;
    }
    if (IMP_AO_SetVol(devID, chnID, vol)) {
        handle_audio_error("Failed to set volume attribute");
    }

    int gain = attrs.SetGainItem ? attrs.SetGainItem->valueint : DEFAULT_AO_GAIN;
    if (gain < 0 || gain > 31) {
        IMP_LOG_ERR(TAG, "SetGain value out of range: %d. Using default value: %d.\n", gain, DEFAULT_AO_GAIN);
        gain = DEFAULT_AO_GAIN;
    }
    if (IMP_AO_SetGain(devID, chnID, gain)) {
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
 * @param devID Device ID.
 * @param chnID Channel ID.
 */
void reinitialize_audio_device(int devID, int chnID) {
    IMP_AO_DisableChn(devID, chnID);
    IMP_AO_Disable(devID);
    initialize_audio_device(devID, chnID);
}

// The following functions pause, clear, resume, flush, and mute the audio output respectively.

void pause_audio_output() {
    int devID, chnID;
    get_audio_device_attributes(&devID, &chnID);
    if (IMP_AO_PauseChn(devID, chnID)) {
        handle_audio_error("AO: Failed to pause audio output");
    }
}

void clear_audio_output_buffer() {
    int devID, chnID;
    get_audio_device_attributes(&devID, &chnID);
    if (IMP_AO_ClearChnBuf(devID, chnID)) {
        handle_audio_error("AO: Failed to clear audio output buffer");
    }
}

void resume_audio_output() {
    int devID, chnID;
    get_audio_device_attributes(&devID, &chnID);
    if (IMP_AO_ResumeChn(devID, chnID)) {
        handle_audio_error("AO: Failed to resume audio output");
    }
}

void flush_audio_output_buffer() {
    int devID, chnID;
    get_audio_device_attributes(&devID, &chnID);
    if (IMP_AO_FlushChnBuf(devID, chnID)) {
        handle_audio_error("AO: Failed to flush audio output buffer");
    }
}

void mute_audio_output_device(int mute_enable) {
    int devID, chnID;
    get_audio_device_attributes(&devID, &chnID);

    if (IMP_AO_SetVolMute(devID, chnID, mute_enable)) {
        handle_audio_error("AO: Failed to mute audio output device");
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

/**
 * Disables the audio channel and audio devices.
 * @return 0 on success, -1 on failure.
 */
int disable_audio_output() {
    int ret;

    int devID, chnID;
    get_audio_device_attributes(&devID, &chnID);

    /* Disable the audio channel */
    ret = IMP_AO_DisableChn(devID, chnID);
    if (ret != 0) {
        IMP_LOG_ERR(TAG, "Audio channel disable error\n");
        return -1;
    }

    /* Disable the audio device */
    ret = IMP_AO_Disable(devID);
    if (ret != 0) {
        IMP_LOG_ERR(TAG, "Audio device disable error\n");
        return -1;
    }

    cleanup_audio_output();

    return 0;
}
