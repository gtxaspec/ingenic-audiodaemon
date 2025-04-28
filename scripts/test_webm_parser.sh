#!/bin/bash
# Script to test the WebM parser with a sample file

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

# Convert to WebM/Opus using our script
echo "Converting to WebM/Opus..."
./scripts/convert_to_webm.sh "$TEST_DIR/test_tone.wav" "$TEST_DIR/test_tone.webm" medium

# Make the script executable
chmod +x scripts/convert_to_webm.sh

# Test with our client
echo "Testing with iac client..."
./build/bin/iac -w "$TEST_DIR/test_tone.webm"

# Clean up
echo "Cleaning up..."
rm -rf "$TEST_DIR"

echo "Test complete!"