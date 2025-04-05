#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <ctype.h>
#include "webm_opus.h"
#include "playback.h"

// Improved WebM parser with better debugging and more robust parsing

// EBML and WebM element IDs
#define EBML_ID_HEADER         0x1A45DFA3
#define EBML_ID_VERSION        0x4286
#define EBML_ID_READVERSION    0x42F7
#define EBML_ID_MAXIDLENGTH    0x42F2
#define EBML_ID_MAXSIZELENGTH  0x42F3
#define EBML_ID_DOCTYPE        0x4282
#define EBML_ID_DOCTYPEVERSION 0x4287
#define EBML_ID_DOCTYPEREADVERSION 0x4285

#define WEBM_ID_SEGMENT        0x18538067
#define WEBM_ID_INFO           0x1549A966
#define WEBM_ID_TRACKS         0x1654AE6B
#define WEBM_ID_TRACKENTRY     0xAE
#define WEBM_ID_TRACKNUMBER    0xD7
#define WEBM_ID_TRACKUID       0x73C5
#define WEBM_ID_TRACKTYPE      0x83
#define WEBM_ID_CODECID        0x86
#define WEBM_ID_CODECPRIVATE   0x63A2
#define WEBM_ID_AUDIO          0xE1
#define WEBM_ID_SAMPLINGFREQ   0xB5
#define WEBM_ID_CHANNELS       0x9F
#define WEBM_ID_BITDEPTH       0x6264

#define WEBM_ID_CLUSTER        0x1F43B675
#define WEBM_ID_TIMECODE       0xE7
#define WEBM_ID_SIMPLEBLOCK    0xA3
#define WEBM_ID_BLOCK          0xA1
#define WEBM_ID_BLOCKGROUP     0xA0

#define TRACK_TYPE_AUDIO       0x02

// Debug flag - set to 1 to enable verbose debugging
#define DEBUG_WEBM 1

// Helper function to print element ID for debugging
static void print_element_id(uint64_t id) {
    if (!DEBUG_WEBM) return;

    printf("Element ID: 0x%lX - ", id);
    switch (id) {
        case EBML_ID_HEADER: printf("EBML_HEADER"); break;
        case WEBM_ID_SEGMENT: printf("SEGMENT"); break;
        case WEBM_ID_INFO: printf("INFO"); break;
        case WEBM_ID_TRACKS: printf("TRACKS"); break;
        case WEBM_ID_TRACKENTRY: printf("TRACKENTRY"); break;
        case WEBM_ID_TRACKNUMBER: printf("TRACKNUMBER"); break;
        case WEBM_ID_TRACKTYPE: printf("TRACKTYPE"); break;
        case WEBM_ID_CODECID: printf("CODECID"); break;
        case WEBM_ID_AUDIO: printf("AUDIO"); break;
        case WEBM_ID_CHANNELS: printf("CHANNELS"); break;
        case WEBM_ID_SAMPLINGFREQ: printf("SAMPLINGFREQ"); break;
        case WEBM_ID_CLUSTER: printf("CLUSTER"); break;
        case WEBM_ID_TIMECODE: printf("TIMECODE"); break;
        case WEBM_ID_SIMPLEBLOCK: printf("SIMPLEBLOCK"); break;
        case WEBM_ID_BLOCK: printf("BLOCK"); break;
        case WEBM_ID_BLOCKGROUP: printf("BLOCKGROUP"); break;
        default: printf("UNKNOWN"); break;
    }
    printf("\n");
}

// Helper function to read a variable-length integer (EBML format)
static uint64_t read_vint(FILE *f, int *size) {
    uint8_t first_byte;
    if (fread(&first_byte, 1, 1, f) != 1) {
        if (DEBUG_WEBM) {
            printf("read_vint: Failed to read first byte\n");
        }
        *size = 0;
        return 0;
    }

    if (DEBUG_WEBM) {
        printf("read_vint: First byte: 0x%02X\n", first_byte);
    }

    int length = 0;
    uint8_t mask = 0x80;

    while (!(first_byte & mask) && mask) {
        length++;
        mask >>= 1;
    }

    if (DEBUG_WEBM) {
        printf("read_vint: Detected length: %d\n", length + 1);
    }

    if (length > 7) {
        if (DEBUG_WEBM) {
            printf("read_vint: Invalid length: %d\n", length);
        }
        *size = 0;
        return 0;
    }

    uint64_t value = first_byte & (0xFF >> (length + 1));
    length++;

    if (DEBUG_WEBM) {
        printf("read_vint: Initial value: 0x%lX\n", value);
    }

    for (int i = 1; i < length; i++) {
        uint8_t next_byte;
        if (fread(&next_byte, 1, 1, f) != 1) {
            if (DEBUG_WEBM) {
                printf("read_vint: Failed to read byte %d\n", i);
            }
            *size = 0;
            return 0;
        }
        value = (value << 8) | next_byte;

        if (DEBUG_WEBM) {
            printf("read_vint: After byte %d: 0x%lX\n", i, value);
        }
    }

    *size = length;
    return value;
}

// Helper function to read a fixed-size unsigned integer
static uint64_t read_uint(FILE *f, int size) {
    uint64_t value = 0;
    uint8_t byte;

    for (int i = 0; i < size; i++) {
        if (fread(&byte, 1, 1, f) != 1) {
            return 0;
        }
        value = (value << 8) | byte;
    }

    return value;
}

// Helper function to read a string
static char* read_string(FILE *f, uint64_t size) {
    char *str = (char*)malloc(size + 1);
    if (!str) return NULL;

    if (fread(str, 1, size, f) != size) {
        free(str);
        return NULL;
    }

    str[size] = '\0';
    return str;
}

// Helper function to dump binary data for debugging
static void dump_binary(const uint8_t *data, int size) {
    if (!DEBUG_WEBM) return;

    printf("Binary data (%d bytes): ", size);
    for (int i = 0; i < size && i < 16; i++) {
        printf("%02X ", data[i]);
    }
    if (size > 16) printf("...");

    printf(" | ");
    for (int i = 0; i < size && i < 16; i++) {
        printf("%c", isprint(data[i]) ? data[i] : '.');
    }
    printf("\n");
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

// Open and parse WebM file
int open_webm_file(OpusContext *ctx, const char *filename) {
    ctx->webm_file = fopen(filename, "rb");
    if (!ctx->webm_file) {
        perror("Failed to open WebM file");
        return -1;
    }

    printf("Parsing WebM file: %s\n", filename);

    // Dump file header for debugging
    dump_file_header(ctx->webm_file);

    // Parse EBML header and WebM structure
    int size;
    uint64_t id, length;
    int found_opus = 0;
    int track_number = -1;
    int channels = 1;  // Default to mono

    // First, look for the EBML header
    id = read_vint(ctx->webm_file, &size);
    if (size == 0) {
        fprintf(stderr, "Failed to read EBML header ID (size=0)\n");
        fclose(ctx->webm_file);
        ctx->webm_file = NULL;
        return -1;
    }

    printf("First element ID: 0x%lX (expected EBML header: 0x1A45DFA3)\n", id);

    if (id != EBML_ID_HEADER) {
        fprintf(stderr, "Not a valid EBML file (no EBML header, found ID: 0x%lX)\n", id);

        // Try a different approach - some files might have a different format
        // Reset to beginning of file
        fseek(ctx->webm_file, 0, SEEK_SET);

        // Check for WebM signature manually
        unsigned char signature[4];
        if (fread(signature, 1, 4, ctx->webm_file) == 4) {
            printf("First 4 bytes: %02X %02X %02X %02X\n",
                   signature[0], signature[1], signature[2], signature[3]);

            // Check if it matches the EBML header start (0x1A 0x45 0xDF 0xA3)
            if (signature[0] == 0x1A && signature[1] == 0x45 &&
                signature[2] == 0xDF && signature[3] == 0xA3) {
                printf("Found EBML signature manually\n");
                // Reset to beginning of file for normal parsing
                fseek(ctx->webm_file, 0, SEEK_SET);
                // Try reading the ID again
                id = read_vint(ctx->webm_file, &size);
                if (size == 0 || id != EBML_ID_HEADER) {
                    fprintf(stderr, "Still not a valid EBML file after manual check\n");
                    fclose(ctx->webm_file);
                    ctx->webm_file = NULL;
                    return -1;
                }
            } else {
                // Not a WebM file
                fprintf(stderr, "File does not have a valid EBML signature\n");
                fclose(ctx->webm_file);
                ctx->webm_file = NULL;
                return -1;
            }
        } else {
            fprintf(stderr, "File too small to be a valid WebM file\n");
            fclose(ctx->webm_file);
            ctx->webm_file = NULL;
            return -1;
        }
    }

    print_element_id(id);
    length = read_vint(ctx->webm_file, &size);
    if (size == 0) {
        fprintf(stderr, "Invalid EBML header size\n");
        fclose(ctx->webm_file);
        ctx->webm_file = NULL;
        return -1;
    }

    // Skip the EBML header content
    fseek(ctx->webm_file, length, SEEK_CUR);

    // Now look for the Segment
    id = read_vint(ctx->webm_file, &size);
    if (size == 0 || id != WEBM_ID_SEGMENT) {
        fprintf(stderr, "No Segment element found\n");
        fclose(ctx->webm_file);
        ctx->webm_file = NULL;
        return -1;
    }

    print_element_id(id);
    length = read_vint(ctx->webm_file, &size);
    if (size == 0) {
        fprintf(stderr, "Invalid Segment size\n");
        fclose(ctx->webm_file);
        ctx->webm_file = NULL;
        return -1;
    }

    // The Segment contains all other elements
    long segment_end = ftell(ctx->webm_file) + length;
    if (length == (uint64_t)-1) {
        // Unknown size, just continue parsing
        segment_end = -1;
    }

    // Parse elements within the Segment
    while ((segment_end == -1 || ftell(ctx->webm_file) < segment_end) && !feof(ctx->webm_file)) {
        id = read_vint(ctx->webm_file, &size);
        if (size == 0) break;

        print_element_id(id);
        length = read_vint(ctx->webm_file, &size);
        if (size == 0) break;

        if (id == WEBM_ID_TRACKS) {
            printf("Found Tracks element (size: %lu)\n", length);
            long tracks_end = ftell(ctx->webm_file) + length;

            // Parse TrackEntry elements
            while (ftell(ctx->webm_file) < tracks_end && !feof(ctx->webm_file)) {
                id = read_vint(ctx->webm_file, &size);
                if (size == 0) break;

                print_element_id(id);
                length = read_vint(ctx->webm_file, &size);
                if (size == 0) break;

                if (id == WEBM_ID_TRACKENTRY) {
                    printf("Found TrackEntry (size: %lu)\n", length);
                    long track_end = ftell(ctx->webm_file) + length;
                    int is_audio_track = 0;
                    int current_track_number = -1;
                    char *codec_id = NULL;

                    // Parse elements within TrackEntry
                    while (ftell(ctx->webm_file) < track_end && !feof(ctx->webm_file)) {
                        id = read_vint(ctx->webm_file, &size);
                        if (size == 0) break;

                        print_element_id(id);
                        length = read_vint(ctx->webm_file, &size);
                        if (size == 0) break;

                        if (id == WEBM_ID_TRACKTYPE) {
                            uint64_t track_type = read_uint(ctx->webm_file, length);
                            printf("TrackType: %lu\n", track_type);
                            is_audio_track = (track_type == TRACK_TYPE_AUDIO);
                        } else if (id == WEBM_ID_TRACKNUMBER) {
                            current_track_number = read_uint(ctx->webm_file, length);
                            printf("TrackNumber: %d\n", current_track_number);
                        } else if (id == WEBM_ID_CODECID) {
                            codec_id = read_string(ctx->webm_file, length);
                            printf("CodecID: %s\n", codec_id ? codec_id : "NULL");
                        } else if (id == WEBM_ID_AUDIO) {
                            printf("Found Audio element (size: %lu)\n", length);
                            long audio_end = ftell(ctx->webm_file) + length;

                            // Parse Audio elements
                            while (ftell(ctx->webm_file) < audio_end && !feof(ctx->webm_file)) {
                                id = read_vint(ctx->webm_file, &size);
                                if (size == 0) break;

                                print_element_id(id);
                                length = read_vint(ctx->webm_file, &size);
                                if (size == 0) break;

                                if (id == WEBM_ID_CHANNELS) {
                                    channels = read_uint(ctx->webm_file, length);
                                    printf("Channels: %d\n", channels);
                                } else if (id == WEBM_ID_SAMPLINGFREQ) {
                                    // Read as double (IEEE-754 floating point)
                                    // For simplicity, we'll just skip it
                                    fseek(ctx->webm_file, length, SEEK_CUR);
                                } else {
                                    // Skip unknown elements
                                    fseek(ctx->webm_file, length, SEEK_CUR);
                                }
                            }

                            // Make sure we're at the end of the Audio element
                            fseek(ctx->webm_file, audio_end, SEEK_SET);
                        } else {
                            // Skip unknown elements
                            fseek(ctx->webm_file, length, SEEK_CUR);
                        }
                    }

                    // Check if this is an Opus audio track
                    if (is_audio_track && codec_id) {
                        // Check for both "A_OPUS" and "V_OPUS" since some files might use different prefixes
                        if (strstr(codec_id, "OPUS") || strstr(codec_id, "opus")) {
                            printf("Found Opus audio track (TrackNumber: %d, Channels: %d)\n",
                                   current_track_number, channels);
                            found_opus = 1;
                            track_number = current_track_number;
                        }
                    }

                    if (codec_id) {
                        free(codec_id);
                    }

                    // Make sure we're at the end of the TrackEntry
                    fseek(ctx->webm_file, track_end, SEEK_SET);

                    // If we found an Opus track, we can stop looking
                    if (found_opus) {
                        break;
                    }
                } else {
                    // Skip unknown elements
                    fseek(ctx->webm_file, length, SEEK_CUR);
                }
            }

            // Make sure we're at the end of the Tracks element
            fseek(ctx->webm_file, tracks_end, SEEK_SET);

            // If we found an Opus track, we can initialize the decoder
            if (found_opus) {
                printf("Initializing Opus decoder for track %d with %d channels\n",
                       track_number, channels);
                if (init_opus_decoder(ctx, channels) != 0) {
                    fprintf(stderr, "Failed to initialize Opus decoder\n");
                    fclose(ctx->webm_file);
                    ctx->webm_file = NULL;
                    return -1;
                }

                // Store the track number for later use
                ctx->track_number = track_number;

                // Seek to beginning of file to start decoding
                fseek(ctx->webm_file, 0, SEEK_SET);
                return 0;
            }
        } else if (id == WEBM_ID_CLUSTER) {
            // We've reached the clusters without finding an Opus track
            // No need to continue parsing
            break;
        } else {
            // Skip unknown elements
            fseek(ctx->webm_file, length, SEEK_CUR);
        }
    }

    // Last resort: try to scan the file for Opus signature
    printf("Trying fallback method: scanning for Opus signature...\n");

    // Reset to beginning of file
    fseek(ctx->webm_file, 0, SEEK_SET);

    // Buffer for scanning
    unsigned char buffer[4096];
    size_t bytes_read;
    int found_opus_signature = 0;

    // Look for "OpusHead" or "OpusTags" markers
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), ctx->webm_file)) > 0) {
        for (size_t i = 0; i < bytes_read - 8; i++) {
            if ((memcmp(buffer + i, "OpusHead", 8) == 0) ||
                (memcmp(buffer + i, "OpusTags", 8) == 0)) {
                printf("Found Opus signature at offset %ld\n", ftell(ctx->webm_file) - bytes_read + i);
                found_opus_signature = 1;
                break;
            }
        }

        if (found_opus_signature) {
            break;
        }
    }

    if (found_opus_signature) {
        printf("File contains Opus data but WebM structure is not standard\n");
        printf("Attempting to initialize decoder with default parameters\n");

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
        return 0;
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

    // Skip EBML header
    int size;
    uint64_t id, length;

    // Find EBML header
    id = read_vint(ctx->webm_file, &size);
    if (size == 0 || id != EBML_ID_HEADER) {
        fprintf(stderr, "Not a valid EBML file (no EBML header)\n");
        return -1;
    }

    length = read_vint(ctx->webm_file, &size);
    if (size == 0) {
        fprintf(stderr, "Invalid EBML header size\n");
        return -1;
    }

    // Skip the EBML header content
    fseek(ctx->webm_file, length, SEEK_CUR);

    // Find Segment
    id = read_vint(ctx->webm_file, &size);
    if (size == 0 || id != WEBM_ID_SEGMENT) {
        fprintf(stderr, "No Segment element found\n");
        return -1;
    }

    length = read_vint(ctx->webm_file, &size);
    if (size == 0) {
        fprintf(stderr, "Invalid Segment size\n");
        return -1;
    }

    // The Segment contains all other elements
    long segment_end = ftell(ctx->webm_file) + length;
    if (length == (uint64_t)-1) {
        // Unknown size, just continue parsing
        segment_end = -1;
    }

    // Skip to the first Cluster
    while ((segment_end == -1 || ftell(ctx->webm_file) < segment_end) && !feof(ctx->webm_file)) {
        id = read_vint(ctx->webm_file, &size);
        if (size == 0) break;

        length = read_vint(ctx->webm_file, &size);
        if (size == 0) break;

        if (id == WEBM_ID_CLUSTER) {
            // Found a Cluster, start decoding
            break;
        } else {
            // Skip non-Cluster elements
            fseek(ctx->webm_file, length, SEEK_CUR);
        }
    }

    // Process Clusters
    while ((segment_end == -1 || ftell(ctx->webm_file) < segment_end) && !feof(ctx->webm_file)) {
        // Read Cluster ID
        id = read_vint(ctx->webm_file, &size);
        if (size == 0) break;

        length = read_vint(ctx->webm_file, &size);
        if (size == 0) break;

        if (id != WEBM_ID_CLUSTER) {
            // Skip non-Cluster elements
            fseek(ctx->webm_file, length, SEEK_CUR);
            continue;
        }

        printf("Processing Cluster (size: %lu)\n", length);
        long cluster_end = ftell(ctx->webm_file) + length;

        // Process elements within the Cluster
        while (ftell(ctx->webm_file) < cluster_end && !feof(ctx->webm_file)) {
            id = read_vint(ctx->webm_file, &size);
            if (size == 0) break;

            length = read_vint(ctx->webm_file, &size);
            if (size == 0) break;

            if (id == WEBM_ID_SIMPLEBLOCK) {
                // Process SimpleBlock
                uint8_t track_id;
                if (fread(&track_id, 1, 1, ctx->webm_file) != 1) break;

                // Extract track number (remove the most significant bit)
                track_id &= 0x7F;

                // Check if this block belongs to our Opus track
                if (track_id == ctx->track_number) {
                    // Skip timestamp (2 bytes)
                    uint8_t timestamp[2];
                    if (fread(timestamp, 1, 2, ctx->webm_file) != 2) break;

                    // Read flags
                    uint8_t flags;
                    if (fread(&flags, 1, 1, ctx->webm_file) != 1) break;

                    // Calculate Opus packet size (SimpleBlock size - 4 bytes for header)
                    int opus_size = length - 4;
                    if (opus_size > OPUS_MAX_PACKET_SIZE) {
                        fprintf(stderr, "Opus packet too large: %d bytes\n", opus_size);
                        fseek(ctx->webm_file, opus_size, SEEK_CUR);
                        continue;
                    }

                    // Read Opus packet
                    if (fread(opus_packet, 1, opus_size, ctx->webm_file) != opus_size) {
                        fprintf(stderr, "Failed to read Opus packet\n");
                        break;
                    }

                    if (DEBUG_WEBM) {
                        printf("Decoding Opus packet: %d bytes\n", opus_size);
                        dump_binary(opus_packet, opus_size);
                    }

                    // Decode Opus packet to PCM
                    int samples = opus_decode(ctx->decoder, opus_packet, opus_size,
                                             pcm_buffer, OPUS_FRAME_SIZE, 0);

                    if (samples > 0) {
                        if (DEBUG_WEBM) {
                            printf("Decoded %d samples\n", samples);
                        }

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
                    } else {
                        fprintf(stderr, "Failed to decode Opus packet: %s\n",
                                opus_strerror(samples));
                    }
                } else {
                    // Skip blocks for other tracks
                    fseek(ctx->webm_file, length - 1, SEEK_CUR); // -1 because we already read the track ID
                }
            } else if (id == WEBM_ID_BLOCKGROUP) {
                // BlockGroup contains a Block element and other elements
                long blockgroup_end = ftell(ctx->webm_file) + length;

                // Process elements within the BlockGroup
                while (ftell(ctx->webm_file) < blockgroup_end && !feof(ctx->webm_file)) {
                    id = read_vint(ctx->webm_file, &size);
                    if (size == 0) break;

                    length = read_vint(ctx->webm_file, &size);
                    if (size == 0) break;

                    if (id == WEBM_ID_BLOCK) {
                        // Process Block (similar to SimpleBlock)
                        uint8_t track_id;
                        if (fread(&track_id, 1, 1, ctx->webm_file) != 1) break;

                        // Extract track number (remove the most significant bit)
                        track_id &= 0x7F;

                        // Check if this block belongs to our Opus track
                        if (track_id == ctx->track_number) {
                            // Skip timestamp (2 bytes)
                            uint8_t timestamp[2];
                            if (fread(timestamp, 1, 2, ctx->webm_file) != 2) break;

                            // Read flags
                            uint8_t flags;
                            if (fread(&flags, 1, 1, ctx->webm_file) != 1) break;

                            // Calculate Opus packet size (Block size - 4 bytes for header)
                            int opus_size = length - 4;
                            if (opus_size > OPUS_MAX_PACKET_SIZE) {
                                fprintf(stderr, "Opus packet too large: %d bytes\n", opus_size);
                                fseek(ctx->webm_file, opus_size, SEEK_CUR);
                                continue;
                            }

                            // Read Opus packet
                            if (fread(opus_packet, 1, opus_size, ctx->webm_file) != opus_size) {
                                fprintf(stderr, "Failed to read Opus packet\n");
                                break;
                            }

                            if (DEBUG_WEBM) {
                                printf("Decoding Opus packet: %d bytes\n", opus_size);
                                dump_binary(opus_packet, opus_size);
                            }

                            // Decode Opus packet to PCM
                            int samples = opus_decode(ctx->decoder, opus_packet, opus_size,
                                                     pcm_buffer, OPUS_FRAME_SIZE, 0);

                            if (samples > 0) {
                                if (DEBUG_WEBM) {
                                    printf("Decoded %d samples\n", samples);
                                }

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
                            } else {
                                fprintf(stderr, "Failed to decode Opus packet: %s\n",
                                        opus_strerror(samples));
                            }
                        } else {
                            // Skip blocks for other tracks
                            fseek(ctx->webm_file, length - 1, SEEK_CUR); // -1 because we already read the track ID
                        }
                    } else {
                        // Skip other elements in BlockGroup
                        fseek(ctx->webm_file, length, SEEK_CUR);
                    }
                }

                // Make sure we're at the end of the BlockGroup
                fseek(ctx->webm_file, blockgroup_end, SEEK_SET);
            } else {
                // Skip other elements in Cluster
                fseek(ctx->webm_file, length, SEEK_CUR);
            }
        }

        // Make sure we're at the end of the Cluster
        fseek(ctx->webm_file, cluster_end, SEEK_SET);
    }

    printf("[INFO] Finished decoding WebM/Opus file\n");
    return 0;
}