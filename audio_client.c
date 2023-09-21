#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <sys/time.h>

#define SERVER_SOCKET_PATH "\0ingenic_audio" // This is an abstract socket
#define AO_TEST_SAMPLE_TIME 1  // Adding the missing definition
#define AO_MAX_FRAME_SIZE 1280  // Adjusted for the maximum frame size

void print_usage(char *program_name) {
    printf("Usage: %s [-f <audio_file_path>] [-s]\n", program_name);
}

long long current_time_in_milliseconds() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000 + (long long)tv.tv_usec / 1000;
}

int main(int argc, char *argv[]) {
    int use_stdin = 0;
    char *audio_file_path = NULL;

    // Parse command-line arguments
    int opt;
    while ((opt = getopt(argc, argv, "sf:")) != -1) {
        switch (opt) {
            case 's':
                use_stdin = 1;
                break;
            case 'f':
                audio_file_path = optarg;
                break;
            default:
                print_usage(argv[0]);
                exit(1);
        }
    }

    if (!use_stdin && audio_file_path == NULL) {
        print_usage(argv[0]);
        exit(1);
    }

    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        exit(1);
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(&addr.sun_path[1], SERVER_SOCKET_PATH + 1, sizeof(addr.sun_path) - 2);

    if (connect(sockfd, (struct sockaddr*)&addr, sizeof(sa_family_t) + strlen(&addr.sun_path[1]) + 1) == -1) {
        perror("connect");
        close(sockfd);
        exit(1);
    }

    unsigned char buf[AO_MAX_FRAME_SIZE];
    ssize_t read_size;

    FILE *audio_file = use_stdin ? stdin : fopen(audio_file_path, "rb");
    if (!audio_file) {
        perror("fopen");
        close(sockfd);
        exit(1);
    }

    long long start_time, end_time;
    while ((read_size = fread(buf, 1, sizeof(buf), audio_file)) > 0) {
        start_time = current_time_in_milliseconds();
        write(sockfd, buf, read_size);
        end_time = current_time_in_milliseconds();
        
        // Calculate the time taken for playback and adjust sleep duration
        long long playback_time = end_time - start_time;
        long long sleep_duration = AO_TEST_SAMPLE_TIME * 1000 - playback_time;
        
        if (sleep_duration > 0) {
            usleep(sleep_duration);  // Sleep for the adjusted duration
        } else {
            usleep(1000); // Sleep for a minimal duration to avoid busy-waiting
        }
    }

    if (!use_stdin) {
        fclose(audio_file);
    }

    close(sockfd);
    return 0;
}
