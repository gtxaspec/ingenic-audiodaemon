/* INGENIC AUDIO DAEMON */

#include "audio/output.h"
#include "audio/input.h"
#include "network/network.h"
#include "utils/utils.h"
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include "version.h"

int main(int argc, char *argv[]) {
    printf("INGENIC AUDIO DAEMON Version: %s\n", VERSION);

    // Variables to track if we should disable AI or AO
    int disable_ai = 0;
    int disable_ao = 0;

    // Parse command line arguments
    int opt;
    while ((opt = getopt(argc, argv, "d:")) != -1) {
        switch (opt) {
            case 'd':
                if (strcmp(optarg, "AI") == 0) {
                    disable_ai = 1;
                } else if (strcmp(optarg, "AO") == 0) {
                    disable_ao = 1;
                } else {
                    fprintf(stderr, "Invalid option for -d. Use AI or AO.\n");
                    return 1;
                }
                break;
            default:
                fprintf(stderr, "Usage: %s [-d <AI|AO>]\n", argv[0]);
                return 1;
        }
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
        // Initialize the audio input device
        int devID = 0;
        if (initialize_audio_input_device(devID) != 0) {
            fprintf(stderr, "[ERROR] Failed to initialize audio input device\n");
            return 1;
        }

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

    pthread_mutex_destroy(&audio_buffer_lock);
    pthread_cond_destroy(&audio_data_cond);

    return 0;
}
