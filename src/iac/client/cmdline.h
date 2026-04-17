#ifndef CMDLINE_H
#define CMDLINE_H

// Audio control options - value of -1 means not set
typedef struct {
    int ao_gain;    // -g: Audio output gain (0-31)
    int ao_vol;     // -v: Audio output volume (-30 to 120)
    int persist;    // -p: Persist audio control changes (default: restore after exit)
} audio_control_opts_t;

void print_usage(char *program_name);
int parse_arguments(int argc, char *argv[], int *use_stdin, char **audio_file_path,
                    int *record_audio, int *output_to_stdout,
                    audio_control_opts_t *audio_opts);

#endif // CMDLINE_H
