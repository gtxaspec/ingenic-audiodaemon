#include "network.h"
#include "../audio/output.h"
#include "../utils/utils.h"
#include "../audio/input.h"

void *audio_input_server_thread(void *arg) {
    printf("[INFO] Entering audio_input_server_thread\n");

    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return NULL;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(&addr.sun_path[1], AUDIO_INPUT_SOCKET_PATH, sizeof(addr.sun_path) - 2);

    printf("[INFO] Attempting to bind socket\n");
    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(sa_family_t) + strlen(AUDIO_INPUT_SOCKET_PATH) + 1) == -1) {
        perror("bind failed");
        close(sockfd);
        return NULL;
    } else {
        printf("[INFO] Successfully bound socket\n");
        printf("[DEBUG] Binding to socket path: %s\n", &addr.sun_path[1]);
    }

    printf("[INFO] Attempting to listen on socket\n");
    if (listen(sockfd, 5) == -1) {
        perror("listen");
        close(sockfd);
        return NULL;
    }

    while (1) {
        printf("[INFO] Waiting for input client connection\n");
        int client_sock = accept(sockfd, NULL, NULL);
        if (client_sock == -1) {
            perror("accept");
            continue;
        }

        printf("[INFO] Input client connected\n");

        // Handle the input client...
        AiThreadArg thread_arg;
        thread_arg.sockfd = client_sock;

        pthread_t ai_thread;
        if (pthread_create(&ai_thread, NULL, ai_record_thread, &thread_arg) != 0) {
            perror("pthread_create");
            close(client_sock);
            continue;
        }
        pthread_detach(ai_thread);
    }

    close(sockfd);
    return NULL;
}

void *audio_output_server_thread(void *arg) {
    printf("[INFO] Entering audio_output_server_thread\n");

    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return NULL;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(&addr.sun_path[1], AUDIO_OUTPUT_SOCKET_PATH, sizeof(addr.sun_path) - 2);

    printf("[INFO] Attempting to bind socket\n");
    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(sa_family_t) + strlen(AUDIO_OUTPUT_SOCKET_PATH) + 1) == -1) {
        perror("bind failed");
        close(sockfd);
        return NULL;
    } else {
        printf("[INFO] Successfully bound socket\n");
        printf("[DEBUG] Binding to socket path: %s\n", &addr.sun_path[1]);
    }

    printf("[INFO] Attempting to listen on socket\n");
    if (listen(sockfd, 5) == -1) {
        perror("listen");
        close(sockfd);
        return NULL;
    }

    while (1) {
        printf("[INFO] Waiting for output client connection\n");
        int client_sock = accept(sockfd, NULL, NULL);
        if (client_sock == -1) {
            perror("accept");
            continue;
        }

        pthread_mutex_lock(&audio_buffer_lock);

        // Wait until no client is active
        while (active_client_sock != -1) {
            pthread_cond_wait(&audio_data_cond, &audio_buffer_lock);
        }

        // Pause audio output
        pause_audio_output(0, 0);

        active_client_sock = client_sock;  // Set the current client as active
        printf("[INFO] Client connected\n");

        // Clear the audio buffer and the audio output buffer before receiving data from a new client
        memset(audio_buffer, 0, sizeof(audio_buffer));
        audio_buffer_size = 0;
        clear_audio_output_buffer(0, 0);

        pthread_mutex_unlock(&audio_buffer_lock);

        unsigned char buf[AO_MAX_FRAME_SIZE];
        ssize_t read_size;

        // Resume audio output, before receiving audio data
        resume_audio_output(0, 0);

        printf("[INFO] Receiving audio data from client\n");
        while ((read_size = read(client_sock, buf, sizeof(buf))) > 0) {
            pthread_mutex_lock(&audio_buffer_lock);
            memcpy(audio_buffer, buf, read_size);
            audio_buffer_size = read_size;
            pthread_cond_signal(&audio_data_cond); // Notify the playback thread
            pthread_mutex_unlock(&audio_buffer_lock);
        }

        pthread_mutex_lock(&audio_buffer_lock);
        active_client_sock = -1;  // Reset the active client
        pthread_cond_broadcast(&audio_data_cond);
        pthread_mutex_unlock(&audio_buffer_lock);

        close(client_sock);
        printf("[INFO] Client Disconnected\n");
    }

    close(sockfd);
    return NULL;
}
