#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>
#include <playback.h>

long long current_time_in_milliseconds() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000 + (long long)tv.tv_usec / 1000;
}

void playback_audio(int sockfd, FILE *audio_file) {
    printf("[INFO] Playing back audio to daemon\n");

    unsigned char buf[AO_MAX_FRAME_SIZE];
    ssize_t read_size;

    long long start_time, end_time;
    while ((read_size = fread(buf, 1, sizeof(buf), audio_file)) > 0) {
        start_time = current_time_in_milliseconds();
        write(sockfd, buf, read_size);
        end_time = current_time_in_milliseconds();
        
        long long playback_time = end_time - start_time;
        long long sleep_duration = AO_TEST_SAMPLE_TIME * 1000 - playback_time;
        
        if (sleep_duration > 0) {
            usleep(sleep_duration);
        } else {
            usleep(1000);
        }
    }
}
