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

# Verify static libs exist (iOS + darwin)
for target in ios-arm64 ios-arm64-simulator ios-x64-simulator darwin-arm64; do
  if [ ! -f "lib/$target/librgblibcffi.a" ]; then
    echo "ERROR: lib/$target/librgblibcffi.a not found"
    echo "  Run: bash scripts/build-ios.sh  OR  bash scripts/download-libs.sh"
    exit 1
  fi
done

# Check Android libs (warn, don't fail — allows iOS-only builds)
ANDROID_TARGETS=("android-arm64" "android-arm" "android-x64" "android-ia32")
HAVE_ANDROID=true
for target in "${ANDROID_TARGETS[@]}"; do
  if [ ! -f "lib/$target/librgblibcffi.a" ]; then
    echo "WARN: lib/$target/librgblibcffi.a not found — skipping Android prebuilds for $target"
    HAVE_ANDROID=false
  fi
done

echo "=== Building bare addon prebuilds ==="

ANDROID_NDK_HOME="${ANDROID_NDK_HOME:-$HOME/Library/Android/sdk/ndk/27.1.12297006}"
ANDROID_TOOLCHAIN="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake"

build_target() {
  local TARGET_NAME="$1"
  local BUILD_DIR="build-prebuild-$TARGET_NAME"

  echo ""
  echo "--- Building prebuild for $TARGET_NAME ---"

  rm -rf "$BUILD_DIR"
  mkdir -p "$BUILD_DIR"

  CMAKE_ARGS=(
    -Dcmake-bare_DIR="$PKG_DIR/node_modules/cmake-bare"
    -Dcmake-npm_DIR="$PKG_DIR/node_modules/cmake-npm"
  )

  case "$TARGET_NAME" in
    ios-arm64)
      CMAKE_ARGS+=(-DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_SYSROOT=iphoneos) ;;
    ios-arm64-simulator)
      CMAKE_ARGS+=(-DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_SYSROOT=iphonesimulator) ;;
    ios-x64-simulator)
      CMAKE_ARGS+=(-DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_ARCHITECTURES=x86_64 -DCMAKE_OSX_SYSROOT=iphonesimulator) ;;
    darwin-arm64)
      CMAKE_ARGS+=(-DCMAKE_OSX_ARCHITECTURES=arm64) ;;
    android-arm64)
      CMAKE_ARGS+=(-DCMAKE_TOOLCHAIN_FILE="$ANDROID_TOOLCHAIN" -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-24) ;;
    android-arm)
      CMAKE_ARGS+=(-DCMAKE_TOOLCHAIN_FILE="$ANDROID_TOOLCHAIN" -DANDROID_ABI=armeabi-v7a -DANDROID_PLATFORM=android-24) ;;
    android-x64)
      CMAKE_ARGS+=(-DCMAKE_TOOLCHAIN_FILE="$ANDROID_TOOLCHAIN" -DANDROID_ABI=x86_64 -DANDROID_PLATFORM=android-24) ;;
    android-ia32)
      CMAKE_ARGS+=(-DCMAKE_TOOLCHAIN_FILE="$ANDROID_TOOLCHAIN" -DANDROID_ABI=x86 -DANDROID_PLATFORM=android-24) ;;
  esac

  cmake -B "$BUILD_DIR" -S . "${CMAKE_ARGS[@]}" 2>&1 | tail -5
  cmake --build "$BUILD_DIR" 2>&1 | tail -5

  # Find the built .bare file
  BARE_FILE=$(find "$BUILD_DIR" -name "*.bare" -type f | head -1)

  if [ -z "$BARE_FILE" ]; then
    echo "  ✗ No .bare file produced for $TARGET_NAME"
    rm -rf "$BUILD_DIR"
    return 1
  fi

  mkdir -p "prebuilds/$TARGET_NAME"
  cp "$BARE_FILE" "prebuilds/$TARGET_NAME/utexo__rgb-lib-bare.bare"

  SIZE=$(ls -lh "prebuilds/$TARGET_NAME/utexo__rgb-lib-bare.bare" | awk '{print $5}')
  echo "  ✅ prebuilds/$TARGET_NAME/utexo__rgb-lib-bare.bare ($SIZE)"

  # Clean up build dir
  rm -rf "$BUILD_DIR"
}

# Build iOS + darwin prebuilds
for target in ios-arm64 ios-arm64-simulator ios-x64-simulator darwin-arm64; do
  build_target "$target"
done

# Build Android prebuilds (if static libs exist)
if [ "$HAVE_ANDROID" = true ]; then
  for target in "${ANDROID_TARGETS[@]}"; do
    build_target "$target"
  done
else
  echo ""
  echo "⚠️  Skipping Android prebuilds (missing static libs)"
fi

echo ""
echo "=== Prebuilds complete ==="
ls -lhR prebuilds/
