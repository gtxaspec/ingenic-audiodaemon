#include <imp/imp_audio.h>
#include "audio_common.h"
#include "config.h"
#include "logging.h"
#include "output.h"

/**
 * Helper function to get a specific audio attribute from the configuration.
 * @param type The type of audio (e.g., AUDIO_INPUT or AUDIO_OUTPUT).
 * @param attr The specific attribute name.
 * @return The cJSON item for the requested attribute.
 */
static cJSON* get_audio_device_attribute(AudioType type, const char *attr) {
    return get_audio_attribute(type, attr);
}

/**
 * Helper function to free the memory for a specific audio attribute.
 * @param item The cJSON item to be freed.
 */
static void free_audio_device_attribute(cJSON *item) {
    if (item) {
        cJSON_Delete(item);
    }
}

/**
 * Retrieves the device and channel IDs for audio input or uses defaults.
 * @param aiDevID Pointer to store the retrieved Device ID.
 * @param aiChnID Pointer to store the retrieved Channel ID.
 */
void get_audio_input_device_attributes(int *aiDevID, int *aiChnID) {
    cJSON *aiDevIDItem = get_audio_device_attribute(AUDIO_INPUT, "device_id");
    cJSON *aiChnIDItem = get_audio_device_attribute(AUDIO_INPUT, "channel_id");
    *aiDevID = aiDevIDItem ? aiDevIDItem->valueint : DEFAULT_AI_DEV_ID;
    *aiChnID = aiChnIDItem ? aiChnIDItem->valueint : DEFAULT_AI_CHN_ID;
}

/**
 * Retrieves the device and channel IDs for audio output or uses defaults.
 * @param aoDevID Pointer to store the retrieved Device ID.
 * @param aoChnID Pointer to store the retrieved Channel ID.
 */
void get_audio_output_device_attributes(int *aoDevID, int *aoChnID) {
    cJSON *aoDevIDItem = get_audio_device_attribute(AUDIO_OUTPUT, "device_id");
    cJSON *aoChnIDItem = get_audio_device_attribute(AUDIO_OUTPUT, "channel_id");
    *aoDevID = aoDevIDItem ? aoDevIDItem->valueint : DEFAULT_AO_DEV_ID;
    *aoChnID = aoChnIDItem ? aoChnIDItem->valueint : DEFAULT_AO_CHN_ID;
}

/**
 * Fetches the audio input attributes from the configuration.
 * @return A structure containing the audio input attributes.
 */
AudioInputAttributes get_audio_input_attributes() {
    AudioInputAttributes attrs;
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
 * Fetches the audio output attributes from the configuration.
 * @return A structure containing the audio output attributes.
 */
AudioOutputAttributes get_audio_attributes() {
    AudioOutputAttributes attrs;
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
 * Frees the memory allocated for the audio output attributes.
 * @param attrs Pointer to the audio output attributes structure.
 */
void free_audio_output_attributes(AudioOutputAttributes *attrs) {
    cJSON_Delete(attrs->samplerateItem);
    cJSON_Delete(attrs->bitwidthItem);
    cJSON_Delete(attrs->soundmodeItem);
    cJSON_Delete(attrs->frmNumItem);
    cJSON_Delete(attrs->chnCntItem);
    cJSON_Delete(attrs->SetVolItem);
    cJSON_Delete(attrs->SetGainItem);
}

/**
 * Pauses the audio output.
 */
void pause_audio_output() {
    int aoDevID, aoChnID;
    get_audio_output_device_attributes(&aoDevID, &aoChnID);
    if (IMP_AO_PauseChn(aoDevID, aoChnID)) {
        handle_audio_error("AO: Failed to pause audio output");
    }
}

/**
 * Clears the audio output buffer.
 */
void clear_audio_output_buffer() {
    int aoDevID, aoChnID;
    get_audio_output_device_attributes(&aoDevID, &aoChnID);
    if (IMP_AO_ClearChnBuf(aoDevID, aoChnID)) {
        handle_audio_error("AO: Failed to clear audio output buffer");
    }
}

/**
 * Resumes the audio output.
 */
void resume_audio_output() {
    int aoDevID, aoChnID;
    get_audio_output_device_attributes(&aoDevID, &aoChnID);
    if (IMP_AO_ResumeChn(aoDevID, aoChnID)) {
        handle_audio_error("AO: Failed to resume audio output");
    }
}

/**
 * Flushes the audio output buffer.
 */
void flush_audio_output_buffer() {
    int aoDevID, aoChnID;
    get_audio_output_device_attributes(&aoDevID, &aoChnID);
    if (IMP_AO_FlushChnBuf(aoDevID, aoChnID)) {
        handle_audio_error("AO: Failed to flush audio output buffer");
    }
}

/**
 * Mutes or unmutes the audio output device based on the given parameter.
 * @param mute_enable If set to non-zero, mutes the device. If zero, unmutes it.
 */
void mute_audio_output_device(int mute_enable) {
    int aoDevID, aoChnID;
    get_audio_output_device_attributes(&aoDevID, &aoChnID);
    if (IMP_AO_SetVolMute(aoDevID, aoChnID, mute_enable)) {
        handle_audio_error("AO: Failed to mute audio output device");
    }
}
