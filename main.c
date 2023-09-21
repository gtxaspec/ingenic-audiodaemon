#include "audio/output.h"
#include "network/network.h"
#include "utils/utils.h"

int main(void) {
    printf("[INFO] Starting audio daemon\n");

    pthread_t play_thread_id;
    int ret_play = pthread_create(&play_thread_id, NULL, ao_test_play_thread, NULL);
    if (ret_play) {
        fprintf(stderr, "[ERROR] pthread_create for play thread failed with error code: %d\n", ret_play);
        return 1;
    }

    pthread_t server_thread;
    int ret_server = pthread_create(&server_thread, NULL, audio_server_thread, NULL);
    if (ret_server) {
        fprintf(stderr, "[ERROR] pthread_create for server thread failed with error code: %d\n", ret_server);
        return 1;
    }

    pthread_join(server_thread, NULL);
    pthread_join(play_thread_id, NULL);

    pthread_mutex_destroy(&audio_buffer_lock);
    pthread_cond_destroy(&audio_data_cond);

    return 0;
}
