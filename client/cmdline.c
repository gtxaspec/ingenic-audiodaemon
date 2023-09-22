#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

void print_usage(char *program_name) {
    printf("Usage: %s [-f <audio_file_path>] [-s] [-r <audio_output_file_path>]\n", program_name);
}

int parse_arguments(int argc, char *argv[], int *use_stdin, char **audio_file_path, int *record_audio) {
    int opt;
    while ((opt = getopt(argc, argv, "sf:r:")) != -1) {
        switch (opt) {
            case 's':
                *use_stdin = 1;
                break;
            case 'f':
                *audio_file_path = optarg;
                break;
            case 'r':
                *audio_file_path = optarg;  // The provided file will be the output file where audio input will be saved
                *record_audio = 1;
                break;
            default:
                print_usage(argv[0]);
                return -1;
        }
    }

    if (!(*use_stdin) && *audio_file_path == NULL) {
        print_usage(argv[0]);
        return -1;
    }

    return 0;
}
