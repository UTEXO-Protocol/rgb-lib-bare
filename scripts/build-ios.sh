#!/bin/bash
set -euo pipefail

# ============================================================================
# Build librgblibcffi.a for iOS and Android targets
#
# Uses `cargo rustc --crate-type staticlib` instead of `cargo build`
# because Cargo.toml defines crate-type = ["staticlib", "cdylib"] and the
# cdylib linker step fails on iOS (missing ___chkstk_darwin from aws-lc-sys).
#
# Usage:
#   bash build-ios.sh           # Build iOS + host (default)
#   bash build-ios.sh ios       # iOS targets only
#   bash build-ios.sh android   # Android targets only
#   bash build-ios.sh all       # iOS + Android + host
#
# Prerequisites:
#   iOS:     Xcode, rustup target add aarch64-apple-ios aarch64-apple-ios-sim
#   Android: Android NDK, rustup target add aarch64-linux-android
#   Source:  rgb-lib-nodejs at ../../rgb-lib-nodejs (or set CFFI_DIR env var)
# ============================================================================

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PKG_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
MODE="${1:-ios}"

# Allow overriding the C-FFI source directory
if [ -z "${CFFI_DIR:-}" ]; then
  CFFI_DIR="$(cd "$PKG_DIR/../../rgb-lib-nodejs/rgb-lib/bindings/c-ffi" 2>/dev/null && pwd)" || {
    echo "ERROR: rgb-lib-nodejs not found at ../../rgb-lib-nodejs"
    echo "  Either clone it there or set CFFI_DIR env var:"
    echo "  CFFI_DIR=/path/to/rgb-lib-nodejs/rgb-lib/bindings/c-ffi bash $0"
    exit 1
  }
fi

OUT_DIR="$PKG_DIR/lib"
FEATURES="--features camel_case"

echo "=== Building rgb-lib C FFI static libraries ==="
echo "Source: $CFFI_DIR"
echo "Output: $OUT_DIR"
echo "Mode:   $MODE"

# ── Helper: build a single target ──────────────────────────────────────────
build_target() {
  local RUST_TARGET="$1"
  local DIR_NAME="$2"
  local EXTRA_ENV="${3:-}"

  echo ""
  echo "--- Building for $RUST_TARGET → $DIR_NAME ---"

  cd "$CFFI_DIR"

  if [ -n "$EXTRA_ENV" ]; then
    eval "$EXTRA_ENV"
  fi

  if [ -z "$RUST_TARGET" ]; then
    # Host build (no --target flag)
    cargo rustc --release $FEATURES --crate-type staticlib 2>&1 | tail -3
    mkdir -p "$OUT_DIR/$DIR_NAME"
    cp target/release/librgblibcffi.a "$OUT_DIR/$DIR_NAME/"
  else
    cargo rustc --release --target "$RUST_TARGET" $FEATURES --crate-type staticlib 2>&1 | tail -3
    mkdir -p "$OUT_DIR/$DIR_NAME"
    cp "target/$RUST_TARGET/release/librgblibcffi.a" "$OUT_DIR/$DIR_NAME/"
  fi

  # Strip debug symbols to reduce size (skip on Android — use llvm-strip)
  if [[ "$DIR_NAME" == android-* ]]; then
    "${LLVM_STRIP:-llvm-strip}" --strip-debug "$OUT_DIR/$DIR_NAME/librgblibcffi.a" 2>/dev/null || true
  else
    strip -S "$OUT_DIR/$DIR_NAME/librgblibcffi.a" 2>/dev/null || true
  fi

  SIZE=$(ls -lh "$OUT_DIR/$DIR_NAME/librgblibcffi.a" | awk '{print $5}')
  echo "✅ $DIR_NAME: $SIZE"
}

# ── Detect Android NDK ─────────────────────────────────────────────────────
detect_ndk() {
  if [ -n "${ANDROID_NDK_HOME:-}" ]; then
    echo "$ANDROID_NDK_HOME"
    return 0
  fi

  # Search common locations
  local SDK_DIR="${ANDROID_HOME:-$HOME/Library/Android/sdk}"
  if [ -d "$SDK_DIR/ndk" ]; then
    # Pick the latest NDK version
    local NDK_DIR=$(ls -d "$SDK_DIR/ndk/"* 2>/dev/null | sort -V | tail -1)
    if [ -n "$NDK_DIR" ]; then
      echo "$NDK_DIR"
      return 0
    fi
  fi

  echo ""
  return 1
}

setup_android_env() {
  local NDK_HOME
  NDK_HOME=$(detect_ndk) || {
    echo "ERROR: Android NDK not found"
    echo "  Set ANDROID_NDK_HOME or install NDK via Android Studio"
    exit 1
  }
  echo "  Using NDK: $NDK_HOME"

  # Detect host OS for prebuilt toolchain path
  local HOST_TAG
  case "$(uname -s)" in
    Darwin) HOST_TAG="darwin-x86_64" ;;
    Linux)  HOST_TAG="linux-x86_64" ;;
    *)      echo "ERROR: Unsupported host OS"; exit 1 ;;
  esac

  local TOOLCHAIN="$NDK_HOME/toolchains/llvm/prebuilt/$HOST_TAG"

  if [ ! -d "$TOOLCHAIN" ]; then
    echo "ERROR: NDK toolchain not found at $TOOLCHAIN"
    exit 1
  fi

  # Export for cargo and CMake (aws-lc-sys uses CMake internally)
  # ANDROID_NDK_ROOT is required by CMake's Android platform detection
  export ANDROID_NDK_HOME="$NDK_HOME"
  export ANDROID_NDK_ROOT="$NDK_HOME"
  export ANDROID_NDK="$NDK_HOME"
  export LLVM_STRIP="$TOOLCHAIN/bin/llvm-strip"

  # arm64 (aarch64)
  export CARGO_TARGET_AARCH64_LINUX_ANDROID_LINKER="$TOOLCHAIN/bin/aarch64-linux-android24-clang"
  export CC_aarch64_linux_android="$TOOLCHAIN/bin/aarch64-linux-android24-clang"
  export CXX_aarch64_linux_android="$TOOLCHAIN/bin/aarch64-linux-android24-clang++"
  export AR_aarch64_linux_android="$TOOLCHAIN/bin/llvm-ar"

  # arm (armv7)
  export CARGO_TARGET_ARMV7_LINUX_ANDROIDEABI_LINKER="$TOOLCHAIN/bin/armv7a-linux-androideabi24-clang"
  export CC_armv7_linux_androideabi="$TOOLCHAIN/bin/armv7a-linux-androideabi24-clang"
  export CXX_armv7_linux_androideabi="$TOOLCHAIN/bin/armv7a-linux-androideabi24-clang++"
  export AR_armv7_linux_androideabi="$TOOLCHAIN/bin/llvm-ar"

  # x64 (x86_64)
  export CARGO_TARGET_X86_64_LINUX_ANDROID_LINKER="$TOOLCHAIN/bin/x86_64-linux-android24-clang"
  export CC_x86_64_linux_android="$TOOLCHAIN/bin/x86_64-linux-android24-clang"
  export CXX_x86_64_linux_android="$TOOLCHAIN/bin/x86_64-linux-android24-clang++"
  export AR_x86_64_linux_android="$TOOLCHAIN/bin/llvm-ar"

  # ia32 (i686)
  export CARGO_TARGET_I686_LINUX_ANDROID_LINKER="$TOOLCHAIN/bin/i686-linux-android24-clang"
  export CC_i686_linux_android="$TOOLCHAIN/bin/i686-linux-android24-clang"
  export CXX_i686_linux_android="$TOOLCHAIN/bin/i686-linux-android24-clang++"
  export AR_i686_linux_android="$TOOLCHAIN/bin/llvm-ar"

  echo "  NDK toolchain: $TOOLCHAIN"
}

# ── Build host (darwin-arm64) ──────────────────────────────────────────────
if [ "$MODE" = "ios" ] || [ "$MODE" = "all" ]; then
  build_target "" "darwin-arm64"

  # Copy generated header
  cp "$CFFI_DIR/rgblib.h" "$PKG_DIR/rgblib.h"
  echo "✅ Header copied"
fi

# ── Build iOS targets ─────────────────────────────────────────────────────
if [ "$MODE" = "ios" ] || [ "$MODE" = "all" ]; then
  IOS_SDK=$(xcrun --sdk iphoneos --show-sdk-path)
  IOS_SIM_SDK=$(xcrun --sdk iphonesimulator --show-sdk-path)

  build_target "aarch64-apple-ios" "ios-arm64" "export SDKROOT='$IOS_SDK'"
  build_target "aarch64-apple-ios-sim" "ios-arm64-simulator" "export SDKROOT='$IOS_SIM_SDK'"
  build_target "x86_64-apple-ios" "ios-x64-simulator" "export SDKROOT='$IOS_SIM_SDK'"
fi

# ── Build Android targets ─────────────────────────────────────────────────
if [ "$MODE" = "android" ] || [ "$MODE" = "all" ]; then
  echo ""
  echo "=== Setting up Android NDK ==="
  setup_android_env

  # Clear SDKROOT to avoid iOS toolchain contamination
  unset SDKROOT

  build_target "aarch64-linux-android" "android-arm64"
  build_target "armv7-linux-androideabi" "android-arm"
  build_target "x86_64-linux-android" "android-x64"
  build_target "i686-linux-android" "android-ia32"
fi

echo ""
echo "=== Build complete ==="
ls -lhR "$OUT_DIR"
