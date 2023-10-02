#include <unistd.h>
#include <errno.h>
#include <imp/imp_audio.h>
#include <imp/imp_log.h>
#include "input.h"
#include "utils.h"
#include "config.h"

int initialize_audio_input_device(int devID, int chnID) {
    int ret;
    IMPAudioIOAttr attr;

    cJSON* samplerateItem = get_audio_attribute(AUDIO_INPUT, "sample_rate");
    cJSON* bitwidthItem = get_audio_attribute(AUDIO_INPUT, "bitwidth");
    cJSON* soundmodeItem = get_audio_attribute(AUDIO_INPUT, "soundmode");
    cJSON* frmNumItem = get_audio_attribute(AUDIO_INPUT, "frmNum");
    cJSON* chnCntItem = get_audio_attribute(AUDIO_INPUT, "chnCnt");
    cJSON* SetVolItem = get_audio_attribute(AUDIO_INPUT, "SetVol");
    cJSON* SetGainItem = get_audio_attribute(AUDIO_INPUT, "SetGain");

    attr.bitwidth = bitwidthItem ? string_to_bitwidth(bitwidthItem->valuestring) : AUDIO_BIT_WIDTH_16;
    attr.soundmode = soundmodeItem ? string_to_soundmode(soundmodeItem->valuestring) : AUDIO_SOUND_MODE_MONO;
    attr.frmNum = frmNumItem ? frmNumItem->valueint : DEFAULT_AI_FRM_NUM;
    attr.samplerate = samplerateItem ? samplerateItem->valueint : DEFAULT_AI_SAMPLE_RATE;
    attr.numPerFrm = compute_numPerFrm(attr.samplerate); // Updated this line to compute at runtime
    attr.chnCnt = chnCntItem ? chnCntItem->valueint : DEFAULT_AI_CHN_CNT;

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
        return -1;
    }

    // Enable AI device
    ret = IMP_AI_Enable(devID);
    if (ret != 0) {
        IMP_LOG_ERR(TAG, "IMP_AI_Enable failed");
        return -1;
    }

    IMPAudioIChnParam chnParam;
    // TODO: this should be configureable from json in the future
    chnParam.usrFrmDepth = 40;

    // Set audio channel attribute
    ret = IMP_AI_SetChnParam(devID, chnID, &chnParam);
    if (ret != 0) {
        IMP_LOG_ERR(TAG, "IMP_AI_SetChnParam failed");
        return -1;
    }

    // Enable AI channel
    ret = IMP_AI_EnableChn(devID, chnID);
    if (ret != 0) {
        IMP_LOG_ERR(TAG, "IMP_AI_EnableChn failed");
        return -1;
    }

    // Set audio channel volume
    int vol = SetVolItem ? SetVolItem->valueint : DEFAULT_AI_CHN_VOL;

    ret = IMP_AI_SetVol(devID, chnID, vol);
    if (ret != 0) {
        IMP_LOG_ERR(TAG, "IMP_AI_SetVol failed");
        return -1;
    }

    // Set audio channel gain
    int gain = SetGainItem ? SetGainItem->valueint : DEFAULT_AI_GAIN;
    ret = IMP_AI_SetGain(devID, chnID, gain);
    if (ret != 0) {
        IMP_LOG_ERR(TAG, "IMP_AI_SetGain failed");
        return -1;
    }

    return 0;
}

void *ai_record_thread(void *arg) {
    int ret;

    cJSON* device_idItem = get_audio_attribute(AUDIO_INPUT, "device_id");
    int devID = device_idItem ? device_idItem->valueint : DEFAULT_AI_DEV_ID;

    cJSON* channel_idItem = get_audio_attribute(AUDIO_INPUT, "channel_id");
    int chnID = channel_idItem ? channel_idItem->valueint : DEFAULT_AI_CHN_ID;

    printf("[INFO] Sending audio data to input client\n");

    while (1) {  // Infinite loop
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

    // This part might never be reached unless there's a mechanism to break the loop.
    printf("[INFO] Input Client Disconnected\n");
    return NULL;
}
