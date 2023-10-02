#include <unistd.h>
#include <errno.h>
#include <imp/imp_audio.h>
#include <imp/imp_log.h>
#include "input.h"
#include "utils.h"
#include "config.h"

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
}

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
        free_audio_input_attributes(&attrs);
        return -1;
    }

    // Enable AI device
    ret = IMP_AI_Enable(devID);
    if (ret != 0) {
        IMP_LOG_ERR(TAG, "IMP_AI_Enable failed");
        free_audio_input_attributes(&attrs);
        return -1;
    }

    IMPAudioIChnParam chnParam;
    chnParam.usrFrmDepth = 40; // TODO: this should be configurable from json in the future

    // Set audio channel attribute
    ret = IMP_AI_SetChnParam(devID, chnID, &chnParam);
    if (ret != 0) {
        IMP_LOG_ERR(TAG, "IMP_AI_SetChnParam failed");
        free_audio_input_attributes(&attrs);
        return -1;
    }

    // Enable AI channel
    ret = IMP_AI_EnableChn(devID, chnID);
    if (ret != 0) {
        IMP_LOG_ERR(TAG, "IMP_AI_EnableChn failed");
        free_audio_input_attributes(&attrs);
        return -1;
    }

    // Set audio channel volume
    int vol = attrs.SetVolItem ? attrs.SetVolItem->valueint : DEFAULT_AI_CHN_VOL;
    ret = IMP_AI_SetVol(devID, chnID, vol);
    if (ret != 0) {
        IMP_LOG_ERR(TAG, "IMP_AI_SetVol failed");
        free_audio_input_attributes(&attrs);
        return -1;
    }

    // Set audio channel gain
    int gain = attrs.SetGainItem ? attrs.SetGainItem->valueint : DEFAULT_AI_GAIN;
    ret = IMP_AI_SetGain(devID, chnID, gain);
    if (ret != 0) {
        IMP_LOG_ERR(TAG, "IMP_AI_SetGain failed");
        free_audio_input_attributes(&attrs);
        return -1;
    }

    free_audio_input_attributes(&attrs);
    return 0;
}

void *ai_record_thread(void *arg) {
    int ret;

    cJSON* device_idItem = get_audio_attribute(AUDIO_INPUT, "device_id");
    int devID = device_idItem ? device_idItem->valueint : DEFAULT_AI_DEV_ID;

    cJSON* channel_idItem = get_audio_attribute(AUDIO_INPUT, "channel_id");
    int chnID = channel_idItem ? channel_idItem->valueint : DEFAULT_AI_CHN_ID;

    printf("[INFO] Sending audio data to input client\n");

    while (1) {
        // Polling for frame
        ret = IMP_AI_PollingFrame(devID, chnID, 1000);
        if (ret != 0) {
            IMP_LOG_ERR(TAG, "IMP_AI_PollingFrame failed");
            cJSON_Delete(device_idItem);
            cJSON_Delete(channel_idItem);
            return NULL;
        }

        IMPAudioFrame frm;
        ret = IMP_AI_GetFrame(devID, chnID, &frm, 1000);
        if (ret != 0) {
            IMP_LOG_ERR(TAG, "IMP_AI_GetFrame failed");
            cJSON_Delete(device_idItem);
            cJSON_Delete(channel_idItem);
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
                    perror("write to sockfd");
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

    cJSON_Delete(device_idItem);
    cJSON_Delete(channel_idItem);
    return NULL;
}
