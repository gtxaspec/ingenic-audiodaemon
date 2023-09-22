#include "output.h"
#include <pthread.h>
#include <sched.h>
#include "../utils/utils.h"
#include "../utils/logging.h"

/* function seems to be error-tolerant. If there's an error in setting audio attributes, the function prints an error and continues.
consider whether we want to continue executing the function or return early when an error is encountered. */

void reinitialize_audio_device(int devID) {
    IMP_AO_DisableChn(devID, 0);
    IMP_AO_Disable(devID);

    // Initialize the audio device again
    IMPAudioIOAttr attr = {
        .samplerate = AO_SAMPLE_RATE,
        .bitwidth = AUDIO_BIT_WIDTH_16,
        .soundmode = AUDIO_SOUND_MODE_MONO,
        .frmNum = 20,
        .numPerFrm = AO_NUM_PER_FRM,
        .chnCnt = 1
    };

    if (IMP_AO_SetPubAttr(devID, &attr) || IMP_AO_GetPubAttr(devID, &attr) || IMP_AO_Enable(devID) || IMP_AO_EnableChn(devID, 0)) {
        handle_audio_error("Failed to reinitialize audio attributes");
    }

    // Set chnVol and aogain again
    if (IMP_AO_SetVol(devID, 0, CHN_VOL) || IMP_AO_SetGain(devID, 0, AO_GAIN)) {
        handle_audio_error("Failed to set volume or gain attributes");
    }
}

void *ao_test_play_thread(void *arg) {
    // Increase the thread's priority
    struct sched_param param;
    param.sched_priority = sched_get_priority_max(SCHED_FIFO);
    pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);

    int devID = 0;
    reinitialize_audio_device(devID);

    while (1) {
        pthread_mutex_lock(&audio_buffer_lock);
        // Wait for new audio data
        while (audio_buffer_size == 0) {
            pthread_cond_wait(&audio_data_cond, &audio_buffer_lock);
        }

        IMPAudioFrame frm = {.virAddr = (uint32_t *)audio_buffer, .len = audio_buffer_size};
        if (IMP_AO_SendFrame(devID, 0, &frm, BLOCK)) {
            pthread_mutex_unlock(&audio_buffer_lock);
            handle_audio_error("IMP_AO_SendFrame data error");
            reinitialize_audio_device(devID);
            continue;
        }
        audio_buffer_size = 0;
        pthread_mutex_unlock(&audio_buffer_lock);
    }
    return NULL;
}
