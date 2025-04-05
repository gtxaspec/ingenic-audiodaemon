#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <ctype.h>
#include "webm_opus.h"
#include "playback.h"

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

// Direct approach to read a 4-byte ID
static uint32_t read_id(FILE *f) {
    uint8_t bytes[4];
    if (fread(bytes, 1, 4, f) != 4) {
        return 0;
    }
    
    return (bytes[0] << 24) | (bytes[1] << 16) | (bytes[2] << 8) | bytes[3];
}

// Direct approach to read a 2-byte ID
static uint16_t read_short_id(FILE *f) {
    uint8_t bytes[2];
    if (fread(bytes, 1, 2, f) != 2) {
        return 0;
    }
    
    return (bytes[0] << 8) | bytes[1];
}

// Direct approach to read a 1-byte ID
static uint8_t read_byte_id(FILE *f) {
    uint8_t byte;
    if (fread(&byte, 1, 1, f) != 1) {
        return 0;
    }
    
    return byte;
}

// Direct approach to read a variable-length size field
static uint64_t read_size(FILE *f, int *bytes_read) {
    uint8_t first_byte;
    if (fread(&first_byte, 1, 1, f) != 1) {
        *bytes_read = 0;
        return 0;
    }
    
    int length = 1;
    uint8_t mask = 0x80;
    
    // Count leading zeros
    while (!(first_byte & mask) && mask) {
        length++;
        mask >>= 1;
    }
    
    if (length > 8) {
        // Invalid length
        *bytes_read = 0;
        return 0;
    }
    
    // First byte with mask bit cleared
    uint64_t size = first_byte & (0xFF >> length);
    
    // Read remaining bytes
    for (int i = 1; i < length; i++) {
        uint8_t next_byte;
        if (fread(&next_byte, 1, 1, f) != 1) {
            *bytes_read = 0;
            return 0;
        }
        size = (size << 8) | next_byte;
    }
    
    *bytes_read = length;
    return size;
}

// Skip an element with known size
static void skip_element(FILE *f, uint64_t size) {
    fseek(f, size, SEEK_CUR);
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

// Open and parse WebM file using a direct approach
int open_webm_file(OpusContext *ctx, const char *filename) {
    ctx->webm_file = fopen(filename, "rb");
    if (!ctx->webm_file) {
        perror("Failed to open WebM file");
        return -1;
    }
    
    printf("Parsing WebM file: %s\n", filename);
    
    // Dump file header for debugging
    dump_file_header(ctx->webm_file);
    
    // Variables for parsing
    int found_opus = 0;
    int track_number = -1;
    int channels = 1;  // Default to mono
    
    // Check for EBML header directly
    uint32_t id = read_id(ctx->webm_file);
    if (id != EBML_ID_HEADER) {
        fprintf(stderr, "Not a valid WebM file (EBML header not found, ID: 0x%X)\n", id);
        fclose(ctx->webm_file);
        ctx->webm_file = NULL;
        return -1;
    }
    
    if (DEBUG_WEBM) {
        printf("Found EBML header (0x%X)\n", id);
    }
    
    // Read EBML header size
    int size_bytes;
    uint64_t header_size = read_size(ctx->webm_file, &size_bytes);
    if (size_bytes == 0) {
        fprintf(stderr, "Invalid EBML header size\n");
        fclose(ctx->webm_file);
        ctx->webm_file = NULL;
        return -1;
    }
    
    if (DEBUG_WEBM) {
        printf("EBML header size: %lu bytes\n", header_size);
    }
    
    // Skip EBML header content
    skip_element(ctx->webm_file, header_size);
    
    // Look for Segment
    id = read_id(ctx->webm_file);
    if (id != WEBM_ID_SEGMENT) {
        fprintf(stderr, "No Segment element found (found ID: 0x%X)\n", id);
        fclose(ctx->webm_file);
        ctx->webm_file = NULL;
        return -1;
    }
    
    if (DEBUG_WEBM) {
        printf("Found Segment (0x%X)\n", id);
    }
    
    // Read Segment size
    uint64_t segment_size = read_size(ctx->webm_file, &size_bytes);
    if (size_bytes == 0) {
        fprintf(stderr, "Invalid Segment size\n");
        fclose(ctx->webm_file);
        ctx->webm_file = NULL;
        return -1;
    }
    
    if (DEBUG_WEBM) {
        printf("Segment size: %lu bytes\n", segment_size);
    }
    
    // Parse elements within Segment
    long segment_end = ftell(ctx->webm_file) + segment_size;
    if (segment_size == (uint64_t)-1) {
        // Unknown size, just continue parsing
        segment_end = -1;
    }
    
    // Search for Tracks element
    while ((segment_end == -1 || ftell(ctx->webm_file) < segment_end) && !feof(ctx->webm_file)) {
        id = read_id(ctx->webm_file);
        if (id == 0) break;
        
        if (DEBUG_WEBM) {
            printf("Found element ID: 0x%X\n", id);
        }
        
        uint64_t element_size = read_size(ctx->webm_file, &size_bytes);
        if (size_bytes == 0) break;
        
        if (DEBUG_WEBM) {
            printf("Element size: %lu bytes\n", element_size);
        }
        
        if (id == WEBM_ID_TRACKS) {
            if (DEBUG_WEBM) {
                printf("Found Tracks element\n");
            }
            
            long tracks_end = ftell(ctx->webm_file) + element_size;
            
            // Parse TrackEntry elements
            while (ftell(ctx->webm_file) < tracks_end && !feof(ctx->webm_file)) {
                // TrackEntry can have different ID lengths
                long pos = ftell(ctx->webm_file);
                uint8_t peek;
                if (fread(&peek, 1, 1, ctx->webm_file) != 1) break;
                fseek(ctx->webm_file, pos, SEEK_SET);
                
                uint64_t track_id;
                if (peek == 0xAE) {
                    // Single byte ID
                    track_id = read_byte_id(ctx->webm_file);
                } else {
                    // Try 4-byte ID
                    track_id = read_id(ctx->webm_file);
                }
                
                if (track_id == 0) break;
                
                if (DEBUG_WEBM) {
                    printf("Track element ID: 0x%lX\n", track_id);
                }
                
                if (track_id != WEBM_ID_TRACKENTRY) {
                    // Skip non-track elements
                    uint64_t skip_size = read_size(ctx->webm_file, &size_bytes);
                    if (size_bytes == 0) break;
                    skip_element(ctx->webm_file, skip_size);
                    continue;
                }
                
                if (DEBUG_WEBM) {
                    printf("Found TrackEntry\n");
                }
                
                uint64_t track_size = read_size(ctx->webm_file, &size_bytes);
                if (size_bytes == 0) break;
                
                long track_end = ftell(ctx->webm_file) + track_size;
                int is_audio_track = 0;
                int current_track_number = -1;
                char codec_id[32] = {0};
                
                // Parse elements within TrackEntry
                while (ftell(ctx->webm_file) < track_end && !feof(ctx->webm_file)) {
                    // Try different ID lengths
                    pos = ftell(ctx->webm_file);
                    if (fread(&peek, 1, 1, ctx->webm_file) != 1) break;
                    fseek(ctx->webm_file, pos, SEEK_SET);
                    
                    uint64_t elem_id;
                    if (peek < 0x80) {
                        // Single byte ID
                        elem_id = read_byte_id(ctx->webm_file);
                    } else if (peek < 0xE0) {
                        // 2-byte ID
                        elem_id = read_short_id(ctx->webm_file);
                    } else {
                        // 4-byte ID
                        elem_id = read_id(ctx->webm_file);
                    }
                    
                    if (elem_id == 0) break;
                    
                    if (DEBUG_WEBM) {
                        printf("Track element ID: 0x%lX\n", elem_id);
                    }
                    
                    uint64_t elem_size = read_size(ctx->webm_file, &size_bytes);
                    if (size_bytes == 0) break;
                    
                    if (elem_id == WEBM_ID_TRACKTYPE) {
                        uint8_t track_type;
                        if (fread(&track_type, 1, 1, ctx->webm_file) == 1) {
                            is_audio_track = (track_type == TRACK_TYPE_AUDIO);
                            if (DEBUG_WEBM) {
                                printf("TrackType: %d (is audio: %d)\n", track_type, is_audio_track);
                            }
                        } else {
                            skip_element(ctx->webm_file, elem_size);
                        }
                    } else if (elem_id == WEBM_ID_TRACKNUMBER) {
                        uint8_t track_num;
                        if (fread(&track_num, 1, 1, ctx->webm_file) == 1) {
                            current_track_number = track_num;
                            if (DEBUG_WEBM) {
                                printf("TrackNumber: %d\n", current_track_number);
                            }
                        } else {
                            skip_element(ctx->webm_file, elem_size);
                        }
                    } else if (elem_id == WEBM_ID_CODECID) {
                        if (elem_size < sizeof(codec_id)) {
                            if (fread(codec_id, 1, elem_size, ctx->webm_file) == elem_size) {
                                codec_id[elem_size] = '\0';
                                if (DEBUG_WEBM) {
                                    printf("CodecID: %s\n", codec_id);
                                }
                            } else {
                                skip_element(ctx->webm_file, elem_size);
                            }
                        } else {
                            skip_element(ctx->webm_file, elem_size);
                        }
                    } else if (elem_id == WEBM_ID_AUDIO) {
                        if (DEBUG_WEBM) {
                            printf("Found Audio element\n");
                        }
                        
                        long audio_end = ftell(ctx->webm_file) + elem_size;
                        
                        // Parse Audio elements
                        while (ftell(ctx->webm_file) < audio_end && !feof(ctx->webm_file)) {
                            // Try different ID lengths
                            pos = ftell(ctx->webm_file);
                            if (fread(&peek, 1, 1, ctx->webm_file) != 1) break;
                            fseek(ctx->webm_file, pos, SEEK_SET);
                            
                            uint64_t audio_id;
                            if (peek < 0x80) {
                                // Single byte ID
                                audio_id = read_byte_id(ctx->webm_file);
                            } else if (peek < 0xE0) {
                                // 2-byte ID
                                audio_id = read_short_id(ctx->webm_file);
                            } else {
                                // 4-byte ID
                                audio_id = read_id(ctx->webm_file);
                            }
                            
                            if (audio_id == 0) break;
                            
                            uint64_t audio_size = read_size(ctx->webm_file, &size_bytes);
                            if (size_bytes == 0) break;
                            
                            if (audio_id == WEBM_ID_CHANNELS) {
                                uint8_t ch;
                                if (fread(&ch, 1, 1, ctx->webm_file) == 1) {
                                    channels = ch;
                                    if (DEBUG_WEBM) {
                                        printf("Channels: %d\n", channels);
                                    }
                                } else {
                                    skip_element(ctx->webm_file, audio_size);
                                }
                            } else {
                                skip_element(ctx->webm_file, audio_size);
                            }
                        }
                        
                        // Make sure we're at the end of the Audio element
                        fseek(ctx->webm_file, audio_end, SEEK_SET);
                    } else {
                        skip_element(ctx->webm_file, elem_size);
                    }
                }
                
                // Check if this is an Opus audio track
                if (is_audio_track && strlen(codec_id) > 0) {
                    if (strstr(codec_id, "OPUS") || strstr(codec_id, "opus")) {
                        printf("Found Opus audio track (TrackNumber: %d, Channels: %d)\n", 
                               current_track_number, channels);
                        found_opus = 1;
                        track_number = current_track_number;
                    }
                }
                
                // Make sure we're at the end of the TrackEntry
                fseek(ctx->webm_file, track_end, SEEK_SET);
            }
            
            // Make sure we're at the end of the Tracks element
            fseek(ctx->webm_file, tracks_end, SEEK_SET);
            
            // If we found an Opus track, we can stop parsing
            if (found_opus) {
                break;
            }
        } else {
            // Skip unknown elements
            skip_element(ctx->webm_file, element_size);
        }
    }
    
    if (found_opus) {
        printf("Successfully parsed WebM file with Opus audio\n");
        ctx->track_number = track_number;
        
        // Initialize Opus decoder
        if (init_opus_decoder(ctx, channels) != 0) {
            fprintf(stderr, "Failed to initialize Opus decoder\n");
            fclose(ctx->webm_file);
            ctx->webm_file = NULL;
            return -1;
        }
        
        // Seek to beginning of file for decoding
        fseek(ctx->webm_file, 0, SEEK_SET);
        return 0;
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
    if (!ctx->webm_file || !ctx->decoder) {
        fprintf(stderr, "Invalid Opus context\n");
        return -1;
    }
    
    // Seek to beginning of file
    fseek(ctx->webm_file, 0, SEEK_SET);
    
    // Skip EBML header
    int size_bytes;
    uint32_t id = read_id(ctx->webm_file);
    if (id != EBML_ID_HEADER) {
        fprintf(stderr, "Not a valid EBML file (no EBML header)\n");
        return -1;
    }
    
    uint64_t header_size = read_size(ctx->webm_file, &size_bytes);
    if (size_bytes == 0) {
        fprintf(stderr, "Invalid EBML header size\n");
        return -1;
    }
    
    // Skip EBML header content
    skip_element(ctx->webm_file, header_size);
    
    // Find Segment
    id = read_id(ctx->webm_file);
    if (id != WEBM_ID_SEGMENT) {
        fprintf(stderr, "No Segment element found\n");
        return -1;
    }
    
    uint64_t segment_size = read_size(ctx->webm_file, &size_bytes);
    if (size_bytes == 0) {
        fprintf(stderr, "Invalid Segment size\n");
        return -1;
    }
    
    // Variables for decoding
    int16_t pcm_buffer[PCM_BUFFER_SIZE * 2]; // Extra space for stereo
    uint8_t opus_packet[2048];
    int track_number = ctx->track_number;
    int timecode = 0;
    int last_timecode = 0;
    int samples;
    
    // Parse elements within Segment looking for Clusters
    long segment_end = ftell(ctx->webm_file) + segment_size;
    if (segment_size == (uint64_t)-1) {
        // Unknown size, just continue parsing
        segment_end = -1;
    }
    
    while ((segment_end == -1 || ftell(ctx->webm_file) < segment_end) && !feof(ctx->webm_file)) {
        id = read_id(ctx->webm_file);
        if (id == 0) break;
        
        uint64_t element_size = read_size(ctx->webm_file, &size_bytes);
        if (size_bytes == 0) break;
        
        if (id == WEBM_ID_CLUSTER) {
            if (DEBUG_WEBM) {
                printf("Found Cluster\n");
            }
            
            long cluster_end = ftell(ctx->webm_file) + element_size;
            
            // Parse elements within Cluster
            while (ftell(ctx->webm_file) < cluster_end && !feof(ctx->webm_file)) {
                // Try different ID lengths
                long pos = ftell(ctx->webm_file);
                uint8_t peek;
                if (fread(&peek, 1, 1, ctx->webm_file) != 1) break;
                fseek(ctx->webm_file, pos, SEEK_SET);
                
                uint64_t cluster_elem_id;
                if (peek < 0x80) {
                    // Single byte ID
                    cluster_elem_id = read_byte_id(ctx->webm_file);
                } else if (peek < 0xE0) {
                    // 2-byte ID
                    cluster_elem_id = read_short_id(ctx->webm_file);
                } else {
                    // 4-byte ID
                    cluster_elem_id = read_id(ctx->webm_file);
                }
                
                if (cluster_elem_id == 0) break;
                
                uint64_t cluster_elem_size = read_size(ctx->webm_file, &size_bytes);
                if (size_bytes == 0) break;
                
                if (cluster_elem_id == WEBM_ID_TIMECODE) {
                    // Read cluster timecode
                    timecode = read_uint(ctx->webm_file, cluster_elem_size);
                    if (DEBUG_WEBM) {
                        printf("Cluster Timecode: %d\n", timecode);
                    }
                } else if (cluster_elem_id == WEBM_ID_SIMPLEBLOCK) {
                    // Process SimpleBlock
                    long block_start = ftell(ctx->webm_file);
                    
                    // Read track number (variable length)
                    uint64_t block_track = read_size(ctx->webm_file, &size_bytes);
                    if (size_bytes == 0) {
                        skip_element(ctx->webm_file, cluster_elem_size);
                        continue;
                    }
                    
                    // Check if this block belongs to our audio track
                    if (block_track == track_number) {
                        // Read timecode (2 bytes, big-endian, signed)
                        int16_t block_timecode;
                        if (fread(&block_timecode, 2, 1, ctx->webm_file) != 1) {
                            skip_element(ctx->webm_file, cluster_elem_size - (ftell(ctx->webm_file) - block_start));
                            continue;
                        }
                        
                        // Convert from big-endian
                        block_timecode = (block_timecode >> 8) | ((block_timecode & 0xFF) << 8);
                        
                        // Calculate absolute timecode
                        int abs_timecode = timecode + block_timecode;
                        
                        // Read flags (1 byte)
                        uint8_t flags;
                        if (fread(&flags, 1, 1, ctx->webm_file) != 1) {
                            skip_element(ctx->webm_file, cluster_elem_size - (ftell(ctx->webm_file) - block_start));
                            continue;
                        }
                        
                        // Calculate Opus packet size
                        uint64_t packet_size = cluster_elem_size - (ftell(ctx->webm_file) - block_start);
                        
                        if (packet_size > sizeof(opus_packet)) {
                            fprintf(stderr, "Opus packet too large: %lu bytes\n", packet_size);
                            skip_element(ctx->webm_file, cluster_elem_size - (ftell(ctx->webm_file) - block_start));
                            continue;
                        }
                        
                        // Read Opus packet
                        if (fread(opus_packet, 1, packet_size, ctx->webm_file) != packet_size) {
                            fprintf(stderr, "Failed to read Opus packet\n");
                            continue;
                        }
                        
                        if (DEBUG_WEBM) {
                            printf("Decoding Opus packet: size=%lu, timecode=%d\n", 
                                   packet_size, abs_timecode);
                        }
                        
                        // Decode Opus packet to PCM
                        samples = opus_decode(ctx->decoder, opus_packet, packet_size, 
                                             pcm_buffer, OPUS_FRAME_SIZE, 0);
                        
                        if (samples < 0) {
                            fprintf(stderr, "Failed to decode Opus packet: %s\n", 
                                    opus_strerror(samples));
                            continue;
                        }
                        
                        if (DEBUG_WEBM) {
                            printf("Decoded %d samples\n", samples);
                        }
                        
                        // Send PCM data to socket
                        int bytes_to_send = samples * ctx->channels * sizeof(int16_t);
                        if (send(sockfd, pcm_buffer, bytes_to_send, 0) != bytes_to_send) {
                            perror("Failed to send PCM data");
                            return -1;
                        }
                        
                        // Update last timecode
                        last_timecode = abs_timecode;
                    } else {
                        // Skip this block
                        skip_element(ctx->webm_file, cluster_elem_size - (ftell(ctx->webm_file) - block_start));
                    }
                } else {
                    // Skip unknown elements
                    skip_element(ctx->webm_file, cluster_elem_size);
                }
            }
            
            // Make sure we're at the end of the Cluster
            fseek(ctx->webm_file, cluster_end, SEEK_SET);
        } else {
            // Skip unknown elements
            skip_element(ctx->webm_file, element_size);
        }
    }
    
    return 0;
}