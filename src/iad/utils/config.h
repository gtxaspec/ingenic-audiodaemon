#ifndef CONFIG_H
#define CONFIG_H
#include <cJSON.h>

// Initialize the configuration system
int config_load_from_file(const char *config_file_path);

// Cleanup the configuration system
void config_cleanup(void);

// Getters for configuration values
cJSON *get_audio_config(void);
cJSON *get_network_config(void);

int config_get_ai_enabled(void);
int config_get_ao_enabled(void);

// Retrieve socket paths; caller is responsible for freeing the returned string using free()
char* config_get_ai_socket(void);
char* config_get_ao_socket(void);
char* config_get_ctrl_socket(void);

typedef enum {
    AUDIO_INPUT,
    AUDIO_OUTPUT
} AudioType;

cJSON* get_audio_attribute(AudioType type, const char* attribute_name);

#endif // CONFIG_H
