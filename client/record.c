#include <stdio.h>
#include <unistd.h>
#include "record.h"

#define RECORD_BUFFER_SIZE 4096

void record_from_server(int sockfd, char *output_file_path) {
    printf("[INFO] Receiving audio from daemon\n");

    FILE *output_file = output_file_path ? fopen(output_file_path, "wb") : stdout;
    if (!output_file) {
        perror("fopen");
        return;
    }

    unsigned char buffer[RECORD_BUFFER_SIZE];
    ssize_t bytes_received;

    while ((bytes_received = read(sockfd, buffer, RECORD_BUFFER_SIZE)) > 0) {
        size_t bytes_written = fwrite(buffer, 1, bytes_received, output_file);
        if (bytes_written != bytes_received) {
            perror("fwrite");
            fclose(output_file);
            return;
        }
    }

    if (output_file_path) {  // Only close if it's an actual file
        fclose(output_file);
    }
}
