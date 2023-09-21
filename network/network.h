#ifndef NETWORK_H
#define NETWORK_H

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define SERVER_SOCKET_PATH "ingenic_audio"

// Functions
void *audio_server_thread(void *arg);

#endif // NETWORK_H
