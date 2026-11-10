#!/usr/bin/env python3
"""Assemble a source-built KSU module without modifying the source tree."""
import argparse
from pathlib import Path
import hashlib
import stat
import struct
import zipfile

LIBRARIES = (
    'android.frameworks.cameraservice.common-V1-ndk.so',
    'android.frameworks.cameraservice.device-V3-ndk.so',
    'android.frameworks.cameraservice.service-V3-ndk.so',
    'android.hardware.common-V2-ndk.so',
    'android.hardware.common.fmq-V1-ndk.so',
    'libcamera2ndk_vendor.so', 'libc++_shared.so', 'libfacehal_jni.so',
    'libjni_stfaceunlock_api.so', 'libtensorflowlite.so',
    'libtensorflowlite_gpu_delegate.so',
)

def elf(path):
    data = path.read_bytes()
    if len(data) < 64 or data[:5] != b'\x7fELF\x02' or struct.unpack_from('<H', data, 18)[0] != 183:
        raise ValueError(f'Expected an arm64 ELF: {path.name}')
    return data

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--hal', type=Path, required=True)
    parser.add_argument('--bridge', type=Path, required=True)
    parser.add_argument('--probe', type=Path, required=True)
    parser.add_argument('--lib-dir', type=Path, required=True)
    parser.add_argument('--output', type=Path, default=Path('out/facehal-xiaomi-mtk.zip'))
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    files = {}
    templates = (
        'module.prop', 'control.sh', 'bridge_control.sh', 'service.sh', 'action.sh',
        'customize.sh', 'uninstall.sh', 'mode.conf', 'mirror.conf', 'skip_mount',
        'reference/device-manifest.xml',
    )
    module = root / 'module'
    unexpected = [p.relative_to(module).as_posix() for p in module.rglob('*')
                  if p.is_file() and p.relative_to(module).as_posix() not in templates]
    if unexpected:
        raise ValueError(f'Unexpected module template files: {unexpected}')
    for name in templates:
        path = module / name
        if not path.is_file() or path.is_symlink():
            raise ValueError(f'Missing or linked module template: {name}')
        data = path.read_bytes()
        if data.startswith(b'\xef\xbb\xbf') or b'\r' in data:
            raise ValueError(f'Expected BOM-free LF text: {name}')
        files[name] = data
    files['bin/facehal'] = elf(args.hal)
    files['bin/virtual_bridge'] = elf(args.bridge)
    probe = elf(args.probe)
    files['bin/binder_client'] = probe
    files['bin/bridge_client'] = probe
    for name in LIBRARIES:
        files['lib64/' + name] = elf(args.lib_dir / name)
    assert 'skip_mount' in files and not any(name.startswith('system/') for name in files)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(args.output, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
        for name, data in sorted(files.items()):
            info = zipfile.ZipInfo(name, (2026, 11, 11, 0, 0, 0))
            info.create_system = 3
            mode = 0o755 if name.endswith('.sh') or name.startswith('bin/') else 0o644
            info.external_attr = (stat.S_IFREG | mode) << 16
            info.compress_type = zipfile.ZIP_DEFLATED
            archive.writestr(info, data, compresslevel=6)
    with zipfile.ZipFile(args.output) as archive:
        assert archive.testzip() is None
    print(args.output)
    print('sha256=' + hashlib.sha256(args.output.read_bytes()).hexdigest())
    print('Private-library, CRC, LF and arm64 ELF checks passed')

if __name__ == '__main__':
    main()
