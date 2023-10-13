#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include "cmdline.h"

// Function to print the usage of the program.
void print_usage(const char *prog_name) {
    printf("Usage: %s [options]\n\n", prog_name);
    printf("Options:\n");
    printf("  -c <path>   Path to configuration file (default: ./iad.json)\n");
    printf("  -d <AI|AO>  Disable AI (Audio Input) or AO (Audio Output)\n");
    printf("  -h          Display this help message\n");
}

// Function to parse command line arguments.
// Populates the options structure based on user input.
int parse_cmdline(int argc, char *argv[], CmdOptions *options) {
    int opt;

    // Set default values
    options->config_file_path = "./iad.json";  // Default configuration file path
    options->disable_ai = 0;
    options->disable_ao = 0;

    // Use getopt to parse the command line arguments
    while ((opt = getopt(argc, argv, "d:c:h")) != -1) {
        switch (opt) {
            case 'c':
                if (optarg) {  // Check if optarg is not NULL before assigning
                    options->config_file_path = optarg;
                } else {
                    fprintf(stderr, "No path provided for -c option.\n");
                    print_usage(argv[0]);
                    return 1;
                }
                break;
            case 'd':
                if (strcmp(optarg, "AI") == 0) {
                    options->disable_ai = 1;
                } else if (strcmp(optarg, "AO") == 0) {
                    options->disable_ao = 1;
                } else {
                    fprintf(stderr, "Invalid option for -d. Use AI or AO.\n");
                    print_usage(argv[0]);
                    return 1;
                }
                break;
            case 'h':
                print_usage(argv[0]);
                exit(0);
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    return 0;
}
