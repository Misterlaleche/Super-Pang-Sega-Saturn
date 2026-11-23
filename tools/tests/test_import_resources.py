"""Resource integrity and filesystem boundaries, using small private fixtures.

Run: python3 -m unittest discover -s tools/tests -v
No game resources or downloaded tools are required by these tests.
"""
from __future__ import annotations

import hashlib
import importlib.util
import json
import os
from pathlib import Path
import stat
import sys
import tempfile
import unittest
from unittest.mock import patch
import warnings
import zipfile


SCRIPT = Path(__file__).resolve().parents[1] / "import_resources.py"


class ResourceImportTests(unittest.TestCase):
    def setUp(self):
        self.assertTrue(SCRIPT.is_file(), "The resource importer is not implemented yet")
        spec = importlib.util.spec_from_file_location("resource_importer", SCRIPT)
        self.importer = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.importer)
        self.temp = tempfile.TemporaryDirectory(prefix="spang-import-test-")
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)
        self.destination = self.base / "checkout"
        self.destination.mkdir()
        self.archive = self.base / "own resources.zip"
        self.files = {
            "cd/ART/TEST.BIN": b"owned art fixture\x00\x01",
            "frontend/data.c": b"const unsigned char fixture[] = {1, 2};\n",
            "Compiler/COMMON/IP.BIN": b"owned boot fixture",
        }
        self.manifest_path = self.base / "manifest.json"
        self.write_archive()

    def write_archive(self, *, entries=None, extras=()):
        entries = self.files if entries is None else entries
        with warnings.catch_warnings():
            warnings.simplefilter("ignore", UserWarning)
            with zipfile.ZipFile(self.archive, "w", zipfile.ZIP_DEFLATED) as archive:
                for name, data in entries.items():
                    archive.writestr("native320_r09/" + name, data)
                for name, data in extras:
                    archive.writestr(name, data)
        self.write_manifest()

    def write_manifest(self):
        self.manifest_path.write_text(json.dumps({
            "schema_version": 1,
            "source_archive": {
                "filename": "SuperPang_1.4_FUENTES.zip",
                "root": "native320_r09",
                "size": self.archive.stat().st_size,
                "sha256": hashlib.sha256(self.archive.read_bytes()).hexdigest(),
            },
            "files": [
                {"path": name, "size": len(data),
                 "sha256": hashlib.sha256(data).hexdigest(), "purpose": "test fixture"}
                for name, data in self.files.items()
            ],
        }), encoding="utf-8")
        self.manifest = self.importer.load_manifest(self.manifest_path)

    def import_now(self, source=None, *, dry_run=False):
        return self.importer.import_resources(
            source or self.archive, self.destination, self.manifest, dry_run=dry_run
        )

    def installed(self):
        return {p.relative_to(self.destination).as_posix(): p.read_bytes()
                for p in self.destination.rglob("*") if p.is_file()}

    def test_import_is_idempotent_and_never_copies_unlisted_files(self):
        self.write_archive(extras=[("native320_r09/private/rom.zip", b"excluded"),
                                   ("README.txt", b"excluded archive metadata")])
        self.import_now()
        self.assertEqual(self.installed(), self.files)
        times = {name: (self.destination / name).stat().st_mtime_ns for name in self.files}
        self.import_now()
        self.assertEqual(self.installed(), self.files)
        self.assertEqual(times, {name: (self.destination / name).stat().st_mtime_ns
                                 for name in self.files})
        self.importer.check_resources(self.destination, self.manifest)

    def test_wrong_archive_identity_or_invalid_zip_writes_nothing(self):
        self.archive.write_bytes(b"different archive")
        with self.assertRaises(self.importer.ResourceError):
            self.import_now()
        self.assertEqual(self.installed(), {})
        self.write_manifest()  # Correct hash for invalid ZIP: parsing must also fail.
        with self.assertRaises(self.importer.ResourceError):
            self.import_now()
        self.assertEqual(self.installed(), {})

    def test_missing_or_corrupt_allowed_resource_cannot_leave_partial_import(self):
        for mode in ("missing", "corrupt"):
            with self.subTest(mode=mode):
                entries = dict(self.files)
                last = next(reversed(entries))
                if mode == "missing":
                    del entries[last]
                else:
                    entries[last] = b"corrupt resource"
                self.write_archive(entries=entries)
                with self.assertRaises(self.importer.ResourceError):
                    self.import_now()
                self.assertEqual(self.installed(), {})

    def test_existing_edit_is_preserved_without_installing_other_files(self):
        target = self.destination / "Compiler/COMMON/IP.BIN"
        target.parent.mkdir(parents=True)
        target.write_bytes(b"local work must survive")
        with self.assertRaises(self.importer.ResourceError):
            self.import_now()
        self.assertEqual(self.installed(), {"Compiler/COMMON/IP.BIN": b"local work must survive"})

    def test_dry_run_validates_without_creating_resources(self):
        self.import_now(dry_run=True)
        self.assertEqual(list(self.destination.iterdir()), [])

    def test_extracted_directory_works_and_checks_every_input_before_writing(self):
        source = self.base / "native320_r09"
        for name, data in self.files.items():
            path = source / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        (source / "extra.bin").write_bytes(b"do not import")
        last = source / "Compiler/COMMON/IP.BIN"
        last.write_bytes(b"bad")
        with self.assertRaises(self.importer.ResourceError):
            self.import_now(source)
        self.assertEqual(self.installed(), {})
        last.write_bytes(self.files["Compiler/COMMON/IP.BIN"])
        self.import_now(source)
        self.assertEqual(self.installed(), self.files)

    def test_archive_path_tricks_duplicates_and_symlinks_are_rejected(self):
        link = zipfile.ZipInfo("native320_r09/private/link")
        link.create_system = 3
        link.external_attr = (stat.S_IFLNK | 0o777) << 16
        cases = [
            ("native320_r09/../../outside.bin", b"escape"),
            ("/absolute.bin", b"escape"),
            ("native320_r09\\cd\\ART\\TEST.BIN", b"ambiguous"),
            ("native320_r09//extra.bin", b"ambiguous"),
            ("native320_r09/cd/ART/TEST.BIN", b"duplicate"),
            (link, b"../../outside.bin"),
        ]
        for entry in cases:
            with self.subTest(entry=str(entry[0])):
                self.write_archive(extras=[entry])
                with self.assertRaises(self.importer.ResourceError):
                    self.import_now()
                self.assertEqual(self.installed(), {})
                self.assertFalse((self.base / "outside.bin").exists())

    def test_symlink_in_destination_cannot_redirect_an_import(self):
        outside = self.base / "outside"
        outside.mkdir()
        (self.destination / "cd").symlink_to(outside, target_is_directory=True)
        with self.assertRaises(self.importer.ResourceError):
            self.import_now()
        self.assertEqual(list(outside.iterdir()), [])
        self.assertFalse((self.destination / "frontend").exists())

    def test_symlink_in_source_is_rejected_even_when_target_has_correct_bytes(self):
        source = self.base / "native320_r09"
        for name, data in self.files.items():
            path = source / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        linked = source / "frontend/data.c"
        linked.unlink()
        outside = self.base / "outside.c"
        outside.write_bytes(self.files["frontend/data.c"])
        linked.symlink_to(outside)
        with self.assertRaises(self.importer.ResourceError):
            self.import_now(source)
        self.assertEqual(self.installed(), {})

    def test_check_detects_missing_or_modified_installed_resources(self):
        with self.assertRaises(self.importer.ResourceError):
            self.importer.check_resources(self.destination, self.manifest)
        self.import_now()
        self.importer.check_resources(self.destination, self.manifest)
        (self.destination / "frontend/data.c").write_bytes(b"local modification")
        with self.assertRaises(self.importer.ResourceError):
            self.importer.check_resources(self.destination, self.manifest)

    def test_build_preflight_preserves_previous_output_when_dependencies_are_missing(self):
        build_script = SCRIPT.with_name("build.py")
        self.assertTrue(build_script.is_file(), "The build preflight is not implemented yet")
        sys.path.insert(0, str(SCRIPT.parent))
        self.addCleanup(sys.path.remove, str(SCRIPT.parent))
        spec = importlib.util.spec_from_file_location("resource_build", build_script)
        build_module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(build_module)
        resources = self.destination / "resources"
        resources.mkdir()
        (resources / "manifest.json").write_bytes(self.manifest_path.read_bytes())
        previous = self.destination / "game.elf"
        previous.write_bytes(b"previous build")
        with self.assertRaises(build_module.BuildError):
            build_module.build(self.destination, toolchain_root=None)
        self.assertEqual(previous.read_bytes(), b"previous build")
        self.import_now()
        with self.assertRaises(build_module.BuildError):
            build_module.build(self.destination, toolchain_root=str(self.base / "missing-toolchain"))
        self.assertEqual(previous.read_bytes(), b"previous build")

    def test_compiler_without_usable_lto_is_rejected_before_make_clean(self):
        sys.path.insert(0, str(SCRIPT.parent))
        self.addCleanup(sys.path.remove, str(SCRIPT.parent))
        spec = importlib.util.spec_from_file_location("resource_build_lto", SCRIPT.with_name("build.py"))
        build_module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(build_module)
        resources = self.destination / "resources"
        resources.mkdir()
        (resources / "manifest.json").write_bytes(self.manifest_path.read_bytes())
        self.import_now()
        (self.destination / "jo_engine").mkdir()
        (self.destination / "makefile").write_text("all:\n\ttrue\n")
        (self.destination / "build.sh").write_text("#!/bin/sh\nexit 0\n")
        previous = self.destination / "game.elf"
        previous.write_bytes(b"previous build")
        toolchain = self.base / "toolchain"
        (toolchain / "bin").mkdir(parents=True)

        def executable(path, text):
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(text)
            path.chmod(0o755)

        # The compiler reports the expected version and target but fails a real
        # compile/link request, reproducing the incomplete LTO package boundary.
        executable(toolchain / "bin/sh2eb-elf-gcc", """#!/usr/bin/env python3
import sys
if sys.argv[1:] == ['-dumpfullversion']:
    print('13.2.0')
elif sys.argv[1:] == ['-dumpmachine']:
    print('sh2eb-elf')
else:
    print('linker plugin is not supported in this configuration', file=sys.stderr)
    raise SystemExit(1)
""")
        for name in ("objcopy", "nm", "objdump"):
            executable(toolchain / ("bin/sh2eb-elf-" + name), "#!/bin/sh\nexit 0\n")
        executable(self.destination / "iso-runtime/usr/bin/xorrisofs", "#!/bin/sh\nexit 0\n")
        clean_marker = self.base / "make-was-called"
        host = self.base / "host-bin"
        executable(host / "make", "#!/usr/bin/env python3\nfrom pathlib import Path\n"
                   + f"Path({str(clean_marker)!r}).write_text('called')\n")
        with patch.dict(os.environ, {"PATH": str(host) + os.pathsep + os.environ["PATH"]}):
            with self.assertRaises(build_module.BuildError):
                build_module.build(self.destination, toolchain_root=str(toolchain))
        self.assertFalse(clean_marker.exists(), "Incomplete LTO must be caught before make clean")
        self.assertEqual(previous.read_bytes(), b"previous build")


if __name__ == "__main__":
    unittest.main()
