/* INGENIC AUDIO CLIENT */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "client/cmdline.h"
#include "client/client_network.h"
#include "client/playback.h"
#include "client/record.h"
#include "client/webm_opus.h"
#include "version.h"

int main(int argc, char *argv[]) {
    int use_stdin = 0;
    char *audio_file_path = NULL;
    int record_audio = 0;
    int output_to_stdout = 0;
    int use_webm = 0;
    int request_type;

    printf("INGENIC AUDIO CLIENT Version: %s\n", VERSION);

    if (parse_arguments(argc, argv, &use_stdin, &audio_file_path, &record_audio, &output_to_stdout, &use_webm) != 0) {
        exit(1);
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
        // Send audio output request to the server
        int request_type = AUDIO_OUTPUT_REQUEST;
        write(sockfd, &request_type, sizeof(int));

        if (use_webm) {
            // Handle WebM/Opus file
            printf("[INFO] Processing WebM/Opus file: %s\n", audio_file_path);
            OpusContext opus_ctx = {0};
            if (open_webm_file(&opus_ctx, audio_file_path) == 0) {
                decode_webm_to_pcm(&opus_ctx, sockfd);
                cleanup_opus_context(&opus_ctx);
            } else {
                fprintf(stderr, "Failed to open or process WebM file: %s\n", audio_file_path);
                close(sockfd);
                exit(1);
            }
        } else {
            // Handle regular PCM audio
            FILE *audio_file = use_stdin ? stdin : fopen(audio_file_path, "rb");
            if (!audio_file) {
                perror("fopen");
                close(sockfd);
                exit(1);
            }

            playback_audio(sockfd, audio_file);

            if (!use_stdin) {
                fclose(audio_file);
            }
        }
    }

    close(sockfd);
    return 0;
}
