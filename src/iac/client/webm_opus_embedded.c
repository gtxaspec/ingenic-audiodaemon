#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <ctype.h>
#include <arpa/inet.h> // For htonl
#ifdef __linux__ // Include endian.h only if available (typically Linux)
#include <endian.h>
#endif
#include "webm_opus.h"
#include "playback.h" // For PCM_BUFFER_SIZE if still needed elsewhere

// EBML and WebM element IDs (Big-Endian)
#define EBML_ID_HEADER         0x1A45DFA3
#define WEBM_ID_SEGMENT        0x18538067
#define WEBM_ID_TRACKS         0x1654AE6B
#define WEBM_ID_TRACKENTRY     0xAE       // Note: This is 1 byte, careful with comparison
#define WEBM_ID_TRACKNUMBER    0xD7       // Note: This is 1 byte
#define WEBM_ID_TRACKTYPE      0x83       // Note: This is 1 byte
#define WEBM_ID_CODECID        0x86       // Note: This is 1 byte
#define WEBM_ID_AUDIO          0xE1       // Note: This is 1 byte
#define WEBM_ID_CHANNELS       0x9F       // Note: This is 1 byte
#define WEBM_ID_CLUSTER        0x1F43B675
#define WEBM_ID_TIMECODE       0xE7       // Note: This is 1 byte
#define WEBM_ID_SIMPLEBLOCK    0xA3       // Note: This is 1 byte

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
    ctx->sample_rate = OPUS_SAMPLE_RATE; // Now 16000

    // Initialize Opus Decoder for the target sample rate
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
    // No resampler to destroy
    
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
    long start_pos = ftell(f); // Debug: Position before read
    uint8_t first_byte;
    if (fread(&first_byte, 1, 1, f) != 1) {
        if (DEBUG_WEBM) fprintf(stderr, "read_vint: fread failed at pos %ld, eof=%d, err=%d\n", start_pos, feof(f), ferror(f));
        if (size_out) *size_out = 0;
        return 0;
    }
    if (DEBUG_WEBM) printf("read_vint: Read first byte 0x%02X at pos %ld\n", first_byte, start_pos);
    
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
             if (DEBUG_WEBM) fprintf(stderr, "read_vint: fread failed reading byte %d of %d at pos %ld\n", i + 1, length, ftell(f) -1);
            if (size_out) *size_out = 0;
            return 0; // Error
        }
        value = (value << 8) | next_byte;
    }

    // Use %llu for uint64_t
    if (DEBUG_WEBM) printf("read_vint: Returning size %llu (length %d bytes)\n", (unsigned long long)value, length);
    if (size_out) *size_out = length;
    return value;
}

// Read an EBML element ID
static uint32_t read_ebml_id(FILE *f, int *size_out) {
    long start_pos = ftell(f); // Debug: Position before read
    uint8_t first_byte;
    if (fread(&first_byte, 1, 1, f) != 1) {
        if (DEBUG_WEBM) fprintf(stderr, "read_ebml_id: fread failed at pos %ld, eof=%d, err=%d\n", start_pos, feof(f), ferror(f));
        if (size_out) *size_out = 0;
        return 0;
    }
    if (DEBUG_WEBM) printf("read_ebml_id: Read first byte 0x%02X at pos %ld\n", first_byte, start_pos);
    
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
    
    // Start ID reconstruction with the first byte
    uint32_t id = first_byte;

    // Read remaining (length - 1) bytes
    for (int i = 1; i < length; i++) {
        uint8_t next_byte;
        if (fread(&next_byte, 1, 1, f) != 1) {
             if (DEBUG_WEBM) fprintf(stderr, "read_ebml_id: fread failed reading byte %d of %d at pos %ld\n", i + 1, length, ftell(f) -1);
            if (size_out) *size_out = 0;
            return 0; // Error
        }
        id = (id << 8) | next_byte; // Append subsequent bytes
    }

    if (size_out) *size_out = length;

    // The ID is read in big-endian order, return it as is.
    if (DEBUG_WEBM) printf("read_ebml_id: Returning raw ID 0x%X (length %d bytes)\n", id, length);
    return id;
}

// Skip an element with known size
static void skip_element(FILE *f, uint64_t size) {
    // Check for potential large skips that might indicate parsing errors
    if (size > 1024 * 1024 * 100) { // Arbitrary limit (e.g., 100MB)
        if (DEBUG_WEBM) fprintf(stderr, "Warning: Attempting to skip very large element size: %llu bytes at pos %ld\n", (unsigned long long)size, ftell(f));
        // Optionally add error handling here if huge skips are always invalid
    }
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
            // Don't print this every time, can be noisy
            // printf("SimpleBlock track %d doesn't match our Opus track %d\n", block_track, track_number);
        }
        return 0;
    }

    // Skip track number (1 byte) and timestamp (2 bytes)
    // The 4th byte contains flags
    int offset = 4;

    // The rest is Opus data
    if (offset < size) {
        if (DEBUG_WEBM) {
            // printf("Found Opus data in SimpleBlock: %d bytes (track %d)\n", size - offset, block_track);
            // Print first few bytes for debugging
            // printf("Data starts with: ");
            // for (int i = 0; i < 8 && offset + i < size; i++) {
            //     printf("%02X ", data[offset + i]);
            // }
            // printf("\n");
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
    uint32_t id; // Will hold the raw ID read from file

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
    // Use %llu for uint64_t
    printf("EBML header size: %llu bytes (%d size bytes)\n", (unsigned long long)header_size, size_bytes);
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
    
    // Read Segment size (we don't need to use it, but need to read it)
    uint64_t segment_size = read_vint(f, &size_bytes);
    if (size_bytes == 0) {
        fprintf(stderr, "Error reading Segment size\n");
        return -1;
    }
    
    // Variables to track Opus track
    int opus_track_number = -1;
    int found_opus_track = 0;
    
    // Parse elements inside Segment
    long segment_start = ftell(f);
    // Define segment_end based on segment_size (handle unknown size)
    long segment_end = (segment_size == 0x01FFFFFFFFFFFFFF || segment_size == (uint64_t)-1) ? 
                       -1 : segment_start + segment_size; // -1 indicates unknown end

    while (1) { // Loop until break (EOF, error, or end of segment)
        long loop_start_pos = ftell(f); // Track position at start of loop iteration
        if (segment_end != -1 && loop_start_pos >= segment_end) {
            if (DEBUG_WEBM) printf("Reached end of Segment at pos %ld\n", loop_start_pos);
            break; // Reached defined end of segment
        }

        // --- Read Element ID ---
        id = read_ebml_id(f, &id_size); // Read the raw ID
        if (id_size == 0) {
             if (feof(f)) {
                 if (DEBUG_WEBM) printf("Reached EOF while reading ID at pos %ld\n", loop_start_pos);
             } else {
                 if (DEBUG_WEBM) fprintf(stderr, "Error reading ID at pos %ld\n", loop_start_pos);
             }
             break; // Stop parsing on EOF or read error
        }

        // --- Read Element Size ---
        uint64_t element_size = read_vint(f, &size_bytes);
         if (size_bytes == 0) {
             if (feof(f)) {
                 if (DEBUG_WEBM) printf("Reached EOF while reading size at pos %ld for ID 0x%X\n", ftell(f), id);
             } else {
                 if (DEBUG_WEBM) fprintf(stderr, "Error reading VINT size at pos %ld for ID 0x%X\n", ftell(f), id);
             }
             break; // Stop parsing on EOF or read error
        }

        long element_content_start_pos = ftell(f); // Position where element content starts

        // Use %llu for uint64_t element_size
        if (DEBUG_WEBM) {
             printf("Processing Element: ID=0x%X, Size=%llu, Content Start Pos=%ld\n",
                    id, (unsigned long long)element_size, element_content_start_pos);
        }

        // --- Process Element ---
        if (id == WEBM_ID_TRACKS) { // Compare raw ID
             // Use %llu for uint64_t element_size
            if (DEBUG_WEBM) printf("Found Tracks element (size: %llu)\n", (unsigned long long)element_size);
            long tracks_content_end = element_content_start_pos + element_size;
            // Parse Tracks element to find Opus track
            while (ftell(f) < tracks_content_end) {
                // Read TrackEntry ID
                uint32_t track_id = read_ebml_id(f, &id_size); // Read raw track ID
                 if (id_size == 0) { if (DEBUG_WEBM) fprintf(stderr, "Error or EOF reading TrackEntry ID at pos %ld\n", ftell(f)); break; }

                // Read TrackEntry Size
                uint64_t track_size = read_vint(f, &size_bytes);
                 if (size_bytes == 0) { if (DEBUG_WEBM) fprintf(stderr, "Error or EOF reading TrackEntry size at pos %ld for ID 0x%X\n", ftell(f), track_id); break; }

                long track_content_start_pos = ftell(f);

                if (track_id == WEBM_ID_TRACKENTRY) { // Compare raw ID
                     // Use %llu for uint64_t track_size
                    if (DEBUG_WEBM) printf("Found TrackEntry (size: %llu)\n", (unsigned long long)track_size);
                    long trackentry_content_end = track_content_start_pos + track_size;
                    // Parse TrackEntry to find Opus codec
                    int track_number = -1;
                    int is_audio = 0;
                    int is_opus = 0;

                    while (ftell(f) < trackentry_content_end) {
                        // Read TrackEntry sub-element ID
                        uint32_t entry_id = read_ebml_id(f, &id_size); // Read raw entry ID
                         if (id_size == 0) { if (DEBUG_WEBM) fprintf(stderr, "Error or EOF reading TrackEntry sub-ID at pos %ld\n", ftell(f)); break; }

                        // Read TrackEntry sub-element Size
                        uint64_t entry_size = read_vint(f, &size_bytes);
                         if (size_bytes == 0) { if (DEBUG_WEBM) fprintf(stderr, "Error or EOF reading TrackEntry sub-size at pos %ld for ID 0x%X\n", ftell(f), entry_id); break; }

                        long entry_content_start_pos = ftell(f);

                        if (entry_id == WEBM_ID_TRACKNUMBER) { // Compare raw ID
                             // Use %llu for uint64_t entry_size
                            if (DEBUG_WEBM) printf("Found TrackNumber (size: %llu)\n", (unsigned long long)entry_size);
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
                        } else if (entry_id == WEBM_ID_TRACKTYPE) { // Compare raw ID
                             // Use %llu for uint64_t entry_size
                            if (DEBUG_WEBM) printf("Found TrackType (size: %llu)\n", (unsigned long long)entry_size);
                            // Check if it's an audio track
                            unsigned char type_buf[1];
                            if (entry_size == 1 && fread(type_buf, 1, 1, f) == 1) {
                                is_audio = (type_buf[0] == TRACK_TYPE_AUDIO);
                            } else {
                                skip_element(f, entry_size);
                            }
                        } else if (entry_id == WEBM_ID_CODECID) { // Compare raw ID
                             // Use %llu for uint64_t entry_size
                            if (DEBUG_WEBM) printf("Found CodecID (size: %llu)\n", (unsigned long long)entry_size);
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
                        } else if (entry_id == WEBM_ID_AUDIO) { // Compare raw ID
                             // Use %llu for uint64_t entry_size
                            if (DEBUG_WEBM) printf("Found Audio element (size: %llu)\n", (unsigned long long)entry_size);
                            // Parse Audio element to get channels
                            long audio_content_end = entry_content_start_pos + entry_size;
                            int channels = 1; // Default to mono

                            while (ftell(f) < audio_content_end) {
                                // Read Audio sub-element ID
                                uint32_t audio_id = read_ebml_id(f, &id_size); // Read raw audio ID
                                 if (id_size == 0) { if (DEBUG_WEBM) fprintf(stderr, "Error or EOF reading Audio sub-ID at pos %ld\n", ftell(f)); break; }

                                // Read Audio sub-element Size
                                uint64_t audio_size = read_vint(f, &size_bytes);
                                 if (size_bytes == 0) { if (DEBUG_WEBM) fprintf(stderr, "Error or EOF reading Audio sub-size at pos %ld for ID 0x%X\n", ftell(f), audio_id); break; }

                                long audio_sub_content_start_pos = ftell(f);

                                if (audio_id == WEBM_ID_CHANNELS) { // Compare raw ID
                                     // Use %llu for uint64_t audio_size
                                    if (DEBUG_WEBM) printf("Found Channels element (size: %llu)\n", (unsigned long long)audio_size);
                                    // Read channels
                                    unsigned char chan_buf[2];
                                    if (audio_size <= 2 && fread(chan_buf, 1, audio_size, f) == audio_size) {
                                        channels = 0;
                                        for (int i = 0; i < audio_size; i++) {
                                            channels = (channels << 8) | chan_buf[i];
                                        }
                                    } else {
                                        // Skip if channel read failed
                                        skip_element(f, audio_size);
                                    }
                                } else {
                                    // Skip unknown audio sub-element
                                     // Use %llu for uint64_t audio_size
                                    if (DEBUG_WEBM) printf("Skipping unknown Audio sub-element: 0x%X (size: %llu)\n", audio_id, (unsigned long long)audio_size);
                                    skip_element(f, audio_size);
                                }
                                // Ensure position after Audio sub-element
                                fseek(f, audio_sub_content_start_pos + audio_size, SEEK_SET);
                            } // End while Audio sub-elements

                            // Update context with channels
                            if (is_opus && channels > 0) { // Ensure channels were read correctly
                                ctx->channels = channels;
                                if (DEBUG_WEBM) {
                                    printf("Opus track has %d channels\n", channels);
                                }
                            }
                        } else {
                                 // Use %llu for uint64_t entry_size
                                if (DEBUG_WEBM) printf("Skipping unknown TrackEntry element: 0x%X (size: %llu)\n", entry_id, (unsigned long long)entry_size);
                                skip_element(f, entry_size);
                        }
                        // Ensure position after TrackEntry sub-element
                        fseek(f, entry_content_start_pos + entry_size, SEEK_SET);
                    } // End while TrackEntry sub-elements

                    // If this is an Opus audio track, save the track number
                    if (is_audio && is_opus && track_number > 0) {
                        opus_track_number = track_number;
                        found_opus_track = 1;
                        
                        if (DEBUG_WEBM) {
                            printf("Found Opus audio track: %d\n", opus_track_number);
                        }
                    }
                } else {
                    // Skip unknown element within Tracks
                    if (DEBUG_WEBM) printf("Skipping unknown Tracks sub-element: 0x%X (size: %llu)\n", track_id, (unsigned long long)track_size);
                    skip_element(f, track_size);
                }
                
                // If we've already found the track, we can skip the rest
                if (found_opus_track) {
                    break;
                }
            }
            // Ensure we are at the end of the Tracks element
            fseek(f, element_content_start_pos + element_size, SEEK_SET);

        } else if (id == WEBM_ID_CLUSTER && found_opus_track) {
            // Parse Cluster to find SimpleBlocks with Opus data
            long cluster_content_end = element_content_start_pos + element_size;

             // Use %llu for uint64_t element_size
            if (DEBUG_WEBM) printf("Parsing Cluster at offset %ld (size: %llu)\n", element_content_start_pos - id_size - size_bytes, (unsigned long long)element_size);

            while (ftell(f) < cluster_content_end) {
                // Read Cluster sub-element ID
                uint32_t cluster_id = read_ebml_id(f, &id_size); // Read raw cluster ID
                 if (id_size == 0) { if (DEBUG_WEBM) fprintf(stderr, "Error or EOF reading Cluster sub-ID at pos %ld\n", ftell(f)); break; }

                // Read Cluster sub-element Size
                uint64_t cluster_size = read_vint(f, &size_bytes);
                 if (size_bytes == 0) { if (DEBUG_WEBM) fprintf(stderr, "Error or EOF reading Cluster sub-size at pos %ld for ID 0x%X\n", ftell(f), cluster_id); break; }

                long cluster_content_start_pos = ftell(f);

                if (cluster_id == WEBM_ID_SIMPLEBLOCK) { // Compare raw ID
                    // This is a SimpleBlock, check if it contains Opus data
                     // Use %llu for uint64_t cluster_size
                    if (DEBUG_WEBM) printf("Found SimpleBlock (size: %llu) at offset %ld\n",
                           (unsigned long long)cluster_size, cluster_content_start_pos - id_size - size_bytes);

                    // Read SimpleBlock header (track number + timecode + flags)
                    unsigned char block_header[4];
                    if (cluster_size >= 4 && fread(block_header, 1, 4, f) == 4) {
                        int block_track = block_header[0] & 0x7F;

                        if (block_track == opus_track_number) {
                            // This block belongs to our track, read the rest of the data
                            int opus_data_size = cluster_size - 4;
                            unsigned char *opus_data = malloc(opus_data_size);
                            if (opus_data) {
                                if (fread(opus_data, 1, opus_data_size, f) == opus_data_size) {
                                    // Combine header and data for packet extraction function
                                    unsigned char *full_block_data = malloc(cluster_size);
                                    if (full_block_data) {
                                        memcpy(full_block_data, block_header, 4);
                                        memcpy(full_block_data + 4, opus_data, opus_data_size);

                                        if (extract_opus_packets_from_block(buffer, full_block_data, cluster_size, opus_track_number) == 0) {
                                            // Packet added successfully
                                        } else {
                                            fprintf(stderr, "Failed to add Opus packet from SimpleBlock\n");
                                        }
                                        free(full_block_data);
                                    } else {
                                         fprintf(stderr, "Failed to allocate memory for full SimpleBlock data\n");
                                    }
                                } else {
                                    fprintf(stderr, "Failed to read SimpleBlock Opus data (payload size %d)\n", opus_data_size);
                                }
                                free(opus_data);
                            } else {
                                fprintf(stderr, "Failed to allocate memory for SimpleBlock Opus data\n");
                                skip_element(f, opus_data_size); // Skip rest of block payload
                            }
                        } else {
                            // Block belongs to a different track, skip its payload
                            skip_element(f, cluster_size - 4);
                        }
                    } else {
                        // Failed to read block header or block too small
                         // Use %llu for uint64_t cluster_size
                        fprintf(stderr, "Failed to read SimpleBlock header or block too small (size %llu)\n", (unsigned long long)cluster_size);
                        // Seek to end of element based on size read earlier
                        fseek(f, cluster_content_start_pos + cluster_size, SEEK_SET);
                    }
                } else if (cluster_id == WEBM_ID_TIMECODE) { // Compare raw ID
                    // Timecode element - just skip it
                     // Use %llu for uint64_t cluster_size
                    if (DEBUG_WEBM) printf("Found Timecode element (size: %llu)\n", (unsigned long long)cluster_size);
                    skip_element(f, cluster_size);
                } else {
                    // Unknown element - skip it
                     // Use %llu for uint64_t cluster_size
                    if (DEBUG_WEBM) printf("Skipping unknown Cluster element ID: 0x%X (size: %llu)\n",
                           cluster_id, (unsigned long long)cluster_size);
                    skip_element(f, cluster_size);
                }
                // Ensure position after Cluster sub-element by seeking to its end
                fseek(f, cluster_content_start_pos + cluster_size, SEEK_SET);
            } // End while Cluster sub-elements

            // Ensure we are at the end of the Cluster element
             if (ftell(f) != cluster_content_end) {
                 if (DEBUG_WEBM) printf("Correcting file position after Cluster element\n");
                 fseek(f, cluster_content_end, SEEK_SET);
             }
        } else {
            // Skip unknown top-level element within Segment
             // Use %llu for uint64_t element_size
            if (DEBUG_WEBM) printf("Skipping unknown Segment element ID: 0x%X (size: %llu)\n", id, (unsigned long long)element_size);
            skip_element(f, element_size);
        }

        // Ensure we are positioned correctly for the next top-level element
        // This handles cases where skip_element was used or parsing a sub-element finished early
        long expected_end_pos = element_content_start_pos + element_size;
        if (ftell(f) != expected_end_pos) {
             if (DEBUG_WEBM) printf("Correcting file position after segment element 0x%X. Current: %ld, Expected: %ld\n",
                    id, ftell(f), expected_end_pos);
             fseek(f, expected_end_pos, SEEK_SET);
        }

        // Check for infinite loop / no progress (should not happen with fseek correction)
        if (ftell(f) <= loop_start_pos) {
            if (DEBUG_WEBM) fprintf(stderr, "Parser stuck at position %ld. Aborting.\n", loop_start_pos);
            break;
        }

    } // End while Segment elements

    // --- Fallback pattern matching section ---
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
    // Allocate buffer large enough for max frame size and max channels
    // Use OPUS_MAX_FRAME_SIZE (now 1920 for 16kHz)
    int16_t pcm_buffer[OPUS_MAX_FRAME_SIZE * OPUS_MAX_CHANNELS]; 
    int samples;
    int total_samples = 0;
    
    // Decode each packet
    while (buffer->current < buffer->count) {
        OpusPacket *packet = &buffer->packets[buffer->current];
        
        // Decode the packet, passing the max possible frame size for 16kHz
        samples = opus_decode(ctx->decoder, packet->data, packet->size, 
                             pcm_buffer, OPUS_MAX_FRAME_SIZE, 0);
        
        if (samples > 0) {
            total_samples += samples;
            
            if (DEBUG_WEBM && buffer->current % 50 == 0) { // Print less often
                printf("Decoded %d samples (%dHz) from packet %d/%d (size %d)\n", 
                       samples, ctx->sample_rate, buffer->current + 1, buffer->count, packet->size);
            }
            
            // Send PCM data to socket
            int bytes_to_send = samples * ctx->channels * sizeof(int16_t);
            if (send(sockfd, pcm_buffer, bytes_to_send, 0) != bytes_to_send) {
                perror("Failed to send PCM data");
                return -1;
            }
        } else {
            // Opus decoding error
            fprintf(stderr, "Failed to decode packet %d: %s\n", 
                    buffer->current + 1, opus_strerror(samples));
            // Decide if we should continue or stop on decoding error
            // For now, let's continue but log the error
        }
        
        // Move to next packet
        buffer->current++;
    }
    
    printf("Decoded %d total samples (%dHz) from %d packets.\n", total_samples, ctx->sample_rate, buffer->count);
    
    return 0;
}
