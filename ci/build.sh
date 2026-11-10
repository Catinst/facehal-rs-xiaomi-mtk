#!/usr/bin/env bash
set -euo pipefail

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"
: "${ANDROID_NDK_HOME:?set ANDROID_NDK_HOME to official Linux NDK r27c}"
export FACEHAL_MIN_SDK=31
export FACEHAL_VERBOSE_LOGS=${FACEHAL_VERBOSE_LOGS:-0}
case "$FACEHAL_VERBOSE_LOGS" in
    0|1) ;;
    *) printf '%s\n' 'FACEHAL_VERBOSE_LOGS must be 0 or 1' >&2; exit 1 ;;
esac
export FACEHAL_BUILD_JOBS=${FACEHAL_BUILD_JOBS:-2}
export LIBCLANG_PATH=${LIBCLANG_PATH:-/usr/lib/llvm-18/lib}

python3 -B ci/prepare_link_stubs.py --ndk "$ANDROID_NDK_HOME" --api 31
export ANDROID_SYSTEM_LIB64="$root/runtime/ci-link-stubs/lib64"
export ANDROID_VENDOR_LIB64="$ANDROID_SYSTEM_LIB64"
export BINDER_NDK_LIB_DIR="$ANDROID_SYSTEM_LIB64"
# Link against symbols only; production sources still use the official NDK headers.
# Avoid both host library search-path pollution and runtime execution of any target ELF.
unset LD_LIBRARY_PATH LD_PRELOAD
sh scripts/build-production.sh
python3 -B ci/stage_artifact.py --ndk "$ANDROID_NDK_HOME"
