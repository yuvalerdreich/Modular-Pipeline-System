#!/bin/bash

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${GREEN}[BUILD]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Create output directory
print_status "Creating output directory..."
mkdir -p output

# Build main application
print_status "Building main application..."
gcc -Wall -Wextra -o output/analyzer main.c -ldl -lpthread || {
    print_error "Failed to build main application"
    exit 1
}

# List of plugins to build
plugins="logger typewriter uppercaser rotator flipper expander"

# Build each plugin
for plugin_name in $plugins; do
    print_status "Building plugin: $plugin_name"
    gcc -fPIC -shared -o output/${plugin_name}.so \
        plugins/${plugin_name}.c \
        plugins/plugin_common.c \
        plugins/sync/monitor.c \
        plugins/sync/consumer_producer.c \
        -ldl -lpthread || {
        print_error "Failed to build $plugin_name"
        exit 1
    }
done

print_status "Build completed successfully!"
print_status "Main executable: output/analyzer"
print_status "Plugins built: $plugins"