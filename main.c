#include "audio/output.h"
#include "audio/input.h"
#include "network/network.h"
#include "utils/utils.h"  // Make sure to include utils
#include <signal.h>

int main(void) {
    // Ignore SIGPIPE to prevent the daemon from exiting when writing to a closed socket
    signal(SIGPIPE, SIG_IGN);
    printf("[INFO] Starting audio daemon\n");

    pthread_t play_thread_id, input_server_thread, output_server_thread;

    if (create_thread(&play_thread_id, ao_test_play_thread, NULL)) {
        return 1;
    }

    // Initialize the audio input device
    int devID = 0;
    if (initialize_audio_input_device(devID) != 0) {
        fprintf(stderr, "[ERROR] Failed to initialize audio input device\n");
        return 1;
    }

    if (create_thread(&input_server_thread, audio_input_server_thread, NULL)) {
        return 1;
    }

    if (create_thread(&output_server_thread, audio_output_server_thread, NULL)) {
        return 1;
    }

    pthread_join(input_server_thread, NULL);
    pthread_join(output_server_thread, NULL);
    pthread_join(play_thread_id, NULL);

    pthread_mutex_destroy(&audio_buffer_lock);
    pthread_cond_destroy(&audio_data_cond);

    return 0;
}
