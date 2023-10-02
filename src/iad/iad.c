/* INGENIC AUDIO DAEMON */

#include <signal.h>
#include <unistd.h>
#include <string.h>
#include "audio/output.h"
#include "audio/input.h"
#include "network/network.h"
#include "utils/utils.h"
#include "utils/cmdline.h"
#include "version.h"
#include "config.h"

void perform_cleanup() {
    pthread_mutex_destroy(&audio_buffer_lock);
    pthread_cond_destroy(&audio_data_cond);
    config_cleanup();
}

// Signal handler function
void handle_sigint(int sig) {
    printf("Caught signal %d. Exiting gracefully...\n", sig);

    // Call the cleanup function
    perform_cleanup();

    exit(0);
}

int main(int argc, char *argv[]) {
    printf("INGENIC AUDIO DAEMON Version: %s\n", VERSION);

    // Set up the signal handler for SIGINT
    signal(SIGINT, handle_sigint);

    CmdOptions options;
    if (parse_cmdline(argc, argv, &options)) {
        return 1; // Exit if there was an error parsing the command line
    }

    char *config_file_path = options.config_file_path;
    int disable_ai = options.disable_ai;
    int disable_ao = options.disable_ao;

    // Initialize the configuration system
    if (config_load_from_file(config_file_path) != 0) {
        fprintf(stderr, "Failed to load configuration from %s\n", config_file_path);
        // Continue with the default settings
    }

    // Update disable_ai and disable_ao based on config if they weren't set by command line
    if (!disable_ai) {
        disable_ai = !config_get_ai_enabled();
    }

    if (!disable_ao) {
        disable_ao = !config_get_ao_enabled();
    }

    // Ignore SIGPIPE to prevent the daemon from exiting when writing to a closed socket
    signal(SIGPIPE, SIG_IGN);
    printf("[INFO] Starting audio daemon\n");

    pthread_t play_thread_id, input_server_thread, output_server_thread, control_server_thread;

    if (!disable_ao) {
        if (create_thread(&play_thread_id, ao_test_play_thread, NULL)) {
            return 1;
        }
    }

    if (!disable_ai) {
        if (create_thread(&input_server_thread, audio_input_server_thread, NULL)) {
            return 1;
        }
    }

    if (!disable_ao) {
        if (create_thread(&output_server_thread, audio_output_server_thread, NULL)) {
            return 1;
        }
    }

    // Create the control server thread
    if (create_thread(&control_server_thread, audio_control_server_thread, NULL)) {
        return 1;
    }

    if (!disable_ai) {
        pthread_join(input_server_thread, NULL);
    }

    if (!disable_ao) {
        pthread_join(output_server_thread, NULL);
        pthread_join(play_thread_id, NULL);
    }

    pthread_join(control_server_thread, NULL);  // Wait for control server thread to finish

    // Call the cleanup function
    perform_cleanup();

    return 0;
}
