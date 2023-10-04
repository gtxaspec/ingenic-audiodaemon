#ifndef CONFIG_H
#define CONFIG_H
#include <cJSON.h>

// -----------------------------
// Configuration Handling Functions
// -----------------------------

/**
 * @brief Initialize the configuration system by loading the provided file.
 *
 * @param config_file_path Path to the configuration file to load.
 * @return int 0 on success, -1 on error.
 */
int config_load_from_file(const char *config_file_path);

/**
 * @brief Cleanup and free resources associated with the configuration system.
 */
void config_cleanup(void);

// -----------------------------
// Configuration Retrieval Functions
// -----------------------------

/**
 * @brief Retrieve the 'audio' configuration object.
 *
 * @return cJSON* Pointer to the 'audio' cJSON object, or NULL if not found.
 */
cJSON *get_audio_config(void);

/**
 * @brief Retrieve the 'network' configuration object.
 *
 * @return cJSON* Pointer to the 'network' cJSON object, or NULL if not found.
 */
cJSON *get_network_config(void);

/**
 * @brief Check if AI (Audio Input) is enabled in the configuration.
 *
 * @return int 1 if enabled, 0 otherwise.
 */
int config_get_ai_enabled(void);

/**
 * @brief Check if AO (Audio Output) is enabled in the configuration.
 *
 * @return int 1 if enabled, 0 otherwise.
 */
int config_get_ao_enabled(void);

// -----------------------------
// Socket Path Retrieval Functions
// -----------------------------

/**
 * @brief Retrieve the AI (Audio Input) socket path from the configuration.
 *
 * @return char* The AI socket path. Caller is responsible for freeing the returned string using free().
 */
char* config_get_ai_socket(void);

/**
 * @brief Retrieve the AO (Audio Output) socket path from the configuration.
 *
 * @return char* The AO socket path. Caller is responsible for freeing the returned string using free().
 */
char* config_get_ao_socket(void);

/**
 * @brief Retrieve the control socket path from the configuration.
 *
 * @return char* The control socket path. Caller is responsible for freeing the returned string using free().
 */
char* config_get_ctrl_socket(void);

// -----------------------------
// Audio Attribute Retrieval Functions
// -----------------------------

/**
 * @brief Enum to differentiate between audio input and output.
 */
typedef enum {
    AUDIO_INPUT,
    AUDIO_OUTPUT
} AudioType;

/**
 * @brief Retrieve a specific attribute from the audio configuration.
 *
 * @param type The type of audio (input or output).
 * @param attribute_name The name of the attribute to retrieve.
 * @return cJSON* Pointer to the attribute's cJSON object, or NULL if not found.
 */
cJSON* get_audio_attribute(AudioType type, const char* attribute_name);

/**
 * @brief Retrieve the AO (Audio Output) frame size from the configuration.
 *
 * @return int The AO frame size, or DEFAULT_AO_MAX_FRAME_SIZE if not found in the configuration.
 */
int config_get_ao_frame_size(void);

/**
 * Checks if the provided samplerate is valid.
 * @param samplerate The samplerate value to be checked.
 * @return 1 if the samplerate is valid, 0 otherwise.
 */
int is_valid_samplerate(int samplerate);

/**
 * @brief Validates the loaded configuration JSON for correct structure and keys.
 *
 * @param root The root cJSON object of the loaded configuration.
 * @return int 1 if the configuration is valid, 0 otherwise.
 */
int validate_json(cJSON *root);

#endif // CONFIG_H
