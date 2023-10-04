/*
 * INGENIC AUDIO DAEMON
 *
 * This daemon manages audio input and output for the Ingenic Tomahawk class devices.
 */

#include "iad.h"
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "utils/logging.h"

#define TAG "IAD"

/**
 * @brief Clean up resources.
 *
 * This function is responsible for cleaning up any allocated resources
 * and restoring the system to its initial state.
 */
void perform_cleanup() {
    pthread_mutex_destroy(&audio_buffer_lock);
    pthread_cond_destroy(&audio_data_cond);
    config_cleanup();
    cleanup_audio_output();
   //cleanup input
}

/**
 * @brief Signal handler for SIGINT.
 *
 * This function handles the SIGINT signal (typically sent from the
 * command line via CTRL+C). It ensures that the daemon exits gracefully.
 *
 * @param sig Signal number (expected to be SIGINT).
 */
void handle_sigint(int sig) {
    printf("Caught signal %d. Exiting gracefully...\n", sig);
    perform_cleanup();
    exit(0);
}

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
    if (config_load_from_file(config_file_path) != 0) {
        // Continue with the default settings
	handle_audio_error("Failed to load configuration. Continuing with default settings.  File", config_file_path);
    }

    // Fetch audio play attributes
    PlayInputAttributes get_audio_input_play_attributes(void);
    PlayInputAttributes attrs = get_audio_input_play_attributes();

    // Determine device and channel IDs, using defaults if not provided
    int devID = attrs.device_idItem ? attrs.device_idItem->valueint : DEFAULT_AI_DEV_ID;
    int chnID = attrs.channel_idItem ? attrs.channel_idItem->valueint : DEFAULT_AI_CHN_ID;

    // Initialize audio input device if not disabled
    if (!disable_ai) {
        if (initialize_audio_input_device(devID, chnID) != 0) {
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
