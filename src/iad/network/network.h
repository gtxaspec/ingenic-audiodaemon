#ifndef NETWORK_H
#define NETWORK_H

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define AUDIO_INPUT_SOCKET_PATH "ingenic_audio_input"
#define AUDIO_OUTPUT_SOCKET_PATH "ingenic_audio_output"
#define AUDIO_CONTROL_SOCKET_PATH "ingenic_audio_control"

#define AUDIO_INPUT_REQUEST 1
#define AUDIO_OUTPUT_REQUEST 2

// Functions
void *audio_input_server_thread(void *arg);
void *audio_output_server_thread(void *arg);
void *audio_control_server_thread(void *arg);

#endif // NETWORK_H
