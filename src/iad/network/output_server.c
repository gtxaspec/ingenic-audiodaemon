#include <bits/socket.h>       // for SOCK_STREAM
#include <string.h>            // for NULL, strncpy, memset, strcmp, strncmp
#include <sys/socket.h>        // for sa_family_t, ssize_t, __pthread, pthre...
#include <pthread.h>           // for pthread_mutex_unlock, pthread_mutex_lock
#include <stdio.h>             // for printf, snprintf, sscanf
#include <sys/un.h>            // for strlen, sockaddr_un
#include <unistd.h>            // for close, write, read
#include "audio_common.h"    // for audio common
#include "logging.h"  // for handle_audio_error
#include "utils.h"    // for audio_buffer_lock, ClientNode, active_...
#include "network.h"
#include "output.h"            // for g_ao_max_frame_size
#include "output_server.h"

#define TAG "NET_OUTPUT"

void *audio_output_server_thread(void *arg) {
    printf("[INFO] Entering audio_output_server_thread\n");

    update_socket_paths_from_config();

    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        handle_audio_error(TAG, "socket");
        return NULL;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(&addr.sun_path[1], AUDIO_OUTPUT_SOCKET_PATH, sizeof(addr.sun_path) - 2);
    addr.sun_path[sizeof(addr.sun_path) - 1] = '\0';

    printf("[INFO] Attempting to bind socket\n");
    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(sa_family_t) + strlen(AUDIO_OUTPUT_SOCKET_PATH) + 1) == -1) {
        handle_audio_error(TAG, "bind failed");
        close(sockfd);
        return NULL;
    }

    printf("[INFO] Attempting to listen on socket\n");
    if (listen(sockfd, 5) == -1) {
        handle_audio_error(TAG, "listen");
        close(sockfd);
        return NULL;
    }

    while (1) {
        printf("[INFO] Waiting for output client connection\n");
        int client_sock = accept(sockfd, NULL, NULL);
        if (client_sock == -1) {
            handle_audio_error(TAG, "accept");
            continue;
        }

        pthread_mutex_lock(&audio_buffer_lock);

        while (active_client_sock != -1) {
            pthread_cond_wait(&audio_data_cond, &audio_buffer_lock);
        }

        pause_audio_output();

        active_client_sock = client_sock;
        printf("[INFO] Client connected\n");

        memset(audio_buffer, 0, g_ao_max_frame_size);
        audio_buffer_size = 0;
        clear_audio_output_buffer();

        pthread_mutex_unlock(&audio_buffer_lock);

        unsigned char buf[g_ao_max_frame_size];  // Use the global variable instead of DEFAULT_AO_MAX_FRAME_SIZE
        ssize_t read_size;

        resume_audio_output();
        int mute_status = 0;
        mute_audio_output_device(mute_status);

        printf("[INFO] Receiving audio data from client\n");
        while ((read_size = read(client_sock, buf, sizeof(buf))) > 0) {
            pthread_mutex_lock(&audio_buffer_lock);
            memcpy(audio_buffer, buf, read_size);
            audio_buffer_size = read_size;
            pthread_cond_signal(&audio_data_cond);
            pthread_mutex_unlock(&audio_buffer_lock);
        }

        mute_status = 1;  // or some value
        mute_audio_output_device(mute_status);

        // Flush audio buffer so audio doesn't get stuck in the buffer and playback next time a client connects.
        flush_audio_output_buffer();
        pthread_mutex_lock(&audio_buffer_lock);
        active_client_sock = -1;
        pthread_cond_broadcast(&audio_data_cond);
        pthread_mutex_unlock(&audio_buffer_lock);

        close(client_sock);
        printf("[INFO] Client Disconnected\n");
    }

    close(sockfd);
    return NULL;
}
