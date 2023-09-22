#include "input.h"
#include "../utils/utils.h"
#include <imp/imp_audio.h>
#include <imp/imp_log.h>
#include <unistd.h>
#include <errno.h>

int initialize_audio_input_device(int devID) {
    int ret;
    IMPAudioIOAttr attr;

    // Set public attribute of AI device
    attr.samplerate = AUDIO_SAMPLE_RATE_16000;
    attr.bitwidth = AUDIO_BIT_WIDTH_16;
    attr.soundmode = AUDIO_SOUND_MODE_MONO;
    attr.frmNum = 40;
    attr.numPerFrm = 640;
    attr.chnCnt = 1;
    
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

    int chnID = 0;
    IMPAudioIChnParam chnParam;
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
    ret = IMP_AI_SetVol(devID, chnID, AI_CHN_VOL);
    if (ret != 0) {
        IMP_LOG_ERR(TAG, "IMP_AI_SetVol failed");
        return -1;
    }

    // Set audio channel gain
    ret = IMP_AI_SetGain(devID, chnID, AI_GAIN);
    if (ret != 0) {
        IMP_LOG_ERR(TAG, "IMP_AI_SetGain failed");
        return -1;
    }

    return 0;
}

void *ai_record_thread(void *arg) {
    AiThreadArg *thread_arg = (AiThreadArg *) arg;
    int sockfd = thread_arg->sockfd;

    int ret;

    printf("[INFO] Sending audio data to input client\n");  // Moved out of the loop

    while (1) {  // Infinite loop
        // Polling for frame
        ret = IMP_AI_PollingFrame(0, 0, 1000);
        if (ret != 0) {
            IMP_LOG_ERR(TAG, "IMP_AI_PollingFrame failed");
            return NULL;
        }

        IMPAudioFrame frm;
        ret = IMP_AI_GetFrame(0, 0, &frm, 1000);
        if (ret != 0) {
            IMP_LOG_ERR(TAG, "IMP_AI_GetFrame failed");
            return NULL;
        }

        ssize_t wr_sock = write(sockfd, frm.virAddr, frm.len);  // Send the recorded audio data to the client over the socket

        // Check for SIGPIPE or other errors
       if (wr_sock < 0) {
           if (errno == EPIPE) {
               printf("[INFO] Client disconnected\n");
           } else {
               perror("write to sockfd");
       }
       IMP_AI_ReleaseFrame(0, 0, &frm);
       return NULL;
}

        IMP_AI_ReleaseFrame(0, 0, &frm);
    }

    // This part might never be reached unless there's a mechanism to break the loop.
    printf("[INFO] Input Client Disconnected\n");
    return NULL;
}
