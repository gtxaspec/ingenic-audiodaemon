#!/bin/bash
# Script to test the embedded WebM parser with a standard WebM file

# Check if ffmpeg is installed
if ! command -v ffmpeg &> /dev/null; then
    echo "Error: ffmpeg is not installed. Please install it first."
    exit 1
fi

# Create a test directory
TEST_DIR="/tmp/webm_test"
mkdir -p "$TEST_DIR"

# Generate a test tone
echo "Generating test tone..."
ffmpeg -f lavfi -i "sine=frequency=440:duration=5" -c:a pcm_s16le -ar 48000 -ac 1 "$TEST_DIR/test_tone.wav"

# Convert to WebM/Opus using standard ffmpeg command
echo "Converting to WebM/Opus using standard ffmpeg..."
ffmpeg -i "$TEST_DIR/test_tone.wav" -c:a libopus -b:a 64k -vbr on -f webm "$TEST_DIR/standard_webm.webm"

# Test with our client
echo "Testing with iac client..."
./build/bin/iac -w "$TEST_DIR/standard_webm.webm"

# If that fails, try with our simplified conversion
echo "Creating alternative WebM file..."
./scripts/convert_opus_simple.sh "$TEST_DIR/test_tone.wav" "$TEST_DIR/simple_webm.webm"

echo "Testing with alternative WebM file..."
./build/bin/iac -w "$TEST_DIR/simple_webm.webm"

# Clean up
echo "Cleaning up..."
# Uncomment to clean up: rm -rf "$TEST_DIR"

echo "Test complete!"