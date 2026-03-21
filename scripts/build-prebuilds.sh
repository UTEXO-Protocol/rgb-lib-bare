#!/bin/bash
set -euo pipefail

# Build .bare native addon prebuilds for all target platforms
#
# This compiles binding.cc against the pre-built librgblibcffi.a static
# libraries using cmake-bare's CMake infrastructure.
#
# Prerequisites:
#   - npm install (cmake-bare, cmake-npm, require-addon)
#   - Static libraries in lib/ (run build-ios.sh or download-libs.sh first)
#   - CMake 3.25+
#   - Xcode with iOS SDK

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PKG_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$PKG_DIR"

# Verify static libs exist
for target in ios-arm64 ios-arm64-simulator ios-x64-simulator darwin-arm64; do
  if [ ! -f "lib/$target/librgblibcffi.a" ]; then
    echo "ERROR: lib/$target/librgblibcffi.a not found"
    echo "  Run: bash scripts/build-ios.sh  OR  bash scripts/download-libs.sh"
    exit 1
  fi
done

echo "=== Building bare addon prebuilds ==="

# Target configs: name:CMAKE_SYSTEM_NAME:CMAKE_OSX_ARCHITECTURES:CMAKE_OSX_SYSROOT
TARGETS=(
  "ios-arm64:iOS:arm64:iphoneos"
  "ios-arm64-simulator:iOS:arm64:iphonesimulator"
  "ios-x64-simulator:iOS:x86_64:iphonesimulator"
  "darwin-arm64::arm64:"
)

for entry in "${TARGETS[@]}"; do
  IFS=':' read -r TARGET_NAME SYS_NAME ARCH SYSROOT <<< "$entry"
  BUILD_DIR="build-prebuild-$TARGET_NAME"

  echo ""
  echo "--- Building prebuild for $TARGET_NAME ---"

  rm -rf "$BUILD_DIR"
  mkdir -p "$BUILD_DIR"

  CMAKE_ARGS=(
    -DCMAKE_OSX_ARCHITECTURES="$ARCH"
    -Dcmake-bare_DIR="$PKG_DIR/node_modules/cmake-bare"
    -Dcmake-npm_DIR="$PKG_DIR/node_modules/cmake-npm"
  )

  if [ -n "$SYS_NAME" ]; then
    CMAKE_ARGS+=(-DCMAKE_SYSTEM_NAME="$SYS_NAME")
  fi

  if [ -n "$SYSROOT" ]; then
    CMAKE_ARGS+=(-DCMAKE_OSX_SYSROOT="$SYSROOT")
  fi

  cmake -B "$BUILD_DIR" -S . "${CMAKE_ARGS[@]}" 2>&1 | tail -5
  cmake --build "$BUILD_DIR" 2>&1 | tail -5

  # Find the built .bare file
  BARE_FILE=$(find "$BUILD_DIR" -name "*.bare" -type f | head -1)

  if [ -z "$BARE_FILE" ]; then
    echo "  ✗ No .bare file produced for $TARGET_NAME"
    exit 1
  fi

  mkdir -p "prebuilds/$TARGET_NAME"
  cp "$BARE_FILE" "prebuilds/$TARGET_NAME/utexo__rgb-lib-bare.bare"

  SIZE=$(ls -lh "prebuilds/$TARGET_NAME/utexo__rgb-lib-bare.bare" | awk '{print $5}')
  echo "  ✅ prebuilds/$TARGET_NAME/utexo__rgb-lib-bare.bare ($SIZE)"

  # Clean up build dir
  rm -rf "$BUILD_DIR"
done

echo ""
echo "=== Prebuilds complete ==="
ls -lhR prebuilds/
