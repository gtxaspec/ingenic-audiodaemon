#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include "webm_opus.h"
#include "playback.h"

// Simple WebM parser - this is a basic implementation that assumes a valid WebM file
// A production implementation would need more robust parsing and error handling

// EBML header IDs
#define EBML_ID_HEADER 0x1A45DFA3
#define WEBM_ID_SEGMENT 0x18538067
#define WEBM_ID_CLUSTER 0x1F43B675
#define WEBM_ID_SIMPLEBLOCK 0xA3
#define WEBM_ID_BLOCK 0xA1
#define WEBM_ID_TRACKS 0x1654AE6B
#define WEBM_ID_TRACKENTRY 0xAE
#define WEBM_ID_CODECID 0x86
#define WEBM_ID_AUDIO 0xE1

// Helper function to read a variable-length integer (EBML format)
static uint64_t read_vint(FILE *f, int *size) {
    uint8_t first_byte;
    if (fread(&first_byte, 1, 1, f) != 1) {
        *size = 0;
        return 0;
    }
    
    int length = 0;
    uint8_t mask = 0x80;
    
    while (!(first_byte & mask) && mask) {
        length++;
        mask >>= 1;
    }
    
    if (length > 7) {
        *size = 0;
        return 0;
    }
    
    uint64_t value = first_byte & (0xFF >> (length + 1));
    length++;
    
    for (int i = 1; i < length; i++) {
        uint8_t next_byte;
        if (fread(&next_byte, 1, 1, f) != 1) {
            *size = 0;
            return 0;
        }
        value = (value << 8) | next_byte;
    }
    
    *size = length;
    return value;
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

// Open and parse WebM file
int open_webm_file(OpusContext *ctx, const char *filename) {
    ctx->webm_file = fopen(filename, "rb");
    if (!ctx->webm_file) {
        perror("Failed to open WebM file");
        return -1;
    }
    
    // Very basic WebM parsing - just to find Opus audio data
    // A real implementation would need more robust parsing
    
    // Skip EBML header and find the first cluster
    int size;
    uint64_t id, length;
    int found_opus = 0;
    
    while (!feof(ctx->webm_file)) {
        id = read_vint(ctx->webm_file, &size);
        if (size == 0) break;
        
        length = read_vint(ctx->webm_file, &size);
        if (size == 0) break;
        
        if (id == WEBM_ID_TRACKS) {
            long tracks_end = ftell(ctx->webm_file) + length;
            
            while (ftell(ctx->webm_file) < tracks_end) {
                uint64_t track_id = read_vint(ctx->webm_file, &size);
                if (size == 0) break;
                
                uint64_t track_size = read_vint(ctx->webm_file, &size);
                if (size == 0) break;
                
                if (track_id == WEBM_ID_TRACKENTRY) {
                    long track_end = ftell(ctx->webm_file) + track_size;
                    
                    while (ftell(ctx->webm_file) < track_end) {
                        uint64_t elem_id = read_vint(ctx->webm_file, &size);
                        if (size == 0) break;
                        
                        uint64_t elem_size = read_vint(ctx->webm_file, &size);
                        if (size == 0) break;
                        
                        if (elem_id == WEBM_ID_CODECID) {
                            char codec_id[20] = {0};
                            if (elem_size < sizeof(codec_id)) {
                                fread(codec_id, 1, elem_size, ctx->webm_file);
                                if (strstr(codec_id, "OPUS")) {
                                    found_opus = 1;
                                    break;
                                }
                            } else {
                                fseek(ctx->webm_file, elem_size, SEEK_CUR);
                            }
                        } else {
                            fseek(ctx->webm_file, elem_size, SEEK_CUR);
                        }
                    }
                    
                    if (found_opus) break;
                    fseek(ctx->webm_file, track_end, SEEK_SET);
                } else {
                    fseek(ctx->webm_file, track_size, SEEK_CUR);
                }
            }
            
            if (found_opus) {
                // Found Opus codec, initialize decoder
                if (init_opus_decoder(ctx, 1) != 0) {
                    fclose(ctx->webm_file);
                    ctx->webm_file = NULL;
                    return -1;
                }
                
                // Seek to beginning of file to start decoding
                fseek(ctx->webm_file, 0, SEEK_SET);
                return 0;
            }
            
            fseek(ctx->webm_file, tracks_end, SEEK_SET);
        } else {
            fseek(ctx->webm_file, length, SEEK_CUR);
        }
    }
    
    fprintf(stderr, "No Opus audio track found in WebM file\n");
    fclose(ctx->webm_file);
    ctx->webm_file = NULL;
    return -1;
}

// Decode WebM/Opus file and send PCM to socket
int decode_webm_to_pcm(OpusContext *ctx, int sockfd) {
    if (!ctx->decoder || !ctx->webm_file) {
        fprintf(stderr, "Opus decoder not initialized\n");
        return -1;
    }
    
    printf("[INFO] Decoding WebM/Opus audio and sending to daemon\n");
    
    // Buffer for Opus packets
    unsigned char opus_packet[OPUS_MAX_PACKET_SIZE];
    // Buffer for decoded PCM data
    int16_t pcm_buffer[PCM_BUFFER_SIZE];
    
    // Seek to beginning of file
    fseek(ctx->webm_file, 0, SEEK_SET);
    
    // Simple WebM parsing to find and decode Opus packets
    int size;
    uint64_t id, length;
    
    while (!feof(ctx->webm_file)) {
        id = read_vint(ctx->webm_file, &size);
        if (size == 0) break;
        
        length = read_vint(ctx->webm_file, &size);
        if (size == 0) break;
        
        if (id == WEBM_ID_CLUSTER) {
            long cluster_end = ftell(ctx->webm_file) + length;
            
            while (ftell(ctx->webm_file) < cluster_end) {
                uint64_t block_id = read_vint(ctx->webm_file, &size);
                if (size == 0) break;
                
                uint64_t block_size = read_vint(ctx->webm_file, &size);
                if (size == 0) break;
                
                if (block_id == WEBM_ID_SIMPLEBLOCK || block_id == WEBM_ID_BLOCK) {
                    // Parse SimpleBlock/Block
                    uint8_t track_num;
                    if (fread(&track_num, 1, 1, ctx->webm_file) != 1) break;
                    
                    // Skip timestamp (2 bytes) and flags (1 byte)
                    fseek(ctx->webm_file, 3, SEEK_CUR);
                    
                    // Read Opus packet size (remaining block size minus 4 bytes for header)
                    int opus_size = block_size - 4;
                    if (opus_size > OPUS_MAX_PACKET_SIZE) {
                        fseek(ctx->webm_file, opus_size, SEEK_CUR);
                        continue;
                    }
                    
                    // Read Opus packet
                    if (fread(opus_packet, 1, opus_size, ctx->webm_file) != opus_size) break;
                    
                    // Decode Opus packet to PCM
                    int samples = opus_decode(ctx->decoder, opus_packet, opus_size, 
                                             pcm_buffer, OPUS_FRAME_SIZE, 0);
                    
                    if (samples > 0) {
                        // Send decoded PCM to socket
                        long long start_time = current_time_in_milliseconds();
                        write(sockfd, pcm_buffer, samples * ctx->channels * sizeof(int16_t));
                        long long end_time = current_time_in_milliseconds();
                        
                        // Timing control for playback
                        long long playback_time = end_time - start_time;
                        long long sleep_duration = 20 - playback_time; // 20ms per frame
                        
                        if (sleep_duration > 0) {
                            usleep(sleep_duration * 1000);
                        }
                    }
                } else {
                    fseek(ctx->webm_file, block_size, SEEK_CUR);
                }
            }
            
            fseek(ctx->webm_file, cluster_end, SEEK_SET);
        } else {
            fseek(ctx->webm_file, length, SEEK_CUR);
        }
    }
    
    return 0;
}