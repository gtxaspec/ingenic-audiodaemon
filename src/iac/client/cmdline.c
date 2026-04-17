#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include "cmdline.h"

void print_usage(char *program_name) {
    printf("Usage: %s [options]\n\n", program_name);
    printf("Options:\n");
    printf("  -f <path>   Specify the path to an audio file to play\n");
    printf("  -s          Use stdin for audio input\n");
    printf("  -r <path>   Record audio to given file path\n");
    printf("  -o          Output recorded audio to stdout\n");
    printf("  -h          Display this help message\n");
    printf("\nAudio Control Options (temporary by default, restored on exit):\n");
    printf("  -g <gain>   Set audio output gain (0-31)\n");
    printf("  -v <vol>    Set audio output volume (-30 to 120)\n");
    printf("  -p          Persist audio control changes (don't restore on exit)\n");
}

int parse_arguments(int argc, char *argv[], int *use_stdin, char **audio_file_path,
                    int *record_audio, int *output_to_stdout,
                    audio_control_opts_t *audio_opts) {
    int opt;
    *record_audio = 0;
    *output_to_stdout = 0;
    // Initialize audio control options to -1 (not set)
    audio_opts->ao_gain = -1;
    audio_opts->ao_vol = -1;
    audio_opts->persist = 0;

    while ((opt = getopt(argc, argv, "sf:r:ohg:v:p")) != -1) {
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
            case 'h':
                print_usage(argv[0]);
                exit(0);
            case 'g':
                audio_opts->ao_gain = atoi(optarg);
                break;
            case 'v':
                audio_opts->ao_vol = atoi(optarg);
                break;
            case 'p':
                audio_opts->persist = 1;
                break;
            default:
                print_usage(argv[0]);
                return -1;
        }
    }

    // Only require audio file/stdin/stdout if persist flag is not set
    if (!(*use_stdin) && *audio_file_path == NULL && !(*output_to_stdout) &&
        opts->persist == 0) {
        print_usage(argv[0]);
        return -1;
    }

    return 0;
}
