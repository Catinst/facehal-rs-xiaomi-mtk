#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
workspace="$root/standalone-build"
ndk=${ANDROID_NDK_HOME:?set ANDROID_NDK_HOME to a Linux Android NDK}
: "${ANDROID_SYSTEM_LIB64:?set ANDROID_SYSTEM_LIB64 to matching system lib64}"
: "${ANDROID_VENDOR_LIB64:?set ANDROID_VENDOR_LIB64 to camera vendor libraries}"
toolchain="$ndk/toolchains/llvm/prebuilt/linux-x86_64"
api=${FACEHAL_MIN_SDK:-31}
verbose=${FACEHAL_VERBOSE_LOGS:-0}
case "$verbose" in 0|1) ;; *) echo 'invalid FACEHAL_VERBOSE_LOGS' >&2; exit 1 ;; esac
export FACEHAL_VERBOSE_LOGS="$verbose"
case "$api" in ''|*[!0-9]*) echo 'invalid FACEHAL_MIN_SDK' >&2; exit 1 ;; esac
[ "$api" -ge 29 ] || { echo 'Binder requires API 29 or newer' >&2; exit 1; }
cxx="$toolchain/bin/aarch64-linux-android${api}-clang++"
cc="$toolchain/bin/aarch64-linux-android${api}-clang"
ar="$toolchain/bin/llvm-ar"
[ -x "$cxx" ] && [ -x "$cc" ] && [ -x "$ar" ] || { echo 'Linux NDK toolchain missing' >&2; exit 1; }
mkdir -p "$workspace/native" "$root/out"
for source in CameraBridge PreviewBridge SenseTimeBridge AlgorithmThreadBridge; do
    "$cxx" -DFACEHAL_VENDOR_CAMERA_NDK=1 -DFACEHAL_VERBOSE_LOGS="$verbose" -std=c++17 -O2 -fPIC -Wall -Wextra -Werror \
        -I "$root/native" -c "$root/native/$source.cpp" -o "$workspace/native/$source.o"
done
"$ar" rcs "$workspace/native/libfacehal_bridge.a" \
    "$workspace/native/CameraBridge.o" "$workspace/native/PreviewBridge.o" \
    "$workspace/native/SenseTimeBridge.o" "$workspace/native/AlgorithmThreadBridge.o"
export FACEHAL_ROOT="$root" ANDROID_NDK_HOME="$ndk"
export BINDER_PLATFORM_HEADERS="$workspace/third_party/binder-headers/include_platform"
export BINDER_NDK_LIB_DIR=${BINDER_NDK_LIB_DIR:-"$toolchain/sysroot/usr/lib/aarch64-linux-android/$api"}
if [ -z "${LIBCLANG_PATH:-}" ]; then
    for version in 18 19 20 21; do
        candidate="/usr/lib/llvm-$version/lib"
        if [ -e "$candidate/libclang.so" ]; then LIBCLANG_PATH="$candidate"; break; fi
    done
fi
export LIBCLANG_PATH=${LIBCLANG_PATH:-"$toolchain/lib"}
resource=$("$toolchain/bin/clang" -print-resource-dir)
export BINDGEN_EXTRA_CLANG_ARGS_aarch64_linux_android="${BINDGEN_EXTRA_CLANG_ARGS_aarch64_linux_android:-} -isystem $resource/include -include stddef.h"
export FACEHAL_ANDROID_API="$api"
export CARGO_TARGET_AARCH64_LINUX_ANDROID_LINKER="$cc"
export CARGO_TARGET_AARCH64_LINUX_ANDROID_AR="$ar"
cd "$workspace"
set -- build --locked --release --target aarch64-linux-android -p facehal_service --jobs "${FACEHAL_BUILD_JOBS:-2}"
if [ "${FACEHAL_OFFLINE:-0}" = 1 ]; then set -- "$@" --offline; fi
cargo "$@"
output=${FACEHAL_OUTPUT:-"$root/out/android.hardware.biometrics.face-service.facehal"}
mkdir -p "$(dirname -- "$output")"
install -m 0755 target/aarch64-linux-android/release/facehal_service "$output"
"$cc" -DFACEHAL_VERBOSE_LOGS="$verbose" -O2 -Wall -Wextra -Werror "$root/native/FaceVirtualBridge.c" -lbinder_ndk -llog -ldl \
    -o "$root/out/facehal-virtual-bridge"
"$cc" -DFACEHAL_VERBOSE_LOGS="$verbose" -O2 -Wall -Wextra -Werror "$root/tools/iface_probe.c" -lbinder_ndk -ldl \
    -o "$root/out/iface-probe"
echo "Built HAL: $output"
echo "Built helpers: $root/out/facehal-virtual-bridge and $root/out/iface-probe"
echo 'Runtime compatibility still depends on the device system/vendor libraries.'
