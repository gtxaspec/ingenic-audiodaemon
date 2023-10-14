/*
 * INGENIC WEBSOCKET AUDIO CLIENT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libwebsockets.h>
#include "web_client_src/cmdline.h"
#include "web_client_src/client_network.h"
#include "web_client_src/playback.h"
#include "version.h"

// Global constants and variables
#define INITIAL_AUDIO_BUFFER_SIZE 1280
#define MAX_AUDIO_BUFFER_SIZE 65536  // Set to 64KB for safety

static int active_ws_connections = 0;  // Active WebSocket connections count
static int daemon_sockfd = -1;  // Socket descriptor for daemon connection
static unsigned char *audio_buffer = NULL;
static size_t audio_buffer_size = INITIAL_AUDIO_BUFFER_SIZE;
static size_t audio_buffer_len = 0;

// Forward declarations for callback functions
static int ws_callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);
static int http_callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);

// WebSocket callback function
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

// HTTP callback for handling streamed audio data via POST
static int http_callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len) {
    switch (reason) {
        case LWS_CALLBACK_HTTP:
            if (lws_hdr_total_length(wsi, WSI_TOKEN_POST_URI)) {
                if (daemon_sockfd == -1) {
                    daemon_sockfd = setup_client_connection(AUDIO_OUTPUT_REQUEST);
                }
            } else {
                return -1;
            }
            break;

        case LWS_CALLBACK_HTTP_BODY:
            if (daemon_sockfd != -1) {
                if (audio_buffer_len + len > audio_buffer_size) {
                    audio_buffer_size = audio_buffer_len + len;
                    audio_buffer = realloc(audio_buffer, audio_buffer_size);
                    if (!audio_buffer) {
                        fprintf(stderr, "Failed to reallocate audio buffer.\n");
                        return -1;
                    }
                }
                memcpy(audio_buffer + audio_buffer_len, in, len);
                audio_buffer_len += len;
                write(daemon_sockfd, audio_buffer, audio_buffer_len);
                audio_buffer_len = 0;
            }
            break;

        case LWS_CALLBACK_HTTP_BODY_COMPLETION:
            lws_return_http_status(wsi, HTTP_STATUS_OK, NULL);
            usleep(1000000); // Temporary solution: wait before closing socket to prevent audio cutoff
            if (daemon_sockfd != -1) {
                close_client_connection(daemon_sockfd);
                daemon_sockfd = -1;
            }
            break;

        default:
            break;
    }
    return 0;
}

// Protocols structure
static struct lws_protocols protocols[] = {
    {
        "http-only",        // Name
        http_callback,      // Callback
        0,                  // Per session data size
        0                   // Maximum frame size
    },
    {
        "audio-protocol",
        ws_callback,
        0,
        AO_MAX_FRAME_SIZE
    },
    { NULL, NULL, 0, 0 }
};

// Main function
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

    audio_buffer = malloc(INITIAL_AUDIO_BUFFER_SIZE);
    if (!audio_buffer) {
        perror("Failed to allocate initial audio buffer");
        exit(EXIT_FAILURE);
    }

    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    info.port = port;
    info.iface = ip_address;
    info.protocols = protocols;

    struct lws_context *context = lws_create_context(&info);
    if (!context) {
        fprintf(stderr, "Failed to create WebSocket context\n");
        free(audio_buffer);
        return 1;
    }

    while (1) {
        lws_service(context, 1000); // Poll every 1000ms
    }

    lws_context_destroy(context);
    free(audio_buffer);
    return 0;
}
