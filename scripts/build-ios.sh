#!/bin/bash
set -euo pipefail

# Build librgblibcffi.a for all iOS targets + host
#
# IMPORTANT: Uses `cargo rustc --crate-type staticlib` instead of `cargo build`
# because Cargo.toml defines crate-type = ["staticlib", "cdylib"] and the cdylib
# linker step fails on iOS due to missing ___chkstk_darwin symbol from aws-lc-sys.
# Building only the staticlib avoids the linker entirely.
#
# Prerequisites:
#   - Rust toolchain with targets: rustup target add aarch64-apple-ios aarch64-apple-ios-sim
#   - Xcode with iOS SDK
#   - rgb-lib-nodejs repo cloned at ../../rgb-lib-nodejs (or set CFFI_DIR env var)

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PKG_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

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

echo "=== Building rgb-lib C FFI static libraries ==="
echo "Source: $CFFI_DIR"
echo "Output: $OUT_DIR"

FEATURES="--features camel_case"

# Target mappings: rust_target:output_dir
TARGETS=(
  "aarch64-apple-ios:ios-arm64"
  "aarch64-apple-ios-sim:ios-arm64-simulator"
)

# Build host (darwin-arm64) first
echo ""
echo "--- Building for host (darwin-arm64) ---"
cd "$CFFI_DIR"
cargo rustc --release $FEATURES --crate-type staticlib 2>&1 | tail -3
mkdir -p "$OUT_DIR/darwin-arm64"
cp target/release/librgblibcffi.a "$OUT_DIR/darwin-arm64/"
echo "✅ darwin-arm64: $(ls -lh "$OUT_DIR/darwin-arm64/librgblibcffi.a" | awk '{print $5}')"

# Copy generated header
cp rgblib.h "$PKG_DIR/rgblib.h"
echo "✅ Header copied"

# Build for each iOS target
for entry in "${TARGETS[@]}"; do
  RUST_TARGET="${entry%%:*}"
  DIR_NAME="${entry##*:}"

  echo ""
  echo "--- Building for $RUST_TARGET -> $DIR_NAME ---"

  # Set iOS SDK sysroot
  if [[ "$RUST_TARGET" == *"-sim" ]]; then
    SDK_PATH=$(xcrun --sdk iphonesimulator --show-sdk-path)
  else
    SDK_PATH=$(xcrun --sdk iphoneos --show-sdk-path)
  fi

  export SDKROOT="$SDK_PATH"

  cd "$CFFI_DIR"
  cargo rustc --release --target "$RUST_TARGET" $FEATURES --crate-type staticlib 2>&1 | tail -3

  mkdir -p "$OUT_DIR/$DIR_NAME"
  cp "target/$RUST_TARGET/release/librgblibcffi.a" "$OUT_DIR/$DIR_NAME/"

  # Strip debug symbols to reduce size
  strip -S "$OUT_DIR/$DIR_NAME/librgblibcffi.a" 2>/dev/null || true

  SIZE=$(ls -lh "$OUT_DIR/$DIR_NAME/librgblibcffi.a" | awk '{print $5}')
  echo "✅ $DIR_NAME: $SIZE"
done

echo ""
echo "=== Build complete ==="
ls -lhR "$OUT_DIR"
