/*
 * INGENIC AUDIO DAEMON
 *
 * This daemon manages audio input and output for the Ingenic Tomahawk class devices.
 */

#include <signal.h>         // for signal, SIG_IGN
#include <stdio.h>          // for printf, NULL, fprintf, stderr
#include <pthread.h>        // for pthread_join, pthread_cond_destroy, pthre...
#include "iad.h"
#include "cJSON.h"          // for cJSON
#include "audio/input.h"          // for initialize_audio_input_device, DEFAULT_AI...
#include "audio/audio_common.h"   // for PlayInputAttributes, disable_audio_input
#include "network/network.h"        // for audio_control_server_thread, audio_input_...
#include "audio/output.h"         // for disable_audio_output, ao_test_play_thread
#include "utils/cmdline.h"        // for CmdOptions, parse_cmdline
#include "utils/config.h"         // for config_cleanup, config_get_ai_enabled
#include "utils/utils.h"          // for create_thread, audio_buffer_lock, audio_d...
#include "utils/logging.h"  // for handle_audio_error
#include "version.h"        // for VERSION

#define TAG "IAD"

/**
 * @brief Main function for the Ingenic Audio Daemon.
 *
 * This is the entry point of the daemon. It initializes the audio system,
 * sets up networking, and manages the main execution loop.
 *
 * @param argc Number of command-line arguments.
 * @param argv Array of command-line arguments.
 * @return int Returns 0 on successful execution, non-zero otherwise.
 */
int main(int argc, char *argv[]) {
    printf("INGENIC AUDIO DAEMON Version: %s\n", VERSION);

    // Set up the signal handler for SIGINT to allow graceful exit
    signal(SIGINT, handle_sigint);
    CmdOptions options;
    if (parse_cmdline(argc, argv, &options)) {
        return 1; // Exit on command line parsing error
    }

    char *config_file_path = options.config_file_path;
    int disable_ai = options.disable_ai;
    int disable_ao = options.disable_ao;

    // Load configuration settings from the provided file
    if (config_load_from_file(config_file_path) == 0) {
        // Successfully loaded the configuration, now validate its format
        if (!validate_json(get_audio_config())) {
            handle_audio_error("Invalid configuration format. Continuing with default settings.", config_file_path);
        }
    } else {
        handle_audio_error("Failed to load configuration. Continuing with default settings. File", config_file_path);
    }

    /* Debug only
    cJSON *loaded_config = get_audio_config();
    printf("Loaded JSON: %s\n", cJSON_Print(loaded_config));
    */

    // Fetch audio play attributes
    int aiDevID, aiChnID;
    get_audio_input_device_attributes(&aiDevID, &aiChnID);

    // Initialize audio input device if not disabled
    if (!disable_ai) {
        if (initialize_audio_input_device(aiDevID, aiChnID) != 0) {
            fprintf(stderr, "[ERROR] Failed to initialize audio input device\n");
            return 1;
        }
    }

    // Update audio input/output enable status based on configuration
    if (!disable_ai) {
        disable_ai = !config_get_ai_enabled();
    }
    if (!disable_ao) {
        disable_ao = !config_get_ao_enabled();
    }

    // Ignore SIGPIPE to prevent unexpected exits when writing to a closed socket
    signal(SIGPIPE, SIG_IGN);
    printf("[INFO] Starting audio daemon\n");

    pthread_t play_thread_id, input_server_thread, output_server_thread, control_server_thread;

    // Start audio play thread if output is not disabled
    if (!disable_ao) {
        if (create_thread(&play_thread_id, ao_test_play_thread, NULL)) {
            return 1;
        }
    }

    // Start audio input server thread if input is not disabled
    if (!disable_ai) {
        if (create_thread(&input_server_thread, audio_input_server_thread, NULL)) {
            return 1;
        }
    }

    // Start audio output server thread if output is not disabled
    if (!disable_ao) {
        if (create_thread(&output_server_thread, audio_output_server_thread, NULL)) {
            return 1;
        }
    }

    // Start the control server thread
    if (create_thread(&control_server_thread, audio_control_server_thread, NULL)) {
        return 1;
    }

    // Wait for threads to complete
    if (!disable_ai) {
        pthread_join(input_server_thread, NULL);
    }
    if (!disable_ao) {
        pthread_join(output_server_thread, NULL);
        pthread_join(play_thread_id, NULL);
    }
    pthread_join(control_server_thread, NULL);

    // Clean up resources before exiting
    perform_cleanup();
    return 0;
}
