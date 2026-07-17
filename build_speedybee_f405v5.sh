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
REQUIRED_ARM_GCC_VERSION="13.3.1"
ARM_SDK_DIR="$REPO_ROOT/tools/arm-gnu-toolchain-13.3.rel1-x86_64-arm-none-eabi"
ARM_SDK_BIN="$ARM_SDK_DIR/bin/arm-none-eabi-gcc"

echo "=========================================="
echo " Betaflight CV2 Tracker — Build Script"
echo " Target : $CONFIG"
echo "=========================================="

# ── 1. Install ARM toolchain and required build tools ────────────────────────
install_build_deps() {
    sudo apt-get update -qq
    sudo apt-get install -y --no-install-recommends \
        curl \
        tar \
        make \
        python3-intelhex \
        dfu-util \
        git
}

find_arm_gcc() {
    if [ -x "$ARM_SDK_BIN" ]; then
        echo "$ARM_SDK_BIN"
    elif command -v arm-none-eabi-gcc &>/dev/null; then
        command -v arm-none-eabi-gcc
    else
        echo ""
    fi
}

toolchain_healthy() {
    local gcc_cmd="$1"
    local cc1_path=""

    [ -x "$gcc_cmd" ] || return 1
    cc1_path=$($gcc_cmd -print-prog-name=cc1 2>/dev/null || true)

    # A healthy GCC should resolve cc1 to an executable absolute path.
    if [ -z "$cc1_path" ] || [ "$cc1_path" = "cc1" ] || [ ! -x "$cc1_path" ]; then
        return 1
    fi

    return 0
}

ARM_GCC_CMD=$(find_arm_gcc)
ARM_GCC_VERSION=""
if [ -n "$ARM_GCC_CMD" ]; then
    ARM_GCC_VERSION=$($ARM_GCC_CMD -dumpversion 2>/dev/null || true)
fi

if [ "$ARM_GCC_VERSION" != "$REQUIRED_ARM_GCC_VERSION" ] || ! toolchain_healthy "$ARM_GCC_CMD"; then
    echo "[*] Installing required ARM GCC toolchain $REQUIRED_ARM_GCC_VERSION..."
    install_build_deps
    cd "$REPO_ROOT"
    # Clear any partial/broken install before reinstalling.
    make arm_sdk_clean
    make GCC_REQUIRED_VERSION="$REQUIRED_ARM_GCC_VERSION" arm_sdk_install
    ARM_GCC_CMD="$ARM_SDK_BIN"
    ARM_GCC_VERSION=$($ARM_GCC_CMD -dumpversion 2>/dev/null || true)
fi

if [ -z "$ARM_GCC_CMD" ] || [ -z "$ARM_GCC_VERSION" ] || ! toolchain_healthy "$ARM_GCC_CMD"; then
    echo "[ERROR] arm-none-eabi-gcc not found. Install the ARM toolchain manually."
    exit 1
fi

ARM_VER=$($ARM_GCC_CMD --version | head -1)
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
