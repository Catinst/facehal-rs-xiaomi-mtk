#!/usr/bin/env python3
"""Small host-only contract tests; no target ELF or link stub is executed."""

from pathlib import Path
import struct
import unittest

import prepare_link_stubs as stubs
import stage_artifact as artifact

ROOT = Path(__file__).resolve().parent.parent


class LinkStubTests(unittest.TestCase):
    def test_dynamic_exports_skip_labels_and_undefined(self):
        text = "LIBNDK A 0 0\nACamera_open@@LIBNDK T 100 4\nAImage_get W 120 4\nmissing U\nweak_missing w\n"
        self.assertEqual(stubs.dynamic_exports(text), {"ACamera_open", "AImage_get"})

    def test_invalid_or_empty_exports_fail(self):
        for text in ("missing U\n", "bad-name T 100 4\n"):
            with self.assertRaises(ValueError):
                stubs.dynamic_exports(text)

    def test_vendored_platform_names_are_present(self):
        symbols = stubs.binder_functions(ROOT / "standalone-build/third_party/binder-headers")
        required = {"ABinderProcess_startThreadPool", "ABinderProcess_joinThreadPool",
                    "AIBinder_markVendorStability", "AParcel_markSensitive",
                    "AServiceManager_addService", "AIBinder_Class_setTransactionCodeToFunctionNameMap"}
        self.assertTrue(required.issubset(symbols))

    def test_sources_have_marker_and_only_trapping_functions(self):
        source = stubs.stub_source({"AParcel_fixture", "AIBinder_fixture"})
        self.assertIn(stubs.MARKER, source)
        self.assertEqual(source.count("__builtin_trap();"), 2)
        self.assertIn(".facehal_ci_link_only", source)
        with self.assertRaises(ValueError):
            stubs.stub_source({"symbol; injected"})


class ArtifactTests(unittest.TestCase):
    @staticmethod
    def elf_header():
        data = bytearray(64)
        data[:6] = b"\x7fELF\x02\x01"
        struct.pack_into("<HH", data, 16, 3, 183)
        return bytes(data)

    def test_aarch64_header_is_accepted(self):
        artifact.validate_elf(self.elf_header(), "fixture")

    def test_stubs_and_wrong_arch_are_rejected(self):
        invalid = [b"text", self.elf_header() + stubs.MARKER.encode("ascii"),
                   self.elf_header() + b".facehal_ci_link_only"]
        x86 = bytearray(self.elf_header())
        struct.pack_into("<H", x86, 18, 62)
        invalid.append(bytes(x86))
        for data in invalid:
            with self.assertRaises(ValueError):
                artifact.validate_elf(data, "fixture")

    def test_templates_are_text_only(self):
        for name in artifact.TEMPLATES:
            artifact.validate_text((ROOT / "module" / name).read_bytes(), name)
        for data in (b"\xef\xbb\xbftext", b"x\r\n", b"x\0", b"\x7fELF", b"\xff"):
            with self.assertRaises((ValueError, UnicodeDecodeError)):
                artifact.validate_text(data, "fixture")

    def test_scoped_source_encoding(self):
        paths = [ROOT / ".github/workflows/manual-android-build.yml", ROOT / "docs/ci.md"]
        paths += sorted((ROOT / "ci").glob("*.py")) + sorted((ROOT / "ci").glob("*.sh"))
        for path in paths:
            artifact.validate_text(path.read_bytes(), path.name)

    def test_workflow_manual_readonly_and_fixed_upload(self):
        text = (ROOT / ".github/workflows/manual-android-build.yml").read_text(encoding="utf-8")
        self.assertIn("  workflow_dispatch:", text)
        self.assertIn("  contents: read", text)
        self.assertIn("          persist-credentials: false", text)
        self.assertIn("          path: out/ci-artifact/", text)
        self.assertIn("FACEHAL_VERBOSE_LOGS: ${{ inputs.verbose_logs && '1' || '0' }}", text)
        self.assertNotIn("  push:", text)
        self.assertNotIn("  pull_request:", text)
        self.assertNotIn("gh run", text)
        self.assertNotIn("gh workflow", text)
        self.assertNotIn("contents: write", text)
        self.assertEqual(len(artifact.ELFS), 3)


if __name__ == "__main__":
    unittest.main()
