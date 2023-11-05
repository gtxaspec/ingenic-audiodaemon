#include <errno.h>          // for errno
#include <stdio.h>          // for NULL, ssize_t
#include <stdlib.h>         // for exit, free, EXIT_FAILURE
#include <unistd.h>         // for write
#include <pthread.h>        // for pthread_mutex_lock, pthread_mutex_unlock
#include <bits/errno.h>     // for EPIPE
#include "imp/imp_audio.h"  // for IMPAudioIOAttr, IMPAudioFrame, IMP_AI_Dis...
#include "imp/imp_log.h"    // for IMP_LOG_ERR
#include "audio_common.h"   // for AudioInputAttributes, PlayInputAttributes
#include "cJSON.h"          // for cJSON
#include "config.h"         // for is_valid_samplerate
#include "input.h"
#include "logging.h"        // for handle_audio_error
#include "utils.h"          // for ClientNode, client_list_head, compute_num...

#define TAG "IMP"

int aoDevID, aoChnID, aiDevID, aiChnID;
//get_audio_output_device_attributes(&aoDevID, &aoChnID);
//get_audio_input_device_attributes(&aiDevID, &aiChnID);


int ai_device(int enable) {
    int ret;

    if (enable) {
        ret = IMP_AI_Enable(aiDevID);
        if (ret != 0) {
            fprintf(stderr, "AI: Failed to enable input device %d\n", aiDevID);
            return -1;
        }
    } else {
        ret = IMP_AI_Disable(aiDevID);
        if (ret != 0) {
            fprintf(stderr, "AI: Failed to disable input device %d\n", aiDevID);
            return -1;
        }
    }
    return 0;
}

int ai_channel(int enable) {
    int ret;

    if (enable) {
        ret = IMP_AI_EnableChn(aiDevID, aiChnID);
        if (ret != 0) {
            fprintf(stderr, "AI: Failed to enable input channel %d\n", aiChnID);
            return -1;
        }
    } else {
        ret = IMP_AI_DisableChn(aiDevID, aiChnID);
        if (ret != 0) {
            fprintf(stderr, "AI: Failed to disable input channel %d\n", aiChnID);
            return -1;
        }
    }
    return 0;
}

int ai_aec(int enable) {
    int ret;

    if (enable) {
        ret = IMP_AI_EnableAec(aiDevID, aiChnID, aoDevID, aoChnID);
        if (ret != 0) {
            fprintf(stderr, "AI: Failed to enable input AEC on %d\n", aiChnID);
            return -1;
        }
    } else {
        ret = IMP_AI_DisableAec(aiDevID, aiChnID);
        if (ret != 0) {
            fprintf(stderr, "AI: Failed to disable input AEC on %d\n", aiChnID);
            return -1;
        }
    }
    return 0;
}

int ai_ref_frame(int enable) {
    int ret;

    if (enable) {
        ret = IMP_AI_EnableAecRefFrame(aiDevID, aiChnID, aoDevID, aoChnID);
        if (ret != 0) {
            fprintf(stderr, "AI: Failed to enable input AEC reference frame on %d\n", aiChnID);
            return -1;
        }
    } else {
        ret = IMP_AI_DisableAecRefFrame(aiDevID, aiChnID, aoDevID, aoChnID);
        if (ret != 0) {
            fprintf(stderr, "AI: Failed to disable input AEC reference frame on %d\n", aiChnID);
            return -1;
        }
    }
    return 0;
}

int ai_set_volume(int aiVol) {
    int ret = IMP_AI_SetVol(aiDevID, aiChnID, aiVol);
    if (ret != 0) {
        fprintf(stderr, "AI: Failed to set input volume on %d\n", aiChnID);
        return -1;
    }
    return 0;
}

int ai_get_volume(void) {
    int aiVol;
    int ret = IMP_AI_GetVol(aiDevID, aiChnID, &aiVol);
    if (ret != 0) {
        fprintf(stderr, "AI: Failed to get input volume on %d\n", aiChnID);
        exit(EXIT_FAILURE);
    }
    return aiVol;
}


int ai_set_gain(int aiGain) {
    int ret = IMP_AI_SetGain(aiDevID, aiChnID, aiGain);
    if (ret != 0) {
        fprintf(stderr, "AI: Failed to set input gain on %d\n", aiChnID);
        return -1;
    }
    return 0;
}

int ai_get_gain(void) {
    int aiGain;
    int ret = IMP_AI_GetGain(aiDevID, aiChnID, &aiGain);
    if (ret != 0) {
        fprintf(stderr, "AI: Failed to get input gain on %d\n", aiChnID);
        exit(EXIT_FAILURE);
    }
    return aiGain;
}

int ai_set_mute(int enable) {
    int ret = IMP_AI_SetVolMute(aiDevID, aiChnID, enable);
    if (ret != 0) {
        fprintf(stderr, "AI: Failed to set input mute on %d\n", aiChnID);
        return -1;
    }
    return 0;
}

int ai_set_alc_gain(int aiPgaGain) {
    int ret = IMP_AI_SetAlcGain(aiDevID, aiChnID, aiPgaGain);
    if (ret != 0) {
        fprintf(stderr, "AI: Failed to set input alc gain on %d\n", aiChnID);
        return -1;
    }
    return 0;
}  

int ai_get_alc_gain(void) {
    int aiPgaGain;
    int ret = IMP_AI_GetAlcGain(aiDevID, aiChnID, &aiPgaGain);
    if (ret != 0) {
        fprintf(stderr, "AI: Failed to get input alc gain on %d\n", aiChnID);
        exit(EXIT_FAILURE);
    }
    return aiPgaGain;
}
