#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "client_network.h"

int setup_control_client_connection() {
    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(&addr.sun_path[1], AUDIO_CONTROL_SOCKET_PATH, sizeof(addr.sun_path) - 2);

    if (connect(sockfd, (struct sockaddr*)&addr, sizeof(sa_family_t) + strlen(&addr.sun_path[1]) + 1) == -1) {
        perror("connect");
        close(sockfd);
        return -1;
    }

    return sockfd;
}

int setup_client_connection(int request_type) {
    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;

    // Determine the socket path based on the type of request (input or output)
    if (request_type == AUDIO_INPUT_REQUEST) {
        strncpy(&addr.sun_path[1], AUDIO_INPUT_SOCKET_PATH, sizeof(addr.sun_path) - 2);
    } else if (request_type == AUDIO_OUTPUT_REQUEST) {
        strncpy(&addr.sun_path[1], AUDIO_OUTPUT_SOCKET_PATH, sizeof(addr.sun_path) - 2);
    } else {
        fprintf(stderr, "Invalid request type\n");
        close(sockfd);
        return -1;
    }

    if (connect(sockfd, (struct sockaddr*)&addr, sizeof(sa_family_t) + strlen(&addr.sun_path[1]) + 1) == -1) {
        perror("connect");
        close(sockfd);
        return -1;
    }

    return sockfd;
}
