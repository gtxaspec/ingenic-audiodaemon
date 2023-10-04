#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <imp/imp_audio.h>
#include <imp/imp_log.h>
#include "input.h"
#include "utils.h"
#include "config.h"
#include "logging.h"

#define TRUE 1
#define TAG "AI"

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
 * Initializes the audio input device with the specified attributes.
 *
 * This function sets up the audio input device with attributes either
 * fetched from the configuration or defaults to pre-defined values.
 *
 * @param devID Device ID.
 * @param chnID Channel ID.
 * @return 0 on success, -1 on failure.
 */
int initialize_audio_input_device(int devID, int chnID) {
    int ret;
    IMPAudioIOAttr attr;
    AudioInputAttributes attrs = get_audio_input_attributes();

    attr.bitwidth = attrs.bitwidthItem ? string_to_bitwidth(attrs.bitwidthItem->valuestring) : AUDIO_BIT_WIDTH_16;
    attr.soundmode = attrs.soundmodeItem ? string_to_soundmode(attrs.soundmodeItem->valuestring) : AUDIO_SOUND_MODE_MONO;
    attr.frmNum = attrs.frmNumItem ? attrs.frmNumItem->valueint : DEFAULT_AI_FRM_NUM;
    attr.samplerate = attrs.samplerateItem ? attrs.samplerateItem->valueint : DEFAULT_AI_SAMPLE_RATE;
    attr.numPerFrm = compute_numPerFrm(attr.samplerate);
    attr.chnCnt = attrs.chnCntItem ? attrs.chnCntItem->valueint : DEFAULT_AI_CHN_CNT;

    // Debugging prints
    printf("[DEBUG] AI samplerate: %d\n", attr.samplerate);
    printf("[DEBUG] AI bitwidth: %d\n", attr.bitwidth);
    printf("[DEBUG] AI soundmode: %d\n", attr.soundmode);
    printf("[DEBUG] AI frmNum: %d\n", attr.frmNum);
    printf("[DEBUG] AI numPerFrm: %d\n", attr.numPerFrm);
    printf("[DEBUG] AI chnCnt: %d\n", attr.chnCnt);

    // Set public attribute of AI device
    ret = IMP_AI_SetPubAttr(devID, &attr);
    if (ret != 0) {
        IMP_LOG_ERR(TAG, "IMP_AI_SetPubAttr failed");
        handle_audio_error(TAG, "Failed to initialize audio attributes");
	exit(EXIT_FAILURE);
    }

    // Enable AI device
    ret = IMP_AI_Enable(devID);
    if (ret != 0) {
        IMP_LOG_ERR(TAG, "IMP_AI_Enable failed");
	exit(EXIT_FAILURE);
    }

    // Set audio frame depth attribute
    IMPAudioIChnParam chnParam;
    chnParam.usrFrmDepth = attrs.usrFrmDepthItem ? attrs.usrFrmDepthItem->valueint : DEFAULT_AI_USR_FRM_DEPTH;

    // Set audio channel attributes
    ret = IMP_AI_SetChnParam(devID, chnID, &chnParam);
    if (ret != 0) {
        IMP_LOG_ERR(TAG, "IMP_AI_SetChnParam failed");
	exit(EXIT_FAILURE);
    }

    // Enable AI channel
    ret = IMP_AI_EnableChn(devID, chnID);
    if (ret != 0) {
        IMP_LOG_ERR(TAG, "IMP_AI_EnableChn failed");
	exit(EXIT_FAILURE);
    }

    // Set audio channel volume
    int vol = attrs.SetVolItem ? attrs.SetVolItem->valueint : DEFAULT_AI_CHN_VOL;
    ret = IMP_AI_SetVol(devID, chnID, vol);
    if (ret != 0) {
        IMP_LOG_ERR(TAG, "IMP_AI_SetVol failed");
        return -1;
    }

    // Set audio channel gain
    int gain = attrs.SetGainItem ? attrs.SetGainItem->valueint : DEFAULT_AI_GAIN;
    ret = IMP_AI_SetGain(devID, chnID, gain);
    if (ret != 0) {
        IMP_LOG_ERR(TAG, "IMP_AI_SetGain failed");
        return -1;
    }

    return 0;
}

/**
 * The main thread function for recording audio input.
 *
 * This function continuously records audio from the specified device and
 * channel, and sends the recorded data to connected clients. It handles
 * errors gracefully by releasing any acquired resources.
 *
 * @param arg Unused thread argument.
 * @return NULL.
 */
void *ai_record_thread(void *arg) {
    int ret;

    PlayInputAttributes attrs = get_audio_input_play_attributes();
    int devID = attrs.device_idItem ? attrs.device_idItem->valueint : DEFAULT_AI_DEV_ID;
    int chnID = attrs.channel_idItem ? attrs.channel_idItem->valueint : DEFAULT_AI_CHN_ID;

    printf("[INFO] Sending audio data to input client\n");

    while (TRUE) {
        // Polling for frame
        ret = IMP_AI_PollingFrame(devID, chnID, 1000);
        if (ret != 0) {
            IMP_LOG_ERR(TAG, "IMP_AI_PollingFrame failed");
            return NULL;
        }

        IMPAudioFrame frm;
        ret = IMP_AI_GetFrame(devID, chnID, &frm, 1000);
        if (ret != 0) {
            IMP_LOG_ERR(TAG, "IMP_AI_GetFrame failed");
            return NULL;
        }

        pthread_mutex_lock(&audio_buffer_lock);

        // Iterate over all clients and send the audio data
        ClientNode *current = client_list_head;
        while (current) {
            ssize_t wr_sock = write(current->sockfd, frm.virAddr, frm.len);

            if (wr_sock < 0) {
                if (errno == EPIPE) {
                    printf("[INFO] Client disconnected\n");
                } else {
		    handle_audio_error("AI: write to sockfd");
                }

                // Remove the client from the list
                if (current == client_list_head) {
                    client_list_head = current->next;
                    free(current);
                    current = client_list_head;
                } else {
                    ClientNode *temp = client_list_head;
                    while (temp->next != current) {
                        temp = temp->next;
                    }
                    temp->next = current->next;
                    free(current);
                    current = temp->next;
                }
                continue;
            }
            current = current->next;
        }

        pthread_mutex_unlock(&audio_buffer_lock);

        // Release audio frame
        IMP_AI_ReleaseFrame(devID, chnID, &frm);
    }

    return NULL;
}

int disable_audio_input() {
    int ret;

    PlayInputAttributes attrs = get_audio_input_play_attributes();
    int devID = attrs.device_idItem ? attrs.device_idItem->valueint : DEFAULT_AI_DEV_ID;
    int chnID = attrs.channel_idItem ? attrs.channel_idItem->valueint : DEFAULT_AI_CHN_ID;

    /* Disable the audio channel. */
    ret = IMP_AI_DisableChn(devID, chnID);
    if(ret != 0) {
        IMP_LOG_ERR(TAG, "Audio channel disable error\n");
	return -1;
    }
    /* Disable the audio devices. */
    ret = IMP_AI_Disable(devID);
    if(ret != 0) {
        IMP_LOG_ERR(TAG, "Audio device disable error\n");
	return -1;
    }
    return 0;
}
