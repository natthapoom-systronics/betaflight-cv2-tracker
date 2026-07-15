#!/bin/bash
# =============================================================================
# build_speedybee_f405v5.sh
# Build Betaflight CV2-Tracker firmware for SpeedyBee F405 V5
# Run inside WSL Ubuntu: bash build_speedybee_f405v5.sh
# =============================================================================

set -e

REPO_ROOT="$(cd "$(dirname "$0")" && pwd)"
CONFIG=SPEEDYBEEF405V5
OUTPUT_DIR="$REPO_ROOT/build"

echo "=========================================="
echo " Betaflight CV2 Tracker — Build Script"
echo " Target : $CONFIG"
echo "=========================================="

# ── 1. Install ARM toolchain if missing ──────────────────────────────────────
if ! command -v arm-none-eabi-gcc &>/dev/null; then
    echo "[*] Installing ARM GCC toolchain..."
    sudo apt-get update -qq
    sudo apt-get install -y --no-install-recommends \
        gcc-arm-none-eabi \
        binutils-arm-none-eabi \
        libnewlib-arm-none-eabi \
        make \
        python3-intelhex \
        dfu-util \
        git
fi

ARM_VER=$(arm-none-eabi-gcc --version | head -1)
echo "[✓] Toolchain: $ARM_VER"

# ── 2. Make sure submodules are initialised ───────────────────────────────────
echo "[*] Checking submodules..."
cd "$REPO_ROOT"
git submodule update --init src/config lib/main/STM32F4 lib/main/CMSIS 2>/dev/null || true

# ── 3. Build ──────────────────────────────────────────────────────────────────
echo "[*] Building CONFIG=$CONFIG ..."
make CONFIG=$CONFIG -j$(nproc) 2>&1 | tee "$REPO_ROOT/build_${CONFIG}.log"

# ── 4. Locate output hex ─────────────────────────────────────────────────────
HEX=$(find "$REPO_ROOT/obj" -name "*.hex" 2>/dev/null | grep -i "$CONFIG" | head -1)
if [ -z "$HEX" ]; then
    HEX=$(find "$REPO_ROOT/obj" -name "betaflight_*.hex" | tail -1)
fi

if [ -n "$HEX" ]; then
    mkdir -p "$OUTPUT_DIR"
    cp "$HEX" "$OUTPUT_DIR/"
    echo ""
    echo "=========================================="
    echo " BUILD SUCCESSFUL"
    echo " Firmware: $OUTPUT_DIR/$(basename $HEX)"
    echo "=========================================="
else
    echo "[ERROR] .hex file not found. Check build_${CONFIG}.log"
    exit 1
fi
