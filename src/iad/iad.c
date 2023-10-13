/*
 * INGENIC AUDIO DAEMON
 *
 * This daemon manages audio input and output for the Ingenic Tomahawk class devices.
 */

#include <bits/signal.h>             // Signal definitions
#include <signal.h>                  // Signal handling functions
#include <stdio.h>                   // Standard I/O functions
#include <stdlib.h>
#include <pthread.h>                 // Multithreading functions
#include "iad.h"
#include "network/input_server.h"    // Audio input server functions
#include "network/output_server.h"   // Audio output server functions
#include "network/control_server.h"  // Audio control server functions
#include "audio/output.h"            // Audio output functions
#include "utils/cmdline.h"           // Command-line argument parsing
#include "utils/config.h"            // Configuration file handling
#include "utils/utils.h"             // Utility functions
#include "utils/logging.h"           // Logging functions
#include "version.h"                 // Version information

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

    // Parse and store command-line arguments
    CmdOptions options;
    if (parse_cmdline(argc, argv, &options)) {
        return 1; // Exit on command line parsing error
    }

    if (options.daemonize) {
        daemonize();
    }

    // Ensure only one instance of the daemon is running
    if (is_already_running()) {
        exit(1);
    }

    // Set up signal handling for graceful termination
    setup_signal_handling();

    // Ignore the SIGPIPE signal to prevent unexpected program termination
    signal(SIGPIPE, SIG_IGN);

    printf("[INFO] Starting audio daemon\n");

    char *config_file_path = options.config_file_path;
    int disable_ai = options.disable_ai;
    int disable_ao = options.disable_ao;

    // Load and validate the audio configuration from the specified file
    if (config_load_from_file(config_file_path) == 0) {
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

    // Determine whether to enable audio input/output based on configuration
    if (!disable_ai) {
        disable_ai = !config_get_ai_enabled();
    }

    if (!disable_ao) {
        disable_ao = !config_get_ao_enabled();
    }

    pthread_t control_server_thread, input_server_thread, output_server_thread, play_thread_id;

    // Launch the control server thread
    if (create_thread(&control_server_thread, audio_control_server_thread, NULL)) {
        return 1;
    }

    // Launch the audio input server thread (if audio input is enabled)
    if (!disable_ai) {
        if (create_thread(&input_server_thread, audio_input_server_thread, NULL)) {
            return 1;
        }
    }

    // Launch the audio output server thread (if audio output is enabled)
    if (!disable_ao) {
        if (create_thread(&output_server_thread, audio_output_server_thread, NULL)) {
            return 1;
        }
    }

    // Launch the audio playback thread (if audio output is enabled)
    if (!disable_ao) {
        if (create_thread(&play_thread_id, ao_play_thread, NULL)) {
            return 1;
        }
    }

    // Wait for all launched threads to complete their execution
    pthread_join(control_server_thread, NULL);

    if (!disable_ai) {
        pthread_join(input_server_thread, NULL);
    }

    if (!disable_ao) {
        pthread_join(output_server_thread, NULL);
        pthread_join(play_thread_id, NULL);
    }

    return 0; // Successful termination
}
