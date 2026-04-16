#!/bin/bash
set -euo pipefail

# Downloads pre-built static libraries and bare addon prebuilds
# from GitHub Releases. Runs automatically via npm postinstall.

REPO="UTEXO-Protocol/rgb-lib-bare"
VERSION="v0.3.0-beta.17"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PKG_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

# Platform assets to download
# Format: "github_asset_name:local_path"
ASSETS=(
  # iOS static libs
  "librgblibcffi-ios-arm64.a:lib/ios-arm64/librgblibcffi.a"
  "librgblibcffi-ios-arm64-simulator.a:lib/ios-arm64-simulator/librgblibcffi.a"
  "librgblibcffi-ios-x64-simulator.a:lib/ios-x64-simulator/librgblibcffi.a"
  "librgblibcffi-darwin-arm64.a:lib/darwin-arm64/librgblibcffi.a"
  # Android static libs
  "librgblibcffi-android-arm64.a:lib/android-arm64/librgblibcffi.a"
  "librgblibcffi-android-arm.a:lib/android-arm/librgblibcffi.a"
  "librgblibcffi-android-x64.a:lib/android-x64/librgblibcffi.a"
  "librgblibcffi-android-ia32.a:lib/android-ia32/librgblibcffi.a"
  # iOS prebuilds
  "utexo__rgb-lib-bare-ios-arm64.bare:prebuilds/ios-arm64/utexo__rgb-lib-bare.bare"
  "utexo__rgb-lib-bare-ios-arm64-simulator.bare:prebuilds/ios-arm64-simulator/utexo__rgb-lib-bare.bare"
  "utexo__rgb-lib-bare-ios-x64-simulator.bare:prebuilds/ios-x64-simulator/utexo__rgb-lib-bare.bare"
  "utexo__rgb-lib-bare-darwin-arm64.bare:prebuilds/darwin-arm64/utexo__rgb-lib-bare.bare"
  # Android prebuilds
  "utexo__rgb-lib-bare-android-arm64.bare:prebuilds/android-arm64/utexo__rgb-lib-bare.bare"
  "utexo__rgb-lib-bare-android-arm.bare:prebuilds/android-arm/utexo__rgb-lib-bare.bare"
  "utexo__rgb-lib-bare-android-x64.bare:prebuilds/android-x64/utexo__rgb-lib-bare.bare"
  "utexo__rgb-lib-bare-android-ia32.bare:prebuilds/android-ia32/utexo__rgb-lib-bare.bare"
)

cd "$PKG_DIR"

NEED_DOWNLOAD=false
for entry in "${ASSETS[@]}"; do
  LOCAL="${entry##*:}"
  if [ ! -f "$LOCAL" ]; then
    NEED_DOWNLOAD=true
    break
  fi
done

if [ "$NEED_DOWNLOAD" = false ]; then
  echo "[rgb-lib-bare] All binary assets present, skipping download."
  exit 0
fi

echo "[rgb-lib-bare] Downloading binary assets from $REPO@$VERSION..."

# Check if gh CLI is available, fall back to curl
if command -v gh &>/dev/null; then
  USE_GH=true
else
  USE_GH=false
  echo "[rgb-lib-bare] gh CLI not found, using curl"
fi

for entry in "${ASSETS[@]}"; do
  ASSET_NAME="${entry%%:*}"
  LOCAL_PATH="${entry##*:}"
  LOCAL_DIR="$(dirname "$LOCAL_PATH")"

  if [ -f "$LOCAL_PATH" ]; then
    echo "  ✓ $LOCAL_PATH (exists)"
    continue
  fi

  mkdir -p "$LOCAL_DIR"

  if [ "$USE_GH" = true ]; then
    echo "  ↓ $ASSET_NAME → $LOCAL_PATH"
    gh release download "$VERSION" \
      --repo "$REPO" \
      --pattern "$ASSET_NAME" \
      --output "$LOCAL_PATH" \
      --clobber
  else
    URL="https://github.com/$REPO/releases/download/$VERSION/$ASSET_NAME"
    echo "  ↓ $URL → $LOCAL_PATH"
    curl -fSL "$URL" -o "$LOCAL_PATH"
  fi

  if [ -f "$LOCAL_PATH" ]; then
    SIZE=$(ls -lh "$LOCAL_PATH" | awk '{print $5}')
    echo "  ✓ $LOCAL_PATH ($SIZE)"
  else
    echo "  ✗ Failed to download $ASSET_NAME"
    exit 1
  fi
done

echo "[rgb-lib-bare] All binary assets downloaded."
