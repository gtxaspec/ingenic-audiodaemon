#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "libwebsockets.h"
#include "version.h"

int silent = 0;  // Global flag to check if the program should run silently

static int callback_audio(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len) {
    switch (reason) {
        case LWS_CALLBACK_ESTABLISHED:
            if (!silent) printf("[INFO] Connection established\n");
            break;
        case LWS_CALLBACK_RECEIVE:
            fwrite(in, 1, len, stdout);
            fflush(stdout);  // Ensure the data is written immediately
            break;
        case LWS_CALLBACK_CLOSED:
            if (!silent) printf("[INFO] Connection closed\n");
            break;
        default:
            break;
    }
    return 0;
}

static struct lws_protocols protocols[] = {
    {
        "audio-protocol",
        callback_audio,
        0,
        65536,
    },
    {NULL, NULL, 0, 0}  // terminator
};

void display_help(const char *progname) {
    printf("Usage: %s [options]\n\n", progname);
    printf("Options:\n");
    printf("  -s           Run in silent mode\n");
    printf("  -i <IP>      IP address to bind to (default: bind to all available interfaces)\n");
    printf("  -p <port>    Port to listen on (default: 8089)\n");
    printf("  -h           Display this help message\n");
}

int main(int argc, char **argv) {
    int opt;
    char *ip = NULL;
    int port = 8089;

    // Parse command line arguments using getopt
    while ((opt = getopt(argc, argv, "si:p:h")) != -1) {
        switch (opt) {
            case 's':
                silent = 1;
                break;
            case 'i':
                ip = optarg;
                break;
            case 'p':
                port = atoi(optarg);
                break;
            case 'h':
                display_help(argv[0]);
                return 0;
            default:
                display_help(argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    // If silent flag is set, silence the libwebsockets debug output
    if (silent) {
        lws_set_log_level(0, NULL); // Set log level to 0
    }

    if (!silent) {
        printf("INGENIC WC-CONSOLE Version: %s\n", VERSION);
    }

    // Disable buffering for stdout
    setbuf(stdout, NULL);

    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    info.port = port;
    info.iface = ip;
    info.protocols = protocols;
    info.gid = -1;
    info.uid = -1;

    struct lws_context *context = lws_create_context(&info);
    if (!context) {
        fprintf(stderr, "lws init failed\n");
        return -1;
    }

    if (!silent) printf("[INFO] Server started on port %d\n", port);

    while (1) {
        lws_service(context, 50);
    }

    lws_context_destroy(context);

    return 0;
}
