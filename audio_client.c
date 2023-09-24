#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "client/cmdline.h"
#include "client/client_network.h"
#include "client/playback.h"
#include "client/record.h"
#include "version.h"

int main(int argc, char *argv[]) {
    int use_stdin = 0;
    char *audio_file_path = NULL;
    int record_audio = 0;
    int output_to_stdout = 0;

    printf("INGENIC AUDIO CLIENT Version: %s\n", VERSION);

    if (parse_arguments(argc, argv, &use_stdin, &audio_file_path, &record_audio, &output_to_stdout) != 0) {
        exit(1);
    }

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
    return 0;
}
