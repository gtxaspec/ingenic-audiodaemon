#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <opus/opus.h>
#include "imp/imp_audio.h" // For IMP_AO_SendFrame etc.
#include "imp/imp_log.h"
#include "webm_opus_parser.h"
#include "logging.h" // Assuming iad uses this logging

#define TAG "WEBM_PARSER"

// Define EBML/WebM constants locally
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

// Initial capacity for dynamic packet buffer
#define INITIAL_PACKET_CAPACITY 256 

// --- Function implementations moved from iac/client/webm_opus_embedded.c ---
// --- Adapted for iad context (IadOpusContext, IMP logging, etc.) ---

// Read a variable-length integer (EBML format)
static uint64_t iad_read_vint(FILE *f, int *size_out) {
    uint8_t first_byte;
    if (fread(&first_byte, 1, 1, f) != 1) {
        if (size_out) *size_out = 0;
        return 0;
    }
    
    int length = 1;
    uint8_t mask = 0x80;
    while ((length <= 8) && !(first_byte & mask)) {
        length++;
        mask >>= 1;
    }
    
    if (length > 8) {
        if (size_out) *size_out = 0;
        return 0;
    }
    
    uint64_t value = first_byte & (0xFF >> length);
    
    for (int i = 1; i < length; i++) {
        uint8_t next_byte;
        if (fread(&next_byte, 1, 1, f) != 1) {
            if (size_out) *size_out = 0;
            return 0; 
        }
        value = (value << 8) | next_byte;
    }

    if (size_out) *size_out = length;
    return value;
}

// Read an EBML element ID
static uint32_t iad_read_ebml_id(FILE *f, int *size_out) {
    uint8_t first_byte;
    if (fread(&first_byte, 1, 1, f) != 1) {
        if (size_out) *size_out = 0;
        return 0;
    }
    
    int length = 1;
    uint8_t mask = 0x80;
    while ((length <= 4) && !(first_byte & mask)) {
        length++;
        mask >>= 1;
    }
    
    if (length > 4) {
        if (size_out) *size_out = 0;
        return 0;
    }
    
    uint32_t id = first_byte;

    for (int i = 1; i < length; i++) {
        uint8_t next_byte;
        if (fread(&next_byte, 1, 1, f) != 1) {
            if (size_out) *size_out = 0;
            return 0; 
        }
        id = (id << 8) | next_byte; 
    }

    if (size_out) *size_out = length;
    return id;
}

// Skip an element with known size
static void iad_skip_element(FILE *f, uint64_t size) {
    fseek(f, size, SEEK_CUR);
}

// Add an Opus packet to the dynamic buffer, resizing if necessary
static int iad_add_opus_packet(IadOpusPacketBuffer *buffer, const unsigned char *data, int size) {
    if (buffer->count >= buffer->capacity) {
        int new_capacity = buffer->capacity == 0 ? INITIAL_PACKET_CAPACITY : buffer->capacity * 2;
        IadOpusPacket *new_packets = realloc(buffer->packets, new_capacity * sizeof(IadOpusPacket));
        if (!new_packets) {
            IMP_LOG_ERR(TAG, "Failed to reallocate packet buffer to capacity %d\n", new_capacity);
            return -1; 
        }
        buffer->packets = new_packets;
        buffer->capacity = new_capacity;
    }
    
    buffer->packets[buffer->count].data = (unsigned char *)malloc(size);
    if (!buffer->packets[buffer->count].data) {
        IMP_LOG_ERR(TAG, "Failed to allocate memory for Opus packet\n");
        return -1;
    }
    
    memcpy(buffer->packets[buffer->count].data, data, size);
    buffer->packets[buffer->count].size = size;
    buffer->count++;
    
    return 0;
}

// Extract Opus packets from a SimpleBlock
static int iad_extract_opus_packets_from_block(IadOpusPacketBuffer *buffer, const unsigned char *data, int size, int track_number) {
    if (size < 4) return 0; 
    int block_track = data[0] & 0x7F;
    if (block_track != track_number) return 0;
    int offset = 4;
    if (offset < size) {
        return iad_add_opus_packet(buffer, data + offset, size - offset);
    }
    return 0;
}

// Parse WebM file and extract Opus packets into the buffer
static int iad_parse_webm_file(IadOpusContext *ctx) {
    FILE *f = ctx->webm_file;
    IadOpusPacketBuffer *buffer = (IadOpusPacketBuffer *)ctx->user_data;

    int size_bytes;
    int id_size;
    uint32_t id; 

    fseek(f, 0, SEEK_SET);

    unsigned char signature[4];
    if (fread(signature, 1, 4, f) != 4 || memcmp(signature, "\x1A\x45\xDF\xA3", 4) != 0) {
        IMP_LOG_ERR(TAG, "Not a valid WebM file (EBML header not found)\n");
        return -1;
    }

    uint64_t header_size = iad_read_vint(f, &size_bytes);
    iad_skip_element(f, header_size);

    if (fread(signature, 1, 4, f) != 4 || memcmp(signature, "\x18\x53\x80\x67", 4) != 0) {
         IMP_LOG_ERR(TAG, "Segment element not found at expected position.\n");
         return -1;
    }
    
    uint64_t segment_size = iad_read_vint(f, &size_bytes);
    if (size_bytes == 0) {
        IMP_LOG_ERR(TAG, "Error reading Segment size\n");
        return -1;
    }
    
    int opus_track_number = -1;
    int found_opus_track = 0;
    ctx->channels = 1; // Default channel count

    long segment_start = ftell(f);
    long segment_end = (segment_size == 0x01FFFFFFFFFFFFFF || segment_size == (uint64_t)-1) ? 
                       -1 : segment_start + segment_size; 

    while (1) { 
        long loop_start_pos = ftell(f);
        if (segment_end != -1 && loop_start_pos >= segment_end) break; 

        id = iad_read_ebml_id(f, &id_size); 
        if (id_size == 0) break; 

        uint64_t element_size = iad_read_vint(f, &size_bytes);
         if (size_bytes == 0) break; 

        long element_content_start_pos = ftell(f); 

        if (id == WEBM_ID_TRACKS) { 
            long tracks_content_end = element_content_start_pos + element_size;
            while (ftell(f) < tracks_content_end) {
                uint32_t track_id = iad_read_ebml_id(f, &id_size); 
                 if (id_size == 0) break; 
                uint64_t track_size = iad_read_vint(f, &size_bytes);
                 if (size_bytes == 0) break; 
                long track_content_start_pos = ftell(f);

                if (track_id == WEBM_ID_TRACKENTRY) { 
                    long trackentry_content_end = track_content_start_pos + track_size;
                    int track_number = -1;
                    int is_audio = 0;
                    int is_opus = 0;
                    while (ftell(f) < trackentry_content_end) {
                        uint32_t entry_id = iad_read_ebml_id(f, &id_size); 
                         if (id_size == 0) break; 
                        uint64_t entry_size = iad_read_vint(f, &size_bytes);
                         if (size_bytes == 0) break; 
                        long entry_content_start_pos = ftell(f);

                        if (entry_id == WEBM_ID_TRACKNUMBER) { 
                            unsigned char num_buf[8];
                            if (entry_size <= 8 && fread(num_buf, 1, entry_size, f) == entry_size) {
                                track_number = 0;
                                for (int i = 0; i < entry_size; i++) track_number = (track_number << 8) | num_buf[i];
                            } else iad_skip_element(f, entry_size);
                        } else if (entry_id == WEBM_ID_TRACKTYPE) { 
                            unsigned char type_buf[1];
                            if (entry_size == 1 && fread(type_buf, 1, 1, f) == 1) is_audio = (type_buf[0] == TRACK_TYPE_AUDIO);
                            else iad_skip_element(f, entry_size);
                        } else if (entry_id == WEBM_ID_CODECID) { 
                            char codec_id_str[32];
                            if (entry_size < sizeof(codec_id_str)) {
                                if (fread(codec_id_str, 1, entry_size, f) == entry_size) {
                                    codec_id_str[entry_size] = '\0';
                                    is_opus = (strstr(codec_id_str, "OPUS") != NULL);
                                } else iad_skip_element(f, entry_size);
                            } else iad_skip_element(f, entry_size);
                        } else if (entry_id == WEBM_ID_AUDIO) { 
                            long audio_content_end = entry_content_start_pos + entry_size;
                            int channels = 1; 
                            while (ftell(f) < audio_content_end) {
                                uint32_t audio_id = iad_read_ebml_id(f, &id_size); 
                                 if (id_size == 0) break; 
                                uint64_t audio_size = iad_read_vint(f, &size_bytes);
                                 if (size_bytes == 0) break; 
                                long audio_sub_content_start_pos = ftell(f);
                                if (audio_id == WEBM_ID_CHANNELS) { 
                                    unsigned char chan_buf[2];
                                    if (audio_size <= 2 && fread(chan_buf, 1, audio_size, f) == audio_size) {
                                        channels = 0;
                                        for (int i = 0; i < audio_size; i++) channels = (channels << 8) | chan_buf[i];
                                    } else iad_skip_element(f, audio_size);
                                } else iad_skip_element(f, audio_size);
                                fseek(f, audio_sub_content_start_pos + audio_size, SEEK_SET);
                            } 
                            if (is_opus && channels > 0) ctx->channels = channels;
                        } else iad_skip_element(f, entry_size);
                        fseek(f, entry_content_start_pos + entry_size, SEEK_SET);
                    } 
                    if (is_audio && is_opus && track_number > 0) {
                        opus_track_number = track_number;
                        found_opus_track = 1;
                    }
                } else iad_skip_element(f, track_size);
                if (found_opus_track) break;
            }
            fseek(f, element_content_start_pos + element_size, SEEK_SET);

        } else if (id == WEBM_ID_CLUSTER && found_opus_track) {
            long cluster_content_end = element_content_start_pos + element_size;
            while (ftell(f) < cluster_content_end) {
                uint32_t cluster_id = iad_read_ebml_id(f, &id_size); 
                 if (id_size == 0) break; 
                uint64_t cluster_size = iad_read_vint(f, &size_bytes);
                 if (size_bytes == 0) break; 
                long cluster_content_start_pos = ftell(f);

                if (cluster_id == WEBM_ID_SIMPLEBLOCK) { 
                    unsigned char *block_data = malloc(cluster_size); 
                    if (block_data) {
                        if (fread(block_data, 1, cluster_size, f) == cluster_size) {
                             iad_extract_opus_packets_from_block(buffer, block_data, cluster_size, opus_track_number);
                        } else {
                             IMP_LOG_ERR(TAG, "Failed to read SimpleBlock data (size %llu)\n", (unsigned long long)cluster_size);
                        }
                        free(block_data);
                    } else {
                         IMP_LOG_ERR(TAG, "Failed to allocate memory for SimpleBlock data\n");
                         iad_skip_element(f, cluster_size);
                    }
                } else {
                    iad_skip_element(f, cluster_size);
                }
                fseek(f, cluster_content_start_pos + cluster_size, SEEK_SET);
            } 
             if (ftell(f) != cluster_content_end) {
                 fseek(f, cluster_content_end, SEEK_SET);
             }
        } else {
            iad_skip_element(f, element_size);
        }

        long expected_end_pos = element_content_start_pos + element_size;
        if (ftell(f) != expected_end_pos) {
             fseek(f, expected_end_pos, SEEK_SET);
        }
        if (ftell(f) <= loop_start_pos) break; 

    } // End while Segment elements
    
    ctx->track_number = opus_track_number;
    IMP_LOG_INFO(TAG, "Extracted %d Opus packets\n", buffer->count);
    return found_opus_track ? 0 : -1;
}

// Initialize Opus decoder (adapted name)
int iad_init_opus_decoder(IadOpusContext *ctx, int channels) {
    int error;
    ctx->channels = channels > 0 ? channels : 1; // Ensure at least 1 channel
    ctx->sample_rate = IAD_OPUS_SAMPLE_RATE; 

    ctx->decoder = opus_decoder_create(ctx->sample_rate, ctx->channels, &error);
    if (error != OPUS_OK) {
        IMP_LOG_ERR(TAG, "Failed to create Opus decoder: %s\n", opus_strerror(error));
        return -1;
    }
    IMP_LOG_INFO(TAG, "Initialized Opus decoder: channels=%d, sample_rate=%d\n",
               ctx->channels, ctx->sample_rate);
    return 0;
}

// Clean up Opus context (adapted name)
void iad_cleanup_opus_context(IadOpusContext *ctx) {
    if (ctx->decoder) {
        opus_decoder_destroy(ctx->decoder);
        ctx->decoder = NULL;
    }
    if (ctx->webm_file) {
        fclose(ctx->webm_file);
        ctx->webm_file = NULL;
    }
    if (ctx->user_data) {
        IadOpusPacketBuffer *buffer = (IadOpusPacketBuffer *)ctx->user_data;
        for (int i = 0; i < buffer->count; i++) {
            if (buffer->packets[i].data) free(buffer->packets[i].data);
        }
        if (buffer->packets) free(buffer->packets);
        free(buffer); 
        ctx->user_data = NULL;
    }
}


// High-level function to handle WebM playback within the daemon
// Reads from file, decodes, and sends frames directly to IMP_AO
int iad_play_webm_file(const char *filename, int aoDevID, int aoChnID) {
    IMP_LOG_INFO(TAG, "Starting WebM playback for: %s (Device: %d, Channel: %d)\n", filename, aoDevID, aoChnID);
    
    IadOpusContext ctx = {0}; // Initialize context
    int ret = -1; // Default to error
    int total_decoded_samples = 0;

    // Allocate packet buffer struct
    IadOpusPacketBuffer *buffer = (IadOpusPacketBuffer *)calloc(1, sizeof(IadOpusPacketBuffer)); 
    if (!buffer) {
        IMP_LOG_ERR(TAG, "Failed to allocate packet buffer struct\n");
        return -1;
    }
    buffer->packets = NULL; 
    buffer->count = 0;
    buffer->capacity = 0; 
    buffer->current = 0;
    ctx.user_data = buffer;

    // Open file
    ctx.webm_file = fopen(filename, "rb");
    if (!ctx.webm_file) {
        IMP_LOG_ERR(TAG, "Failed to open WebM file: %s\n", filename);
        perror("fopen");
        iad_cleanup_opus_context(&ctx); // Frees buffer struct
        return -1;
    }

    // Parse file to populate buffer
    if (iad_parse_webm_file(&ctx) != 0 || buffer->count == 0) {
        IMP_LOG_ERR(TAG, "Failed to parse WebM file or no Opus track/packets found in: %s\n", filename);
        iad_cleanup_opus_context(&ctx);
        return -1;
    }

    // Initialize decoder (using channels found during parsing)
    if (iad_init_opus_decoder(&ctx, ctx.channels) != 0) {
        IMP_LOG_ERR(TAG, "Failed to initialize Opus decoder for: %s\n", filename);
        iad_cleanup_opus_context(&ctx);
        return -1;
    }

    // --- Decoding and Playback Loop ---
    int16_t pcm_buffer[IAD_OPUS_MAX_FRAME_SIZE * IAD_OPUS_MAX_CHANNELS]; 
    int samples;
    
    for (int i = 0; i < buffer->count; i++) {
        IadOpusPacket *packet = &buffer->packets[i];
        
        samples = opus_decode(ctx.decoder, packet->data, packet->size, 
                             pcm_buffer, IAD_OPUS_MAX_FRAME_SIZE, 0);
        
        if (samples > 0) {
            total_decoded_samples += samples;
            
            IMPAudioFrame frm = {
                .virAddr = (uint32_t *)pcm_buffer, 
                .len = samples * ctx.channels * sizeof(int16_t)
            };

            // Send the decoded PCM frame directly to the audio output device
            if (IMP_AO_SendFrame(aoDevID, aoChnID, &frm, BLOCK)) {
                 IMP_LOG_ERR(TAG, "IMP_AO_SendFrame failed during WebM playback.\n");
                 // Potentially handle reinitialization or just break
                 ret = -1;
                 goto cleanup; // Use goto for cleanup on error
            }
        } else {
            IMP_LOG_ERR(TAG, "Failed to decode packet %d: %s\n", i + 1, opus_strerror(samples));
            // Continue decoding other packets? For now, yes.
        }
    }

    IMP_LOG_INFO(TAG, "Finished decoding %d total samples (%dHz) from %d packets for %s.\n", 
                 total_decoded_samples, ctx.sample_rate, buffer->count, filename);
    ret = 0; // Success

cleanup:
    iad_cleanup_opus_context(&ctx);
    return ret;
}

// Placeholder for the main function that was previously here (now implemented as iad_play_webm_file)
// int iad_parse_and_decode_webm(const char *filename, int aoDevID, int aoChnID) {
//     IMP_LOG_ERR(TAG, "iad_parse_and_decode_webm is deprecated, use iad_play_webm_file.\n");
//     return iad_play_webm_file(filename, aoDevID, aoChnID);
// }
