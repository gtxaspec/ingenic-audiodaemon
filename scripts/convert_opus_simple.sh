#!/bin/bash
# Script to convert audio files to Opus format in a WebM container
# This is a simplified version that focuses on compatibility

if [ $# -lt 2 ]; then
    echo "Usage: $0 <input_file> <output_file.webm> [bitrate_kbps]"
    exit 1
fi

INPUT_FILE="$1"
OUTPUT_FILE="$2"
BITRATE="${3:-64}"  # Default to 64 kbps if not specified

# Check if ffmpeg is installed
if ! command -v ffmpeg &> /dev/null; then
    echo "Error: ffmpeg is not installed. Please install it first."
    exit 1
fi

echo "Converting $INPUT_FILE to WebM/Opus format (${BITRATE}kbps)..."

# Use a three-step process for maximum compatibility
# Step 1: Convert to WAV (PCM) for clean input
TEMP_WAV="/tmp/temp_$$.wav"
ffmpeg -i "$INPUT_FILE" -ar 48000 -ac 1 "$TEMP_WAV"

# Step 2: Convert to raw Opus with specific settings
TEMP_OPUS="/tmp/temp_$$.opus"
ffmpeg -i "$TEMP_WAV" \
    -c:a libopus \
    -b:a "${BITRATE}k" \
    -vbr on \
    -compression_level 10 \
    -frame_duration 20 \
    -application audio \
    -ar 48000 \
    -ac 1 \
    "$TEMP_OPUS"

# Step 3: Package in WebM container with minimal options
ffmpeg -i "$TEMP_OPUS" \
    -c:a copy \
    -map_metadata -1 \
    -metadata title="" \
    -metadata encoder="" \
    -metadata creation_time=0 \
    -fflags +bitexact \
    -flags:a +bitexact \
    -f webm \
    "$OUTPUT_FILE"

# Clean up
rm -f "$TEMP_WAV" "$TEMP_OPUS"

if [ -f "$OUTPUT_FILE" ]; then
    echo "Conversion successful: $OUTPUT_FILE"
    echo "You can now play this file with: ./iac -w $OUTPUT_FILE"

    # Verify the file
    echo "Verifying WebM/Opus file..."
    ffprobe -v error -show_entries stream=codec_name,channels,sample_rate -of default=noprint_wrappers=1 "$OUTPUT_FILE"

    # Show file size
    FILE_SIZE=$(du -h "$OUTPUT_FILE" | cut -f1)
    echo "File size: $FILE_SIZE"
else
    echo "Conversion failed."
    exit 1
fi

exit 0