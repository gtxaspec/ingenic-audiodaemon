#include <string.h>
#include <sys/socket.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdlib.h> // For mkstemp
#include "imp/imp_log.h" // For IMP_LOG_* macros
#include "audio_common.h"
#include "logging.h"
#include "utils.h"
#include "network.h"
#include "output.h"
#include <fcntl.h> // For O_RDWR, O_CREAT
#include <errno.h> // For errno
#include <limits.h> // For PATH_MAX
#include "output_server.h"
#include "webm_opus_parser.h" // Include the parser header

#define TAG "NET_OUTPUT"
#define READ_BUFFER_SIZE 4096 // Define locally for reading socket data

// Global state for WebM playback request (protected by audio_buffer_lock)
extern char g_pending_webm_path[PATH_MAX]; // Declare as extern
extern volatile int g_webm_playback_requested; // Declare as extern

void *audio_output_server_thread(void *arg) {
    IMP_LOG_INFO(TAG, "Entering audio_output_server_thread\n");

    update_socket_paths_from_config();

    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        handle_audio_error(TAG, "socket");
        return NULL;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(&addr.sun_path[1], AUDIO_OUTPUT_SOCKET_PATH, sizeof(addr.sun_path) - 2);
    addr.sun_path[sizeof(addr.sun_path) - 1] = '\0';

    printf("[INFO] [AO] Attempting to bind socket\n");
    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(sa_family_t) + strlen(AUDIO_OUTPUT_SOCKET_PATH) + 1) == -1) {
        handle_audio_error(TAG, "bind failed");
        close(sockfd);
        return NULL;
    }
    else {
        printf("[INFO] [AO] Bind to output socket succeeded\n");
    }

    printf("[INFO] [AO] Attempting to listen on socket\n");
    if (listen(sockfd, 5) == -1) {
        handle_audio_error(TAG, "listen");
        close(sockfd);
        return NULL;
    }
    else {
        printf("[INFO] [AO] Listening on output socket\n");
    }

    while (1) {
        int should_stop = 0;
        pthread_mutex_lock(&g_stop_thread_mutex);
        should_stop = g_stop_thread;
        pthread_mutex_unlock(&g_stop_thread_mutex);

        if (should_stop) {
            break;
        }

        IMP_LOG_INFO(TAG, "Waiting for output client connection\n");
        int client_sock = accept(sockfd, NULL, NULL);
        if (client_sock == -1) {
            handle_audio_error(TAG, "accept");
            continue;
        }
        IMP_LOG_INFO(TAG, "Client connected (socket %d)\n", client_sock);

        // --- Check stream type ---
        unsigned char header_buf[4];
        // Use blocking peek to wait for the first 4 bytes
        ssize_t peek_size = recv(client_sock, header_buf, 4, MSG_PEEK); 
        int is_webm = 0;

        if (peek_size == 4 && memcmp(header_buf, "\x1A\x45\xDF\xA3", 4) == 0) {
            is_webm = 1;
            IMP_LOG_INFO(TAG, "Detected WebM stream from client %d\n", client_sock);
        } else if (peek_size <= 0) { // Handle error or closed connection
             IMP_LOG_ERR(TAG, "Error or connection closed while peeking header from client %d (ret=%zd, errno=%d)\n", client_sock, peek_size, errno);
             close(client_sock);
             continue; // Skip this client
        } else {
             IMP_LOG_INFO(TAG, "Assuming PCM stream from client %d (header: %02X %02X %02X %02X)\n", 
                          client_sock, header_buf[0], header_buf[1], header_buf[2], header_buf[3]);
             is_webm = 0;
        }
        // --- End Check stream type ---


        if (is_webm) {
            // --- Handle WebM Stream ---
            char temp_template[] = "/tmp/iad_webm_XXXXXX";
            int temp_fd = mkstemp(temp_template);
            if (temp_fd < 0) {
                handle_audio_error(TAG, "mkstemp failed");
                close(client_sock);
                continue;
            }
            IMP_LOG_INFO(TAG, "Created temporary file for WebM stream: %s\n", temp_template);

            unsigned char read_buf[READ_BUFFER_SIZE]; // Use a reasonable buffer size
            ssize_t bytes_read;
            ssize_t total_bytes_written = 0;
            while ((bytes_read = read(client_sock, read_buf, sizeof(read_buf))) > 0) {
                ssize_t bytes_written = write(temp_fd, read_buf, bytes_read);
                if (bytes_written != bytes_read) {
                    handle_audio_error(TAG, "write to temp file failed");
                    close(temp_fd);
                    unlink(temp_template); // Clean up temp file
                    close(client_sock);
                    goto next_client; // Use goto for cleaner exit from nested loops
                }
                total_bytes_written += bytes_written;
            }
            close(temp_fd); // Close file descriptor

            if (bytes_read < 0) {
                 handle_audio_error(TAG, "read from client socket failed");
                 unlink(temp_template); // Clean up temp file
            } else {
                 IMP_LOG_INFO(TAG, "Received %zd bytes into %s\n", total_bytes_written, temp_template);
                 // Signal the audio thread to play this WebM file
                 pthread_mutex_lock(&audio_buffer_lock);
                 strncpy(g_pending_webm_path, temp_template, PATH_MAX - 1);
                 g_pending_webm_path[PATH_MAX - 1] = '\0'; // Ensure null termination
                 g_webm_playback_requested = 1;
                 pthread_cond_signal(&audio_data_cond); // Wake up audio thread
                 pthread_mutex_unlock(&audio_buffer_lock);
                 // Note: The audio thread will be responsible for deleting the temp file after playback
            }
            // --- End Handle WebM Stream ---

        } else {
            // --- Handle PCM Stream (Existing Logic) ---
            pthread_mutex_lock(&audio_buffer_lock);
            // Wait if another client is active (shouldn't happen with current single-client logic, but good practice)
            while (active_client_sock != -1) {
                pthread_cond_wait(&audio_data_cond, &audio_buffer_lock);
            }
            active_client_sock = client_sock;

            // Clear any stale WebM request
            g_webm_playback_requested = 0; 
            g_pending_webm_path[0] = '\0';

            // Enabling the channel might clear buffers - consider if needed here
            enable_output_channel(); 
            IMP_LOG_INFO(TAG, "Handling PCM stream from client %d\n", client_sock);

            memset(audio_buffer, 0, g_ao_max_frame_size);
            audio_buffer_size = 0;
            pthread_mutex_unlock(&audio_buffer_lock);

            unsigned char pcm_buf[g_ao_max_frame_size]; // Use existing buffer size logic
            ssize_t read_size;

            while ((read_size = read(client_sock, pcm_buf, sizeof(pcm_buf))) > 0) {
                pthread_mutex_lock(&audio_buffer_lock);
                // Ensure we don't overflow the shared buffer
                ssize_t copy_size = (read_size <= g_ao_max_frame_size) ? read_size : g_ao_max_frame_size;
                memcpy(audio_buffer, pcm_buf, copy_size);
                audio_buffer_size = copy_size;
                pthread_cond_signal(&audio_data_cond); // Signal audio thread
                pthread_mutex_unlock(&audio_buffer_lock);
                // Simple flow control: wait briefly if buffer was full? Or rely on socket blocking?
                // For now, assume socket blocking or fast consumption is sufficient.
            }

            // Clear shared buffer after client disconnects or error
            pthread_mutex_lock(&audio_buffer_lock);
            memset(audio_buffer, 0, g_ao_max_frame_size);
            audio_buffer_size = 0;
            active_client_sock = -1; // Mark socket as inactive
            pthread_cond_broadcast(&audio_data_cond); // Wake potentially waiting threads
            pthread_mutex_unlock(&audio_buffer_lock);

            if (read_size < 0) {
                 handle_audio_error(TAG, "read from client socket failed (PCM)");
            }
            // --- End Handle PCM Stream ---
        }

next_client: // Label for goto
        close(client_sock);
        IMP_LOG_INFO(TAG, "Client disconnected (socket %d)\n", client_sock);
    }

    close(sockfd);
    IMP_LOG_INFO(TAG, "Exiting audio_output_server_thread\n");
    return NULL;
}
