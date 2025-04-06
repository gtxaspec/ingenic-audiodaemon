#ifndef WEBM_OPUS_PARSER_H
#define WEBM_OPUS_PARSER_H

#include <stdio.h>
#include <stdint.h>
#include <opus/opus.h>

// Constants related to Opus decoding within the daemon
#define IAD_OPUS_SAMPLE_RATE 16000 // Daemon operates at 16kHz
#define IAD_OPUS_MAX_FRAME_SIZE 1920 // 120ms * 16kHz
#define IAD_OPUS_MAX_CHANNELS 2      // Max channels supported

// Structure to hold Opus decoder state within the daemon
typedef struct {
    OpusDecoder *decoder;
    FILE *webm_file;
    int channels;
    int sample_rate; // Should match IAD_OPUS_SAMPLE_RATE
    int track_number;  // Track number for the Opus audio track
    void *user_data;   // Pointer to implementation-specific data (e.g., packet buffer)
} IadOpusContext;

// Structure to hold an Opus packet (can be reused or adapted)
typedef struct {
    unsigned char *data;
    int size;
} IadOpusPacket;

// Structure to hold Opus packets (can be reused or adapted)
typedef struct {
    IadOpusPacket *packets;
    int count;
    int capacity;
    int current;
} IadOpusPacketBuffer;


// Function Prototypes for parser/decoder functions to be moved here
// (These will be filled in later)

// Main function called by output thread to handle WebM playback
int iad_play_webm_file(const char *filename, int aoDevID, int aoChnID); 


#endif // WEBM_OPUS_PARSER_H
