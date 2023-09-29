#include "network.h"
#include "../audio/input.h"
#include "../audio/output.h"
#include "../utils/config.h"
#include "../utils/utils.h"

char AUDIO_INPUT_SOCKET_PATH[32] = "ingenic_audio_input";
char AUDIO_OUTPUT_SOCKET_PATH[32] = "ingenic_audio_output";
char AUDIO_CONTROL_SOCKET_PATH[32] = "ingenic_audio_control";

void update_socket_paths_from_config() {
    char *ao_socket_from_config = config_get_ao_socket();
    if (ao_socket_from_config) {
        strncpy(AUDIO_OUTPUT_SOCKET_PATH, ao_socket_from_config, sizeof(AUDIO_OUTPUT_SOCKET_PATH) - 1);
        free(ao_socket_from_config);  // Free the dynamically allocated string
    }
    char *ai_socket_from_config = config_get_ai_socket();
    if (ai_socket_from_config) {
        strncpy(AUDIO_INPUT_SOCKET_PATH, ai_socket_from_config, sizeof(AUDIO_INPUT_SOCKET_PATH) - 1);
        free(ai_socket_from_config);  // Free the dynamically allocated string
    }
    char *ctrl_socket_from_config = config_get_ctrl_socket();
    if (ctrl_socket_from_config) {
        strncpy(AUDIO_CONTROL_SOCKET_PATH, ctrl_socket_from_config, sizeof(AUDIO_CONTROL_SOCKET_PATH) - 1);
        free(ctrl_socket_from_config);  // Free the dynamically allocated string
    }
}

void *audio_control_server_thread(void *arg) {
    printf("[INFO] Entering audio_control_server_thread\n");

    // Update the socket path from the configuration (if it's provided)
    update_socket_paths_from_config();

    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return NULL;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(&addr.sun_path[1], AUDIO_CONTROL_SOCKET_PATH, sizeof(addr.sun_path) - 2);

    printf("[INFO] Attempting to bind control socket\n");
    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(sa_family_t) + strlen(AUDIO_CONTROL_SOCKET_PATH) + 1) == -1) {
        perror("bind failed");
        close(sockfd);
        return NULL;
    } else {
        printf("[INFO] Successfully bound control socket\n");
        printf("[DEBUG] Bound to control socket path: %s\n", &addr.sun_path[1]);
    }

    printf("[INFO] Attempting to listen on control socket\n");
    if (listen(sockfd, 5) == -1) {
        perror("listen");
        close(sockfd);
        return NULL;
    }

    while (1) {
        printf("[INFO] Waiting for control client connection\n");
        int client_sock = accept(sockfd, NULL, NULL);
        if (client_sock == -1) {
            perror("accept");
            continue;
        }

        int client_request_type;
        recv(client_sock, &client_request_type, sizeof(int), 0);

        pthread_mutex_lock(&audio_buffer_lock);

        if (client_request_type == AUDIO_OUTPUT_REQUEST && active_client_sock != -1) {
            write(client_sock, "queued", strlen("queued"));
        } else {
            write(client_sock, "not_queued", strlen("not_queued"));
        }

        pthread_mutex_unlock(&audio_buffer_lock);

        close(client_sock);  // Close the control socket after sending the message
    }

    close(sockfd);
    return NULL;
}

void *audio_input_server_thread(void *arg) {
    printf("[INFO] Entering audio_input_server_thread\n");

    printf("[INFO] Initializing audio input device\n");

    cJSON* device_idItem = get_audio_attribute(AUDIO_INPUT, "device_id");
    int devID = device_idItem ? device_idItem->valueint : DEFAULT_AI_DEV_ID;

    if (initialize_audio_input_device(devID) != 0) {
        fprintf(stderr, "[ERROR] Failed to initialize audio input device\n");
        return NULL;
    }

    // Update the socket path from the configuration (if it's provided)
    update_socket_paths_from_config();

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
        printf("[DEBUG] Bound to socket path: %s\n", &addr.sun_path[1]);
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

        pthread_mutex_lock(&audio_buffer_lock);

        // Add the client to the list
        ClientNode *new_client = (ClientNode *)malloc(sizeof(ClientNode));
        new_client->sockfd = client_sock;
        new_client->next = client_list_head;
        client_list_head = new_client;

        pthread_mutex_unlock(&audio_buffer_lock);

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

    // Update the socket path from the configuration (if it's provided)
    update_socket_paths_from_config();

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
        printf("[DEBUG] Bound to socket path: %s\n", &addr.sun_path[1]);
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
