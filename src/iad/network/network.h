#ifndef NETWORK_H
#define NETWORK_H

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

extern char AUDIO_INPUT_SOCKET_PATH[];
extern char AUDIO_OUTPUT_SOCKET_PATH[];
extern char AUDIO_CONTROL_SOCKET_PATH[];

#define CLIENT_QUEUED 1
#define CLIENT_NOT_QUEUED 0

#define AUDIO_INPUT_REQUEST 1
#define AUDIO_OUTPUT_REQUEST 2

// Functions
void *audio_input_server_thread(void *arg);
void *audio_output_server_thread(void *arg);
void *audio_control_server_thread(void *arg);

#endif // NETWORK_H
