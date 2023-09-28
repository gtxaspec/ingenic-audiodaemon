#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include "cmdline.h"

void print_usage(char *program_name) {
    printf("Usage: %s [-f <audio_file_path>] [-s] [-r <audio_output_file_path>] [-o]\n", program_name);
}

int parse_arguments(int argc, char *argv[], int *use_stdin, char **audio_file_path, int *record_audio, int *output_to_stdout) {
    int opt;
    *record_audio = 0;
    *output_to_stdout = 0;
    while ((opt = getopt(argc, argv, "sf:r:o")) != -1) {
        switch (opt) {
            case 's':
                *use_stdin = 1;
                break;
            case 'f':
                *audio_file_path = optarg;
                break;
            case 'r':
                *audio_file_path = optarg;
                *record_audio = 1;
                break;
            case 'o':
                *output_to_stdout = 1;
                *record_audio = 1;
                break;
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
