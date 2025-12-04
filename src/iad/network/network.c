#include <stdlib.h>            // for free, malloc
#include <string.h>            // for NULL, strncpy, memset, strcmp, strncmp
#include <stdio.h>             // for printf, snprintf, sscanf
#include "config.h"            // for config_get_ai_socket, config_get_ao_so...
#include "network.h
#include "audio_common.h"      // for get_audio_output_gain/get_audio_output_volume...

#define TAG "NET"

char AUDIO_INPUT_SOCKET_PATH[32] = "ingenic_audio_input";
char AUDIO_OUTPUT_SOCKET_PATH[32] = "ingenic_audio_output";
char AUDIO_CONTROL_SOCKET_PATH[32] = "ingenic_audio_control";

char* get_variable_value(const char* variable_name) {
    char* value = (char*) malloc(16 * sizeof(char));
    if (!value) return NULL;

    int int_val = 0;
    int aoDevID, aoChnID;
    if (strcmp(variable_name, "ao_gain") == 0) {
        snprintf(value, 16, "%d", get_audio_output_gain());
        return value;
    } else if (strcmp(variable_name, "ao_vol") == 0) {
        snprintf(value, 16, "%d", get_audio_output_volume());
        return value;
    }

    free(value);
    return NULL;
}

int set_variable_value(const char* variable_name, const char* value) {
    int int_val = atoi(value);
    int aiDevID, aiChnID, aoDevID, aoChnID;
    int result;

    if (strcmp(variable_name, "ao_gain") == 0) {
        set_audio_output_gain(int_val);
        return 0;
    } else if (strcmp(variable_name, "ao_vol") == 0) {
        set_audio_output_volume(int_val);
        return 0;
    }
    return -1;
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
