#!/usr/bin/env bash
set -euo pipefail

# Builds firmware for esp32dev and copies firmware.bin to scripts/serve_dir/

PROJECT_ROOT="$(cd "$(dirname "$0")" && pwd)/.."
BUILD_DIR="$PROJECT_ROOT/.pio/build/esp32dev"
OUT_DIR="$PROJECT_ROOT/scripts/serve_dir"

echo "Building firmware..."
pio run -e esp32dev

BIN="$BUILD_DIR/firmware.bin"
if [ ! -f "$BIN" ]; then
  echo "Build artifact not found: $BIN" >&2
  exit 2
fi

mkdir -p "$OUT_DIR"
cp "$BIN" "$OUT_DIR/firmware.bin"
echo "Firmware copied to $OUT_DIR/firmware.bin"
