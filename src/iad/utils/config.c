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

	//Debug: Print loaded json config
	//cJSON *loaded_config = get_audio_config();
	//printf("Loaded JSON: %s\n", cJSON_Print(loaded_config));

        free(content);
        return -1;
    }

    free(content);
    return 0;
}

/**
 * @brief Validates the loaded configuration JSON for correct structure and keys.
 *
 * This function checks the loaded configuration to ensure that it contains all
 * the necessary keys and that they have the expected data types. It checks both
 * the high-level structure as well as specific audio attributes and settings.
 *
 * @param root The root cJSON object of the loaded configuration.
 * @return int 1 if the configuration is valid, 0 otherwise.
 */
int validate_json(cJSON *root) {
    // Check if the root object exists and is of the correct type
    if (!root || !cJSON_IsObject(root)) {
        fprintf(stderr, "Invalid configuration format. Root should be an object.\n");
        return 0;
    }

    // Since the root is now the 'audio' object directly, we don't need to fetch it again.
    cJSON *audio = root;

    // Define the struct type for attributes
    struct Attribute {
        const char *key;
        cJSON_bool (*validator)(const cJSON * const);
    };

    // List of required attributes and their expected validators for AO_attributes
    struct Attribute ao_attributes[] = {
        {"enabled", cJSON_IsBool},
        {"device_id", cJSON_IsNumber},
        {"channel_id", cJSON_IsNumber},
        {"sample_rate", cJSON_IsNumber},
        {"frmNum", cJSON_IsNumber},
        {"bitwidth", cJSON_IsString},
        {"soundmode", cJSON_IsString},
        {"chnCnt", cJSON_IsNumber},
        {"SetVol", cJSON_IsNumber},
        {"SetGain", cJSON_IsNumber},
        {"Enable_Agc", cJSON_IsBool},
        {"AGC_attributes", cJSON_IsObject},
        {"Enable_Hpf", cJSON_IsBool},
        {"HPF_attributes", cJSON_IsObject}
    };

    // List of required attributes and their expected validators for AI_attributes
    struct Attribute ai_attributes[] = {
        {"enabled", cJSON_IsBool},
        {"device_id", cJSON_IsNumber},
        {"channel_id", cJSON_IsNumber},
        {"sample_rate", cJSON_IsNumber},
        {"frmNum", cJSON_IsNumber},
        {"bitwidth", cJSON_IsString},
        {"soundmode", cJSON_IsString},
        {"chnCnt", cJSON_IsNumber},
        {"SetVol", cJSON_IsNumber},
        {"SetGain", cJSON_IsNumber},
        {"SetAlcGain", cJSON_IsNumber},
        {"Enable_Ns", cJSON_IsBool},
        {"Level_Ns", cJSON_IsNumber},
        {"Enable_Hpf", cJSON_IsBool},
        {"EnableAec", cJSON_IsBool},
        {"Enable_Agc", cJSON_IsBool},
        {"AGC_attributes", cJSON_IsObject},
    };

    // Inside AGC_attributes
    struct {
        const char *key;
        cJSON_bool (*validator)(const cJSON * const);
    } agc_attributes[] = {
        {"TargetLevelDbfs", cJSON_IsNumber},
        {"CompressionGaindB", cJSON_IsNumber}
    };

    // Inside HPF_attributes
    struct {
        const char *key;
        cJSON_bool (*validator)(const cJSON * const);
    } hpf_attributes[] = {
        {"SetHpfCoFrequency", cJSON_IsNumber}
    };

    // Validate the 'AO_attributes' and 'AI_attributes' objects
    const char *sections[] = {"AO_attributes", "AI_attributes"};
    struct Attribute *current_attributes;

    for (int s = 0; s < 2; s++) {
        if (strcmp(sections[s], "AO_attributes") == 0) {
            current_attributes = ao_attributes;
        } else if (strcmp(sections[s], "AI_attributes") == 0) {
            current_attributes = ai_attributes;
        }

        cJSON *section = cJSON_GetObjectItemCaseSensitive(audio, sections[s]);
        if (!section || !cJSON_IsObject(section)) {
            fprintf(stderr, "Invalid configuration format. '%s' key is missing or not an object.\n", sections[s]);
            return 0;
        }

        int attributes_len;
        if (strcmp(sections[s], "AO_attributes") == 0) {
            attributes_len = sizeof(ao_attributes) / sizeof(ao_attributes[0]);
        } else {
            attributes_len = sizeof(ai_attributes) / sizeof(ai_attributes[0]);
        }

        for (int i = 0; i < attributes_len; i++) {
            cJSON *item = cJSON_GetObjectItemCaseSensitive(section, current_attributes[i].key);
            if (!item || !current_attributes[i].validator(item)) {
                fprintf(stderr, "Invalid configuration format. '%s' key in '%s' is missing or of incorrect type.\n", current_attributes[i].key, sections[s]);
                return 0;
            }

            if (strcmp(current_attributes[i].key, "AGC_attributes") == 0) {
                for (int j = 0; j < sizeof(agc_attributes) / sizeof(agc_attributes[0]); j++) {
                    cJSON *agc_item = cJSON_GetObjectItemCaseSensitive(item, agc_attributes[j].key);
                    if (!agc_item || !agc_attributes[j].validator(agc_item)) {
                        fprintf(stderr, "Invalid configuration format. '%s' key in 'AGC_attributes' of '%s' is missing or of incorrect type.\n", agc_attributes[j].key, sections[s]);
                        return 0;
                    }
                }
            }

            if (strcmp(ao_attributes[i].key, "HPF_attributes") == 0 && strcmp(sections[s], "AO_attributes") == 0) {
                for (int j = 0; j < sizeof(hpf_attributes) / sizeof(hpf_attributes[0]); j++) {
                    cJSON *hpf_item = cJSON_GetObjectItemCaseSensitive(item, hpf_attributes[j].key);
                    if (!hpf_item || !hpf_attributes[j].validator(hpf_item)) {
                        fprintf(stderr, "Invalid configuration format. '%s' key in 'HPF_attributes' of '%s' is missing or of incorrect type.\n", hpf_attributes[j].key, sections[s]);
                        return 0;
                    }
                }
            }
        }
    }

    cJSON *network = cJSON_GetObjectItemCaseSensitive(audio, "network");
    if (!network || !cJSON_IsObject(network)) {
        fprintf(stderr, "Invalid configuration format. 'network' key is missing or not an object.\n");
        return 0;
    }

    const char *network_keys[] = {
        "audio_input_socket_path",
        "audio_output_socket_path",
        "audio_control_socket_path"
    };

    for (int i = 0; i < sizeof(network_keys) / sizeof(network_keys[0]); i++) {
        cJSON *item = cJSON_GetObjectItemCaseSensitive(network, network_keys[i]);
        if (!item || !cJSON_IsString(item)) {
            fprintf(stderr, "Invalid configuration format. '%s' key in 'network' is missing or not a string.\n", network_keys[i]);
            return 0;
        }
    }

    return 1;  // Configuration is valid
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

/**
 * Retrieves the socket path for the specified socket name from the configuration.
 * This function is internally synchronized using a mutex to prevent race conditions.
 * @param socket_name The name of the socket whose path is to be retrieved.
 * @return The socket path as a string or NULL if not found or an error occurred.
 */
char* config_get_socket_path(const char *socket_name) {
    pthread_mutex_lock(&config_mutex);

    // Get the audio configuration
    cJSON *audio = get_audio_config();
    if (!audio) {
        pthread_mutex_unlock(&config_mutex);
        return NULL;
    }

    // Get the network configuration within the audio configuration
    cJSON *network = cJSON_GetObjectItemCaseSensitive(audio, "network");
    if (!network) {
        pthread_mutex_unlock(&config_mutex);
        return NULL;
    }

    // Fetch the socket item based on the provided socket name
    cJSON *socket_item = cJSON_GetObjectItemCaseSensitive(network, socket_name);
    if (!socket_item || socket_item->type != cJSON_String) {
        pthread_mutex_unlock(&config_mutex);
        return NULL;
    }

    char* result = strdup(socket_item->valuestring);
    pthread_mutex_unlock(&config_mutex);

    return result;
}

/**
 * Retrieves the audio input socket path from the configuration.
 * @return The audio input socket path as a string or NULL if not found.
 */
char* config_get_ai_socket() {
    return config_get_socket_path("audio_input_socket_path");
}

/**
 * Retrieves the audio output socket path from the configuration.
 * @return The audio output socket path as a string or NULL if not found.
 */
char* config_get_ao_socket() {
    return config_get_socket_path("audio_output_socket_path");
}

/**
 * Retrieves the audio control socket path from the configuration.
 * @return The audio control socket path as a string or NULL if not found.
 */
char* config_get_ctrl_socket() {
    return config_get_socket_path("audio_control_socket_path");
}

/**
 * Fetches a specific audio attribute from the configuration.
 * @param type Specifies the type of audio (input or output).
 * @param attribute_name The name of the attribute to be fetched.
 * @return The cJSON object representing the requested attribute or NULL if not found.
 */
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

/**
 * Retrieves the audio output frame size from the configuration.
 * @return The audio output frame size as an integer. If not found or an error occurred, it returns the default frame size.
 */
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

/**
 * Checks if the given samplerate is valid.
 * @param samplerate The samplerate to check.
 * @return 1 if valid, 0 otherwise.
 */
int is_valid_samplerate(int samplerate) {
    switch (samplerate) {
        case AUDIO_SAMPLE_RATE_8000:
        case AUDIO_SAMPLE_RATE_16000:
        case AUDIO_SAMPLE_RATE_24000:
        case AUDIO_SAMPLE_RATE_32000:
        case AUDIO_SAMPLE_RATE_44100:
        case AUDIO_SAMPLE_RATE_48000:
        case AUDIO_SAMPLE_RATE_96000:
            return 1;  // valid
        default:
            return 0;  // invalid
    }
}
