#ifndef WEBM_OPUS_H
#define WEBM_OPUS_H

#include <stdio.h>
#include <stdint.h>
#include <opus/opus.h>

// WebM/Opus related constants
#define OPUS_MAX_PACKET_SIZE 1500
#define OPUS_SAMPLE_RATE 48000
#define OPUS_FRAME_SIZE 960  // 20ms at 48kHz
#define PCM_BUFFER_SIZE (OPUS_FRAME_SIZE * 2)  // 16-bit samples (2 bytes per sample)

// Structure to hold Opus decoder state
typedef struct {
    OpusDecoder *decoder;
    FILE *webm_file;
    int channels;
    int sample_rate;
    int track_number;  // Track number for the Opus audio track
    void *user_data;   // Pointer to implementation-specific data
} OpusContext;

// Function prototypes
int init_opus_decoder(OpusContext *ctx, int channels);
void cleanup_opus_context(OpusContext *ctx);
int open_webm_file(OpusContext *ctx, const char *filename);
int decode_webm_to_pcm(OpusContext *ctx, int sockfd);

#endif // WEBM_OPUS_H