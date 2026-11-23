#!/usr/bin/env python3
"""Install the Linux GCC 13.2 LTO overlay without modifying either input tree.

Usage: install_overlay.py BASE_TOOLCHAIN BUILT_HOST_PREFIX OUTPUT_TOOLCHAIN
The shell preparer also uses --prepare-workdir BASE OUTPUT WORK_PARENT to
validate its inputs and allocate a new, private build directory.
"""
import os
from pathlib import Path
import re
import secrets
import shutil
import subprocess
import sys

VERSION_DIR = "gcc/sh2eb-elf/13.2.0"
WRITE_DIRS = ("bin", "sh2eb-elf/bin", "libexec/" + VERSION_DIR,
              "lib/" + VERSION_DIR, "lib/bfd-plugins")
HOST_TOOLS = ("addr2line", "ar", "c++filt", "elfedit", "ld", "ld.bfd",
              "nm", "objcopy", "objdump", "ranlib", "readelf", "size",
              "strings", "strip")
TARGET_TOOLS = ("ar", "ld", "ld.bfd", "nm", "objcopy", "objdump",
                "ranlib", "readelf", "strip")
PLUGIN = "libexec/" + VERSION_DIR + "/liblto_plugin.so"
SPECS = "lib/" + VERSION_DIR + "/specs"
OLD_PLUGIN = "%{fuse-linker-plugin:    %e-fuse-linker-plugin is not supported in this configuration}"
NEW_PLUGIN = "%{!fno-use-linker-plugin:%{!fno-lto: -plugin %(linker_plugin_file) -plugin-opt=%(lto_wrapper) -plugin-opt=-fresolution=%u.res }}"


def checked_path(value):
    """Reject unsupported characters and symlinks before canonicalizing a path."""
    value = os.fspath(value)
    if not re.fullmatch(r"[A-Za-z0-9_./+\-]+", value):
        raise ValueError("Unsupported characters in path: " + value)
    path = Path(value).absolute()
    # Check the caller's spelling, including prefixes before '..'. Resolving
    # first would erase evidence of a symlinked parent.
    for component in (*reversed(path.parents), path):
        if component.is_symlink():
            raise ValueError("Symlinked path component is unsupported: " + str(component))
    return Path(os.path.abspath(path))


def overlaps(first, second):
    return first == second or first in second.parents or second in first.parents


def require_directory(path):
    path = checked_path(path)
    if not path.is_dir():
        raise ValueError("Required directory is missing: " + str(path))


def require_file(path):
    path = checked_path(path)
    if not path.is_file():
        raise ValueError("Required regular file is missing: " + str(path))


def new_output(value):
    output = checked_path(value)
    if output.exists():
        raise ValueError("Output must be a new directory: " + str(output))
    return output


def inspect_base(base):
    require_directory(base)
    for relative in WRITE_DIRS:
        require_directory(base / relative)
    # Other symlinks are preserved without following them, but all later write
    # parents and the specs leaf must be independent of the input tree.
    checked_path(base / SPECS)
    compiler = base / "bin/sh2eb-elf-gcc"
    require_file(compiler)
    machine = subprocess.check_output([str(compiler), "-dumpmachine"], text=True).strip()
    version = subprocess.check_output([str(compiler), "-dumpfullversion"], text=True).strip()
    if machine != "sh2eb-elf" or version != "13.2.0":
        raise ValueError("Expected the supplied sh2eb-elf GCC 13.2.0 base toolchain")
    specs = subprocess.check_output([str(compiler), "-dumpspecs"], text=True)
    if specs.count(OLD_PLUGIN) != 1 or specs.count("*linker_plugin_file:\n\n") != 1:
        raise ValueError("Unexpected GCC specs; refusing automatic replacement")
    return specs


def prepare_workdir(base_value, output_value, parent_value):
    output = new_output(output_value)
    base = checked_path(base_value)
    parent = checked_path(parent_value)
    if overlaps(base, output):
        raise ValueError("Output and base toolchain must be disjoint")
    # An ancestor such as /tmp is a valid parent; the allocated child must be
    # disjoint. A parent inside either protected tree is never acceptable.
    if parent == base or base in parent.parents or parent == output or output in parent.parents:
        raise ValueError("Build directory parent must be outside base and output")
    inspect_base(base)
    if parent.exists() and not parent.is_dir():
        raise ValueError("Build directory parent is not a directory: " + str(parent))
    # Every check above is read-only. Always allocate a new private child;
    # existing files in a caller-supplied parent are never extraction targets.
    parent.mkdir(parents=True, exist_ok=True)
    for _ in range(100):
        candidate = parent / ("spang-toolchain-lto-" + secrets.token_hex(8))
        if overlaps(candidate, base) or overlaps(candidate, output):
            continue
        try:
            candidate.mkdir(mode=0o700)
        except FileExistsError:
            continue
        return candidate
    raise ValueError("Could not allocate a fresh build directory")


def install_overlay(base_value, built_value, output_value):
    output = new_output(output_value)
    base, built = checked_path(base_value), checked_path(built_value)
    if overlaps(output, base) or overlaps(output, built) or overlaps(base, built):
        raise ValueError("Base, built prefix, and output must be disjoint")
    specs = inspect_base(base)
    require_directory(built)
    sources = [("bin/sh2eb-elf-" + name) for name in HOST_TOOLS]
    sources += [("sh2eb-elf/bin/" + name) for name in TARGET_TOOLS]
    sources.append(PLUGIN)
    for relative in sources:
        require_file(built / relative)
    # Validate all inputs and write-parent chains before creating any output.
    shutil.copytree(base, output, symlinks=True)

    def install(relative):
        destination = output / relative
        require_directory(destination.parent)
        if destination.exists() or destination.is_symlink():
            destination.unlink()
        shutil.copy2(built / relative, destination)
        subprocess.run(["strip", "--strip-unneeded", str(destination)], check=True)

    for relative in sources:
        install(relative)
    plugin = output / PLUGIN
    for relative in ("lib/" + VERSION_DIR + "/liblto_plugin.so",
                     "lib/bfd-plugins/liblto_plugin.so"):
        destination = output / relative
        require_directory(destination.parent)
        if destination.exists() or destination.is_symlink():
            destination.unlink()
        destination.symlink_to(os.path.relpath(plugin, destination.parent))
    specs = specs.replace(OLD_PLUGIN, NEW_PLUGIN)
    specs = specs.replace("*linker_plugin_file:\n\n",
                          "*linker_plugin_file:\n%:find-file(liblto_plugin.so)\n")
    # Preserve the COMPLETE target specs, including SH ELF emulation.
    destination = output / SPECS
    require_directory(destination.parent)
    if destination.exists() or destination.is_symlink():
        destination.unlink()
    destination.write_text(specs)
    return output


def main(argv):
    try:
        if len(argv) == 5 and argv[1] == "--prepare-workdir":
            result = prepare_workdir(*argv[2:])
        elif len(argv) == 4:
            result = install_overlay(*argv[1:])
        else:
            raise ValueError(__doc__)
        print(result)
        return 0
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        print("Toolchain preparation error: " + str(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
