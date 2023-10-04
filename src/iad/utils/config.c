#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include "config.h"
#include "utils.h"

// Global pointer for the root of the configuration JSON object
static cJSON *config_root = NULL;

// Mutex to ensure thread-safe access to the configuration
static pthread_mutex_t config_mutex;
static pthread_mutexattr_t config_mutex_attr;  // Attributes for the recursive mutex

/**
 * Load configuration from the specified file into the global config_root cJSON object.
 * This function will initialize the mutex, read the file, and parse its contents as JSON.
 * @param config_file_path Path to the configuration file.
 * @return 0 on success, -1 on failure.
 */
int config_load_from_file(const char *config_file_path) {
    // Initialize the recursive mutex attribute and the mutex
    pthread_mutexattr_init(&config_mutex_attr);
    pthread_mutexattr_settype(&config_mutex_attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&config_mutex, &config_mutex_attr);

    FILE *file = fopen(config_file_path, "r");
    if (!file) {
        fprintf(stderr, "[ERROR] Configuration file '%s' not found.\n", config_file_path);
        return -1;
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Check if the file is empty
    if (length == 0) {
        fprintf(stderr, "[ERROR] Configuration file '%s' is empty.\n", config_file_path);
        fclose(file);
        return -1;
    }

    char *content = calloc(1, length + 1);  // +1 for the null terminator
    if (!content) {
        fclose(file);
        return -1;
    }

    if (fread(content, 1, length, file) != length) {
        fclose(file);
        free(content);
        return -1;
    }

    fclose(file);
    config_root = cJSON_Parse(content);

    // Check if parsing was successful and log an error if it wasn't
    if (!config_root) {
        fprintf(stderr, "Failed to parse JSON config. Error near: %s\n", cJSON_GetErrorPtr());
        free(content);
        return -1;
    }

    free(content);
    return 0;
}

/**
 * Cleanup the loaded configuration. This function will free the memory associated 
 * with the global cJSON object and destroy the mutex.
 */
void config_cleanup(void) {
    pthread_mutex_lock(&config_mutex);
    if (config_root) {
        cJSON_Delete(config_root);
        config_root = NULL;
    }
    pthread_mutex_unlock(&config_mutex);

    // Destroy the mutex
    pthread_mutex_destroy(&config_mutex);
    pthread_mutexattr_destroy(&config_mutex_attr);
}

/**
 * Retrieve the 'audio' configuration object.
 * @return Pointer to the cJSON 'audio' object, or NULL if not found.
 */
cJSON *get_audio_config(void) {
    pthread_mutex_lock(&config_mutex);
    cJSON *item = cJSON_GetObjectItemCaseSensitive(config_root, "audio");
    pthread_mutex_unlock(&config_mutex);
    return item;
}

/**
 * Retrieve the 'network' configuration object.
 * @return Pointer to the cJSON 'network' object, or NULL if not found.
 */
cJSON *get_network_config(void) {
    pthread_mutex_lock(&config_mutex);
    cJSON *item = cJSON_GetObjectItemCaseSensitive(config_root, "network");
    pthread_mutex_unlock(&config_mutex);
    return item;
}

/**
 * Check if AI (Audio Input) is enabled in the configuration.
 * @return 1 if enabled, 0 otherwise.
 */
int config_get_ai_enabled() {
    cJSON *audio = get_audio_config();
    if (!audio) {
        fprintf(stderr, "Warning: Using default AI enabled value.\n");
        return 1; // Default to enabled if no audio config
    }

    cJSON *AI_attributes = cJSON_GetObjectItemCaseSensitive(audio, "AI_attributes");
    if (!AI_attributes) {
        fprintf(stderr, "Warning: Using default AI enabled value.\n");
        return 1; // Default to enabled if no AI_attributes
    }

    cJSON *enabled = cJSON_GetObjectItemCaseSensitive(AI_attributes, "enabled");
    return cJSON_IsTrue(enabled);
}

/**
 * Check if AO (Audio Output) is enabled in the configuration.
 * @return 1 if enabled, 0 otherwise.
 */
int config_get_ao_enabled() {
    cJSON *audio = get_audio_config();
    if (!audio) {
        fprintf(stderr, "Warning: Using default AO enabled value.\n");
        return 1;
    }

    cJSON *AO_attributes = cJSON_GetObjectItemCaseSensitive(audio, "AO_attributes");
    if (!AO_attributes) {
        fprintf(stderr, "Warning: Using default AO enabled value.\n");
        return 1;
    }

    cJSON *enabled = cJSON_GetObjectItemCaseSensitive(AO_attributes, "enabled");
    return cJSON_IsTrue(enabled);
}

char* config_get_socket_path(const char *socket_name) {
    pthread_mutex_lock(&config_mutex);

    cJSON *audio = get_audio_config();
    if (!audio) {
        pthread_mutex_unlock(&config_mutex);
        return NULL;
    }

    cJSON *network = cJSON_GetObjectItemCaseSensitive(audio, "network");
    if (!network) {
        pthread_mutex_unlock(&config_mutex);
        return NULL;
    }

    cJSON *socket_item = cJSON_GetObjectItemCaseSensitive(network, socket_name);
    if (!socket_item || socket_item->type != cJSON_String) {
        pthread_mutex_unlock(&config_mutex);
        return NULL;
    }

    char* result = strdup(socket_item->valuestring);
    pthread_mutex_unlock(&config_mutex);

    return result;
}

char* config_get_ai_socket() {
    return config_get_socket_path("audio_input_socket_path");
}

char* config_get_ao_socket() {
    return config_get_socket_path("audio_output_socket_path");
}

char* config_get_ctrl_socket() {
    return config_get_socket_path("audio_control_socket_path");
}

cJSON* get_audio_attribute(AudioType type, const char* attribute_name) {
    cJSON *audioConfigRoot = get_audio_config();
    if (audioConfigRoot) {
        const char* typeStr = (type == AUDIO_INPUT) ? "AI_attributes" : "AO_attributes";
        cJSON *audioConfig = cJSON_GetObjectItemCaseSensitive(audioConfigRoot, typeStr);
        if (audioConfig) {
            return cJSON_GetObjectItemCaseSensitive(audioConfig, attribute_name);
        }
    }
    return NULL;
}

int config_get_ao_frame_size() {
    cJSON *audio = get_audio_config();
    if (!audio) return DEFAULT_AO_MAX_FRAME_SIZE;

    cJSON *AO_attributes = cJSON_GetObjectItemCaseSensitive(audio, "AO_attributes");
    if (!AO_attributes) return DEFAULT_AO_MAX_FRAME_SIZE;

    cJSON *frame_size = cJSON_GetObjectItemCaseSensitive(AO_attributes, "frame_size");
    if (!frame_size || !cJSON_IsNumber(frame_size)) {
        return DEFAULT_AO_MAX_FRAME_SIZE;
    }

    return frame_size->valueint;
}
