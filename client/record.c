#include <stdio.h>
#include <unistd.h>
#include "record.h"

#define RECORD_BUFFER_SIZE 4096  // You can adjust this size as needed

void record_from_server(int sockfd, char *output_file_path) {
    printf("[INFO] Receiving audio from daemon\n");

    FILE *output_file = fopen(output_file_path, "wb");
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

    fclose(output_file);
}
