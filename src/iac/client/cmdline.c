#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include "cmdline.h"

void print_usage(char *program_name) {
    printf("Usage: %s [options]\n\n", program_name);
    printf("Options:\n");
    printf("  -f <path>   Specify the path to an audio file (PCM or WebM) to play\n");
    // -w option removed as daemon now auto-detects
    printf("  -s          Use stdin for audio input (PCM only)\n");
    printf("  -r <path>   Record audio to given file path\n");
    printf("  -o          Output recorded audio to stdout\n");
    printf("  -h          Display this help message\n");
}

int parse_arguments(int argc, char *argv[], int *use_stdin, char **audio_file_path, int *record_audio, int *output_to_stdout, int *use_webm) {
    int opt;
    *record_audio = 0;
    *output_to_stdout = 0;
    if (use_webm) *use_webm = 0; // Initialize dummy variable if pointer is not NULL

    while ((opt = getopt(argc, argv, "sf:r:oh")) != -1) { // Removed 'w' from getopt string
        switch (opt) {
            case 's':
                *use_stdin = 1;
                break;
            case 'f':
                *audio_file_path = optarg;
                // No need to set use_webm here
                break;
            // case 'w' removed
            case 'r':
                *audio_file_path = optarg;
                *record_audio = 1;
                break;
            case 'o':
                *output_to_stdout = 1;
                *record_audio = 1;
                break;
            case 'h':
                print_usage(argv[0]);
                exit(0);
            default:
                print_usage(argv[0]);
                return -1;
        }
    }

    if (!(*use_stdin) && *audio_file_path == NULL && !(*output_to_stdout)) {
        print_usage(argv[0]);
        return -1;
    }

    return 0;
}
