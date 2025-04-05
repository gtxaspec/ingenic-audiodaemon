#!/bin/bash
# Script to convert audio files to Opus format in a WebM container
# This is a simplified version that focuses on compatibility

if [ $# -lt 2 ]; then
    echo "Usage: $0 <input_file> <output_file.webm>"
    exit 1
fi

INPUT_FILE="$1"
OUTPUT_FILE="$2"

# Check if ffmpeg is installed
if ! command -v ffmpeg &> /dev/null; then
    echo "Error: ffmpeg is not installed. Please install it first."
    exit 1
fi

echo "Converting $INPUT_FILE to WebM/Opus format..."

# Use a two-step process for maximum compatibility
# Step 1: Convert to raw Opus
TEMP_OPUS="/tmp/temp_$$.opus"

ffmpeg -i "$INPUT_FILE" \
    -c:a libopus \
    -b:a 64k \
    -vbr on \
    -ar 48000 \
    -ac 1 \
    "$TEMP_OPUS"

# Step 2: Package in WebM container
ffmpeg -i "$TEMP_OPUS" \
    -c:a copy \
    -map_metadata -1 \
    -f webm \
    "$OUTPUT_FILE"

# Clean up
rm -f "$TEMP_OPUS"

if [ $? -eq 0 ]; then
    echo "Conversion successful: $OUTPUT_FILE"
    echo "You can now play this file with: ./iac -w $OUTPUT_FILE"
else
    echo "Conversion failed."
    exit 1
fi

# Verify the file
echo "Verifying WebM/Opus file..."
ffprobe -v error -show_entries stream=codec_name,channels,sample_rate -of default=noprint_wrappers=1 "$OUTPUT_FILE"

exit 0