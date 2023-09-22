#include "input.h"
#include "../utils/utils.h"
#include <imp/imp_audio.h>
#include <imp/imp_log.h>
#include <unistd.h>
#include <fcntl.h>

#define AI_BASIC_TEST_RECORD_NUM 500

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
    int chnVol = 60;
    ret = IMP_AI_SetVol(devID, chnID, chnVol);
    if (ret != 0) {
        IMP_LOG_ERR(TAG, "IMP_AI_SetVol failed");
        return -1;
    }

    // Set audio channel gain
    int aigain = 28;
    ret = IMP_AI_SetGain(devID, chnID, aigain);
    if (ret != 0) {
        IMP_LOG_ERR(TAG, "IMP_AI_SetGain failed");
        return -1;
    }

    return 0;
}

void *ai_record_thread(void *arg) {
    AiThreadArg *thread_arg = (AiThreadArg *) arg;
    int sockfd = thread_arg->sockfd;
    char *output_file_path = thread_arg->output_file_path;

    int ret;

    int fd = open(output_file_path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
        IMP_LOG_ERR(TAG, "Failed to open output file %s", output_file_path);
        return NULL;
    }

    int i = 0;
    while (i < AI_BASIC_TEST_RECORD_NUM) {
        // Polling for frame
        ret = IMP_AI_PollingFrame(0, 0, 1000);
        if (ret != 0) {
            IMP_LOG_ERR(TAG, "IMP_AI_PollingFrame failed");
            close(fd);
            return NULL;
        }

        IMPAudioFrame frm;
        ret = IMP_AI_GetFrame(0, 0, &frm, 1000);
        if (ret != 0) {
            IMP_LOG_ERR(TAG, "IMP_AI_GetFrame failed");
            close(fd);
            return NULL;
        }

        ssize_t wr_fd = write(fd, frm.virAddr, frm.len);       // Write the recorded audio data to the file
        ssize_t wr_sock = write(sockfd, frm.virAddr, frm.len);  // Send the recorded audio data to the client over the socket
        
        // Check for SIGPIPE or other errors
        if (wr_sock < 0) {
            perror("write to sockfd");
            IMP_AI_ReleaseFrame(0, 0, &frm);
            close(fd);
            return NULL;
        }

        if (wr_fd < 0) {
            perror("write to fd");
            IMP_AI_ReleaseFrame(0, 0, &frm);
            close(fd);
            return NULL;
        }

        IMP_AI_ReleaseFrame(0, 0, &frm);
        i++;
    }

    close(fd);
    return NULL;
}
