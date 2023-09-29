#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>

static cJSON *config_root = NULL;

static pthread_mutex_t config_mutex = PTHREAD_MUTEX_INITIALIZER;

int config_load_from_file(const char *config_file_path) {
    FILE *file = fopen(config_file_path, "r");
    if (!file) {
        return -1;
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *content = malloc(length);
    if (!content) {
        fclose(file);
        return -1;
    }

    fread(content, 1, length, file);
    fclose(file);

    config_root = cJSON_Parse(content);
    free(content);
    
    if (!config_root) {
        return -1;
    }

    return 0;
}

void config_cleanup(void) {
    if (config_root) {
        cJSON_Delete(config_root);
        config_root = NULL;
    }
}

cJSON *get_audio_config(void) {
    if (config_root) {
        return cJSON_GetObjectItemCaseSensitive(config_root, "audio");
    }
    return NULL;
}

cJSON *get_network_config(void) {
    if (config_root) {
        return cJSON_GetObjectItemCaseSensitive(config_root, "network");
    }
    return NULL;
}

int config_get_ai_enabled() {
    cJSON *audio = cJSON_GetObjectItemCaseSensitive(config_root, "audio");
    if (!audio) return 0;

    cJSON *AI_attributes = cJSON_GetObjectItemCaseSensitive(audio, "AI_attributes");
    if (!AI_attributes) return 0;

    cJSON *enabled = cJSON_GetObjectItemCaseSensitive(AI_attributes, "enabled");
    return cJSON_IsTrue(enabled);
}

int config_get_ao_enabled() {
    cJSON *audio = cJSON_GetObjectItemCaseSensitive(config_root, "audio");
    if (!audio) return 0;

    cJSON *AO_attributes = cJSON_GetObjectItemCaseSensitive(audio, "AO_attributes");
    if (!AO_attributes) return 0;

    cJSON *enabled = cJSON_GetObjectItemCaseSensitive(AO_attributes, "enabled");
    return cJSON_IsTrue(enabled);
}

char* config_get_ao_socket() {
    pthread_mutex_lock(&config_mutex);

    // First, retrieve the "audio" object from the root.
    cJSON *audio = cJSON_GetObjectItemCaseSensitive(config_root, "audio");
    if (!audio) {
        pthread_mutex_unlock(&config_mutex);
        return NULL;
    }

    // Now, get the "network" object from within the "audio" object.
    cJSON *network = cJSON_GetObjectItemCaseSensitive(audio, "network");
    if (!network) {
        pthread_mutex_unlock(&config_mutex);
        return NULL;
    }

    // Finally, retrieve the "audio_output_socket_path" from the "network" object.
    cJSON *socket_item = cJSON_GetObjectItemCaseSensitive(network, "audio_output_socket_path");
    if (!socket_item || socket_item->type != cJSON_String) {
        pthread_mutex_unlock(&config_mutex);
        return NULL;
    }

    char* result = strdup(socket_item->valuestring);  // Dynamically allocate a copy of the string

    pthread_mutex_unlock(&config_mutex);
    
    return result;  // Caller is responsible for freeing this string using free()
}

char* config_get_ai_socket() {
    pthread_mutex_lock(&config_mutex);

    // First, retrieve the "audio" object from the root.
    cJSON *audio = cJSON_GetObjectItemCaseSensitive(config_root, "audio");
    if (!audio) {
        pthread_mutex_unlock(&config_mutex);
        return NULL;
    }

    // Now, get the "network" object from within the "audio" object.
    cJSON *network = cJSON_GetObjectItemCaseSensitive(audio, "network");
    if (!network) {
        pthread_mutex_unlock(&config_mutex);
        return NULL;
    }

    // Finally, retrieve the "audio_input_socket_path" from the "network" object.
    cJSON *socket_item = cJSON_GetObjectItemCaseSensitive(network, "audio_input_socket_path");
    if (!socket_item || socket_item->type != cJSON_String) {
        pthread_mutex_unlock(&config_mutex);
        return NULL;
    }

    char* result = strdup(socket_item->valuestring);  // Dynamically allocate a copy of the string

    pthread_mutex_unlock(&config_mutex);
    
    return result;  // Caller is responsible for freeing this string using free()
}

char* config_get_ctrl_socket() {
    pthread_mutex_lock(&config_mutex);

    // First, retrieve the "audio" object from the root.
    cJSON *audio = cJSON_GetObjectItemCaseSensitive(config_root, "audio");
    if (!audio) {
        pthread_mutex_unlock(&config_mutex);
        return NULL;
    }

    // Now, get the "network" object from within the "audio" object.
    cJSON *network = cJSON_GetObjectItemCaseSensitive(audio, "network");
    if (!network) {
        pthread_mutex_unlock(&config_mutex);
        return NULL;
    }

    // Finally, retrieve the "audio_output_socket_path" from the "network" object.
    cJSON *socket_item = cJSON_GetObjectItemCaseSensitive(network, "audio_control_socket_path");
    if (!socket_item || socket_item->type != cJSON_String) {
        pthread_mutex_unlock(&config_mutex);
        return NULL;
    }

    char* result = strdup(socket_item->valuestring);  // Dynamically allocate a copy of the string

    pthread_mutex_unlock(&config_mutex);
    
    return result;  // Caller is responsible for freeing this string using free()
}
