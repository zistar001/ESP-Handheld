#!/bin/bash
# ESP-Handheld — Linux/macOS Setup & Build Script
# ================================================
# Prerequisites:
#   - ESP-IDF v5.5.4 installed (see https://docs.espressif.com/projects/esp-idf/)
#   - Python 3.11+
#   - Git
#
# Usage:
#   ./scripts/setup.sh              # Setup + build
#   ./scripts/setup.sh --build      # Build only (skip submodule init)
#   ./scripts/setup.sh --menuconfig # Open menuconfig after setup

set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

GREEN='\033[0;32m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${CYAN}=== ESP-Handheld Firmware Setup ===${NC}"

# 1. Check ESP-IDF
if [ -z "$IDF_PATH" ]; then
    # Try to source export.sh from common locations
    for f in ~/esp/esp-idf/export.sh /opt/esp-idf/export.sh /usr/local/esp-idf/export.sh; do
        if [ -f "$f" ]; then
            echo -e "${GREEN}Found ESP-IDF at: $(dirname "$f")${NC}"
            . "$f"
            break
        fi
    done
    if [ -z "$IDF_PATH" ]; then
        echo -e "${YELLOW}ESP-IDF not found. Please install it:${NC}"
        echo "  https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/get-started/"
        echo "Then run: . \$IDF_PATH/export.sh"
        exit 1
    fi
fi

# 2. Init submodules
if [ "$1" != "--build" ]; then
    echo -e "${CYAN}=== Initializing submodules ===${NC}"
    git submodule update --init --recursive
fi

# 3. Open menuconfig if requested
if [ "$1" = "--menuconfig" ] || [ "$2" = "--menuconfig" ]; then
    echo -e "${CYAN}=== Opening menuconfig ===${NC}"
    echo "Set your Weather API Key in: ESP-Handheld Configuration → Weather API Key"
    idf.py menuconfig
fi

# 4. Build
echo -e "${CYAN}=== Building ===${NC}"
export IDF_COMPONENT_MANAGER=0
idf.py build

echo ""
echo -e "${GREEN}=== Build complete! ===${NC}"
echo ""
echo "To flash:"
echo "  python -m esptool --chip esp32s3 -p /dev/ttyACM0 -b 921600 write_flash 0x10000 build/esp_handheld.bin"
