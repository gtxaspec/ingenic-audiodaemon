#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/un.h>
#include <unistd.h>
#include "input.h"
#include "logging.h"
#include "utils.h"
#include "network.h"
#include "input_server.h"

#define TAG "NET_INPUT"

// Global flag and mutex to control thread termination
extern volatile int g_stop_thread;
extern pthread_mutex_t g_stop_thread_mutex;

void handle_audio_input_client(int client_sock) {
    pthread_mutex_lock(&audio_buffer_lock);

    ClientNode *new_client = (ClientNode *)malloc(sizeof(ClientNode));
    if (!new_client) {
        handle_audio_error(TAG, "malloc");
        close(client_sock);
        pthread_mutex_unlock(&audio_buffer_lock);
        return;
    }
    new_client->sockfd = client_sock;
    new_client->next = client_list_head;
    client_list_head = new_client;

    pthread_mutex_unlock(&audio_buffer_lock);

    printf("[INFO] Input client connected\n");

    AiThreadArg thread_arg;
    thread_arg.sockfd = client_sock;

    pthread_t ai_thread;
    if (pthread_create(&ai_thread, NULL, ai_record_thread, &thread_arg) != 0) {
        handle_audio_error(TAG, "pthread_create");
        close(client_sock);
        free(new_client);
    } else {
        pthread_detach(ai_thread);
    }
}

void *audio_input_server_thread(void *arg) {
    printf("[INFO] Entering audio_input_server_thread\n");

    update_socket_paths_from_config();

    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        handle_audio_error(TAG, "socket");
        return NULL;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(&addr.sun_path[1], AUDIO_INPUT_SOCKET_PATH, sizeof(addr.sun_path) - 2);
    addr.sun_path[sizeof(addr.sun_path) - 1] = '\0';

    printf("[INFO] Attempting to bind socket\n");
    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(sa_family_t) + strlen(AUDIO_INPUT_SOCKET_PATH) + 1) == -1) {
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
        int should_stop = 0;
        pthread_mutex_lock(&g_stop_thread_mutex);
        should_stop = g_stop_thread;
        pthread_mutex_unlock(&g_stop_thread_mutex);

        if (should_stop) {
            break;
        }

        printf("[INFO] Waiting for input client connection\n");
        int client_sock = accept(sockfd, NULL, NULL);
        if (client_sock == -1) {
            handle_audio_error(TAG, "accept");
            continue;
        }
        handle_audio_input_client(client_sock);
    }

    close(sockfd);
    return NULL;
}
