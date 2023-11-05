#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/un.h>
#include <unistd.h>
#include "logging.h"
#include "utils.h"
#include "network.h"
#include "control_server.h"

#define TAG "NET_CONTROL"

// Global flag and mutex to control thread termination
extern volatile int g_stop_thread;
extern pthread_mutex_t g_stop_thread_mutex;

void handle_control_client(int client_sock) {
    char buffer[256];
    ssize_t bytes_received = recv(client_sock, buffer, sizeof(buffer) - 1, 0);

    if (bytes_received <= 0) {
        handle_audio_error(TAG, "recv");
        close(client_sock);
        return;
    }

    buffer[bytes_received] = '\0';  // Null-terminate the received string

    // Try interpreting buffer as an integer for legacy clients
    int client_request_type = *(int *)buffer;

    if (client_request_type == AUDIO_OUTPUT_REQUEST) {
        pthread_mutex_lock(&audio_buffer_lock);

        if (active_client_sock != -1) {
            if (write(client_sock, "queued", strlen("queued")) == -1) {
                handle_audio_error(TAG, "write");
            }
        } else {
            if (write(client_sock, "not_queued", strlen("not_queued")) == -1) {
                handle_audio_error(TAG, "write");
            }
        }

        pthread_mutex_unlock(&audio_buffer_lock);
    }
    // Check for the new protocol
    else if (strncmp(buffer, "GET ", 4) == 0) {
        char variable_name[100];
        sscanf(buffer + 4, "%s", variable_name);

        char* value = get_variable_value(variable_name);

        if (value) {
            write(client_sock, value, strlen(value));
            free(value);
        } else {
            write(client_sock, "RESPONSE_UNKNOWN_VARIABLE", strlen("RESPONSE_UNKNOWN_VARIABLE"));
        }
    }
    else if (strncmp(buffer, "SET ", 4) == 0) {
        char variable_name[100];
        char value[100];
        sscanf(buffer + 4, "%s %s", variable_name, value);

        int result = set_variable_value(variable_name, value);

        if (result == 0) {
            write(client_sock, "RESPONSE_OK", strlen("RESPONSE_OK"));
        } else {
            write(client_sock, "RESPONSE_ERROR", strlen("RESPONSE_ERROR"));
        }
    }
    else {
        write(client_sock, "RESPONSE_ERROR", strlen("RESPONSE_ERROR"));
    }

    close(client_sock);
}

void *audio_control_server_thread(void *arg) {
    printf("[INFO] [CTRL] Entering audio_control_server_thread\n");

    update_socket_paths_from_config();

    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        handle_audio_error(TAG, "socket");
        return NULL;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(&addr.sun_path[1], AUDIO_CONTROL_SOCKET_PATH, sizeof(addr.sun_path) - 2);
    addr.sun_path[sizeof(addr.sun_path) - 1] = '\0';

    printf("[INFO] [CTRL] Attempting to bind control socket\n");
    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(sa_family_t) + strlen(AUDIO_CONTROL_SOCKET_PATH) + 1) == -1) {
        handle_audio_error(TAG, "bind failed");
        close(sockfd);
        return NULL;
    }
    else {
        printf("[INFO] [CTRL] Bind to control socket succeeded\n");
    }

    printf("[INFO] [CTRL] Attempting to listen on control socket\n");
    if (listen(sockfd, 5) == -1) {
        handle_audio_error(TAG, "listen");
        close(sockfd);
        return NULL;
    }
    else {
        printf("[INFO] [CTRL] Listening on control socket\n");
    }

    while (1) {
        int should_stop = 0;
        pthread_mutex_lock(&g_stop_thread_mutex);
        should_stop = g_stop_thread;
        pthread_mutex_unlock(&g_stop_thread_mutex);

        if (should_stop) {
            break;
        }

        printf("[INFO] [CTRL] Waiting for a control client connection\n");
        int client_sock = accept(sockfd, NULL, NULL);
        if (client_sock == -1) {
            handle_audio_error(TAG, "accept");
            continue;
        }
        handle_control_client(client_sock);
    }

    close(sockfd);
    return NULL;
}
