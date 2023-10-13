#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#include "cmdline.h"
#include "utils.h"

// Function to print the usage of the program.
void print_usage(const char *prog_name) {
    printf("Usage: %s [options]\n\n", prog_name);
    printf("Options:\n");
    printf("  -c <path>   Path to configuration file (default: ./iad.json)\n");
    printf("  -d <AI|AO>  Disable AI (Audio Input) or AO (Audio Output)\n");
    printf("  -r          Start the program as a daemon\n");
    printf("  -h          Display this help message\n");
}

// Function to parse command line arguments.
// Populates the options structure based on user input.
int parse_cmdline(int argc, char *argv[], CmdOptions *options) {
    int opt;

    char exePath[4096];
    static char configFile[4096];  // Make this static so its address remains valid after the function returns

    // Read the symlink to get the path of the currently running executable
    ssize_t len = readlink("/proc/self/exe", exePath, sizeof(exePath)-1);
    if (len != -1) {
        exePath[len] = '\0';  // Null-terminate read string
        // Extract directory
        char* dir = dirname(exePath);
        // Construct path to iad.json
        snprintf(configFile, sizeof(configFile), "%s/iad.json", dir);
    } else {
        strncpy(configFile, "./iad.json", sizeof(configFile)-1); // Default to current directory if path cannot be determined
    }

    // Set default values
    options->config_file_path = configFile;  // Updated configuration file path
    options->disable_ai = 0;
    options->disable_ao = 0;
    options->daemonize = 0;

    // Use getopt to parse the command line arguments
    while ((opt = getopt(argc, argv, "d:c:rh")) != -1) {
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
            case 'r':
                options->daemonize = 1;
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
