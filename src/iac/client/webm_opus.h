#ifndef WEBM_OPUS_H
#define WEBM_OPUS_H

#include <stdio.h>
#include <stdint.h>
#include <opus/opus.h>
// #include <speex/speex_resampler.h> // Removed SpeexDSP include

// WebM/Opus related constants
#define OPUS_MAX_PACKET_SIZE 1500
#define OPUS_SAMPLE_RATE 16000 // Expect 16kHz Opus output now
#define OPUS_FRAME_SIZE 320   // 20ms at 16kHz
#define OPUS_MAX_FRAME_SIZE 1920 // 120ms * 16kHz
#define OPUS_MAX_CHANNELS 2      // Assume max 2 channels for buffer allocation
// #define TARGET_SAMPLE_RATE 16000 // Removed target rate

// Structure to hold Opus decoder state
typedef struct {
    OpusDecoder *decoder;
    // SpeexResamplerState *resampler; // Removed resampler state
    FILE *webm_file;
    int channels;
    int sample_rate; // Will be set to OPUS_SAMPLE_RATE (16000)
    // int target_sample_rate; // Removed target sample rate
    int track_number;  // Track number for the Opus audio track
    void *user_data;   // Pointer to implementation-specific data (packet buffer)
} OpusContext;

// Function prototypes
int init_opus_decoder(OpusContext *ctx, int channels);
void cleanup_opus_context(OpusContext *ctx);
int open_webm_file(OpusContext *ctx, const char *filename);
int decode_webm_to_pcm(OpusContext *ctx, int sockfd);

#endif // WEBM_OPUS_H
