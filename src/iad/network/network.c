#include <stdlib.h>            // for free, malloc
#include <string.h>            // for NULL, strncpy, memset, strcmp, strncmp
#include <stdio.h>             // for printf, snprintf, sscanf
#include "config.h"   // for config_get_ai_socket, config_get_ao_so...
#include "network.h"

#define TAG "NET"

char AUDIO_INPUT_SOCKET_PATH[32] = "ingenic_audio_input";
char AUDIO_OUTPUT_SOCKET_PATH[32] = "ingenic_audio_output";
char AUDIO_CONTROL_SOCKET_PATH[32] = "ingenic_audio_control";

// Sample variables for testing
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
