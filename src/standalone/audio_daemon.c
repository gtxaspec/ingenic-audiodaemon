#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include "imp/imp_audio.h"
#include "imp/imp_log.h"

#define TAG "AO_T31"
#define AO_TEST_SAMPLE_RATE 16000
#define AO_MAX_FRAME_SIZE 1280
#define SERVER_SOCKET_PATH "ingenic_audio"
#define CHN_VOL 100
#define AO_GAIN 20

pthread_mutex_t audio_buffer_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t audio_data_cond = PTHREAD_COND_INITIALIZER;
unsigned char audio_buffer[AO_MAX_FRAME_SIZE];
ssize_t audio_buffer_size = 0;
int active_client_sock = -1;

//void handle_audio_error(const char *msg) {
//    fprintf(stderr, "[ERROR] %s\n", msg);
//}

void reinitialize_audio_device(int devID) {
    IMP_AO_DisableChn(devID, 0);
    IMP_AO_Disable(devID);

    // Initialize the audio device again
    IMPAudioIOAttr attr = {
        .samplerate = AUDIO_SAMPLE_RATE_16000,
        .bitwidth = AUDIO_BIT_WIDTH_16,
        .soundmode = AUDIO_SOUND_MODE_MONO,
        .frmNum = 20,
        .numPerFrm = 640,
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

void *audio_server_thread(void *arg) {
    printf("[INFO] Entering audio_server_thread\n");

    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return NULL;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(&addr.sun_path[1], SERVER_SOCKET_PATH, sizeof(addr.sun_path) - 2);

    printf("[INFO] Attempting to bind socket\n");
    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(sa_family_t) + strlen(SERVER_SOCKET_PATH) + 1) == -1) {
        perror("bind failed");
        close(sockfd);
        return NULL;
    } else {
        printf("[INFO] Successfully bound socket\n");
        printf("[DEBUG] Binding to socket path: %s\n", &addr.sun_path[1]);
    }

    printf("[INFO] Attempting to listen on socket\n");
    if (listen(sockfd, 5) == -1) {
        perror("listen");
        close(sockfd);
        return NULL;
    }

    while (1) {
        printf("[INFO] Waiting for client connection\n");
        int client_sock = accept(sockfd, NULL, NULL);
        if (client_sock == -1) {
            perror("accept");
            continue;
        }

        pthread_mutex_lock(&audio_buffer_lock);

        // Wait until no client is active
        while (active_client_sock != -1) {
            pthread_cond_wait(&audio_data_cond, &audio_buffer_lock);
        }

        active_client_sock = client_sock;  // Set the current client as active
        pthread_mutex_unlock(&audio_buffer_lock);

        unsigned char buf[AO_MAX_FRAME_SIZE];
        ssize_t read_size;

        printf("[INFO] Waiting for audio data from client\n");
        while ((read_size = read(client_sock, buf, sizeof(buf))) > 0) {
            pthread_mutex_lock(&audio_buffer_lock);
            memcpy(audio_buffer, buf, read_size);
            audio_buffer_size = read_size;
            pthread_cond_signal(&audio_data_cond); // Notify the playback thread
            pthread_mutex_unlock(&audio_buffer_lock);
        }

        pthread_mutex_lock(&audio_buffer_lock);
        active_client_sock = -1;  // Reset the active client
        pthread_cond_broadcast(&audio_data_cond);
        pthread_mutex_unlock(&audio_buffer_lock);

        close(client_sock);
    }

    close(sockfd);
    return NULL;
}

int main(void) {
    printf("[INFO] Starting audio daemon\n");

    pthread_t play_thread_id;
    int ret_play = pthread_create(&play_thread_id, NULL, ao_test_play_thread, NULL);
    if (ret_play) {
        fprintf(stderr, "[ERROR] pthread_create for play thread failed with error code: %d\n", ret_play);
        return 1;
    }

    pthread_t server_thread;
    int ret_server = pthread_create(&server_thread, NULL, audio_server_thread, NULL);
    if (ret_server) {
        fprintf(stderr, "[ERROR] pthread_create for server thread failed with error code: %d\n", ret_server);
        return 1;
    }

    pthread_join(server_thread, NULL);
    pthread_join(play_thread_id, NULL);

    pthread_mutex_destroy(&audio_buffer_lock);
    pthread_cond_destroy(&audio_data_cond);

    return 0;
}
