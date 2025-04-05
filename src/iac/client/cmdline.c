#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include "cmdline.h"

void print_usage(char *program_name) {
    printf("Usage: %s [options]\n\n", program_name);
    printf("Options:\n");
    printf("  -f <path>   Specify the path to an audio file to play\n");
    printf("  -w <path>   Specify the path to a WebM/Opus file to play\n");
    printf("  -s          Use stdin for audio input\n");
    printf("  -r <path>   Record audio to given file path\n");
    printf("  -o          Output recorded audio to stdout\n");
    printf("  -h          Display this help message\n");
}

int parse_arguments(int argc, char *argv[], int *use_stdin, char **audio_file_path, int *record_audio, int *output_to_stdout, int *use_webm) {
    int opt;
    *record_audio = 0;
    *output_to_stdout = 0;
    *use_webm = 0;

    while ((opt = getopt(argc, argv, "sf:w:r:oh")) != -1) {
        switch (opt) {
            case 's':
                *use_stdin = 1;
                break;
            case 'f':
                *audio_file_path = optarg;
                break;
            case 'w':
                *audio_file_path = optarg;
                *use_webm = 1;
                break;
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
