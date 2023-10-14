#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "cmdline.h"

void print_usage(char *program_name) {
    printf("Usage: %s [options]\n\n", program_name);
    printf("Options:\n");
    printf("  -s           Run in silent mode\n");
    printf("  -r           Start the program as a daemon\n");
    printf("  -i <IP>      IP address to bind to (default: bind to all available interfaces)\n");
    printf("  -p <port>    Port to listen on (default: 8089)\n");
    printf("  -d           Enable debug mode\n");
    printf("  -h           Display this help message\n");
}

// Function to parse command line arguments.
// Populates the options structure based on user input.
int parse_cmdline(int argc, char *argv[], CmdOptions *options) {

    int opt;

    // Set default values for all fields in the CmdOptions structure
    options->ip_address = NULL;      // Default to NULL, which means bind to all available interfaces
    options->port = 8089;            // Default port
    options->debug = 0;              // Debug mode off by default
    options->silent = 0;             // Silent mode off by default
    options->daemonize = 0;          // Don't run as daemon by default

    while ((opt = getopt(argc, argv, "si:p:rdh")) != -1) {
        switch (opt) {
            case 's':
                options->silent = 1;
                break;
            case 'i':
                options->ip_address = optarg;
                break;
            case 'p':
                options->port = atoi(optarg);
                break;
            case 'r':
                options->daemonize = 1;
                break;
            case 'd':
                options->debug = 1;
                break;
            case 'h':
                print_usage(argv[0]);
                exit(EXIT_SUCCESS);
            default:
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
        }
    }
    return 0;

}
