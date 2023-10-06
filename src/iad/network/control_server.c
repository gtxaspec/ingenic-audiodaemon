#include <bits/socket.h>       // for SOCK_STREAM
#include <stdlib.h>            // for free, malloc
#include <string.h>            // for NULL, strncpy, memset, strcmp, strncmp
#include <sys/socket.h>        // for sa_family_t, ssize_t, __pthread, pthre...
#include <pthread.h>           // for pthread_mutex_unlock, pthread_mutex_lock
#include <stdio.h>             // for printf, snprintf, sscanf
#include <sys/un.h>            // for strlen, sockaddr_un
#include <unistd.h>            // for close, write, read
#include "logging.h"  // for handle_audio_error
#include "utils.h"    // for audio_buffer_lock, ClientNode, active_...
#include "network.h"
#include "control_server.h"

#define TAG "NET_CONTROL"

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
    printf("[INFO] Entering audio_control_server_thread\n");

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

    printf("[INFO] Attempting to bind control socket\n");
    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(sa_family_t) + strlen(AUDIO_CONTROL_SOCKET_PATH) + 1) == -1) {
        handle_audio_error(TAG, "bind failed");
        close(sockfd);
        return NULL;
    }

    printf("[INFO] Attempting to listen on control socket\n");
    if (listen(sockfd, 5) == -1) {
        handle_audio_error(TAG, "listen");
        close(sockfd);
        return NULL;
    }

    while (1) {
        printf("[INFO] Waiting for control client connection\n");
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

