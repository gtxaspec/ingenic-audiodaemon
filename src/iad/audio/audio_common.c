#include <imp/imp_audio.h>  // for IMP_AO_ClearChnBuf, IMP_AO_FlushChnBuf
#include "audio_common.h"
#include "config.h"         // for get_audio_attribute, AUDIO_INPUT, AUDIO_O...
#include "logging.h"        // for handle_audio_error
#include "output.h"         // for DEFAULT_AO_CHN_ID, DEFAULT_AO_DEV_ID

/**
 * Fetches the audio input attributes from the configuration.
 *
 * This function retrieves various audio attributes such as sample rate,
 * bitwidth, soundmode, etc., from the configuration.
 *
 * @return A structure containing the audio input attributes.
 */
AudioInputAttributes get_audio_input_attributes() {
    AudioInputAttributes attrs;

    // Fetch each audio attribute from the configuration
    attrs.samplerateItem = get_audio_attribute(AUDIO_INPUT, "sample_rate");
    attrs.bitwidthItem = get_audio_attribute(AUDIO_INPUT, "bitwidth");
    attrs.soundmodeItem = get_audio_attribute(AUDIO_INPUT, "soundmode");
    attrs.frmNumItem = get_audio_attribute(AUDIO_INPUT, "frmNum");
    attrs.chnCntItem = get_audio_attribute(AUDIO_INPUT, "chnCnt");
    attrs.SetVolItem = get_audio_attribute(AUDIO_INPUT, "SetVol");
    attrs.SetGainItem = get_audio_attribute(AUDIO_INPUT, "SetGain");
    attrs.usrFrmDepthItem = get_audio_attribute(AUDIO_INPUT, "usrFrmDepth");

    return attrs;
}

/**
 * Frees the memory allocated for the audio input attributes.
 *
 * This function ensures that the memory allocated for each of the cJSON items
 * in the audio attributes structure is properly released.
 *
 * @param attrs Pointer to the audio input attributes structure.
 */
void free_audio_input_attributes(AudioInputAttributes *attrs) {
    cJSON_Delete(attrs->samplerateItem);
    cJSON_Delete(attrs->bitwidthItem);
    cJSON_Delete(attrs->soundmodeItem);
    cJSON_Delete(attrs->frmNumItem);
    cJSON_Delete(attrs->chnCntItem);
    cJSON_Delete(attrs->SetVolItem);
    cJSON_Delete(attrs->SetGainItem);
    cJSON_Delete(attrs->usrFrmDepthItem);
}

/**
 * Fetches the play attributes from the configuration.
 * @return A structure containing the play attributes.
 */
PlayInputAttributes get_audio_input_play_attributes() {
    PlayInputAttributes attrs;

    // Populate the structure with play attributes from the configuration
    attrs.device_idItem = get_audio_attribute(AUDIO_INPUT, "device_id");
    attrs.channel_idItem = get_audio_attribute(AUDIO_INPUT, "channel_id");

    return attrs;
}

/**
 * Frees the memory allocated for the play attributes.
 * @param attrs Pointer to the play attributes structure.
 */
void free_audio_input_play_attributes(PlayInputAttributes *attrs) {
    cJSON_Delete(attrs->device_idItem);
    cJSON_Delete(attrs->channel_idItem);
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
    attrs.aoDevIDItem = get_audio_attribute(AUDIO_OUTPUT, "device_id");
    attrs.channel_idItem = get_audio_attribute(AUDIO_OUTPUT, "channel_id");

    return attrs;
}

/**
 * Frees the memory allocated for the play attributes.
 * @param attrs Pointer to the play attributes structure.
 */
void free_audio_play_attributes(PlayAttributes *attrs) {
    cJSON_Delete(attrs->aoDevIDItem);
    cJSON_Delete(attrs->channel_idItem);
}

/**
 * Retrieves audio attributes (device and channel IDs) either from PlayAttributes or defaults.
 * @param aoDevID Pointer to store the retrieved Device ID.
 * @param aoChnID Pointer to store the retrieved Channel ID.
 */
void get_audio_device_attributes(int *aoDevID, int *aoChnID) {
    PlayAttributes attrs = get_audio_play_attributes();
    *aoDevID = attrs.aoDevIDItem ? attrs.aoDevIDItem->valueint : DEFAULT_AO_DEV_ID;
    *aoChnID = attrs.channel_idItem ? attrs.channel_idItem->valueint : DEFAULT_AO_CHN_ID;
}

// The following functions pause, clear, resume, flush, and mute the audio output respectively.

void pause_audio_output() {
    int aoDevID, aoChnID;
    get_audio_device_attributes(&aoDevID, &aoChnID);
    if (IMP_AO_PauseChn(aoDevID, aoChnID)) {
        handle_audio_error("AO: Failed to pause audio output");
    }
}

void clear_audio_output_buffer() {
    int aoDevID, aoChnID;
    get_audio_device_attributes(&aoDevID, &aoChnID);
    if (IMP_AO_ClearChnBuf(aoDevID, aoChnID)) {
        handle_audio_error("AO: Failed to clear audio output buffer");
    }
}

void resume_audio_output() {
    int aoDevID, aoChnID;
    get_audio_device_attributes(&aoDevID, &aoChnID);
    if (IMP_AO_ResumeChn(aoDevID, aoChnID)) {
        handle_audio_error("AO: Failed to resume audio output");
    }
}

void flush_audio_output_buffer() {
    int aoDevID, aoChnID;
    get_audio_device_attributes(&aoDevID, &aoChnID);
    if (IMP_AO_FlushChnBuf(aoDevID, aoChnID)) {
        handle_audio_error("AO: Failed to flush audio output buffer");
    }
}

void mute_audio_output_device(int mute_enable) {
    int aoDevID, aoChnID;
    get_audio_device_attributes(&aoDevID, &aoChnID);

    if (IMP_AO_SetVolMute(aoDevID, aoChnID, mute_enable)) {
        handle_audio_error("AO: Failed to mute audio output device");
    }
}
