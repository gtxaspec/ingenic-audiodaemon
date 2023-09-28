#ifndef PLAYBACK_H
#define PLAYBACK_H

#define AO_TEST_SAMPLE_TIME 1
#define AO_MAX_FRAME_SIZE 1280

long long current_time_in_milliseconds();
void playback_audio(int sockfd, FILE *audio_file);

#endif // PLAYBACK_H
