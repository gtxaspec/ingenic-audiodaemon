#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "cmdline.h"

void print_usage(char *program_name) {
    printf("Usage: %s [options]\n\n", program_name);
    printf("Options:\n");
    printf("  -s           Run in silent mode\n");
    printf("  -i <IP>      IP address to bind to (default: bind to all available interfaces)\n");
    printf("  -p <port>    Port to listen on (default: 8089)\n");
    printf("  -d           Enable debug mode\n");
    printf("  -h           Display this help message\n");
}

void parse_cmdline_args(int argc, char *argv[], char **ip_address, int *port, int *debug, int *silent) {
    int opt;

    while ((opt = getopt(argc, argv, "si:p:dh")) != -1) {
        switch (opt) {
            case 's':
                *silent = 1;
                break;
            case 'i':
                *ip_address = optarg;
                break;
            case 'p':
                *port = atoi(optarg);
                break;
            case 'd':
                *debug = 1;
                break;
            case 'h':
                print_usage(argv[0]);
                exit(EXIT_SUCCESS);
            default:
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
        }
    }
}
