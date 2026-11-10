#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
output_dir=${FACEHAL_SDK_COMPAT_OUTPUT_DIR:-"$root/out/api29-compile"}
ndk=${ANDROID_NDK_HOME:?set ANDROID_NDK_HOME to a Linux NDK}
cxx="$ndk/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang++"
mkdir -p "$output_dir"
FACEHAL_MIN_SDK=29 FACEHAL_OUTPUT="$output_dir/android.hardware.biometrics.face-service.facehal" \
    "$root/scripts/build-production.sh"
"$cxx" -std=c++17 -Wall -Wextra -Werror -fPIC -shared \
    "$root/native/JniGraphicsShim.cpp" -o "$output_dir/libfacehal_jni.so"
echo "Built API29-targeted objects: $output_dir"
echo 'This is a compile target, not universal Android 10-17 runtime verification.'
