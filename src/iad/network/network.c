#include <bits/socket.h>       // for SOCK_STREAM
#include <stdlib.h>            // for free, malloc
#include <string.h>            // for NULL, strncpy, memset, strcmp, strncmp
#include <sys/socket.h>        // for sa_family_t, ssize_t, __pthread, pthre...
#include <pthread.h>           // for pthread_mutex_unlock, pthread_mutex_lock
#include <stdio.h>             // for printf, snprintf, sscanf
#include <sys/un.h>            // for strlen, sockaddr_un
#include <unistd.h>            // for close, write, read
#include "input.h"    // for AiThreadArg, ai_record_thread
#include "audio_common.h"    // for audio common
#include "config.h"   // for config_get_ai_socket, config_get_ao_so...
#include "logging.h"  // for handle_audio_error
#include "utils.h"    // for audio_buffer_lock, ClientNode, active_...
#include "network.h"
#include "output.h"            // for g_ao_max_frame_size

char AUDIO_INPUT_SOCKET_PATH[32] = "ingenic_audio_input";
char AUDIO_OUTPUT_SOCKET_PATH[32] = "ingenic_audio_output";
char AUDIO_CONTROL_SOCKET_PATH[32] = "ingenic_audio_control";

#define TAG "NET"

typedef struct {
    int device_id;
    int channel_id;
} AudioAttributes;

// Sample variables for demonstration purposes
int sampleVariableA = 0;
int sampleVariableB = 1;

char* get_variable_value(const char* variable_name) {
    if (strcmp(variable_name, "sampleVariableA") == 0) {
        char* value = (char*) malloc(10 * sizeof(char));
        snprintf(value, 10, "%d", sampleVariableA);
        return value;
    } else if (strcmp(variable_name, "sampleVariableB") == 0) {
        char* value = (char*) malloc(10 * sizeof(char));
        snprintf(value, 10, "%d", sampleVariableB);
        return value;
    } else {
        return NULL;
    }
}

int set_variable_value(const char* variable_name, const char* value) {
    if (strcmp(variable_name, "sampleVariableA") == 0) {
        sampleVariableA = atoi(value);
        return 0;
    } else if (strcmp(variable_name, "sampleVariableB") == 0) {
        sampleVariableB = atoi(value);
        return 0;
    } else {
        return -1;
    }
}

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

void handle_control_client(int client_sock) {
    char buffer[256];
    ssize_t bytes_received = recv(client_sock, buffer, sizeof(buffer) - 1, 0);

    if (bytes_received <= 0) {
        handle_audio_error(TAG, "recv");
        close(client_sock);
        return;
    }

    buffer[bytes_received] = '\0';  // Null-terminate the received string

    if (strncmp(buffer, "GET ", 4) == 0) {
        char variable_name[100];
        sscanf(buffer + 4, "%s", variable_name);

        char* value = get_variable_value(variable_name);

        if (value) {
            write(client_sock, value, strlen(value));
            free(value);
        } else {
            write(client_sock, "RESPONSE_UNKNOWN_VARIABLE", strlen("RESPONSE_UNKNOWN_VARIABLE"));
        }

    } else if (strncmp(buffer, "SET ", 4) == 0) {
        char variable_name[100];
        char value[100];
        sscanf(buffer + 4, "%s %s", variable_name, value);

        int result = set_variable_value(variable_name, value);

        if (result == 0) {
            write(client_sock, "RESPONSE_OK", strlen("RESPONSE_OK"));
        } else {
            write(client_sock, "RESPONSE_ERROR", strlen("RESPONSE_ERROR"));
        }

    } else {
        write(client_sock, "RESPONSE_ERROR", strlen("RESPONSE_ERROR"));
    }

    close(client_sock);
}

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

        pause_audio_output();

        active_client_sock = client_sock;
        printf("[INFO] Client connected\n");

        memset(audio_buffer, 0, g_ao_max_frame_size);
        audio_buffer_size = 0;
        clear_audio_output_buffer();

        pthread_mutex_unlock(&audio_buffer_lock);

        unsigned char buf[g_ao_max_frame_size];  // Use the global variable instead of DEFAULT_AO_MAX_FRAME_SIZE
        ssize_t read_size;

        resume_audio_output();
        int mute_status = 0;
        mute_audio_output_device(mute_status);

        printf("[INFO] Receiving audio data from client\n");
        while ((read_size = read(client_sock, buf, sizeof(buf))) > 0) {
            pthread_mutex_lock(&audio_buffer_lock);
            memcpy(audio_buffer, buf, read_size);
            audio_buffer_size = read_size;
            pthread_cond_signal(&audio_data_cond);
            pthread_mutex_unlock(&audio_buffer_lock);
        }

        mute_status = 1;  // or some value
        mute_audio_output_device(mute_status);

        // Flush audio buffer so audio doesn't get stuck in the buffer and playback next time a client connects.
        flush_audio_output_buffer();
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
