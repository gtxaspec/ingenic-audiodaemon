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

// Buffer size for reading from file
#define READ_BUFFER_SIZE 4096

// Maximum number of Opus packets to store in memory
#define MAX_OPUS_PACKETS 1024

// Structure to hold an Opus packet
typedef struct {
    unsigned char *data;
    int size;
} OpusPacket;

// Structure to hold Opus packets in memory
typedef struct {
    OpusPacket packets[MAX_OPUS_PACKETS];
    int count;
    int current;
} OpusPacketBuffer;

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
    
    // Free any other resources
    if (ctx->user_data) {
        OpusPacketBuffer *buffer = (OpusPacketBuffer *)ctx->user_data;
        
        // Free all packet data
        for (int i = 0; i < buffer->count; i++) {
            if (buffer->packets[i].data) {
                free(buffer->packets[i].data);
                buffer->packets[i].data = NULL;
            }
        }
        
        free(buffer);
        ctx->user_data = NULL;
    }
}

// Read a variable-length integer (EBML format)
static uint64_t read_vint(FILE *f, int *size_out) {
    uint8_t first_byte;
    if (fread(&first_byte, 1, 1, f) != 1) {
        if (size_out) *size_out = 0;
        return 0;
    }
    
    // Determine length by counting leading zeros
    int length = 1;
    uint8_t mask = 0x80;
    while ((length <= 8) && !(first_byte & mask)) {
        length++;
        mask >>= 1;
    }
    
    if (length > 8) {
        // Invalid length
        if (size_out) *size_out = 0;
        return 0;
    }
    
    // First byte with mask bit cleared
    uint64_t value = first_byte & (0xFF >> length);
    
    // Read remaining bytes
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
static uint32_t read_ebml_id(FILE *f, int *size_out) {
    uint8_t first_byte;
    if (fread(&first_byte, 1, 1, f) != 1) {
        if (size_out) *size_out = 0;
        return 0;
    }
    
    // Determine ID length by counting leading zeros
    int length = 1;
    uint8_t mask = 0x80;
    while ((length <= 4) && !(first_byte & mask)) {
        length++;
        mask >>= 1;
    }
    
    if (length > 4) {
        // Invalid length
        if (size_out) *size_out = 0;
        return 0;
    }
    
    // First byte with mask bit cleared
    uint32_t id = first_byte & (0xFF >> length);
    
    // Read remaining bytes
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
static void skip_element(FILE *f, uint64_t size) {
    fseek(f, size, SEEK_CUR);
}

// Add an Opus packet to the buffer
static int add_opus_packet(OpusPacketBuffer *buffer, const unsigned char *data, int size) {
    if (buffer->count >= MAX_OPUS_PACKETS) {
        fprintf(stderr, "Too many Opus packets (max %d)\n", MAX_OPUS_PACKETS);
        return -1;
    }
    
    // Allocate memory for the packet data
    buffer->packets[buffer->count].data = (unsigned char *)malloc(size);
    if (!buffer->packets[buffer->count].data) {
        fprintf(stderr, "Failed to allocate memory for Opus packet\n");
        return -1;
    }
    
    // Copy the packet data
    memcpy(buffer->packets[buffer->count].data, data, size);
    buffer->packets[buffer->count].size = size;
    
    // Increment the packet count
    buffer->count++;
    
    return 0;
}

// Extract Opus packets from a SimpleBlock
static int extract_opus_packets_from_block(OpusPacketBuffer *buffer, const unsigned char *data, int size, int track_number) {
    if (size < 4) {
        return 0; // Too small to be a valid block
    }

    // First byte of SimpleBlock contains track number (with high bit set)
    int block_track = data[0] & 0x7F;

    // Check if this block belongs to our Opus track
    if (block_track != track_number) {
        if (DEBUG_WEBM) {
            printf("SimpleBlock track %d doesn't match our Opus track %d\n",
                   block_track, track_number);
        }
        return 0;
    }

    // Skip track number (1 byte) and timestamp (2 bytes)
    // The 4th byte contains flags
    int offset = 4;

    // The rest is Opus data
    if (offset < size) {
        if (DEBUG_WEBM) {
            printf("Found Opus data in SimpleBlock: %d bytes (track %d)\n",
                   size - offset, block_track);

            // Print first few bytes for debugging
            printf("Data starts with: ");
            for (int i = 0; i < 8 && offset + i < size; i++) {
                printf("%02X ", data[offset + i]);
            }
            printf("\n");
        }

        // Add the packet to our buffer
        return add_opus_packet(buffer, data + offset, size - offset);
    }

    return 0;
}

// Parse WebM file and extract Opus packets
static int parse_webm_file(OpusContext *ctx) {
    FILE *f = ctx->webm_file;
    OpusPacketBuffer *buffer = (OpusPacketBuffer *)ctx->user_data;

    // Variables for parsing
    int size_bytes;
    int id_size;
    uint32_t id;

    // Seek to beginning of file
    fseek(f, 0, SEEK_SET);

    // Check for EBML header using direct byte comparison
    unsigned char signature[4];
    if (fread(signature, 1, 4, f) != 4 ||
        signature[0] != 0x1A || signature[1] != 0x45 ||
        signature[2] != 0xDF || signature[3] != 0xA3) {
        fprintf(stderr, "Not a valid WebM file (EBML header not found)\n");
        return -1;
    }

    printf("Found EBML header signature: 0x%02X%02X%02X%02X\n",
           signature[0], signature[1], signature[2], signature[3]);

    // Skip the rest of the EBML header
    uint64_t header_size = read_vint(f, &size_bytes);
    printf("EBML header size: %lu bytes (%d size bytes)\n", header_size, size_bytes);
    skip_element(f, header_size);

    // Look for the Segment element using direct byte comparison
    // The Segment ID is 0x18 0x53 0x80 0x67
    if (fread(signature, 1, 4, f) != 4) {
        fprintf(stderr, "Failed to read Segment ID\n");
        return -1;
    }

    printf("Potential Segment ID: 0x%02X%02X%02X%02X\n",
           signature[0], signature[1], signature[2], signature[3]);

    if (signature[0] != 0x18 || signature[1] != 0x53 ||
        signature[2] != 0x80 || signature[3] != 0x67) {

        // Try a more flexible approach - scan for Segment ID
        fprintf(stderr, "Segment element not found at expected position, scanning...\n");

        // Go back to just after the EBML header
        fseek(f, 4 + size_bytes + header_size, SEEK_SET);

        // Scan for Segment ID
        unsigned char scan_buffer[READ_BUFFER_SIZE];
        size_t bytes_read;
        int found_segment = 0;

        while ((bytes_read = fread(scan_buffer, 1, sizeof(scan_buffer), f)) > 0) {
            for (size_t i = 0; i < bytes_read - 4; i++) {
                if (scan_buffer[i] == 0x18 && scan_buffer[i+1] == 0x53 &&
                    scan_buffer[i+2] == 0x80 && scan_buffer[i+3] == 0x67) {

                    printf("Found Segment ID at offset %ld\n", ftell(f) - bytes_read + i);

                    // Position file just after the Segment ID
                    fseek(f, ftell(f) - bytes_read + i + 4, SEEK_SET);
                    found_segment = 1;
                    break;
                }
            }

            if (found_segment) break;
        }

        if (!found_segment) {
            fprintf(stderr, "No Segment element found in file\n");
            return -1;
        }
    } else {
        printf("Found Segment element\n");
    }
    
    // Read Segment size (we don't need to use it)
    uint64_t segment_size = read_vint(f, &size_bytes);
    
    // Variables to track Opus track
    int opus_track_number = -1;
    int found_opus_track = 0;
    
    // Parse elements inside Segment
    long segment_start = ftell(f);
    long segment_end = segment_size == 0x01FFFFFFFFFFFFFF ? 
                       0x7FFFFFFFFFFFFFFF : segment_start + segment_size;
    
    while (ftell(f) < segment_end) {
        id = read_ebml_id(f, &id_size);
        uint64_t element_size = read_vint(f, &size_bytes);
        long element_start = ftell(f);
        
        if (id == WEBM_ID_TRACKS) {
            // Parse Tracks element to find Opus track
            while (ftell(f) < element_start + element_size) {
                uint32_t track_id = read_ebml_id(f, &id_size);
                uint64_t track_size = read_vint(f, &size_bytes);
                long track_start = ftell(f);
                
                if (track_id == WEBM_ID_TRACKENTRY) {
                    // Parse TrackEntry to find Opus codec
                    int track_number = -1;
                    int is_audio = 0;
                    int is_opus = 0;
                    
                    while (ftell(f) < track_start + track_size) {
                        uint32_t entry_id = read_ebml_id(f, &id_size);
                        uint64_t entry_size = read_vint(f, &size_bytes);
                        
                        if (entry_id == WEBM_ID_TRACKNUMBER) {
                            // Read track number
                            unsigned char num_buf[8];
                            if (entry_size <= 8 && fread(num_buf, 1, entry_size, f) == entry_size) {
                                track_number = 0;
                                for (int i = 0; i < entry_size; i++) {
                                    track_number = (track_number << 8) | num_buf[i];
                                }
                            } else {
                                skip_element(f, entry_size);
                            }
                        } else if (entry_id == WEBM_ID_TRACKTYPE) {
                            // Check if it's an audio track
                            unsigned char type_buf[1];
                            if (entry_size == 1 && fread(type_buf, 1, 1, f) == 1) {
                                is_audio = (type_buf[0] == TRACK_TYPE_AUDIO);
                            } else {
                                skip_element(f, entry_size);
                            }
                        } else if (entry_id == WEBM_ID_CODECID) {
                            // Check if it's Opus codec
                            char codec_id[32];
                            if (entry_size < sizeof(codec_id)) {
                                if (fread(codec_id, 1, entry_size, f) == entry_size) {
                                    codec_id[entry_size] = '\0';
                                    is_opus = (strstr(codec_id, "OPUS") != NULL);
                                    
                                    if (is_opus && DEBUG_WEBM) {
                                        printf("Found Opus codec ID: %s\n", codec_id);
                                    }
                                } else {
                                    skip_element(f, entry_size);
                                }
                            } else {
                                skip_element(f, entry_size);
                            }
                        } else if (entry_id == WEBM_ID_AUDIO) {
                            // Parse Audio element to get channels
                            long audio_start = ftell(f);
                            int channels = 1; // Default to mono
                            
                            while (ftell(f) < audio_start + entry_size) {
                                uint32_t audio_id = read_ebml_id(f, &id_size);
                                uint64_t audio_size = read_vint(f, &size_bytes);
                                
                                if (audio_id == WEBM_ID_CHANNELS) {
                                    // Read channels
                                    unsigned char chan_buf[2];
                                    if (audio_size <= 2 && fread(chan_buf, 1, audio_size, f) == audio_size) {
                                        channels = 0;
                                        for (int i = 0; i < audio_size; i++) {
                                            channels = (channels << 8) | chan_buf[i];
                                        }
                                    } else {
                                        skip_element(f, audio_size);
                                    }
                                } else {
                                    skip_element(f, audio_size);
                                }
                            }
                            
                            // Update context with channels
                            if (is_opus) {
                                ctx->channels = channels;
                                if (DEBUG_WEBM) {
                                    printf("Opus track has %d channels\n", channels);
                                }
                            }
                        } else {
                            skip_element(f, entry_size);
                        }
                    }
                    
                    // If this is an Opus audio track, save the track number
                    if (is_audio && is_opus && track_number > 0) {
                        opus_track_number = track_number;
                        found_opus_track = 1;
                        
                        if (DEBUG_WEBM) {
                            printf("Found Opus audio track: %d\n", opus_track_number);
                        }
                    }
                } else {
                    skip_element(f, track_size);
                }
                
                // If we've already found the track, we can skip the rest
                if (found_opus_track) {
                    break;
                }
            }
            
            // Skip to the end of the Tracks element
            fseek(f, element_start + element_size, SEEK_SET);
        } else if (id == WEBM_ID_CLUSTER && found_opus_track) {
            // Parse Cluster to find SimpleBlocks with Opus data
            long cluster_end = element_start + element_size;

            printf("Parsing Cluster at offset %ld (size: %lu)\n", element_start, element_size);

            while (ftell(f) < cluster_end) {
                uint32_t cluster_id = read_ebml_id(f, &id_size);
                uint64_t cluster_size = read_vint(f, &size_bytes);

                if (cluster_id == WEBM_ID_SIMPLEBLOCK) {
                    // This is a SimpleBlock, check if it contains Opus data
                    printf("Found SimpleBlock (size: %lu) at offset %ld\n",
                           cluster_size, ftell(f));

                    unsigned char block_data[8192];  // Larger buffer for SimpleBlocks
                    int block_size = cluster_size < sizeof(block_data) ? cluster_size : sizeof(block_data);

                    if (fread(block_data, 1, block_size, f) == block_size) {
                        // Try to extract Opus data
                        if (extract_opus_packets_from_block(buffer, block_data, block_size, opus_track_number)) {
                            printf("Successfully extracted Opus packet from SimpleBlock\n");
                        }

                        // If we couldn't read the entire block, skip the rest
                        if (block_size < cluster_size) {
                            skip_element(f, cluster_size - block_size);
                        }
                    } else {
                        // Failed to read block data
                        printf("Failed to read SimpleBlock data\n");
                        skip_element(f, cluster_size);
                    }
                } else if (cluster_id == WEBM_ID_TIMECODE) {
                    // Timecode element - just skip it
                    printf("Found Timecode element (size: %lu)\n", cluster_size);
                    skip_element(f, cluster_size);
                } else {
                    // Unknown element - skip it
                    printf("Unknown Cluster element ID: 0x%X (size: %lu)\n",
                           cluster_id, cluster_size);
                    skip_element(f, cluster_size);
                }

                // Check if we've found enough packets
                if (buffer->count >= 100) {
                    printf("Found sufficient number of Opus packets (%d), stopping Cluster parsing\n",
                           buffer->count);
                    break;
                }
            }

            // Skip to the end of the Cluster
            fseek(f, cluster_end, SEEK_SET);
        } else {
            // Skip unknown element
            printf("Skipping unknown element ID: 0x%X (size: %lu)\n", id, element_size);
            skip_element(f, element_size);
        }
    }
    
    if (!found_opus_track || buffer->count == 0) {
        // If we didn't find an Opus track through normal parsing,
        // try a more aggressive approach by scanning for Opus data patterns
        printf("No Opus track found through normal parsing or no packets extracted, trying pattern matching...\n");

        // Seek to beginning of file
        fseek(f, 0, SEEK_SET);

        // Reset packet count if we had any
        buffer->count = 0;

        // Buffer for scanning
        unsigned char scan_buffer[READ_BUFFER_SIZE];
        size_t bytes_read;
        int found_opus_head = 0;

        // First, try to find "OpusHead" marker
        while ((bytes_read = fread(scan_buffer, 1, sizeof(scan_buffer), f)) > 0) {
            for (size_t i = 0; i < bytes_read - 8; i++) {
                // Look for "OpusHead" marker
                if (memcmp(scan_buffer + i, "OpusHead", 8) == 0) {
                    printf("Found OpusHead marker at offset %ld\n", ftell(f) - bytes_read + i);

                    // Try to read channel count from OpusHead
                    if (i + 10 < bytes_read) {
                        // Channel count is typically at offset 9 in OpusHead
                        ctx->channels = scan_buffer[i + 9];
                        printf("Detected %d channels from OpusHead\n", ctx->channels);
                    } else {
                        // Default to mono
                        ctx->channels = 1;
                    }

                    // Set a default track number
                    opus_track_number = 1;
                    found_opus_track = 1;
                    found_opus_head = 1;
                    break;
                }
            }

            if (found_opus_head) break;
        }

        // Now look for "OpusTags" which often comes after OpusHead
        if (found_opus_head) {
            while ((bytes_read = fread(scan_buffer, 1, sizeof(scan_buffer), f)) > 0) {
                for (size_t i = 0; i < bytes_read - 8; i++) {
                    if (memcmp(scan_buffer + i, "OpusTags", 8) == 0) {
                        printf("Found OpusTags marker at offset %ld\n", ftell(f) - bytes_read + i);
                        break;
                    }
                }

                // After OpusTags, we should find actual Opus data
                break;
            }
        }

        // Now scan for actual Opus packets
        // Seek to beginning of file if we didn't find OpusHead
        if (!found_opus_head) {
            fseek(f, 0, SEEK_SET);
        }

        // Scan for Opus packets
        int packet_count = 0;
        while ((bytes_read = fread(scan_buffer, 1, sizeof(scan_buffer), f)) > 0) {
            for (size_t i = 0; i < bytes_read - 4; i++) {
                // Look for potential Opus packet patterns
                // Opus packets typically start with a TOC byte
                uint8_t toc = scan_buffer[i];

                // Check if this looks like a valid TOC byte
                // TOC byte format: [config (3 bits)][stereo (1 bit)][frame count (2 bits)]
                uint8_t config = (toc >> 3) & 0x1F;
                uint8_t stereo = (toc >> 2) & 0x01;
                uint8_t frame_count = toc & 0x03;

                // Simple heuristic: if the next byte is a reasonable packet size
                if (i + 1 < bytes_read) {
                    uint8_t packet_size = scan_buffer[i+1];

                    if (packet_size > 0 && packet_size < 250 && i + 2 + packet_size <= bytes_read) {
                        // This might be an Opus packet, add it to our buffer
                        add_opus_packet(buffer, scan_buffer + i, packet_size + 2);
                        packet_count++;

                        if (packet_count % 10 == 0) {
                            printf("Extracted %d potential Opus packets\n", packet_count);
                        }

                        // Skip to the end of this packet
                        i += packet_size + 1;

                        // Set a default track number if we haven't found one yet
                        if (!found_opus_track) {
                            opus_track_number = 1;
                            found_opus_track = 1;

                            // Use stereo bit from TOC to determine channels
                            ctx->channels = stereo ? 2 : 1;
                            printf("Detected %d channels from Opus TOC\n", ctx->channels);
                        }
                    }
                }
            }

            // If we've found a good number of packets, we can stop
            if (buffer->count >= 100) {
                printf("Found sufficient number of Opus packets (%d)\n", buffer->count);
                break;
            }
        }

        if (buffer->count > 0) {
            printf("Extracted %d potential Opus packets using pattern matching\n", buffer->count);
            found_opus_track = 1;
        }
    }
    
    // Set the track number in the context
    ctx->track_number = opus_track_number;
    
    if (DEBUG_WEBM) {
        printf("Extracted %d Opus packets\n", buffer->count);
    }
    
    return found_opus_track ? 0 : -1;
}

// Open WebM file and initialize Opus decoder
int open_webm_file(OpusContext *ctx, const char *filename) {
    ctx->webm_file = fopen(filename, "rb");
    if (!ctx->webm_file) {
        perror("Failed to open WebM file");
        return -1;
    }
    
    printf("Parsing WebM file: %s\n", filename);
    
    // Dump file header for debugging
    dump_file_header(ctx->webm_file);
    
    // Allocate packet buffer
    OpusPacketBuffer *buffer = (OpusPacketBuffer *)malloc(sizeof(OpusPacketBuffer));
    if (!buffer) {
        fprintf(stderr, "Failed to allocate packet buffer\n");
        fclose(ctx->webm_file);
        ctx->webm_file = NULL;
        return -1;
    }
    
    // Initialize packet buffer
    memset(buffer, 0, sizeof(OpusPacketBuffer));
    ctx->user_data = buffer;
    
    // Parse WebM file and extract Opus packets
    if (parse_webm_file(ctx) != 0) {
        fprintf(stderr, "Failed to parse WebM file or no Opus track found\n");
        cleanup_opus_context(ctx);
        return -1;
    }
    
    // Initialize Opus decoder
    if (init_opus_decoder(ctx, ctx->channels) != 0) {
        fprintf(stderr, "Failed to initialize Opus decoder\n");
        cleanup_opus_context(ctx);
        return -1;
    }
    
    printf("Successfully initialized WebM/Opus file with %d packets\n", 
           ((OpusPacketBuffer *)ctx->user_data)->count);
    
    return 0;
}

// Decode WebM/Opus file and send PCM to socket
int decode_webm_to_pcm(OpusContext *ctx, int sockfd) {
    if (!ctx->decoder || !ctx->user_data) {
        fprintf(stderr, "Invalid Opus context\n");
        return -1;
    }
    
    OpusPacketBuffer *buffer = (OpusPacketBuffer *)ctx->user_data;
    
    // Reset current packet index
    buffer->current = 0;
    
    // Variables for decoding
    int16_t pcm_buffer[PCM_BUFFER_SIZE * 2]; // Extra space for stereo
    int samples;
    int total_samples = 0;
    
    // Decode each packet
    while (buffer->current < buffer->count) {
        OpusPacket *packet = &buffer->packets[buffer->current];
        
        // Decode the packet
        samples = opus_decode(ctx->decoder, packet->data, packet->size, 
                             pcm_buffer, OPUS_FRAME_SIZE, 0);
        
        if (samples > 0) {
            total_samples += samples;
            
            if (DEBUG_WEBM && buffer->current % 10 == 0) {
                printf("Decoded %d samples from packet %d/%d (size %d)\n", 
                       samples, buffer->current + 1, buffer->count, packet->size);
            }
            
            // Send PCM data to socket
            int bytes_to_send = samples * ctx->channels * sizeof(int16_t);
            if (send(sockfd, pcm_buffer, bytes_to_send, 0) != bytes_to_send) {
                perror("Failed to send PCM data");
                return -1;
            }
        } else {
            fprintf(stderr, "Failed to decode packet %d: %s\n", 
                    buffer->current + 1, opus_strerror(samples));
        }
        
        // Move to next packet
        buffer->current++;
    }
    
    printf("Decoded %d samples from %d packets\n", total_samples, buffer->count);
    
    return 0;
}