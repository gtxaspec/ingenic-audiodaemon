/* INGENIC WEBSOCKET AUDIO CLIENT */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libwebsockets.h>
#include "web_client_src/cmdline.h"
#include "web_client_src/client_network.h"
#include "web_client_src/playback.h"
#include "version.h"

static int active_ws_connections = 0;  // Number of active WebSocket connections
static int daemon_sockfd = -1;  // Socket file descriptor for daemon connection

// WebSocket callback
static int ws_callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len) {
    switch (reason) {
        case LWS_CALLBACK_ESTABLISHED:
            active_ws_connections++;
            if (daemon_sockfd == -1) {
                daemon_sockfd = setup_client_connection(AUDIO_OUTPUT_REQUEST);
            }
            break;
        case LWS_CALLBACK_RECEIVE:
            if (daemon_sockfd != -1) {
                write(daemon_sockfd, in, len);
            }
            break;
        case LWS_CALLBACK_CLOSED:
            active_ws_connections--;
            if (active_ws_connections == 0 && daemon_sockfd != -1) {
                close_client_connection(daemon_sockfd);
                daemon_sockfd = -1;
            }
            break;
        default:
            break;
    }
    return 0;
}

static struct lws_protocols protocols[] = {
    {
        "audio-protocol",
        ws_callback,
        0,
        AO_MAX_FRAME_SIZE,
    },
    { NULL, NULL, 0, 0 }
};

int main(int argc, char *argv[]) {
    char *ip_address = NULL;
    int port = 8089;
    int debug = 0;
    int silent = 0;

    parse_cmdline_args(argc, argv, &ip_address, &port, &debug, &silent);

    if (!silent) {
        printf("INGENIC WEB AUDIO CLIENT Version: %s\n", VERSION);
        if (!debug) {
            lws_set_log_level(0, NULL);
        }
    } else {
        lws_set_log_level(LLL_ERR, NULL);
    }

    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));

    info.port = port;
    info.iface = ip_address;
    info.protocols = protocols;

    struct lws_context *context = lws_create_context(&info);
    if (!context) {
        fprintf(stderr, "Failed to create WebSocket context\n");
        return 1;
    }

    while (1) {
        lws_service(context, 1000);
    }

    lws_context_destroy(context);
    return 0;
}
