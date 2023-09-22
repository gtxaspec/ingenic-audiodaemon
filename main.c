#include "audio/output.h"
#include "audio/input.h"   // Include the header for input.h
#include "network/network.h"
#include "utils/utils.h"
#include <signal.h>        // Include for signal handling

int main(void) {
    // Ignore SIGPIPE to prevent the daemon from exiting when writing to a closed socket
    signal(SIGPIPE, SIG_IGN);

    printf("[INFO] Starting audio daemon\n");

    pthread_t play_thread_id;
    int ret_play = pthread_create(&play_thread_id, NULL, ao_test_play_thread, NULL);
    if (ret_play) {
        fprintf(stderr, "[ERROR] pthread_create for play thread failed with error code: %d\n", ret_play);
        return 1;
    }

    // Initialize the audio input device
    int devID = 0;  // Assuming device ID is 0 for now
    int init_result = initialize_audio_input_device(devID);
    if (init_result != 0) {
        fprintf(stderr, "[ERROR] Failed to initialize audio input device with error code: %d\n", init_result);
        return 1;
    }

    pthread_t input_server_thread, output_server_thread;

    int ret_input_server = pthread_create(&input_server_thread, NULL, audio_input_server_thread, NULL);
    if (ret_input_server) {
        fprintf(stderr, "[ERROR] pthread_create for audio input server thread failed with error code: %d\n", ret_input_server);
        return 1;
    }

    int ret_output_server = pthread_create(&output_server_thread, NULL, audio_output_server_thread, NULL);
    if (ret_output_server) {
        fprintf(stderr, "[ERROR] pthread_create for audio output server thread failed with error code: %d\n", ret_output_server);
        return 1;
    }

    pthread_join(input_server_thread, NULL);
    pthread_join(output_server_thread, NULL);
    pthread_join(play_thread_id, NULL);

    pthread_mutex_destroy(&audio_buffer_lock);
    pthread_cond_destroy(&audio_data_cond);

    return 0;
}
