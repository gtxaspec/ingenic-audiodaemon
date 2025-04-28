#!/bin/bash
# Script to convert audio files to WebM/Opus format compatible with the Ingenic Audio Client

if [ $# -lt 2 ]; then
    echo "Usage: $0 <input_file> <output_file.webm> [quality]"
    echo "  quality: low, medium, high (default: medium)"
    exit 1
fi

INPUT_FILE="$1"
OUTPUT_FILE="$2"
QUALITY="${3:-medium}"

# Check if ffmpeg is installed
if ! command -v ffmpeg &> /dev/null; then
    echo "Error: ffmpeg is not installed. Please install it first."
    exit 1
fi

# Set bitrate based on quality
case "$QUALITY" in
    "low")
        BITRATE="32k"
        ;;
    "medium")
        BITRATE="64k"
        ;;
    "high")
        BITRATE="128k"
        ;;
    *)
        echo "Invalid quality option. Using medium quality."
        BITRATE="64k"
        ;;
esac

echo "Converting $INPUT_FILE to WebM/Opus format..."
echo "Quality: $QUALITY (bitrate: $BITRATE)"

# Use strict WebM/Opus format that should be compatible with our parser
ffmpeg -i "$INPUT_FILE" \
    -c:a libopus \
    -b:a "$BITRATE" \
    -vbr on \
    -compression_level 10 \
    -frame_duration 20 \
    -application audio \
    -ar 48000 \
    -ac 1 \
    -map_metadata -1 \
    -metadata title="" \
    -metadata encoder="" \
    -metadata creation_time=0 \
    -fflags +bitexact \
    -flags:v +bitexact \
    -flags:a +bitexact \
    -f webm \
    -strict experimental \
    "$OUTPUT_FILE"

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