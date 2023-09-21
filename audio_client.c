#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "client/cmdline.h"
#include "client/client_network.h"
#include "client/playback.h"

int main(int argc, char *argv[]) {
    int use_stdin = 0;
    char *audio_file_path = NULL;

    if (parse_arguments(argc, argv, &use_stdin, &audio_file_path) != 0) {
        exit(1);
    }

    int sockfd = setup_client_connection();
    if (sockfd < 0) {
        exit(1);
    }

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

    close(sockfd);
    return 0;
}
