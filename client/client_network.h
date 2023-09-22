#ifndef CLIENT_NETWORK_H
#define CLIENT_NETWORK_H

#define AUDIO_INPUT_SOCKET_PATH "ingenic_audio_input"
#define AUDIO_OUTPUT_SOCKET_PATH "ingenic_audio_output"

#define AUDIO_INPUT_REQUEST 1
#define AUDIO_OUTPUT_REQUEST 2

// Function declarations
int setup_client_connection();

#endif // CLIENT_NETWORK_H
