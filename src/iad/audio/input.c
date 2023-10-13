#include <errno.h>          // for errno
#include <stdio.h>          // for NULL, ssize_t
#include <stdlib.h>         // for exit, free, EXIT_FAILURE
#include <unistd.h>         // for write
#include <pthread.h>        // for pthread_mutex_lock, pthread_mutex_unlock
#include "imp/imp_audio.h"  // for IMPAudioIOAttr, IMPAudioFrame, IMP_AI_Dis...
#include "imp/imp_log.h"    // for IMP_LOG_ERR
#include "audio_common.h"   // for AudioInputAttributes, PlayInputAttributes
#include "cJSON.h"          // for cJSON
#include "config.h"         // for is_valid_samplerate
#include "input.h"
#include "logging.h"        // for handle_audio_error
#include "utils.h"          // for ClientNode, client_list_head, compute_num...

#define TRUE 1
#define TAG "AI"

/**
 * Initializes the audio input device with the specified attributes.
 *
 * This function sets up the audio input device with attributes either
 * fetched from the configuration or defaults to pre-defined values.
 *
 * @param aiDevID Device ID.
 * @param aiChnID Channel ID.
 * @return 0 on success, -1 on failure.
 */
int initialize_audio_input_device(int aiDevID, int aiChnID) {
    int ret;
    IMPAudioIOAttr attr;
    AudioInputAttributes attrs = get_audio_input_attributes();

    attr.bitwidth = attrs.bitwidthItem ? string_to_bitwidth(attrs.bitwidthItem->valuestring) : AUDIO_BIT_WIDTH_16;
    attr.soundmode = attrs.soundmodeItem ? string_to_soundmode(attrs.soundmodeItem->valuestring) : AUDIO_SOUND_MODE_MONO;
    attr.frmNum = attrs.frmNumItem ? attrs.frmNumItem->valueint : DEFAULT_AI_FRM_NUM;

    // Validate and set samplerate for the audio device
    attr.samplerate = attrs.samplerateItem ? attrs.samplerateItem->valueint : DEFAULT_AI_SAMPLE_RATE;
    if (!is_valid_samplerate(attr.samplerate)) {
        IMP_LOG_ERR(TAG, "Invalid samplerate value: %d. Using default value: %d.\n", attr.samplerate, DEFAULT_AI_SAMPLE_RATE);
        attr.samplerate = DEFAULT_AI_SAMPLE_RATE;
    }

    attr.numPerFrm = compute_numPerFrm(attr.samplerate);

    int chnCnt = attrs.chnCntItem ? attrs.chnCntItem->valueint : DEFAULT_AI_CHN_CNT;
    if (chnCnt > 1) {
        IMP_LOG_ERR(TAG, "chnCnt value out of range: %d. Using default value: %d.\n", chnCnt, DEFAULT_AI_CHN_CNT);
        chnCnt = DEFAULT_AI_CHN_CNT;
    }
    attr.chnCnt = chnCnt;

    // Set public attribute of AI device
    ret = IMP_AI_SetPubAttr(aiDevID, &attr);
    if (ret != 0) {
        IMP_LOG_ERR(TAG, "IMP_AI_SetPubAttr failed");
        handle_audio_error(TAG, "Failed to initialize audio attributes");
        exit(EXIT_FAILURE);
    }

    // Enable AI device
    ret = IMP_AI_Enable(aiDevID);
    if (ret != 0) {
        IMP_LOG_ERR(TAG, "IMP_AI_Enable failed");
        exit(EXIT_FAILURE);
    }

    // Set audio frame depth attribute
    IMPAudioIChnParam chnParam;
    chnParam.usrFrmDepth = attrs.usrFrmDepthItem ? attrs.usrFrmDepthItem->valueint : DEFAULT_AI_USR_FRM_DEPTH;

    // Set audio channel attributes
    ret = IMP_AI_SetChnParam(aiDevID, aiChnID, &chnParam);
    if (ret != 0) {
        IMP_LOG_ERR(TAG, "IMP_AI_SetChnParam failed");
        exit(EXIT_FAILURE);
    }

    // Enable AI channel
    ret = IMP_AI_EnableChn(aiDevID, aiChnID);
    if (ret != 0) {
        IMP_LOG_ERR(TAG, "IMP_AI_EnableChn failed");
        exit(EXIT_FAILURE);
    }

    // Set volume and gain for the audio device
    int vol = attrs.SetVolItem ? attrs.SetVolItem->valueint : DEFAULT_AI_CHN_VOL;
    if (vol < -30 || vol > 120) {
        IMP_LOG_ERR(TAG, "SetVol value out of range: %d. Using default value: %d.\n", vol, DEFAULT_AI_CHN_VOL);
        vol = DEFAULT_AI_CHN_VOL;
    }
    if (IMP_AI_SetVol(aiDevID, aiChnID, vol)) {
        handle_audio_error("Failed to set volume attribute");
    }

    int gain = attrs.SetGainItem ? attrs.SetGainItem->valueint : DEFAULT_AI_GAIN;
    if (gain < 0 || gain > 31) {
        IMP_LOG_ERR(TAG, "SetGain value out of range: %d. Using default value: %d.\n", gain, DEFAULT_AI_GAIN);
        gain = DEFAULT_AI_GAIN;
    }
    if (IMP_AI_SetGain(aiDevID, aiChnID, gain)) {
        handle_audio_error("Failed to set gain attribute");
    }

    // Debugging prints
    printf("[INFO] AI samplerate: %d\n", attr.samplerate);
    printf("[INFO] AI Volume: %d\n", vol);
    printf("[INFO] AI Gain: %d\n", gain);

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
    printf("[INFO] [AI] Entering ai_record_thread\n");

    int ret;

    int aiDevID, aiChnID;
    get_audio_input_device_attributes(&aiDevID, &aiChnID);

    printf("[INFO] Sending audio data to input client\n");

    while (TRUE) {
        // Polling for frame
        ret = IMP_AI_PollingFrame(aiDevID, aiChnID, 1000);
        if (ret != 0) {
            IMP_LOG_ERR(TAG, "IMP_AI_PollingFrame failed");
            return NULL;
        }

        IMPAudioFrame frm;
        ret = IMP_AI_GetFrame(aiDevID, aiChnID, &frm, 1000);
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
        IMP_AI_ReleaseFrame(aiDevID, aiChnID, &frm);
    }

    return NULL;
}

int disable_audio_input() {
    int ret;

    int aiDevID, aiChnID;
    get_audio_input_device_attributes(&aiDevID, &aiChnID);

    ret = IMP_AI_DisableChn(aiDevID, aiChnID);
    if(ret != 0) {
        IMP_LOG_ERR(TAG, "Audio channel disable error\n");
        return -1;
    }
    ret = IMP_AI_Disable(aiDevID);
    if(ret != 0) {
        IMP_LOG_ERR(TAG, "Audio device disable error\n");
        return -1;
    }
    return 0;
}
