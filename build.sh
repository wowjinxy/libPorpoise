#!/bin/bash
# Build script for Porpoise SDK on Unix-like systems

set -e

BUILD_TYPE="${1:-Release}"
BUILD_DIR="build"

echo "Building Porpoise SDK..."
echo "Build type: $BUILD_TYPE"

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure
cmake .. -DCMAKE_BUILD_TYPE="$BUILD_TYPE"

# Build
cmake --build . --config "$BUILD_TYPE"

echo ""
echo "Build complete!"
echo "Binaries are in: $BUILD_DIR/bin"
echo "Libraries are in: $BUILD_DIR/lib"
echo ""
echo "Run examples:"
echo "  ./bin/simple_example"
echo "  ./bin/thread_example"

