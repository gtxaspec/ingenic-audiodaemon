#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <ctype.h>
#include "webm_opus.h"
#include "playback.h"

// EBML and WebM element IDs (big-endian)
#define EBML_ID_HEADER         0x1A45DFA3
#define WEBM_ID_SEGMENT        0x18538067
#define WEBM_ID_TRACKS         0x1654AE6B
#define WEBM_ID_TRACKENTRY     0xAE
#define WEBM_ID_TRACKNUMBER    0xD7
#define WEBM_ID_TRACKTYPE      0x83
#define WEBM_ID_CODECID        0x86
#define WEBM_ID_AUDIO          0xE1
#define WEBM_ID_CHANNELS       0x9F
#define WEBM_ID_CLUSTER        0x1F43B675
#define WEBM_ID_TIMECODE       0xE7
#define WEBM_ID_SIMPLEBLOCK    0xA3

#define TRACK_TYPE_AUDIO       0x02

// Debug flag - set to 1 to enable verbose debugging
#define DEBUG_WEBM 1

// Helper function to dump file header for debugging
static void dump_file_header(FILE *f) {
    if (!DEBUG_WEBM) return;
    
    // Save current position
    long current_pos = ftell(f);
    
    // Go to beginning of file
    fseek(f, 0, SEEK_SET);
    
    // Read and print first 32 bytes
    printf("File header (first 32 bytes):\n");
    unsigned char header[32];
    size_t bytes_read = fread(header, 1, sizeof(header), f);
    
    printf("Hex: ");
    for (size_t i = 0; i < bytes_read; i++) {
        printf("%02X ", header[i]);
    }
    printf("\n");
    
    printf("ASCII: ");
    for (size_t i = 0; i < bytes_read; i++) {
        printf("%c", isprint(header[i]) ? header[i] : '.');
    }
    printf("\n");
    
    // Restore position
    fseek(f, current_pos, SEEK_SET);
}

// Initialize Opus decoder
int init_opus_decoder(OpusContext *ctx, int channels) {
    int error;
    ctx->channels = channels;
    ctx->sample_rate = OPUS_SAMPLE_RATE;

    ctx->decoder = opus_decoder_create(ctx->sample_rate, ctx->channels, &error);
    if (error != OPUS_OK) {
        fprintf(stderr, "Failed to create Opus decoder: %s\n", opus_strerror(error));
        return -1;
    }

    if (DEBUG_WEBM) {
        printf("Initialized Opus decoder: channels=%d, sample_rate=%d\n",
               ctx->channels, ctx->sample_rate);
    }

    return 0;
}

// Clean up Opus context
void cleanup_opus_context(OpusContext *ctx) {
    if (ctx->decoder) {
        opus_decoder_destroy(ctx->decoder);
        ctx->decoder = NULL;
    }

    if (ctx->webm_file) {
        fclose(ctx->webm_file);
        ctx->webm_file = NULL;
    }
}

// Super simple approach to find Opus data in a WebM file
int open_webm_file(OpusContext *ctx, const char *filename) {
    ctx->webm_file = fopen(filename, "rb");
    if (!ctx->webm_file) {
        perror("Failed to open WebM file");
        return -1;
    }
    
    printf("Parsing WebM file: %s\n", filename);
    
    // Dump file header for debugging
    dump_file_header(ctx->webm_file);
    
    // Check for EBML header directly
    unsigned char header[4];
    if (fread(header, 1, 4, ctx->webm_file) != 4 ||
        header[0] != 0x1A || header[1] != 0x45 || 
        header[2] != 0xDF || header[3] != 0xA3) {
        fprintf(stderr, "Not a valid WebM file (EBML header not found)\n");
        fclose(ctx->webm_file);
        ctx->webm_file = NULL;
        return -1;
    }
    
    printf("Found EBML header\n");
    
    // Scan for "OpusHead" marker
    fseek(ctx->webm_file, 0, SEEK_SET);
    unsigned char buffer[4096];
    size_t bytes_read;
    int found_opus = 0;
    
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), ctx->webm_file)) > 0) {
        for (size_t i = 0; i < bytes_read - 8; i++) {
            if (memcmp(buffer + i, "OpusHead", 8) == 0) {
                printf("Found OpusHead marker at offset %ld\n", 
                       ftell(ctx->webm_file) - bytes_read + i);
                found_opus = 1;
                break;
            }
        }
        
        if (found_opus) break;
    }
    
    if (!found_opus) {
        // Try looking for codec ID
        fseek(ctx->webm_file, 0, SEEK_SET);
        char codec_id[32];
        int found_codec = 0;
        
        while ((bytes_read = fread(buffer, 1, sizeof(buffer), ctx->webm_file)) > 0) {
            for (size_t i = 0; i < bytes_read - 6; i++) {
                // Look for "A_OPUS" or similar
                if ((buffer[i] == 'A' || buffer[i] == 'V') && 
                    buffer[i+1] == '_' && 
                    buffer[i+2] == 'O' && 
                    buffer[i+3] == 'P' && 
                    buffer[i+4] == 'U' && 
                    buffer[i+5] == 'S') {
                    
                    memcpy(codec_id, buffer + i, 6);
                    codec_id[6] = '\0';
                    printf("Found Opus codec ID: %s\n", codec_id);
                    found_codec = 1;
                    break;
                }
            }
            
            if (found_codec) break;
        }
        
        if (!found_codec) {
            fprintf(stderr, "No Opus audio track found in WebM file\n");
            fclose(ctx->webm_file);
            ctx->webm_file = NULL;
            return -1;
        }
    }
    
    // Initialize with default parameters
    if (init_opus_decoder(ctx, 1) != 0) {
        fprintf(stderr, "Failed to initialize Opus decoder\n");
        fclose(ctx->webm_file);
        ctx->webm_file = NULL;
        return -1;
    }
    
    // Set a dummy track number
    ctx->track_number = 1;
    
    // Seek to beginning of file for decoding
    fseek(ctx->webm_file, 0, SEEK_SET);
    printf("Successfully initialized WebM/Opus file\n");
    return 0;
}

// Decode WebM/Opus file and send PCM to socket
int decode_webm_to_pcm(OpusContext *ctx, int sockfd) {
    if (!ctx->webm_file || !ctx->decoder) {
        fprintf(stderr, "Invalid Opus context\n");
        return -1;
    }

    // Extract raw Opus data to a temporary file
    char temp_filename[64];
    snprintf(temp_filename, sizeof(temp_filename), "/tmp/opus_data_%d.opus", getpid());
    FILE *temp_file = fopen(temp_filename, "wb");
    if (!temp_file) {
        perror("Failed to create temporary file");
        return -1;
    }

    // Seek to beginning of file
    fseek(ctx->webm_file, 0, SEEK_SET);

    printf("Extracting Opus data to temporary file...\n");

    // First, try to find the Opus data section
    unsigned char buffer[8192];
    size_t bytes_read;
    int found_opus_data = 0;
    long opus_data_offset = -1;

    // Look for OpusHead or OpusTags markers
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), ctx->webm_file)) > 0) {
        for (size_t i = 0; i < bytes_read - 8; i++) {
            if (memcmp(buffer + i, "OpusHead", 8) == 0 ||
                memcmp(buffer + i, "OpusTags", 8) == 0) {
                opus_data_offset = ftell(ctx->webm_file) - bytes_read + i;
                found_opus_data = 1;
                printf("Found Opus data marker at offset %ld\n", opus_data_offset);
                break;
            }
        }
        if (found_opus_data) break;
    }

    // If we didn't find Opus markers, try looking for codec ID
    if (!found_opus_data) {
        fseek(ctx->webm_file, 0, SEEK_SET);
        while ((bytes_read = fread(buffer, 1, sizeof(buffer), ctx->webm_file)) > 0) {
            for (size_t i = 0; i < bytes_read - 6; i++) {
                if ((buffer[i] == 'A' || buffer[i] == 'V') &&
                    buffer[i+1] == '_' &&
                    buffer[i+2] == 'O' &&
                    buffer[i+3] == 'P' &&
                    buffer[i+4] == 'U' &&
                    buffer[i+5] == 'S') {

                    // Found codec ID, now look for actual data
                    opus_data_offset = ftell(ctx->webm_file) - bytes_read + i;
                    found_opus_data = 1;
                    printf("Found Opus codec ID at offset %ld\n", opus_data_offset);
                    break;
                }
            }
            if (found_opus_data) break;
        }
    }

    // If we still haven't found Opus data, try a different approach
    if (!found_opus_data) {
        printf("No Opus markers found, trying to extract data directly...\n");
        // Just copy the whole file and let the decoder figure it out
        fseek(ctx->webm_file, 0, SEEK_SET);
        while ((bytes_read = fread(buffer, 1, sizeof(buffer), ctx->webm_file)) > 0) {
            fwrite(buffer, 1, bytes_read, temp_file);
        }
    } else {
        // We found Opus data, copy from that point
        fseek(ctx->webm_file, opus_data_offset, SEEK_SET);
        while ((bytes_read = fread(buffer, 1, sizeof(buffer), ctx->webm_file)) > 0) {
            fwrite(buffer, 1, bytes_read, temp_file);
        }
    }

    fclose(temp_file);
    printf("Extracted data to %s\n", temp_filename);

    // Now open the temporary file and decode it
    FILE *opus_file = fopen(temp_filename, "rb");
    if (!opus_file) {
        perror("Failed to open temporary Opus file");
        unlink(temp_filename);
        return -1;
    }

    // Variables for decoding
    int16_t pcm_buffer[PCM_BUFFER_SIZE * 2]; // Extra space for stereo
    uint8_t opus_packet[2048];
    int samples;
    int total_samples = 0;
    int packet_count = 0;

    // Try to find and decode Opus packets
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), opus_file)) > 0) {
        for (size_t i = 0; i < bytes_read - 4; i++) {
            // Look for potential Opus packet
            // Opus packets typically start with a TOC byte followed by frame data
            // The TOC byte contains configuration info
            uint8_t toc = buffer[i];

            // Check if this looks like a valid TOC byte
            // TOC byte format: [config (2 bits)][stereo (1 bit)][frame count (5 bits)]
            uint8_t config = (toc >> 3) & 0x1F;
            uint8_t stereo = (toc >> 2) & 0x01;
            uint8_t frame_count = toc & 0x03;

            // Simple heuristic: if the next byte is a reasonable packet size
            uint8_t packet_size = buffer[i+1];

            if (packet_size > 0 && packet_size < 250 && i + 2 + packet_size <= bytes_read) {
                // This might be an Opus packet, try to decode it
                memcpy(opus_packet, buffer + i, packet_size + 2);

                samples = opus_decode(ctx->decoder, opus_packet, packet_size + 2,
                                     pcm_buffer, OPUS_FRAME_SIZE, 0);

                if (samples > 0) {
                    packet_count++;
                    total_samples += samples;

                    if (DEBUG_WEBM && packet_count % 10 == 0) {
                        printf("Decoded %d samples from packet %d (size %d)\n",
                               samples, packet_count, packet_size + 2);
                    }

                    // Send PCM data to socket
                    int bytes_to_send = samples * ctx->channels * sizeof(int16_t);
                    if (send(sockfd, pcm_buffer, bytes_to_send, 0) != bytes_to_send) {
                        perror("Failed to send PCM data");
                        fclose(opus_file);
                        unlink(temp_filename);
                        return -1;
                    }

                    // Skip to the end of this packet
                    i += packet_size + 1;
                }
            }
        }
    }

    fclose(opus_file);
    unlink(temp_filename);

    printf("Decoded %d samples from %d packets\n", total_samples, packet_count);

    if (packet_count == 0) {
        fprintf(stderr, "No valid Opus packets found\n");
        return -1;
    }

    return 0;
}