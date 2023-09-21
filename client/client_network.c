#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <client_network.h>

int setup_client_connection() {
    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(&addr.sun_path[1], CLIENT_SERVER_SOCKET_PATH + 1, sizeof(addr.sun_path) - 2);

    if (connect(sockfd, (struct sockaddr*)&addr, sizeof(sa_family_t) + strlen(&addr.sun_path[1]) + 1) == -1) {
        perror("connect");
        close(sockfd);
        return -1;
    }

    return sockfd;
}
