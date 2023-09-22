#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "client/cmdline.h"
#include "client/client_network.h"
#include "client/playback.h"
#include "client/record.h"  // Assuming we'll have a record module

int main(int argc, char *argv[]) {
    int use_stdin = 0;
    char *audio_file_path = NULL;
    int record_audio = 0;  // New flag for audio recording

    if (parse_arguments(argc, argv, &use_stdin, &audio_file_path, &record_audio) != 0) {  // Adjusted for the new flag
        exit(1);
    }

    const char *socket_path = record_audio ? AUDIO_INPUT_SOCKET_PATH : AUDIO_OUTPUT_SOCKET_PATH;
    int sockfd = setup_client_connection(record_audio ? AUDIO_INPUT_REQUEST : AUDIO_OUTPUT_REQUEST);
    if (sockfd < 0) {
        exit(1);
    }

    if (record_audio) {
        // Send audio input request to the server
        int request_type = AUDIO_INPUT_REQUEST;
        write(sockfd, &request_type, sizeof(int));

        // Start recording audio from the server
        record_from_server(sockfd, audio_file_path);  // Assuming we'll have a record_from_server function
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
