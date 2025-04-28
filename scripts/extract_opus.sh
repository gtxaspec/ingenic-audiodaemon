#!/bin/bash
# Script to extract Opus data from a WebM file for debugging

if [ $# -lt 2 ]; then
    echo "Usage: $0 <input_webm> <output_opus>"
    exit 1
fi

INPUT_FILE="$1"
OUTPUT_FILE="$2"

# Check if ffmpeg is installed
if ! command -v ffmpeg &> /dev/null; then
    echo "Error: ffmpeg is not installed. Please install it first."
    exit 1
fi

echo "Extracting Opus data from $INPUT_FILE..."

# Extract the Opus stream directly
ffmpeg -i "$INPUT_FILE" -c:a copy -vn -f opus "$OUTPUT_FILE"

if [ -f "$OUTPUT_FILE" ]; then
    echo "Extraction successful: $OUTPUT_FILE"
    
    # Show file info
    echo "File information:"
    ffprobe -v error -show_format -show_streams "$OUTPUT_FILE"
    
    # Show file size
    FILE_SIZE=$(du -h "$OUTPUT_FILE" | cut -f1)
    echo "File size: $FILE_SIZE"
else
    echo "Extraction failed."
    exit 1
fi

exit 0