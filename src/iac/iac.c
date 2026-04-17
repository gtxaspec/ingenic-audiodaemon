/* INGENIC AUDIO CLIENT */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "client/cmdline.h"
#include "client/client_network.h"
#include "client/playback.h"
#include "client/record.h"
#include "version.h"

// Send a GET command to the control server and return the value
static int send_control_get(int sockfd, const char *var_name, int *value) {
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "GET %s", var_name);
    if (write(sockfd, cmd, strlen(cmd)) < 0) {
        perror("write control command");
        return -1;
    }

    char response[64];
    int n = read(sockfd, response, sizeof(response) - 1);
    if (n > 0) {
        response[n] = '\0';
        // Check for error response
        if (strstr(response, "UNKNOWN") != NULL || strstr(response, "ERROR") != NULL) {
            return -1;
        }
        *value = atoi(response);
        return 0;
    }
    return -1;
}

// Send a SET command to the control server and check response
static int send_control_set(int sockfd, const char *var_name, int value) {
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "SET %s %d", var_name, value);
    if (write(sockfd, cmd, strlen(cmd)) < 0) {
        perror("write control command");
        return -1;
    }

    char response[64];
    int n = read(sockfd, response, sizeof(response) - 1);
    if (n > 0) {
        response[n] = '\0';
        if (strstr(response, "OK") != NULL) {
            return 0;
        }
        fprintf(stderr, "Control command '%s' failed: %s\n", cmd, response);
        return -1;
    }
    return -1;
}

// Saved audio state for restoration
static audio_control_opts_t saved_audio_state;
static int audio_state_saved = 0;

// Helper to get a single control value (opens/closes its own connection)
static int get_control_value(const char *var_name, int *value) {
    int sockfd = setup_control_client_connection();
    if (sockfd < 0) {
        return -1;
    }
    int result = send_control_get(sockfd, var_name, value);
    close(sockfd);
    return result;
}

// Helper to set a single control value (opens/closes its own connection)
static int set_control_value(const char *var_name, int value) {
    int sockfd = setup_control_client_connection();
    if (sockfd < 0) {
        return -1;
    }
    int result = send_control_set(sockfd, var_name, value);
    close(sockfd);
    return result;
}

// Save current audio state for options that will be changed
static void save_audio_state(audio_control_opts_t *opts) {
    // Only save values for options that will be changed
    // Each get opens its own connection since server handles one command per connection
    if (opts->ao_gain != -1) {
        get_control_value("ao_gain", &saved_audio_state.ao_gain);
    } else {
        saved_audio_state.ao_gain = -1;
    }
    if (opts->ao_vol != -1) {
        get_control_value("ao_vol", &saved_audio_state.ao_vol);
    } else {
        saved_audio_state.ao_vol = -1;
    }
    audio_state_saved = 1;
}

// Restore saved audio state
static void restore_audio_state(void) {
    if (!audio_state_saved) {
        return;
    }

    // Each set opens its own connection since server handles one command per connection
    if (saved_audio_state.ao_gain != -1) {
        set_control_value("ao_gain", saved_audio_state.ao_gain);
    }
    if (saved_audio_state.ao_vol != -1) {
        set_control_value("ao_vol", saved_audio_state.ao_vol);
    }
}

// Apply audio control options via the control socket
static int apply_audio_control_opts(audio_control_opts_t *opts) {
    int result = 0;

    // Each set opens its own connection since server handles one command per connection
    if (opts->ao_gain != -1) {
        printf("[INFO] Setting audio output gain to %d\n", opts->ao_gain);
        if (set_control_value("ao_gain", opts->ao_gain) != 0) result = -1;
    }
    if (opts->ao_vol != -1) {
        printf("[INFO] Setting audio output volume to %d\n", opts->ao_vol);
        if (set_control_value("ao_vol", opts->ao_vol) != 0) result = -1;
    }
    return result;
}

// Check if any audio control option is set
static int has_audio_opts(audio_control_opts_t *opts) {
    return opts->ao_gain != -1 || opts->ao_vol != -1;
}

int main(int argc, char *argv[]) {
    int use_stdin = 0;
    char *audio_file_path = NULL;
    int record_audio = 0;
    int output_to_stdout = 0;
    int request_type;
    audio_control_opts_t audio_opts;

    printf("INGENIC AUDIO CLIENT Version: %s\n", VERSION);

    if (parse_arguments(argc, argv, &use_stdin, &audio_file_path, &record_audio, &output_to_stdout, &audio_opts) != 0) {
        exit(1);
    }

    // Apply audio control options if any were specified
    if (has_audio_opts(&audio_opts)) {
        // Save current state if not persisting
        if (!audio_opts.persist) {
            save_audio_state(&audio_opts);
        }

        if (apply_audio_control_opts(&audio_opts) != 0) {
            fprintf(stderr, "Warning: Some audio control settings may have failed\n");
        }

        // If only control options were specified with persist flag, exit cleanly
        if (!use_stdin && audio_file_path == NULL && !output_to_stdout) {
            // Don't restore if persist was specified
            return 0;
        }
    }

    if (record_audio) {
        request_type = AUDIO_INPUT_REQUEST;
    } else {
        request_type = AUDIO_OUTPUT_REQUEST;
    }

    int control_sockfd = setup_control_client_connection();
    // Send the request type to the control server
    write(control_sockfd, &request_type, sizeof(int));

    char control_msg[100];
    int read_size = read(control_sockfd, control_msg, sizeof(control_msg) - 1);
    if (read_size > 0) {
	control_msg[read_size] = '\0';
//        Don't print the control for now
//        printf("%s\n", control_msg);
        if (strcmp(control_msg, "queued") == 0) {
        printf("There is another client currently playing audio, audio from this client is queued, waiting for current client to finish.\n");
            close(control_sockfd);
//            exit(0);  // We can exit the client if it's queued, but not right now
        }
    }
    close(control_sockfd);

    int sockfd = setup_client_connection(record_audio ? AUDIO_INPUT_REQUEST : AUDIO_OUTPUT_REQUEST);

    if (sockfd < 0) {
        perror("Failed to connect to daemon");
        exit(1);
    }

    printf("[INFO] Connected to daemon\n");

    if (record_audio) {
        // Send audio input request to the server
        int request_type = AUDIO_INPUT_REQUEST;
        write(sockfd, &request_type, sizeof(int));

        if (output_to_stdout) {
            record_from_server(sockfd, NULL);  // When output_file_path is NULL, the function will write to stdout
        } else {
            record_from_server(sockfd, audio_file_path);
        }
    } else {
        FILE *audio_file = use_stdin ? stdin : fopen(audio_file_path, "rb");
        if (!audio_file) {
            perror("fopen");
            close(sockfd);
            exit(1);
        }

        // Send audio output request to the server
        int request_type = AUDIO_OUTPUT_REQUEST;
        write(sockfd, &request_type, sizeof(int));

		playback_audio(sockfd, audio_file);

        if (!use_stdin) {
            fclose(audio_file);
        }
    }

    close(sockfd);

    // Restore audio state if we saved it (non-persist mode)
    restore_audio_state();

    return 0;
}
