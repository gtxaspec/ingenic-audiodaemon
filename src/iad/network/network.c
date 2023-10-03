#include <string.h>
#include "network.h"
#include "../audio/input.h"
#include "../audio/output.h"
#include "../utils/config.h"
#include "../utils/utils.h"
#include "../utils/logging.h"

char AUDIO_INPUT_SOCKET_PATH[32] = "ingenic_audio_input";
char AUDIO_OUTPUT_SOCKET_PATH[32] = "ingenic_audio_output";
char AUDIO_CONTROL_SOCKET_PATH[32] = "ingenic_audio_control";

#define TAG "NET"

/**
 * Audio attributes structure to encapsulate device and channel IDs.
 */
typedef struct {
    int device_id;
    int channel_id;
} AudioAttributes;

/**
 * Updates socket paths based on the configuration.
 */
void update_socket_paths_from_config() {
    char *ao_socket_from_config = config_get_ao_socket();
    if (ao_socket_from_config) {
        strncpy(AUDIO_OUTPUT_SOCKET_PATH, ao_socket_from_config, sizeof(AUDIO_OUTPUT_SOCKET_PATH) - 1);
        AUDIO_OUTPUT_SOCKET_PATH[sizeof(AUDIO_OUTPUT_SOCKET_PATH) - 1] = '\0';
        free(ao_socket_from_config);
    }

    char *ai_socket_from_config = config_get_ai_socket();
    if (ai_socket_from_config) {
        strncpy(AUDIO_INPUT_SOCKET_PATH, ai_socket_from_config, sizeof(AUDIO_INPUT_SOCKET_PATH) - 1);
        AUDIO_INPUT_SOCKET_PATH[sizeof(AUDIO_INPUT_SOCKET_PATH) - 1] = '\0';
        free(ai_socket_from_config);
    }

    char *ctrl_socket_from_config = config_get_ctrl_socket();
    if (ctrl_socket_from_config) {
        strncpy(AUDIO_CONTROL_SOCKET_PATH, ctrl_socket_from_config, sizeof(AUDIO_CONTROL_SOCKET_PATH) - 1);
        AUDIO_CONTROL_SOCKET_PATH[sizeof(AUDIO_CONTROL_SOCKET_PATH) - 1] = '\0';
        free(ctrl_socket_from_config);
    }
}

/**
 * Handle control client requests.
 * @param client_sock The client socket.
 */
void handle_control_client(int client_sock) {
    int client_request_type;
    ssize_t bytes_received = recv(client_sock, &client_request_type, sizeof(int), 0);
    if (bytes_received <= 0) {
        handle_audio_error(TAG, "recv");
        close(client_sock);
        return;
    }

    pthread_mutex_lock(&audio_buffer_lock);

    if (client_request_type == AUDIO_OUTPUT_REQUEST && active_client_sock != -1) {
        if (write(client_sock, "queued", strlen("queued")) == -1) {
              handle_audio_error(TAG, "write");
        }
    } else {
        if (write(client_sock, "not_queued", strlen("not_queued")) == -1) {
            handle_audio_error(TAG, "write");
        }
    }

    pthread_mutex_unlock(&audio_buffer_lock);
    close(client_sock);
}

/**
 * Handle audio input client requests.
 * @param client_sock The client socket.
 */
void handle_audio_input_client(int client_sock) {
    pthread_mutex_lock(&audio_buffer_lock);

    ClientNode *new_client = (ClientNode *)malloc(sizeof(ClientNode));
    if (!new_client) {
        handle_audio_error(TAG, "malloc");
        close(client_sock);
        pthread_mutex_unlock(&audio_buffer_lock);
        return;
    }
    new_client->sockfd = client_sock;
    new_client->next = client_list_head;
    client_list_head = new_client;

    pthread_mutex_unlock(&audio_buffer_lock);

    printf("[INFO] Input client connected\n");

    AiThreadArg thread_arg;
    thread_arg.sockfd = client_sock;

    pthread_t ai_thread;
    if (pthread_create(&ai_thread, NULL, ai_record_thread, &thread_arg) != 0) {
        handle_audio_error(TAG, "pthread_create");
        close(client_sock);
        free(new_client);
    } else {
        pthread_detach(ai_thread);
    }
}

void *audio_control_server_thread(void *arg) {
    printf("[INFO] Entering audio_control_server_thread\n");

    update_socket_paths_from_config();

    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        handle_audio_error(TAG, "socket");
        return NULL;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(&addr.sun_path[1], AUDIO_CONTROL_SOCKET_PATH, sizeof(addr.sun_path) - 2);
    addr.sun_path[sizeof(addr.sun_path) - 1] = '\0';

    printf("[INFO] Attempting to bind control socket\n");
    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(sa_family_t) + strlen(AUDIO_CONTROL_SOCKET_PATH) + 1) == -1) {
        handle_audio_error(TAG, "bind failed");
        close(sockfd);
        return NULL;
    }

    printf("[INFO] Attempting to listen on control socket\n");
    if (listen(sockfd, 5) == -1) {
        handle_audio_error(TAG, "listen");
        close(sockfd);
        return NULL;
    }

    while (1) {
        printf("[INFO] Waiting for control client connection\n");
        int client_sock = accept(sockfd, NULL, NULL);
        if (client_sock == -1) {
            handle_audio_error(TAG, "accept");
            continue;
        }
        handle_control_client(client_sock);
    }

    close(sockfd);
    return NULL;
}

void *audio_input_server_thread(void *arg) {
    printf("[INFO] Entering audio_input_server_thread\n");

    update_socket_paths_from_config();

    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        handle_audio_error(TAG, "socket");
        return NULL;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(&addr.sun_path[1], AUDIO_INPUT_SOCKET_PATH, sizeof(addr.sun_path) - 2);
    addr.sun_path[sizeof(addr.sun_path) - 1] = '\0';

    printf("[INFO] Attempting to bind socket\n");
    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(sa_family_t) + strlen(AUDIO_INPUT_SOCKET_PATH) + 1) == -1) {
        handle_audio_error(TAG, "bind failed");
        close(sockfd);
        return NULL;
    }

    printf("[INFO] Attempting to listen on socket\n");
    if (listen(sockfd, 5) == -1) {
        handle_audio_error(TAG, "listen");
        close(sockfd);
        return NULL;
    }

    while (1) {
        printf("[INFO] Waiting for input client connection\n");
        int client_sock = accept(sockfd, NULL, NULL);
        if (client_sock == -1) {
            handle_audio_error(TAG, "accept");
            continue;
        }
        handle_audio_input_client(client_sock);
    }

    close(sockfd);
    return NULL;
}

void *audio_output_server_thread(void *arg) {
    printf("[INFO] Entering audio_output_server_thread\n");

    update_socket_paths_from_config();

    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        handle_audio_error(TAG, "socket");
        return NULL;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(&addr.sun_path[1], AUDIO_OUTPUT_SOCKET_PATH, sizeof(addr.sun_path) - 2);
    addr.sun_path[sizeof(addr.sun_path) - 1] = '\0';

    printf("[INFO] Attempting to bind socket\n");
    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(sa_family_t) + strlen(AUDIO_OUTPUT_SOCKET_PATH) + 1) == -1) {
        handle_audio_error(TAG, "bind failed");
        close(sockfd);
        return NULL;
    }

    printf("[INFO] Attempting to listen on socket\n");
    if (listen(sockfd, 5) == -1) {
        handle_audio_error(TAG, "listen");
        close(sockfd);
        return NULL;
    }

    while (1) {
        printf("[INFO] Waiting for output client connection\n");
        int client_sock = accept(sockfd, NULL, NULL);
        if (client_sock == -1) {
            handle_audio_error(TAG, "accept");
            continue;
        }

        pthread_mutex_lock(&audio_buffer_lock);

        while (active_client_sock != -1) {
            pthread_cond_wait(&audio_data_cond, &audio_buffer_lock);
        }

        pause_audio_output(0, 0);

        active_client_sock = client_sock;
        printf("[INFO] Client connected\n");

        memset(audio_buffer, 0, sizeof(audio_buffer));
        audio_buffer_size = 0;
        clear_audio_output_buffer(0, 0);

        pthread_mutex_unlock(&audio_buffer_lock);

        unsigned char buf[DEFAULT_AO_MAX_FRAME_SIZE];
        ssize_t read_size;

        resume_audio_output(0, 0);

        printf("[INFO] Receiving audio data from client\n");
        while ((read_size = read(client_sock, buf, sizeof(buf))) > 0) {
            pthread_mutex_lock(&audio_buffer_lock);
            memcpy(audio_buffer, buf, read_size);
            audio_buffer_size = read_size;
            pthread_cond_signal(&audio_data_cond);
            pthread_mutex_unlock(&audio_buffer_lock);
        }

        pthread_mutex_lock(&audio_buffer_lock);
        active_client_sock = -1;
        pthread_cond_broadcast(&audio_data_cond);
        pthread_mutex_unlock(&audio_buffer_lock);

        close(client_sock);
        printf("[INFO] Client Disconnected\n");
    }

    close(sockfd);
    return NULL;
}
