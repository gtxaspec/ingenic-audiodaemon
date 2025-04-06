#!/bin/bash
# Script to create a very simple WebM file with minimal headers

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

echo "Creating simple WebM file from $INPUT_FILE..."

# Use a two-pass approach with minimal options
# First pass - create a raw Opus file
TEMP_OPUS="/tmp/temp_opus_$$.opus"

ffmpeg -i "$INPUT_FILE" \
    -c:a libopus \
    -b:a 64k \
    -vbr on \
    -ar 48000 \
    -ac 1 \
    -f opus \
    "$TEMP_OPUS"

# Second pass - create a WebM container with minimal options
ffmpeg -i "$TEMP_OPUS" \
    -c:a copy \
    -map_metadata -1 \
    -fflags +bitexact \
    -f webm \
    "$OUTPUT_FILE"

# Clean up
rm -f "$TEMP_OPUS"

if [ -f "$OUTPUT_FILE" ]; then
    echo "Successfully created $OUTPUT_FILE"
    echo "You can now play this file with: ./iac -w $OUTPUT_FILE"
    
    # Show file info
    echo "File information:"
    ffprobe -v error -show_format -show_streams "$OUTPUT_FILE"
else
    echo "Failed to create WebM file"
    exit 1
fi

exit 0