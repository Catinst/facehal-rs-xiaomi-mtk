#!/usr/bin/env python3
"""Stage an exact artifact allowlist, excluding every link stub and runtime library."""

import argparse
import hashlib
import os
from pathlib import Path
import re
import struct
import subprocess

from prepare_link_stubs import MARKER

ELFS = (
    "android.hardware.biometrics.face-service.facehal",
    "facehal-virtual-bridge",
    "iface-probe",
)
TEMPLATES = (
    "module.prop", "control.sh", "bridge_control.sh", "service.sh", "action.sh",
    "customize.sh", "uninstall.sh", "mode.conf", "mirror.conf", "skip_mount",
    "reference/device-manifest.xml",
)


def regular_file(path, root):
    if not path.is_file():
        raise ValueError(f"Missing artifact input: {path}")
    current = path
    while current != root:
        if current.is_symlink():
            raise ValueError(f"Linked artifact input: {path}")
        if current == current.parent:
            raise ValueError(f"Artifact input outside source root: {path}")
        current = current.parent
    return path.read_bytes()


def validate_elf(data, name):
    if len(data) < 64 or data[:6] != b"\x7fELF\x02\x01":
        raise ValueError(f"Expected ELF64 little-endian binary: {name}")
    kind, machine = struct.unpack_from("<HH", data, 16)
    if kind not in {2, 3} or machine != 183:
        raise ValueError(f"Expected AArch64 executable/PIE: {name}")
    if MARKER.encode("ascii") in data or b".facehal_ci_link_only" in data:
        raise ValueError(f"CI link stub detected in executable artifact: {name}")


def validate_text(data, name):
    if data.startswith(b"\xef\xbb\xbf") or b"\r" in data or b"\0" in data or data.startswith(b"\x7fELF"):
        raise ValueError(f"Expected BOM-free LF text artifact: {name}")
    data.decode("utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ndk", type=Path, default=os.environ.get("ANDROID_NDK_HOME"))
    args = parser.parse_args()
    if args.ndk is None:
        parser.error("--ndk or ANDROID_NDK_HOME is required")
    root = Path(__file__).resolve().parent.parent
    staging = root / "out/ci-artifact"
    if staging.exists():
        raise ValueError("Artifact staging directory already exists; use a fresh checkout")
    readelf = args.ndk / "toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-readelf"
    if not readelf.is_file():
        raise ValueError("Missing official NDK llvm-readelf")
    files = {}
    report = [
        "# Source-built CI artifacts: runtime dependencies", "",
        "These three executables are compiled from this repository's production sources.",
        "They were linked against CI-only symbol stubs and the official NDK C++ runtime.",
        "No stub or runtime shared library is included in this artifact.",
        "This is not an installable device module or hardware-validation result.", "",
        "Target: aarch64-linux-android, API 31, Android NDK r27c (27.2.12479018).", "",
        "## Executable hashes and direct ELF dependencies", "",
    ]
    for name in ELFS:
        path = root / "out" / name
        data = regular_file(path, root)
        validate_elf(data, name)
        dynamic = subprocess.check_output([str(readelf), "--dynamic", str(path)], text=True)
        needed = re.findall(r"\(NEEDED\).*?\[([^\]]+)\]", dynamic)
        if not needed:
            raise ValueError(f"Production executable lacks dynamic dependencies: {name}")
        if any("/" in value or "stub" in value.lower() for value in needed):
            raise ValueError(f"Unexpected runtime dependency path: {name}: {needed}")
        if "(RPATH)" in dynamic or "(RUNPATH)" in dynamic:
            raise ValueError(f"Unexpected runtime search path in production executable: {name}")
        files["out/" + name] = data
        report += [f"### {name}", "", f"SHA-256: `{hashlib.sha256(data).hexdigest()}`", "",
                   "Direct DT_NEEDED: " + ", ".join(f"`{value}`" for value in needed) + ".", ""]
    for name in TEMPLATES:
        data = regular_file(root / "module" / name, root)
        validate_text(data, name)
        files["module/" + name] = data
    docs = regular_file(root / "docs/ci.md", root)
    validate_text(docs, "docs/ci.md")
    files["docs/ci.md"] = docs
    report += [
        "## Completing a device-specific module", "",
        "Use matching real system/vendor implementations, never the CI link stubs:", "",
        "- System/vendor camera, media, nativewindow and Binder NDK runtime libraries.",
        "- A compatible real libc++_shared.so and libfacehal_jni.so.",
        "- libjni_stfaceunlock_api.so, libtensorflowlite.so, libtensorflowlite_gpu_delegate.so.",
        "- android.frameworks.cameraservice.common-V1-ndk.so, device-V3-ndk.so, service-V3-ndk.so.",
        "- android.hardware.common-V2-ndk.so and android.hardware.common.fmq-V1-ndk.so.", "",
        "Some libraries are resolved through dlopen/transitive dependencies and are absent from DT_NEEDED.",
        "Consult docs/ci.md and scripts/build-ksu-module.py for the device-module assembly contract.",
        "Verify available platform entry points, linker namespaces, VINTF, SELinux and hardware behavior separately.",
        "The module/ directory contains only text templates; runtime libraries are intentionally absent.", "",
    ]
    files["runtime-dependencies.md"] = ("\n".join(report) + "\n").encode("ascii")
    staging.mkdir(parents=True)
    for name, data in sorted(files.items()):
        destination = staging / name
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(data)
        if name.startswith("out/") or name.endswith(".sh"):
            destination.chmod(0o755)
        else:
            destination.chmod(0o644)
    actual = {path.relative_to(staging).as_posix() for path in staging.rglob("*") if path.is_file()}
    if actual != set(files) or any(path.is_symlink() for path in staging.rglob("*")):
        raise ValueError("Artifact staging differs from the exact allowlist")
    if any(".so" in name or "runtime/" in name or "ci-link-stubs" in name for name in actual):
        raise ValueError("Shared libraries or link-stub files reached artifact staging")
    print(f"Artifact allowlist validated: {len(files)} files (3 production ELF executables, text only otherwise)")
    print(staging)


if __name__ == "__main__":
    main()
