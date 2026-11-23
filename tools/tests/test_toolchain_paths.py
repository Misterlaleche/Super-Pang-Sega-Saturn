"""Filesystem regression checks for toolchain preparation; no builds or network."""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

TOOLS = Path(__file__).resolve().parents[1]
INSTALL = TOOLS / "install_overlay.py"
PREPARE = TOOLS / "prepare_linux_toolchain.sh"
GCC_LIB = Path("lib/gcc/sh2eb-elf/13.2.0")
PLUGIN = Path("libexec/gcc/sh2eb-elf/13.2.0/liblto_plugin.so")
SPECS = "*linker_plugin_file:\n\n*link_command:\n%{fuse-linker-plugin:    %e-fuse-linker-plugin is not supported in this configuration}\n"


class ToolchainPathTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="spang-toolchain-paths-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.base = self.root / "base"
        self.built = self.root / "built"
        self.output = self.root / "overlay"
        self.external = self.root / "external"
        self.external.mkdir()
        for directory in ["bin", "sh2eb-elf/bin", str(GCC_LIB),
                          "lib/bfd-plugins", str(PLUGIN.parent)]:
            (self.base / directory).mkdir(parents=True, exist_ok=True)
        for directory in ["bin", "sh2eb-elf/bin", str(PLUGIN.parent)]:
            (self.built / directory).mkdir(parents=True, exist_ok=True)
        compiler = self.base / "bin/sh2eb-elf-gcc"
        compiler.write_text(
            "#!" + sys.executable + "\nimport sys\n"
            "answers = {'-dumpmachine': 'sh2eb-elf', '-dumpfullversion': '13.2.0', '-dumpspecs': "
            + repr(SPECS) + "}\nprint(answers[sys.argv[1]])\n"
        )
        compiler.chmod(0o755)
        (self.base / "bin/sh2eb-elf-ld").write_text("original linker\n")
        (self.base / "sh2eb-elf/bin/ld").write_text("original linker\n")
        for name in ["addr2line", "ar", "c++filt", "elfedit", "ld", "ld.bfd", "nm", "objcopy", "objdump", "ranlib", "readelf", "size", "strings", "strip"]:
            binary = self.built / ("bin/sh2eb-elf-" + name)
            binary.write_text("replacement linker\n")
            binary.chmod(0o755)
        for name in ["ar", "ld", "ld.bfd", "nm", "objcopy", "objdump", "ranlib", "readelf", "strip"]:
            binary = self.built / "sh2eb-elf/bin" / name
            binary.write_text("replacement linker\n")
            binary.chmod(0o755)
        (self.built / PLUGIN).write_text("plugin placeholder\n")
        # No real binaries are built: stripping is irrelevant to these path tests.
        fake_bin = self.root / "fake-bin"
        fake_bin.mkdir()
        strip = fake_bin / "strip"
        strip.write_text("#!/bin/sh\nexit 0\n")
        strip.chmod(0o755)
        # The shell invokes Python with "-" only for its download block. Intercept
        # that boundary so regressions fail offline rather than contacting GNU.
        self.download_marker = self.root / "download-attempted"
        python = fake_bin / "python3"
        python.write_text(
            "#!" + sys.executable + "\nimport os, pathlib, sys\n"
            "if len(sys.argv) > 1 and sys.argv[1] == '-':\n"
            "    pathlib.Path(os.environ['SPANG_TEST_DOWNLOAD_MARKER']).write_text('attempted')\n"
            "    raise SystemExit(97)\n"
            "os.execv(" + repr(sys.executable) + ", [" + repr(sys.executable) + "] + sys.argv[1:])\n"
        )
        python.chmod(0o755)
        self.env = dict(os.environ, PATH=str(fake_bin) + os.pathsep + os.environ["PATH"],
                        SPANG_TEST_DOWNLOAD_MARKER=str(self.download_marker))

    def install(self, output=None):
        return subprocess.run(
            [sys.executable, str(INSTALL), str(self.base), str(self.built), str(output or self.output)],
            text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            env=self.env, timeout=15,
        )

    def assert_rejected_without_output(self, result, output=None):
        path = output or self.output
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertFalse(path.exists(), "Rejected input left a partial output: " + str(path))

    def test_safe_siblings_install_independent_output(self):
        result = self.install()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual((self.base / "bin/sh2eb-elf-ld").read_text(), "original linker\n")
        self.assertEqual((self.output / "bin/sh2eb-elf-ld").read_text(), "replacement linker\n")
        self.assertIn("-plugin", (self.output / GCC_LIB / "specs").read_text())

    def test_existing_output_is_preserved(self):
        self.output.mkdir()
        marker = self.output / "keep.txt"
        marker.write_text("keep existing output")
        result = self.install()
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(marker.read_text(), "keep existing output")
        self.assertEqual(list(self.output.iterdir()), [marker])

    def test_output_nested_in_base_is_rejected_before_copy(self):
        output = self.base / "overlay"
        self.assert_rejected_without_output(self.install(output), output)

    def test_output_nested_in_built_prefix_is_rejected_before_copy(self):
        output = self.built / "overlay"
        self.assert_rejected_without_output(self.install(output), output)

    def test_base_bin_symlink_cannot_replace_external_linker(self):
        external_bin = self.external / "bin"
        shutil.move(str(self.base / "bin"), external_bin)
        (self.base / "bin").symlink_to(external_bin, target_is_directory=True)
        marker = external_bin / "sh2eb-elf-ld"
        result = self.install()
        self.assertEqual(marker.read_text(), "original linker\n")
        self.assert_rejected_without_output(result)

    def test_base_lib_symlink_cannot_write_external_specs(self):
        external_lib = self.external / "lib"
        shutil.move(str(self.base / "lib"), external_lib)
        (self.base / "lib").symlink_to(external_lib, target_is_directory=True)
        marker = external_lib / "gcc/sh2eb-elf/13.2.0/specs"
        marker.write_text("keep external specs")
        result = self.install()
        self.assertEqual(marker.read_text(), "keep external specs")
        self.assert_rejected_without_output(result)

    def test_specs_leaf_symlink_never_overwrites_target(self):
        marker = self.external / "specs"
        marker.write_text("keep symlink target")
        (self.base / GCC_LIB / "specs").symlink_to(marker)
        result = self.install()
        self.assertEqual(marker.read_text(), "keep symlink target")
        if result.returncode == 0:
            self.assertFalse((self.output / GCC_LIB / "specs").is_symlink())
        else:
            self.assertFalse(self.output.exists())

    def test_symlinked_output_parent_cannot_create_external_tree(self):
        parent = self.root / "output-parent"
        parent.symlink_to(self.external, target_is_directory=True)
        result = self.install(parent / "overlay")
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(list(self.external.iterdir()), [])

    def test_missing_built_plugin_is_rejected_before_copy(self):
        (self.built / PLUGIN).unlink()
        self.assert_rejected_without_output(self.install())

    def test_make_metacharacter_in_output_is_rejected_before_copy(self):
        output = self.root / "overlay#unsafe"
        self.assert_rejected_without_output(self.install(output), output)

    def test_configured_work_parent_gets_a_fresh_child(self):
        parent = self.root / "work-parent"
        parent.mkdir()
        marker = parent / "keep.txt"
        marker.write_text("existing work files")
        linked_source = parent / "binutils-2.40"
        linked_source.symlink_to(self.external, target_is_directory=True)
        env = dict(self.env, SPANG_TOOLCHAIN_BUILD_DIR=str(parent))
        result = subprocess.run(
            ["bash", str(PREPARE), str(self.base), str(self.output)],
            text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            env=env, timeout=15,
        )
        # The download stub intentionally stops execution after path setup.
        self.assertEqual(result.returncode, 97, result.stdout + result.stderr)
        self.assertTrue(self.download_marker.exists())
        self.assertEqual(marker.read_text(), "existing work files")
        self.assertTrue(linked_source.is_symlink())
        children = set(parent.iterdir()) - {marker, linked_source}
        self.assertEqual(len(children), 1, "Preparation reused the supplied work parent")
        child = children.pop()
        self.assertTrue(child.is_dir())
        self.assertFalse(child.is_symlink())
        self.assertEqual(list(self.external.iterdir()), [])
        self.assertFalse(self.output.exists())

    def test_work_directory_equal_to_output_is_rejected_without_creation(self):
        env = dict(self.env, SPANG_TOOLCHAIN_BUILD_DIR=str(self.output))
        result = subprocess.run(
            ["bash", str(PREPARE), str(self.base), str(self.output)],
            text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            env=env, timeout=15,
        )
        self.assert_rejected_without_output(result)
        self.assertFalse(self.download_marker.exists())

    def test_work_directory_inside_base_is_rejected_without_creation(self):
        work = self.base / "unsafe-work"
        env = dict(self.env, SPANG_TOOLCHAIN_BUILD_DIR=str(work))
        result = subprocess.run(
            ["bash", str(PREPARE), str(self.base), str(self.output)],
            text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            env=env, timeout=15,
        )
        self.assert_rejected_without_output(result)
        self.assertFalse(work.exists())
        self.assertFalse(self.download_marker.exists())


if __name__ == "__main__":
    unittest.main()
