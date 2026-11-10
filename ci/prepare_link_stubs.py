#!/usr/bin/env python3
"""Generate CI-only Android link stubs from official NDK exports; never run them."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess

NDK_REVISION = "27.2.12479018"  # Android NDK r27c.
MARKER = "FACEHAL_CI_LINK_ONLY_DO_NOT_SHIP_OR_EXECUTE"
IDENTIFIER = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")
BINDER_FUNCTION = re.compile(
    r"\b((?:ABinderProcess|ABinderRpc|AIBinder|AParcel|AServiceManager|AStatus|"
    r"APersistableBundle)_[A-Za-z0-9_]+)\s*\("
)
LIBRARIES = {
    "libcamera2ndk_vendor.so": ("libcamera2ndk.so", set()),
    "libmediandk.so": ("libmediandk.so", {"AImageReader_getWindowNativeHandle"}),
    "libnativewindow.so": ("libnativewindow.so", {"ANativeWindow_readFromParcel"}),
    "libbinder_ndk.so": ("libbinder_ndk.so", set()),
}


def run(*args):
    return subprocess.check_output([str(arg) for arg in args], text=True)


def dynamic_exports(nm_output):
    symbols = set()
    for line in nm_output.splitlines():
        fields = line.split()
        if len(fields) < 2:
            continue
        name, kind = fields[:2]
        # Exclude absolute symbol-version labels and undefined/weak-undefined entries.
        if kind not in {"T", "W", "D", "B", "R", "V", "S", "G", "I"}:
            continue
        name = name.split("@", 1)[0]
        if not IDENTIFIER.fullmatch(name):
            raise ValueError(f"Unsupported NDK export identifier: {name!r}")
        symbols.add(name)
    if not symbols:
        raise ValueError("Official NDK shared library has no eligible dynamic exports")
    return symbols


def binder_functions(headers):
    symbols = set()
    files = sorted(headers.rglob("*.h"))
    if not files:
        raise ValueError(f"Missing vendored Binder headers: {headers}")
    for header in files:
        text = header.read_text(encoding="utf-8")
        text = re.sub(r"/\*.*?\*/|//[^\n]*", "", text, flags=re.S)
        symbols.update(BINDER_FUNCTION.findall(text))
    required = {"ABinderProcess_startThreadPool", "AIBinder_markVendorStability"}
    if not required.issubset(symbols):
        raise ValueError("Vendored Binder platform declarations are incomplete")
    symbols.add("AIBinder_Class_setTransactionCodeToFunctionNameMap")
    return symbols


def stub_source(symbols):
    lines = [
        "/* CI LINK SYMBOLS ONLY: no API signatures or implementation. */",
        "/* Never execute, install, or package this shared object. */",
        '__attribute__((used, visibility("hidden"), section(".facehal_ci_link_only")))',
        f'const char facehal_ci_link_only_marker[] = "{MARKER}";',
    ]
    for symbol in sorted(symbols):
        if not IDENTIFIER.fullmatch(symbol):
            raise ValueError(f"Invalid stub symbol: {symbol!r}")
        # Deliberately trap if accidentally called; no simulated runtime behavior.
        lines.append(f"void {symbol}(void) {{ __builtin_trap(); }}")
    return "\n".join(lines) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ndk", type=Path, default=os.environ.get("ANDROID_NDK_HOME"))
    parser.add_argument("--api", type=int, default=31)
    parser.add_argument("--output", type=Path, default=Path("runtime/ci-link-stubs"))
    args = parser.parse_args()
    if args.ndk is None:
        parser.error("--ndk or ANDROID_NDK_HOME is required")
    if args.api != 31:
        parser.error("This CI link contract is fixed to API 31")
    ndk = args.ndk.resolve(strict=True)
    properties = (ndk / "source.properties").read_text(encoding="utf-8")
    if not re.search(rf"^Pkg\.Revision\s*=\s*{re.escape(NDK_REVISION)}\s*$", properties, re.M):
        raise ValueError(f"Expected official Android NDK r27c ({NDK_REVISION})")
    toolchain = ndk / "toolchains/llvm/prebuilt/linux-x86_64"
    cc = toolchain / f"bin/aarch64-linux-android{args.api}-clang"
    nm = toolchain / "bin/llvm-nm"
    readelf = toolchain / "bin/llvm-readelf"
    for tool in (cc, nm, readelf):
        if not tool.is_file():
            raise ValueError(f"Missing Linux NDK tool: {tool}")
    ndk_lib = toolchain / "sysroot/usr/lib/aarch64-linux-android"
    root = Path(__file__).resolve().parent.parent
    binder = binder_functions(root / "standalone-build/third_party/binder-headers")
    output = args.output.resolve()
    if output.exists():
        raise ValueError(f"CI link-stub output already exists; use a fresh directory: {output}")
    lib64 = output / "lib64"
    sources = output / "sources"
    lib64.mkdir(parents=True)
    sources.mkdir()
    manifest = {"purpose": MARKER, "ndk_revision": NDK_REVISION, "api": args.api, "libraries": {}}
    for soname, (ndk_name, extra) in LIBRARIES.items():
        source = ndk_lib / str(args.api) / ndk_name
        exports = dynamic_exports(run(nm, "--dynamic", "--defined-only", "--extern-only", "--format=posix", source))
        additions = set(extra) | (binder if soname == "libbinder_ndk.so" else set())
        symbols = exports | additions
        c_file = sources / (soname + ".c")
        c_file.write_text(stub_source(symbols), encoding="ascii", newline="\n")
        library = lib64 / soname
        subprocess.run([
            str(cc), "-x", "c", "-std=c11", "-O0", "-fPIC", "-fno-stack-protector",
            "-nostdlib", "-shared", "-Wl,--no-undefined", "-Wl,--build-id=none",
            f"-Wl,-soname,{soname}", str(c_file), "-o", str(library),
        ], check=True)
        dynamic = run(readelf, "--dynamic", library)
        if f"[{soname}]" not in dynamic or "(NEEDED)" in dynamic:
            raise ValueError(f"Unexpected SONAME or dependency in generated link stub: {soname}")
        generated = dynamic_exports(run(nm, "--dynamic", "--defined-only", "--extern-only", "--format=posix", library))
        if generated != symbols or MARKER.encode("ascii") not in library.read_bytes():
            raise ValueError(f"Generated link-stub exports/marker mismatch: {soname}")
        manifest["libraries"][soname] = {
            "official_ndk_source": source.relative_to(ndk).as_posix(),
            "official_ndk_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
            "ndk_exports": sorted(exports), "added_header_or_platform_symbols": sorted(additions - exports),
        }
        print(f"CI-only link stub: {soname} ({len(symbols)} symbols)")
    cpp = ndk_lib / "libc++_shared.so"
    if not cpp.is_file():
        raise ValueError("Official NDK libc++_shared.so is missing")
    shutil.copyfile(cpp, lib64 / "libc++_shared.so")
    manifest["link_only_cpp_runtime"] = {
        "official_ndk_source": cpp.relative_to(ndk).as_posix(),
        "sha256": hashlib.sha256(cpp.read_bytes()).hexdigest(),
    }
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="ascii", newline="\n")
    (output / "LINK_ONLY_DO_NOT_SHIP.txt").write_text(
        MARKER + "\nGenerated stub functions trap. Never execute or package this directory.\n"
        "libc++_shared.so is the official NDK copy for linking, not an artifact.\n",
        encoding="ascii", newline="\n",
    )


if __name__ == "__main__":
    main()
