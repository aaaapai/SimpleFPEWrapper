#!/bin/bash
set -e

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$PROJECT_DIR/build/android/arm64-v8a"
NDK="/home/bzlzhh/Android/Sdk/ndk/27.3.13750724"
SDK="/home/bzlzhh/Android/Sdk"
LLVM_STRIP="$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-strip"
ZIPALIGN="$SDK/build-tools/37.0.0/zipalign"
APKSIGNER="$SDK/build-tools/37.0.0/apksigner"
KEYSTORE="$HOME/.android/debug.keystore"

# Phone-side paths
APK_PHONE="/storage/emulated/0/MT2/apks/MobileGlues (SFPEW)_1.3.4 with SFPEW.apk"
APK_SO_DIR="lib/arm64-v8a"

TEMP_DIR="/tmp/apk_repack_$$"

echo "==> [1/6] Building .so (Release, arm64-v8a)..."
cmake --build "$BUILD_DIR" --target SimpleFPEWrapper -j$(nproc)

echo "==> [2/6] Stripping..."
"$LLVM_STRIP" --strip-debug "$BUILD_DIR/libSimpleFPEWrapper.so"

echo "==> [3/6] Pulling APK from phone..."
mkdir -p "$TEMP_DIR"
adb pull "$APK_PHONE" "$TEMP_DIR/app.apk"

echo "==> [4/6] Replacing .so in APK..."
cd "$TEMP_DIR"
unzip -o app.apk -d unpacked > /dev/null
cp "$BUILD_DIR/libSimpleFPEWrapper.so" "unpacked/$APK_SO_DIR/"
cd unpacked
zip -r -n .so "$TEMP_DIR/app_repacked.apk" * > /dev/null

echo "==> [5/6] Align + Sign..."
"$ZIPALIGN" -p 4 "$TEMP_DIR/app_repacked.apk" "$TEMP_DIR/app_aligned.apk"
"$APKSIGNER" sign --ks "$KEYSTORE" --ks-pass pass:android "$TEMP_DIR/app_aligned.apk"

echo "==> [6/6] Installing APK..."
adb install -r "$TEMP_DIR/app_aligned.apk"

echo "==> Done! Cleaning up..."
rm -rf "$TEMP_DIR"
